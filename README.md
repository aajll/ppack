# ppack

A generic payload serialisation library for bit-aligned data fields in C.

## Features

- **Bit-aligned fields** - Fields can start at any bit position and span arbitrary bit ranges
- **No dynamic memory** - Fixed-size operations, no `malloc` / `free`
- **Deterministic WCET** - All operations have bounded execution time
- **Scaled fields** - Linear scale/offset transformations for physical-unit encoding
- **Multiple types** - Supports `u8`, `u16`, `s16`, `u32`, `s32`, `f32`, and raw bitfields
- **Error codes** - All APIs return explicit error codes; no errno, no exceptions

## Installation

### Copy-in (recommended for embedded targets)

Copy two files into your project tree:

```
include/ppack.h
src/ppack.c
```

Then include the header:

```c
#include "ppack.h"
```

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
                .type      = PPACK_TYPE_U16,
                .start_bit = 0,
                .bit_length = 16,
                .ptr_offset = offsetof(engine_data_t, rpm),
                .behaviour  = PPACK_BEHAVIOUR_RAW,
        },
        {
                .type       = PPACK_TYPE_S16,
                .start_bit  = 16,
                .bit_length = 16,
                .ptr_offset = offsetof(engine_data_t, temperature),
                .behaviour  = PPACK_BEHAVIOUR_RAW,
        },
        {
                .type       = PPACK_TYPE_F32,
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
        uint8_t payload[8] = {0};
        engine_data_t tx = {.rpm = 3000, .temperature = -10, .voltage = 12.5f};
        engine_data_t rx = {0};

        /* Pack structure into 8-byte payload */
        int ret = ppack_pack(&tx, payload, fields, 3);
        if (ret != PPACK_SUCCESS) {
                return ret;
        }

        /* Unpack payload back into structure */
        ret = ppack_unpack(&rx, payload, fields, 3);

        return ret;
}
```

## Building

```sh
# Library only (release)
meson setup build --buildtype=release -Dbuild_tests=false
meson compile -C build

# With unit tests
meson setup build --buildtype=debug -Dbuild_tests=true
meson compile -C build
meson test -C build --verbose
```

## API Reference

### Pack / Unpack

```c
int ppack_pack(const void *base_ptr, void *payload,
               const struct ppack_field *fields, size_t field_count);

int ppack_unpack(void *base_ptr, const void *payload,
                 const struct ppack_field *fields, size_t field_count);
```

### Field Descriptor

```c
struct ppack_field {
        enum ppack_type      type;        /* Data type */
        uint16_t             start_bit;   /* Bit position in payload (0-63) */
        uint16_t             bit_length;  /* Number of bits */
        size_t               ptr_offset;  /* offsetof() into base structure */
        float                scale;       /* Scaling factor (default: 1.0) */
        float                offset;      /* Offset value (default: 0.0) */
        enum ppack_behaviour behaviour;   /* PPACK_BEHAVIOUR_RAW or _SCALED */
};
```

### Supported Types

| Type | Description |
|---|---|
| `PPACK_TYPE_U8` | 8-bit unsigned |
| `PPACK_TYPE_U16` | 16-bit unsigned |
| `PPACK_TYPE_S16` | 16-bit signed |
| `PPACK_TYPE_U32` | 32-bit unsigned |
| `PPACK_TYPE_S32` | 32-bit signed |
| `PPACK_TYPE_F32` | 32-bit float |
| `PPACK_TYPE_BITS` | Raw bitfield (up to 32 bits) |

### Error Codes

| Code | Value | Meaning |
|---|---|---|
| `PPACK_SUCCESS` | 0 | Operation succeeded |
| `PPACK_ERR_INVALARG` | 1 | Invalid argument (e.g. zero field count) |
| `PPACK_ERR_INVAL` | 2 | Invalid operation or state |
| `PPACK_ERR_NOTFOUND` | 3 | Unknown field type |
| `PPACK_ERR_NULLPTR` | 4 | NULL pointer detected |
| `PPACK_ERR_OVERFLOW` | 5 | Size overflow detected |

Functions return the negated error code on failure (e.g. `-PPACK_ERR_NULLPTR`).

## Scaled Fields

When `behaviour` is set to `PPACK_BEHAVIOUR_SCALED`, the library applies a linear transformation during pack and unpack:

```
raw = (physical - offset) / scale   /* pack */
physical = (raw * scale) + offset   /* unpack */
```

This is useful for encoding floating-point physical values into compact integer fields, e.g. encoding a voltage in millivolts or a temperature with 0.1 °C resolution.

## Input Validation

All public APIs validate arguments at the function boundary.

- Passing `NULL` for any pointer returns `-PPACK_ERR_NULLPTR`.
- Passing `NULL` or an empty `fields` array returns `-PPACK_ERR_INVALARG`.
- An unrecognised field type returns `-PPACK_ERR_NOTFOUND`.

## Use Cases

1. **Protocol Encoding** - Compact data for CAN, I2C, or custom binary protocols
2. **Register Access** - Safe manipulation of peripheral register layouts
3. **Telemetry Frames** - Serialise sensor data into fixed-size telemetry packets
4. **Cross-platform IPC** - Deterministic binary serialisation between heterogeneous targets

## Notes

| Topic | Note |
|---|---|
| **Memory** | All operations use stack memory; no dynamic allocation |
| **MISRA C:2012 aligned** | Suitable for use in IEC 61508 safety-critical environments |
| **Payload size** | Fixed 8-byte (64-bit) payload buffer |
| **Bit ordering** | LSB-first within each byte; fields may span byte boundaries |
| **Field size** | Up to 32 bits per field |
| **Thread safety** | Not thread-safe; protect with an external mutex if needed |
| **WCET** | All operations have deterministic worst-case execution time |
| **Version header** | Auto-generated by the Meson build and placed in the output build folder |
