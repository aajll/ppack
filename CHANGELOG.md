# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2.4.0] - 2026-07-02

### Changed

- Resolved the required MISRA C:2023 violations for Rules 10.4, 10.7 and 10.8 through unsigned suffixes and temporary variables for composite expressions.
- Centralised the type-erased struct-member copies in internal `ppack_member_write` / `ppack_member_read` helpers; Rule 21.15 is now deviated at exactly three documented sites (the two helpers and the deliberate F32 `float`/`uint32_t` pun) rather than silenced per call site.
- Resolved advisory Rule 12.1 findings by making operator precedence explicit with parentheses.
- Moved `PPACK_PAYLOAD_UNITS` from `ppack_platform.h` to `ppack.h` to explicitly surface it as part of the public API. Include `ppack.h` (the documented entry point) rather than `ppack_platform.h` directly.
- Updated `README.md` and `CONTRIBUTING.md` to describe the tool-driven MISRA workflow.

### Removed

- Removed unused `PPACK_WORD_MASK` macro from `ppack_platform.h`.

### Added

- Adopted `misch` (cppcheck-backed MISRA C:2023 analysis) via `misra.toml`. The audit is clean; all deviations (advisory Rules 2.5, 8.7, 11.5, 15.5, 18.4 and required Rule 21.15) are justified at point of use with `cppcheck-suppress` `@deviation` comments or project-wide in `misra-deviations.txt`.
- Added a compile-time guard that `float` is exactly 32 bits (`PPACK_TYPE_F32` wire-format prerequisite).

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
