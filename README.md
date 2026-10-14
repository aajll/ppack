# ppack

[![CI](https://github.com/aajll/ppack/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/ppack/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**Bit-aligned payload serialisation for embedded C11: table-driven packing of C structures into fixed little-endian payloads, with scaled physical-unit fields and an identical wire format on 8-bit and 16-bit addressable targets.**

## Quick start

Copy `include/` and `src/ppack.c` into your project. For a Meson subproject or an installed `pkg-config` package, see the [integration guide](docs/integration.md).

```c
#include <stddef.h>
#include <stdint.h>

typedef struct {
        uint16_t rpm;
        int16_t temperature;
        float voltage;
} engine_data_t;

/* Three 16-bit fields in a 64-bit payload; voltage travels at 0.01 V/LSB. */
static const struct ppack_field engine_fields[] = {
        {.type = PPACK_TYPE_UINT16, .start_bit = 0, .bit_length = 16,
         .ptr_offset = offsetof(engine_data_t, rpm)},
        {.type = PPACK_TYPE_INT16, .start_bit = 16, .bit_length = 16,
         .ptr_offset = offsetof(engine_data_t, temperature)},
        {.type = PPACK_TYPE_UINT16, .start_bit = 32, .bit_length = 16,
         .ptr_offset = offsetof(engine_data_t, voltage), .scale = 0.01f,
         .behaviour = PPACK_BEHAVIOUR_SCALED},
};

int
engine_round_trip(void)
{
        engine_data_t tx = {.rpm = 3000, .temperature = -10, .voltage = 12.5f};
        engine_data_t rx = {0};
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS];

        /* payload holds B8 0B F6 FF E2 04 00 00 after this call. */
        int ret = ppack_pack(&tx, payload, 64u, engine_fields, 3u);
        if (ret == PPACK_SUCCESS) {
                ret = ppack_unpack(&rx, payload, 64u, engine_fields, 3u);
        }
        return ret; /* rx now equals tx */
}
```

## Description

`ppack` copies the members of a C structure into a payload of any whole number of octets up to 65,528 bits, and back, as described by a constant table of field descriptors. Each field starts at any bit and spans 1 to 32 bits, so one table describes a CAN or CAN-FD frame, a telemetry packet, or a stored parameter record.

- Seven field types: `uint8_t`, `uint16_t`, `int16_t`, `uint32_t`, `int32_t`, raw IEEE 754 `float`, and raw bitfields
- Scaled fields that encode a `float` physical value as an integer through a scale and offset, saturating to the integer type
- A fixed wire format: LSB-first bit numbering, little-endian multi-byte fields, the same bytes on byte-addressable and 16-bit-MAU (`CHAR_BIT == 16`) targets
- Every call validates its arguments and returns an explicit error code
- No allocation and no global state; work is bounded by the field table
- Unit tests under ASan and UBSan on both addressing models with 100% line and branch coverage, an exhaustive large-payload sweep, and MISRA C:2023 analysis with [`misch`](https://pypi.org/project/misch/)

## Validated toolchains

| Toolchain               | Target                        | Validation                                                    |
| ----------------------- | ----------------------------- | ------------------------------------------------------------- |
| GCC                     | x86_64 Linux                  | Unit tests under ASan and UBSan, native and simulated 16-bit MAU |
| DSP vendor C11 compiler | 16-bit MAU (`CHAR_BIT == 16`) | Library compile, two object formats                           |

Target linking and execution belong to the consuming firmware. See [cross builds](docs/integration.md#cross-builds).

## Documentation

- [Integration guide](docs/integration.md) — copy-in, Meson subproject, installed package, buffer sizing, building, and cross builds
- [API reference](docs/api.md) — types, functions, error codes, scaled fields, and recipes
- [Design](docs/design.md) — the wire format, addressing models, range argument, failure behaviour, and verification
- [Changelog](CHANGELOG.md) — release history

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before you open a pull request. Report security issues as described in [SECURITY.md](.github/SECURITY.md).

## License

MIT. See [LICENSE](LICENSE).
