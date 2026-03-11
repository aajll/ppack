# ppack

[![CI](https://github.com/ACIDBURN2501/ppack/actions/workflows/ci.yml/badge.svg)](https://github.com/ACIDBURN2501/ppack/actions/workflows/ci.yml)

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
                .type       = PPACK_TYPE_U16,
                .start_bit  = 0,
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
        /*
         * Voltage: encode as a 16-bit unsigned integer with 0.01 V/LSB
         * resolution.  The struct member must be float when using
         * PPACK_BEHAVIOUR_SCALED.  PPACK_TYPE_F32 performs a raw 32-bit
         * IEEE 754 copy and does not support scale/offset.
         */
        {
                .type       = PPACK_TYPE_U16,
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
| `PPACK_ERR_INVALARG` | 1 | Invalid argument (NULL fields, zero field count, `bit_length` out of range) |
| `PPACK_ERR_NOTFOUND` | 3 | Unknown field type |
| `PPACK_ERR_NULLPTR` | 4 | NULL pointer passed for `base_ptr` or `payload` |
| `PPACK_ERR_OVERFLOW` | 5 | `start_bit + bit_length > 64`, or `scale == 0` on a `SCALED` field |

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
- A field with `bit_length == 0` or `bit_length > 32` returns `-PPACK_ERR_INVALARG`.
- A field where `start_bit + bit_length > 64` returns `-PPACK_ERR_OVERFLOW`.
- A `PPACK_BEHAVIOUR_SCALED` field with `scale == 0.0` returns `-PPACK_ERR_OVERFLOW`.
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
| **MISRA C:2012 principles** | Designed with MISRA C:2012 principles: no dynamic allocation, no UB shifts, explicit error codes, `memcpy`-based type punning. Full compliance requires a certified static-analysis tool run with a documented deviation list. |
| **Payload size** | Fixed 8-byte (64-bit) payload buffer |
| **Bit ordering** | LSB-first within each byte; fields may span byte boundaries |
| **Field size** | 1–32 bits per field |
| **Thread safety** | Not thread-safe; caller must provide mutual exclusion when `base_ptr` or `payload` is shared across threads or ISRs |
| **WCET** | Execution time is bounded and deterministic **when field descriptors are `static const`** (loop bounds are compile-time constants). WCET is not guaranteed if descriptors are constructed at runtime with arbitrary `bit_length` values. |
| **F32 and scaling** | `PPACK_TYPE_F32` always performs a raw 32-bit IEEE 754 bit-copy; `scale`, `offset`, and `behaviour` are ignored for this type. Use a scaled integer type (`U16`, `S16`, `U32`, `S32`) to encode floating-point physical values with a resolution factor. |
| **U8 struct member** | `PPACK_TYPE_U8` reads and writes a `uint8_t` in the struct. |
| **Version header** | Auto-generated by the Meson build and placed in the output build folder |
