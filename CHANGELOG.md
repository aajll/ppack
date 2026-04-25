# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.4.1] - 2026-04-25

First public release.

### Added

- Public API: `ppack_pack` and `ppack_unpack` for fixed-size 64-bit
  payload serialisation with bit-aligned fields.
- Field types: `PPACK_TYPE_U8`, `U16`, `S16`, `U32`, `S32`, `F32`,
  `BITS`. Raw and scaled (linear scale/offset) modes.
- Cross-platform support for 8-bit and 16-bit minimum addressable units,
  auto-detected from `<limits.h>`. Validated on x86_64 and via host
  simulation for the TI C2000 family.
- MISRA C:2012 deviation record in `src/ppack.c` (three advisory rules,
  zero required-rule deviations).
- Wire format v1 frozen and verified by `test_wire_v1_lockdown_*`.
- CI: tests on Linux + macOS, ASan + UBSan, 100% line + branch
  coverage gate.

### Fixed

- Undefined behaviour in scaled `U32` / `S32` saturation: clamp to the
  largest float exactly representable below the destination integer
  range, instead of `UINT32_MAX` / `INT32_MAX` literals which round up
  to `2^32` / `2^31` as 32-bit floats.
