# API reference

Every declaration lives in [`include/ppack.h`](../include/ppack.h) and [`include/ppack_platform.h`](../include/ppack_platform.h), which carry the full Doxygen contract. Buffer sizing and the configuration macros are in the [integration guide](integration.md#buffer-sizing), and the wire format in [design.md](design.md#wire-format).

## Conventions

- The caller owns the structure, the payload buffer, and the field table. The library allocates nothing and keeps no global state.
- Both functions validate their arguments and return `PPACK_SUCCESS` (0) or a negated error code, such as `-PPACK_ERR_NULLPTR`. There is no `errno`.
- The codec cannot check what a pointer refers to. The payload buffer must hold at least `payload_bits / 8` elements of `ppack_byte_t`, and every `ptr_offset` must name a member of the type the field requires.
- A call is not thread-safe. Provide mutual exclusion when the structure or the payload is shared between threads or between an interrupt handler and the main loop.
- Keep field tables `static const`. The work per call is then fixed by the table, which is what makes it bounded.

## Field descriptors

<!-- snippet: skip -->
```c
enum ppack_type {
        PPACK_TYPE_UINT8 = 0, /* member: uint8_t  */
        PPACK_TYPE_UINT16,    /* member: uint16_t */
        PPACK_TYPE_INT16,     /* member: int16_t  */
        PPACK_TYPE_INT32,     /* member: int32_t  */
        PPACK_TYPE_UINT32,    /* member: uint32_t */
        PPACK_TYPE_F32,       /* member: float    */
        PPACK_TYPE_BITS,      /* member: uint32_t */
};

enum ppack_behaviour {
        PPACK_BEHAVIOUR_RAW,
        PPACK_BEHAVIOUR_SCALED,
};

struct ppack_field {
        enum ppack_type type;
        uint16_t start_bit;           /* first payload bit */
        uint16_t bit_length;          /* 1 to 32 */
        size_t ptr_offset;            /* offsetof() into the structure */
        float scale;                  /* scaled fields only */
        float offset;                 /* scaled fields only */
        enum ppack_behaviour behaviour;
};
```

A field copies `bit_length` bits between the payload, starting at payload bit `start_bit`, and the structure member at `ptr_offset`. With `PPACK_BEHAVIOUR_RAW` the member has the C type listed against its `type`. With `PPACK_BEHAVIOUR_SCALED` the member is always a `float`, and `type` names the integer that travels on the wire; see [Scaled fields](#scaled-fields).

`PPACK_BEHAVIOUR_RAW` is zero, so a designated initialiser that leaves out `behaviour`, `scale` and `offset` describes a raw field. `PPACK_TYPE_F32` and `PPACK_TYPE_BITS` ignore `behaviour`, `scale` and `offset` and always copy the bits. `PPACK_TYPE_UINT8` has no scaled form.

Set `bit_length` to the natural width of the type unless the protocol says otherwise. The library accepts any width from 1 to 32 for any type:

- On pack, a narrower field keeps the low `bit_length` bits of the value. A wider field fills the extra wire bits with zeros for an unsigned type and with copies of the sign bit for a signed one.
- On unpack, `PPACK_TYPE_INT16` and `PPACK_TYPE_INT32` sign-extend from the top bit of the field; the other types zero-extend. The result is then converted to the member's type, so a 12-bit `PPACK_TYPE_INT16` field carries -2048 to 2047, and a 12-bit `PPACK_TYPE_UINT16` field carries 5000 as 904.

Always compute `ptr_offset` with `offsetof`. On a target where `CHAR_BIT` is 16, `offsetof` counts 16-bit units, which is what the library expects; an offset counted in octets by hand is wrong there. A `PPACK_TYPE_UINT8` member occupies a 16-bit unit on such a target: pack sends its low 8 bits, and unpack writes the whole unit, with the upper 8 bits cleared.

## Pack and unpack

<!-- snippet: skip -->
```c
int ppack_pack(const void *base_ptr, void *payload, size_t payload_bits,
               const struct ppack_field *fields, size_t field_count);
int ppack_unpack(void *base_ptr, const void *payload, size_t payload_bits,
                 const struct ppack_field *fields, size_t field_count);
```

`ppack_pack` clears the first `payload_bits` bits of `payload`, then writes each field from the structure at `base_ptr`, in table order. Bits that no field covers read as zero. A later field that overlaps an earlier one overwrites the shared bits.

`ppack_unpack` reads each field from `payload` and writes it to the structure at `base_ptr`, in table order. It does not modify the payload, and members that no field names keep their values.

`payload_bits` must be a positive multiple of 8 and no greater than `PPACK_MAX_PAYLOAD_BITS`, 65,528. Common values are 64 for a classic CAN frame and 512 for a full CAN-FD frame. Transport limits are the caller's to enforce.

Neither call rolls back on failure. When a field fails validation, the fields before it have already been written: pack has cleared the payload and written them into it, and unpack has written them into the structure. Do not transmit a payload after a non-zero return, and decode into a temporary structure when a partial update would be visible to other code, as the [stored-record recipe](#stored-record) shows.

## Error codes

| Code | Value | Returned when |
| --- | --- | --- |
| `PPACK_SUCCESS` | 0 | Every field was copied. |
| `PPACK_ERR_INVALARG` | 1 | `fields` is `NULL` or `field_count` is 0; `payload_bits` is 0, not a multiple of 8, or above the maximum; a `bit_length` is 0 or above 32; a `PPACK_TYPE_UINT8` field is scaled; or pack computes NaN for a scaled field. |
| `PPACK_ERR_NOTFOUND` | 3 | A field has an unknown `type`. |
| `PPACK_ERR_NULLPTR` | 4 | `base_ptr` or `payload` is `NULL`. |
| `PPACK_ERR_OVERFLOW` | 5 | A field ends beyond `payload_bits`, or pack finds a scaled field with a `scale` of zero. |

Functions return the negated code. Code 2 is reserved.

The checks run in a fixed order, and the first failure is returned: the two pointers, then the field table, then `payload_bits`, then each field in table order. Within a field, the width is checked first, then its position, then its type and scaling. A `NULL` payload with an empty table therefore returns `-PPACK_ERR_NULLPTR`, and an unknown type with a zero width returns `-PPACK_ERR_INVALARG`.

## Scaled fields

A scaled field carries a `float` physical value as an integer of the field's `type`:

<!-- snippet: skip -->
```c
raw      = (physical - offset) / scale;   /* pack */
physical = (float)raw * scale + offset;   /* unpack */
```

`PPACK_TYPE_UINT16`, `PPACK_TYPE_INT16`, `PPACK_TYPE_UINT32` and `PPACK_TYPE_INT32` can be scaled.

**Rounding.** Pack converts to the integer by truncation toward zero, so the round-trip error is less than one step, `scale`. 12.349 at 0.01 per step arrives as 12.34, and -1.5 at a step of 1 arrives as -1. For round to nearest, add half a step in the direction of the value before packing, as the [recipe](#round-to-nearest) shows.

**Saturation.** Pack clamps the raw value to the range of the field's `type`, not to its `bit_length`, and an out-of-range input is not an error. `PPACK_TYPE_UINT16` clamps to 0 to 65,535 and `PPACK_TYPE_INT16` to -32,768 to 32,767. For `PPACK_TYPE_UINT32` the upper clamp is 4,294,967,040 and for `PPACK_TYPE_INT32` it is 2,147,483,520, the largest `float` values below `UINT32_MAX` and `INT32_MAX`; those two maxima round up to 2³² and 2³¹ as `float`, which would overflow the conversion. The `PPACK_TYPE_INT32` lower clamp is exactly -2³¹. Validate the input at the call site when an out-of-range physical value must be detected.

**Narrow fields.** Because the clamp uses the type's range, a scaled field narrower than its type keeps only the low `bit_length` bits of the clamped value. A 10-bit `PPACK_TYPE_UINT16` field at 0.25 °C per step from -40 °C covers -40 to 215.75 °C; 260 °C packs as raw 1,200, which the field truncates to 176, and arrives as 4 °C. Clamp the physical value to the field's range before packing, as the [recipe](#narrow-scaled-field) shows.

**Non-finite values.** Pack rejects a scaled field whose raw value is NaN, from a NaN input or from the arithmetic, with `-PPACK_ERR_INVALARG`. Positive and negative infinity saturate like any other out-of-range value. `PPACK_TYPE_F32` fields copy NaN and infinity bit for bit.

**Zero scale.** Pack rejects a scaled field with a `scale` of zero with `-PPACK_ERR_OVERFLOW`. Unpack does not check it and returns `offset` for every raw value.

## Recipes

### CAN-FD frame

A 512-bit frame needs a buffer of 64 elements. The runtime size is passed on every call.

```c
#include <stddef.h>
#include <stdint.h>

typedef struct {
        uint32_t timestamp_ms;
        uint16_t sequence;
        int32_t current_ma;
} canfd_telemetry_t;

static const struct ppack_field canfd_fields[] = {
        {.type = PPACK_TYPE_UINT32, .start_bit = 0, .bit_length = 32,
         .ptr_offset = offsetof(canfd_telemetry_t, timestamp_ms)},
        {.type = PPACK_TYPE_UINT16, .start_bit = 32, .bit_length = 16,
         .ptr_offset = offsetof(canfd_telemetry_t, sequence)},
        {.type = PPACK_TYPE_INT32, .start_bit = 480, .bit_length = 32,
         .ptr_offset = offsetof(canfd_telemetry_t, current_ma)},
};

int
canfd_encode(const canfd_telemetry_t *tx,
             ppack_byte_t frame[512u / PPACK_ADDR_UNIT_BITS])
{
        return ppack_pack(tx, frame, 512u, canfd_fields, 3u);
}
```

The last field ends exactly at bit 512. Moving it to bit 481 makes the call return `-PPACK_ERR_OVERFLOW`.

### Narrow scaled field

A temperature packed into 10 bits at 0.25 °C per step from -40 °C. The physical value is clamped to the top of the field's range first, so a high reading saturates instead of wrapping; a reading below -40 °C already saturates at raw 0 through the type's clamp.

```c
typedef struct {
        float coolant_c;
} thermal_t;

static const struct ppack_field thermal_fields[] = {
        {.type = PPACK_TYPE_UINT16, .start_bit = 0, .bit_length = 10,
         .ptr_offset = offsetof(thermal_t, coolant_c), .scale = 0.25f,
         .offset = -40.0f, .behaviour = PPACK_BEHAVIOUR_SCALED},
};

int
thermal_encode(thermal_t reading, ppack_byte_t payload[PPACK_PAYLOAD_UNITS])
{
        /* 10 bits hold raw 0 to 1023: -40.0 to 215.75 degC. */
        if (reading.coolant_c > 215.75f) {
                reading.coolant_c = 215.75f;
        }
        return ppack_pack(&reading, payload, 64u, thermal_fields, 1u);
}
```

### Round to nearest

```c
static float
round_half_step(float physical, float offset, float scale)
{
        float half = 0.5f * scale;
        return (physical >= offset) ? (physical + half) : (physical - half);
}
```

Apply it to the member before packing; for 12.349 at 0.01 per step, the field then carries 12.35.

### Stored record

Records up to `PPACK_MAX_PAYLOAD_BITS` work the same way; fields are still 1 to 32 bits wide. Decode into a temporary and publish it only after a successful return, so a failure cannot leave a half-updated record. Integrity checks, versioning and atomic storage remain the application's; see [design.md](design.md#non-goals).

```c
typedef struct {
        uint32_t serial;
        int16_t trim[4];
} parameters_t;

static const struct ppack_field parameter_fields[] = {
        {.type = PPACK_TYPE_UINT32, .start_bit = 0, .bit_length = 32,
         .ptr_offset = offsetof(parameters_t, serial)},
        {.type = PPACK_TYPE_INT16, .start_bit = 1008, .bit_length = 16,
         .ptr_offset = offsetof(parameters_t, trim[3])},
};

static parameters_t live_parameters;

int
parameters_load(const ppack_byte_t record[1024u / PPACK_ADDR_UNIT_BITS])
{
        parameters_t staged = live_parameters;
        int ret = ppack_unpack(&staged, record, 1024u, parameter_fields, 2u);
        if (ret == PPACK_SUCCESS) {
                live_parameters = staged;
        }
        return ret;
}
```

### Other uses

The same table pattern serves I²C and SPI device frames, register images, fixed telemetry packets, and binary exchange between nodes of different addressing models, which see the same bytes.

## Version

`ppack.h` defines the release it belongs to, so a copied-in header still identifies itself:

<!-- snippet: skip -->
```c
#define PPACK_VERSION_MAJOR 2
#define PPACK_VERSION_MINOR 6
#define PPACK_VERSION_PATCH 0
#define PPACK_VERSION "2.6.0"
```

`PPACK_VERSION_NUMBER` is `MAJOR * 10000 + MINOR * 100 + PATCH` as a `long`, so it holds any version where `int` is 16 bits. Compare it in the preprocessor to require a minimum release:

```c
#if PPACK_VERSION_NUMBER < 20600L
#error "ppack 2.6.0 or later is required"
#endif
```

`PPACK_VERSION_STRING` is the same string as `PPACK_VERSION`.
