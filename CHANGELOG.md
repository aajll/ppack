# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2.2.0] - 2026-04-28

### Added

- Support for variable-size payloads up to 512 bits (CAN-FD frame
  data field). Common values: `64` for CAN classic, `512` for a full
  CAN-FD frame. Any positive multiple of `PPACK_ADDR_UNIT_BITS` up to
  512 is accepted.
- `PPACK_PAYLOAD_BITS` is now user-overridable (default 64) at the
  toolchain level, e.g. `-DPPACK_PAYLOAD_BITS=512`. This sizes the
  `PPACK_PAYLOAD_UNITS` convenience macro for stack buffer
  declarations and is independent of the runtime API.
- New validation: `payload_bits` must be > 0, a multiple of
  `PPACK_ADDR_UNIT_BITS`, and ≤ 512. Violations return
  `-PPACK_ERR_INVALARG`.
- New test module `tests/test_payload_size.c` covering
  `payload_bits` validation, payload-size-relative overflow checks,
  round-trips at 128 / 256 / 512 bits, a sweep across every legal
  size, and wire-format lockdown at 128, 256, and 512 bits.

### Changed (BREAKING)

- `ppack_pack` and `ppack_unpack` now take an explicit
  `size_t payload_bits` argument between the payload buffer and the
  field array:
  ```c
  int ppack_pack(const void *base_ptr, void *payload, size_t payload_bits,
                 const struct ppack_field *fields, size_t field_count);
  ```
  All existing call sites must add the `payload_bits` argument;
  passing `64` reproduces the previous fixed-payload behaviour
  exactly. The wire format for a 64-bit payload is unchanged.
- The compile-time `_Static_assert` pinning the payload to 64 bits
  has been relaxed to ≤ 512 bits (≤ 64 byte-units / ≤ 32
  word-units).
- `PPACK_ERR_OVERFLOW` is now triggered by
  `start_bit + bit_length > payload_bits` (was: `> 64`).

## [2.0.0] - 2026-04-26

### Changed (BREAKING)

- Renamed `enum ppack_type` constants to use `INT*` / `UINT*` instead
  of `S*` / `U*`, aligning with `<stdint.h>` conventions:
  - `PPACK_TYPE_U8`  → `PPACK_TYPE_UINT8`
  - `PPACK_TYPE_U16` → `PPACK_TYPE_UINT16`
  - `PPACK_TYPE_U32` → `PPACK_TYPE_UINT32`
  - `PPACK_TYPE_S16` → `PPACK_TYPE_INT16`
  - `PPACK_TYPE_S32` → `PPACK_TYPE_INT32`

  Existing call sites must be updated. The wire format and behaviour
  are unchanged; this is a source-compatibility break only.

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
