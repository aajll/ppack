# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2.4.0] - 2026-07-02

### Changed

- Resolved all required MISRA C:2012 deviations (Rules 10.4, 10.6, 10.7, 10.8, 11.8, 12.1, 21.15) through explicit casts, temporary variables for composite expressions, and operator precedence parentheses.
- Applied `memcpy` pointer-copy workaround to resolve Rule 11.5 (`void *` to object pointer conversions) in `write_bits`, `read_bits`, `ppack_pack`, and `ppack_unpack`.
- Moved `PPACK_PAYLOAD_UNITS` from internal `ppack_platform.h` to public `ppack.h` to explicitly surface it as part of the library API.

### Removed

- Removed unused `PPACK_WORD_MASK` macro from `ppack_platform.h`.

### Added

- Centralised advisory MISRA deviations in `misra-deviations.txt` with targeted inline `cppcheck-suppress` comments for statement-level and macro-level findings.

## [2.3.0] - 2026-06-19

### Changed

- Aligned the memory layout on 16-bit MAU platforms to one octet per addressable unit to ensure seamless interoperability with the `ucrc` CRC library.
- Decoupled logical payload units from physical MAU size, ensuring `PPACK_ADDR_UNIT_BITS` is always 8.
- Updated documentation to use generic "16-bit MAU" terminology instead of vendor-specific references.
- Changed the default `build_tests` Meson option to `false`; CI continues to opt into tests explicitly with `-Dbuild_tests=true`.
- Adjusted the CI coverage gate to the shared primitive baseline of 80% line / 70% branch coverage.

## [2.2.0] - 2026-04-28

### Added

- Added variable-size payload support up to 512 bits, including the `PPACK_PAYLOAD_BITS` override and payload-size validation.
- Added payload-size-focused tests covering legal sizes, 128/256/512-bit round trips, and wire-format lockdown cases.

### Changed

- `ppack_pack` and `ppack_unpack` now take an explicit `payload_bits` argument; passing `64` preserves the previous fixed-payload behaviour.
- Overflow detection now uses the caller-provided payload size instead of the old fixed 64-bit limit.

## [2.0.0] - 2026-04-26

### Changed

- Renamed `enum ppack_type` constants from `S*` / `U*` names to `<stdint.h>`-style `INT*` / `UINT*` names. This is a source-compatibility break only; the wire format is unchanged.

## [1.4.1] - 2026-04-25

### Added

- First public release with `ppack_pack` / `ppack_unpack`, fixed-size 64-bit payload serialisation, bit-aligned field definitions, raw and scaled modes, 8-bit / 16-bit addressable-unit support, MISRA deviation documentation, and CI coverage gates.
