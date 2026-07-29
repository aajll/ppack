# ppack

[![CI](https://github.com/aajll/ppack/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/ppack/actions/workflows/ci.yml)

ppack packs and unpacks structures into bit-aligned payloads of up to 512 bits.

## Features

- **Bit-aligned fields** — Place each field at any bit position in the payload.
- **Variable payload size** — Pass the payload size in bits (multiple of 8, up to 512). Covers CAN classic (64) and CAN-FD (up to 512).
- **No dynamic memory** — All operations use fixed-size buffers. The library does not call `malloc` or `free`.
- **Bounded execution time** — Execution time is deterministic when field descriptors are `static const`.
- **Scaled fields** — Apply linear scale/offset transformations for physical-unit encoding.
- **Multiple types** — Supports `uint8_t`, `uint16_t`, `int16_t`, `uint32_t`, `int32_t`, `float`, and raw bitfields.
- **Cross-platform** — Runs on any C11 target with an 8-bit or 16-bit minimum addressable unit. The library detects the platform from `<limits.h>`. There are no chip-specific code paths.
- **Error codes** — All functions return explicit error codes. The library does not use `errno` or exceptions.

## Requirements

- A C11-compatible toolchain (uses `_Static_assert`)
- Conformant `<stdint.h>` and `<limits.h>`
- IEEE-754 binary32 `float` (every modern target the library targets uses this)

## Installation

### Copy-in (recommended for embedded targets)

Copy three files into your project tree:

```
include/ppack.h
include/ppack_platform.h
src/ppack.c
```

Then include the public header:

```c
#include "ppack.h"
```

`ppack_platform.h` is auto-included by `ppack.h`. Make sure both headers are reachable from the same include path.

### Meson subproject

Add this repo as a wrap dependency or subproject:

```meson
ppack_dep = dependency('ppack', fallback : ['ppack', 'ppack_dep'])
```

## Quick Start

```c
#include <stddef.h>
#include <stdint.h>
#include "ppack.h"

typedef struct {
        uint16_t rpm;
        int16_t  temperature;
        float    voltage;
} engine_data_t;

static const struct ppack_field fields[] = {
        {
                .type       = PPACK_TYPE_UINT16,
                .start_bit  = 0,
                .bit_length = 16,
                .ptr_offset = offsetof(engine_data_t, rpm),
                .behaviour  = PPACK_BEHAVIOUR_RAW,
        },
        {
                .type       = PPACK_TYPE_INT16,
                .start_bit  = 16,
                .bit_length = 16,
                .ptr_offset = offsetof(engine_data_t, temperature),
                .behaviour  = PPACK_BEHAVIOUR_RAW,
        },
        /*
         * Voltage: encode as a 16-bit unsigned integer with 0.01 V/LSB
         * resolution.  The struct member must be float when using
         * PPACK_BEHAVIOUR_SCALED.  PPACK_TYPE_F32 performs a raw 32-bit
         * IEEE 754 copy and does not support scale/offset.
         */
        {
                .type       = PPACK_TYPE_UINT16,
                .start_bit  = 32,
                .bit_length = 16,
                .ptr_offset = offsetof(engine_data_t, voltage),
                .scale      = 0.01f,
                .offset     = 0.0f,
                .behaviour  = PPACK_BEHAVIOUR_SCALED,
        },
};

int main(void)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        engine_data_t tx = {.rpm = 3000, .temperature = -10, .voltage = 12.5f};
        engine_data_t rx = {0};

        /* Pack the structure into a 64-bit payload (CAN classic).
         * The third argument is the payload size in bits.
         * Pass 128, 256, or 512 for CAN-FD frames. */
        int ret = ppack_pack(&tx, payload, 64, fields, 3);
        if (ret != PPACK_SUCCESS) {
                return ret;
        }

        /* Unpack payload back into structure */
        ret = ppack_unpack(&rx, payload, 64, fields, 3);

        return ret;
}
```

`ppack_byte_t[PPACK_PAYLOAD_UNITS]` declares a 64-bit payload by default (`uint8_t[8]` on byte-addressable targets, `uint16_t[8]` on 16-bit MAU targets). Override `PPACK_PAYLOAD_BITS` at the toolchain level (for example, `-DPPACK_PAYLOAD_BITS=512`) or size the buffer manually as `ppack_byte_t payload[N / PPACK_ADDR_UNIT_BITS]`. The wire format is identical across MAU sizes.

### CAN-FD (512-bit) example

For a CAN-FD frame with a full 64-byte data field, declare the
buffer manually and pass `512` as `payload_bits`:

```c
typedef struct {
        uint32_t timestamp_ms;
        uint16_t sequence;
        int32_t  current_ma;
        /* ...up to 512 bits' worth of fields... */
} canfd_telemetry_t;

static const struct ppack_field canfd_fields[] = {
        {
                .type = PPACK_TYPE_UINT32,
                .start_bit =   0,
                .bit_length = 32,
                .ptr_offset = offsetof(canfd_telemetry_t, timestamp_ms),
                .behaviour = PPACK_BEHAVIOUR_RAW
        },
        {
                .type = PPACK_TYPE_UINT16,
                .start_bit =  32,
                .bit_length = 16,
                .ptr_offset = offsetof(canfd_telemetry_t, sequence),
                .behaviour = PPACK_BEHAVIOUR_RAW
        },
        {
                .type = PPACK_TYPE_INT32,
                .start_bit = 480,
                .bit_length = 32,
                .ptr_offset = offsetof(canfd_telemetry_t, current_ma),
                .behaviour = PPACK_BEHAVIOUR_RAW
        },
};

ppack_byte_t payload[512u / PPACK_ADDR_UNIT_BITS] = {0};

uint32_t time = get_time_ms();
uint16_t seq = get_seq_multiplex("current");
int32_t curr = get_current_scaled();

canfd_telemetry_t tx = { .timestamp_ms = time, .sequence = seq, .current_ma = curr };

int ret = ppack_pack(&tx, payload, 512, canfd_fields, 3);
```

Or, if every payload in your project is 512 bits, set
`-DPPACK_PAYLOAD_BITS=512` at the toolchain level and continue using
`ppack_byte_t payload[PPACK_PAYLOAD_UNITS]`. The runtime API still
takes the size explicitly.

## Building

```sh
# Library only (release)
meson setup build --buildtype=release -Dbuild_tests=false
meson compile -C build

# With unit tests (host plus simulated 16-bit MAU)
meson setup build --buildtype=debug -Dbuild_tests=true
meson compile -C build
meson test -C build --verbose
```

The test target runs twice. It runs once natively (8-bit MAU) and once with `-DPPACK_SIMULATE_16BIT_MAU` to exercise the 16-bit MAU code path.

### Code coverage

```sh
# Configure with gcov instrumentation
meson setup build_cov --buildtype=debug -Dbuild_tests=true -Db_coverage=true
meson compile -C build_cov
meson test -C build_cov

# Generate report (requires gcovr; pip install gcovr)
gcovr --root . --filter 'src/' --filter 'include/' --print-summary
gcovr --root . --filter 'src/' --filter 'include/' --html-details build_cov/coverage.html
```

CI requires 100% line coverage and 100% branch coverage. Every reachable code path has an explicit test. A drop below this baseline requires approval and a justification.

## API Reference

### Pack / Unpack

```c
int ppack_pack(const void *base_ptr, void *payload, size_t payload_bits,
               const struct ppack_field *fields, size_t field_count);

int ppack_unpack(void *base_ptr, const void *payload, size_t payload_bits,
                 const struct ppack_field *fields, size_t field_count);
```

`payload_bits` is the payload size in bits. Must be a positive multiple of `PPACK_ADDR_UNIT_BITS` (always 8) and no greater than 512 (CAN-FD frame data field). Common values: `64` for CAN classic, `512` for full CAN-FD frames.

### Field Descriptor

```c
struct ppack_field {
        enum ppack_type      type;        /* Data type */
        uint16_t             start_bit;   /* Bit position (0..payload_bits-1) */
        uint16_t             bit_length;  /* Number of bits */
        size_t               ptr_offset;  /* offsetof() into base structure */
        float                scale;       /* Scaling factor (default: 1.0) */
        float                offset;      /* Offset value (default: 0.0) */
        enum ppack_behaviour behaviour;   /* PPACK_BEHAVIOUR_RAW or _SCALED */
};
```

### Supported Types

| Type                | Description                          | Struct member type (RAW) |
| ------------------- | ------------------------------------ | ------------------------ |
| `PPACK_TYPE_UINT8`  | 8-bit unsigned                       | `uint8_t`                |
| `PPACK_TYPE_UINT16` | 16-bit unsigned                      | `uint16_t`               |
| `PPACK_TYPE_INT16`  | 16-bit signed                        | `int16_t`                |
| `PPACK_TYPE_UINT32` | 32-bit unsigned                      | `uint32_t`               |
| `PPACK_TYPE_INT32`  | 32-bit signed                        | `int32_t`                |
| `PPACK_TYPE_F32`    | 32-bit float (raw IEEE-754 bit copy) | `float`                  |
| `PPACK_TYPE_BITS`   | Raw bitfield, up to 32 bits          | `uint32_t`               |

For `PPACK_BEHAVIOUR_SCALED` fields the struct member must always be `float`, regardless of `type`.

### Error Codes

| Code                 | Value | Meaning                                                                                                                                                                                         |
| -------------------- | ----- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `PPACK_SUCCESS`      | 0     | Operation succeeded                                                                                                                                                                             |
| `PPACK_ERR_INVALARG` | 1     | Invalid argument (NULL fields, zero field count, `bit_length` out of range, `payload_bits` zero or not a multiple of `PPACK_ADDR_UNIT_BITS` or > 512, scaling requested for `PPACK_TYPE_UINT8`) |
| `PPACK_ERR_NOTFOUND` | 3     | Unknown field type                                                                                                                                                                              |
| `PPACK_ERR_NULLPTR`  | 4     | NULL pointer passed for `base_ptr` or `payload`                                                                                                                                                 |
| `PPACK_ERR_OVERFLOW` | 5     | `start_bit + bit_length > payload_bits`, or `scale == 0` on a `SCALED` field                                                                                                                    |

Functions return the negated error code on failure (e.g. `-PPACK_ERR_NULLPTR`). Code `2` is reserved.

## Wire Format

The payload is `payload_bits` bits long (a multiple of 8, between 8 and 512), addressed as `payload_bits / 8` logical bytes numbered 0 to `payload_bits / 8 - 1`.

- **Bit numbering**: payload bit `N` is in logical byte `N / 8`. The bit position inside that byte is `N mod 8`.
- **Bit order within a byte**: bit 0 is the least significant bit.
- **Multi-byte fields**: little-endian byte order. This matches DBC `byte_order=1`. A 16-bit field at `start_bit=0` with value `0x1234` writes `0x34` to byte 0 and `0x12` to byte 1.
- **Cross-platform**: the format is identical on byte-addressable and 16-bit-MAU hosts. Two nodes using ppack interoperate regardless of their addressable-unit size, provided they agree on `payload_bits`.

`PPACK_TYPE_F32` copies 32 IEEE-754 bits directly. The wire bytes follow the host `uint32_t` byte order. Any two little-endian hosts interoperate (for example, x86_64, ARM Cortex-M, AArch64, 16-bit MAU platforms). Little-endian and big-endian hosts do not interoperate without explicit byte swapping in user code.

## Scaled Fields

When `behaviour` is set to `PPACK_BEHAVIOUR_SCALED`, the library applies a linear transformation:

```
raw      = (physical - offset) / scale    /* pack:   float -> integer */
physical = (float)raw * scale + offset    /* unpack: integer -> float */
```

This encodes a floating-point physical value into a compact integer field. Examples include voltage at 0.01 V/LSB and temperature with a -40 °C offset and 0.25 °C resolution.

### Saturation

ppack_pack clamps the raw integer value to the destination type's range. `PPACK_TYPE_UINT16` clamps to `0..65535`. `PPACK_TYPE_INT16` clamps to `-32768..32767`. Out-of-range inputs do not return an error.

For `PPACK_TYPE_UINT32` scaled fields, ppack_pack clamps to `4294967040`. This is the largest float value that fits in a `uint32_t`. For `PPACK_TYPE_INT32`, it clamps to `2147483520`. The values `UINT32_MAX` and `INT32_MAX` round up to `2^32` or `2^31` as 32-bit floats. That would overflow the destination cast.

Validate the input at the call site if your application must detect out-of-range physical values.

### Non-finite values

ppack_pack rejects a scaled field when the result is NaN. The function returns `-PPACK_ERR_INVALARG`. NaN can come from the source value or the scale/offset calculation. NaN has no valid integer representation.

Positive and negative infinity saturate to the type's clamp bounds. `PPACK_TYPE_F32` fields copy bits directly. They carry NaN and infinity values unchanged.

### Quantization

ppack_pack truncates the float-to-integer cast toward zero (standard C cast rules). The maximum round-trip error is one LSB, equal to `scale`. Add `0.5f * scale * sign(physical)` at the call site for round-to-nearest behaviour.

## Platform Support

### Supported architectures

ppack runs on any toolchain that meets the requirements below. The library detects the addressing model from the standard library headers. There are no chip-specific code paths.

| Requirement                         | Notes                                                                                                        |
| ----------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| C11 toolchain                       | Uses `_Static_assert`. C99 with a non-standard equivalent will also build but is not exercised by CI.        |
| `CHAR_BIT == 8` or `CHAR_BIT == 16` | Detected from `<limits.h>`. Other values are rejected at compile time by `_Static_assert`.                   |
| Two's-complement signed integers    | Required for the documented sign-extension and unsigned-to-signed cast behaviour. Universal on real targets. |
| IEEE-754 binary32 `float`           | Required for `PPACK_TYPE_F32` and scaled fields. Universal on real targets.                                  |

Expected targets include x86_64, AArch64, ARMv7-M, ARMv8-M, RISC-V, AVR, and 16-bit MAU platforms.

### Validated configurations

The test suite runs these configurations, or they are verified on hardware:

| Toolchain                                    | Target                    | Status                                                            |
| -------------------------------------------- | ------------------------- | ----------------------------------------------------------------- |
| GCC, Clang                                   | x86_64 Linux              | Run in CI (8-bit MAU code path)                                   |
| GCC, Clang with `-DPPACK_SIMULATE_16BIT_MAU` | x86_64 Linux              | Run in CI (16-bit MAU code path)                                  |
| Native Toolchains                           | 16-bit MAU platforms      | Code path covered by host simulation; no live cross-compile in CI |

Other architectures from the list above should work but are not yet tested routinely. Report results from new targets to extend this table.

### 16-bit-MAU specifics

On targets where `CHAR_BIT == 16` (16-bit MAU platforms) the narrowest integer type is 16 bits, so any `uint8_t` the toolchain provides is backed by 16-bit storage. ppack detects this from `CHAR_BIT`/`UCHAR_MAX` (not from `uint8_t` itself) and handles it transparently via `ppack_platform.h`. Two contract points apply:

| Point             | Rule                                                                                                                                                                                                                                                                                                              |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **UINT8 storage** | A `PPACK_TYPE_UINT8` field reads and writes only the LOW 8 bits of the struct member's storage. The underlying 16-bit storage can technically hold 0..65535 on a 16-bit-MAU target, but only the low 8 bits round-trip through the wire. Do not store values >= 256 in UINT8 fields if you expect them to round-trip. |
| **`ptr_offset`**  | Always use `offsetof()`. The library treats `ptr_offset` as a `char`-unit offset, which is 16 bits on a 16-bit-MAU target. Manually computing offsets in 8-bit units will fail.                                                                                                                                   |

### Forcing the simulation flag

For host CI builds that need to exercise the 16-bit MAU code path without a cross-compile, define `PPACK_SIMULATE_16BIT_MAU` at compile time. Production builds for real targets should leave it undefined and rely on auto-detection.

## Input Validation

All public APIs validate arguments at the function boundary.

- Passing `NULL` for any pointer returns `-PPACK_ERR_NULLPTR`.
- Passing `NULL` or an empty `fields` array returns `-PPACK_ERR_INVALARG`.
- A `payload_bits` argument that is zero, not a multiple of `PPACK_ADDR_UNIT_BITS`, or greater than 512 returns `-PPACK_ERR_INVALARG`.
- A field with `bit_length == 0` or `bit_length > 32` returns `-PPACK_ERR_INVALARG`.
- A field where `start_bit + bit_length > payload_bits` returns `-PPACK_ERR_OVERFLOW`.
- A `PPACK_BEHAVIOUR_SCALED` field with `scale == 0.0` returns `-PPACK_ERR_OVERFLOW`.
- A `PPACK_BEHAVIOUR_SCALED` request on a `PPACK_TYPE_UINT8` field returns `-PPACK_ERR_INVALARG`.
- An unrecognised field type returns `-PPACK_ERR_NOTFOUND`.

`bit_length` must be between 1 and 32 and fit within the payload (`bit_length <= payload_bits - start_bit`). The library does not check `bit_length` against the natural width of the field's `type`. A `PPACK_TYPE_UINT16` with `bit_length=24` passes validation. Only the low 16 bits carry data. Match `bit_length` to the type's natural width unless you have a specific reason.

## Use Cases

1. **Protocol encoding** — Pack data for CAN, I2C, or custom binary protocols.
2. **Register access** — Read and write peripheral register layouts safely.
3. **Telemetry frames** — Encode sensor data into fixed-size telemetry packets.
4. **Cross-platform IPC** — Exchange deterministic binary data between different targets.

## Limitations

ppack packs and unpacks data. It does not perform the tasks below:

- **No integrity checks**: ppack does not compute a CRC, checksum, or frame header. The caller detects bit errors. Use the transport layer for error detection (for example, CAN frame CRC).
- **No schema versioning**: the field-descriptor layout is the schema. Store it in shared headers across nodes.
- **Payload size capped at 512 bits**: this matches a full CAN-FD frame data field. Send larger payloads with multiple ppack calls on separate chunks.
- **No multi-buffer API**: ppack processes one payload per call.
- **No runtime endianness adaptation**: the wire format uses little-endian byte order. A big-endian host needs explicit byte swapping at the boundary.

## Notes

| Topic                       | Note                                                                                                                                                                                                                                                                                                                                              |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Memory**                  | All operations use stack memory. The library does not allocate memory dynamically.
| **MISRA C:2023**            | Audited with `misch` (cppcheck-backed MISRA C:2023 analysis; configuration in `misra.toml`). The audit reports zero findings.

Deviations are justified at point of use with `cppcheck-suppress ... @deviation` comments or project-wide in `misra-deviations.txt`. The only required-rule deviations are three Rule 21.15 sites: the type-erased struct-member copies in two internal helpers and the deliberate F32 `float`/`uint32_t` pun. Design follows MISRA principles throughout: no dynamic allocation, no UB shifts, explicit error codes, `memcpy`-based type punning. |
| **Payload size**            | The caller supplies the payload size via `payload_bits` (multiple of 8, between 8 and 512). A value of 64 matches CAN classic. A value of 512 matches full CAN-FD.
| **Bit ordering**            | Bit 0 is the least significant bit within each byte. Multi-byte fields use little-endian order. Fields may span byte boundaries.
| **Field size**              | 1-32 bits per field                                                                                                                                                                                                                                                                                                                               |
| **Thread safety**           | ppack is not thread-safe. The caller provides mutual exclusion when `base_ptr` or `payload` is shared across threads or ISRs.
| **WCET**                    | Execution time is bounded and deterministic **when field descriptors are `static const`**. The loop bounds are compile-time constants in that case. WCET is not guaranteed if the caller constructs descriptors at runtime with arbitrary `bit_length` values.
| **F32 and scaling**         | `PPACK_TYPE_F32` always copies 32 IEEE-754 bits directly. The library ignores `scale`, `offset`, and `behaviour` for this type. Use a scaled integer type (`UINT16`, `INT16`, `UINT32`, `INT32`) to encode floating-point physical values with a resolution factor.
| **UINT8 struct member**     | `PPACK_TYPE_UINT8` reads and writes a `uint8_t`. On 16-bit MAU platforms the member uses 16-bit storage. Only the low 8 bits round-trip through the payload.
| **Version header**          | The Meson build generates `ppack_version.h` and places it in the build directory.
