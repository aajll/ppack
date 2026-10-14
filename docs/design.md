# Design

This document records how `ppack` lays out a payload, why the layout is the same on every target, the arithmetic bounds the codec relies on, and how it is verified. The API is in [api.md](api.md).

## Goals

- One table-driven codec for any fixed binary layout of up to 32-bit fields, so a protocol is described by data, not by hand-written shifting code.
- A wire format fixed by the library, independent of the compiler, the host's addressing model, and the structure's layout.
- Bounded work per call, with no allocation and no global state, so the codec is safe in an interrupt handler or a fixed-period control loop.
- An explicit error code for every argument it can check.

## Non-goals

- **Integrity.** No CRC, checksum or framing. The transport, or the application, detects bit errors.
- **Schema versioning.** The field table is the schema. Share it between nodes in a common header, and version stored records in the application.
- **Atomic update.** A failing call can leave partial output; see [Failure behaviour](#failure-behaviour).
- **Streaming.** A payload is processed whole in one call, with no multi-buffer form.
- **Big-endian hosts.** The wire format is little-endian and `PPACK_TYPE_F32` copies the host's representation. A big-endian host would need byte swapping at the boundary.

## Wire format

A payload is `payload_bits` bits, a multiple of 8 from 8 to 65,528, held as `payload_bits / 8` logical octets numbered from 0. Logical octet 0 is the first on the wire.

- Payload bit `N` lives in logical octet `N / 8`, at bit `N mod 8` of that octet.
- Bit 0 of an octet is its least significant bit.
- A multi-octet field is little-endian, as in a DBC `byte_order=1` signal. A 16-bit field at bit 0 carrying `0x1234` writes `0x34` to octet 0 and `0x12` to octet 1.
- A field may start at any bit and cross octet boundaries.
- Bits that no field covers are zero after a pack.

`PPACK_TYPE_F32` copies the 32 bits of the `float` in the host's `uint32_t` order. Any two little-endian hosts interoperate; a little-endian and a big-endian host do not without byte swapping in the application.

## Addressing models

Where `CHAR_BIT` is 8, a payload buffer is an array of `uint8_t`, one per logical octet. Where `CHAR_BIT` is 16, the narrowest object is 16 bits, so the buffer is an array of `uint16_t` and each element holds one logical octet in its low 8 bits. The element count, the bit numbering and the octet values are the same in both models; only the storage per element differs. A node of either model therefore exchanges the same payload with a node of the other, provided both agree on `payload_bits`. The upper 8 bits of each 16-bit element are zero after a pack and ignored by an unpack.

`ppack_platform.h` selects the model at compile time from `CHAR_BIT` and `UCHAR_MAX`, with no chip-specific code. `PPACK_SIMULATE_16BIT_MAU` forces the 16-bit storage model on a byte-addressable host so that the host tests exercise it.

Two contract points follow from the 16-bit model:

- `offsetof` counts in `char` units, which are 16 bits there. The library adds `ptr_offset` to a `char` pointer, so an `offsetof` value is correct on both models and a hand-computed octet offset is not.
- A `uint8_t` member occupies a 16-bit unit. Pack masks it to 8 bits; unpack writes the whole unit with the upper 8 bits cleared, so data kept in those bits does not survive an unpack.

## Range argument

After a call has validated `payload_bits` and a field:

1. `payload_bits` is a multiple of 8 from 8 to 65,528.
2. `bit_length` is 1 to 32.
3. `start_bit` is no greater than `payload_bits`.
4. `bit_length` is no greater than `payload_bits - start_bit`.

Condition 3 is checked before the subtraction in condition 4, so the subtraction cannot wrap, even where `size_t` is 16 bits. A sum of `start_bit` and `bit_length` is never formed.

Together, the conditions place every accessed bit between 0 and 65,527, so the `uint16_t` bit-index arithmetic cannot wrap for an accepted field, and the largest logical-octet index is 8,190. The maximum, 65,528, is the largest multiple of 8 that fits a `uint16_t`, which is why it is the limit: `payload_bits`, every bit index, and the octet count all stay representable with a 16-bit `size_t`.

Each loop iteration copies 1 to 8 bits, so a 32-bit field takes at most five iterations. Every shift is by less than 32, wherever the field lies in the payload.

This is a code-level argument, not a machine-checked proof. It assumes caller-owned buffers of the stated size, accessible field tables, and offsets and member types that match the structure.

## Scaled arithmetic

A scaled field is computed in single precision: `(physical - offset) / scale` on pack, then a NaN check, a clamp to the range of the field's integer type, and a conversion that truncates toward zero. The clamp makes the conversion defined for every finite or infinite input, so out-of-range values saturate instead of invoking undefined behaviour.

The clamp limits for the 32-bit types are the largest `float` values that convert without overflow: 4,294,967,040 (`0xFFFFFF00`) for `PPACK_TYPE_UINT32` and 2,147,483,520 (`0x7FFFFF80`) for `PPACK_TYPE_INT32`. `UINT32_MAX` and `INT32_MAX` themselves round up to 2³² and 2³¹ as `float`. The `PPACK_TYPE_INT32` lower limit, -2³¹, is exact.

The clamp uses the type's range rather than the field width, so a scaled field narrower than its type keeps the low bits of the clamped value. [api.md](api.md#scaled-fields) shows the effect and the call-site clamp that avoids it.

## Failure behaviour

Both calls check the pointers, the field table and `payload_bits` before touching memory, then validate each field immediately before copying it. A call that fails on the first checks writes nothing. A call that fails on a field has already copied the fields before it:

- Pack has cleared the whole payload and written those fields, so the buffer holds a partial payload.
- Unpack has written those fields into the structure.

Validation and copying share one pass over the table, so a malformed table is detected only when the call reaches the bad field. Never transmit after a non-zero return, and decode into a temporary before publishing.

## Cost

Pack clears `payload_bits / 8` storage elements, at most 8,191, then copies each field in at most five iterations. Unpack copies each field in at most five iterations and clears nothing. The work is therefore linear in the payload size for pack and in the field count for both, with constant work per field. When the table is `static const`, both are fixed for a given call site, which is what makes the worst-case execution time analysable. A table built at run time with arbitrary contents has no such bound beyond the validation limits.

## Memory and threading

The codec uses only stack locals: no heap, no static or global state, and no recursion. Calls on independent structures and buffers may run concurrently; calls that share a structure or a payload need mutual exclusion supplied by the caller.

## MISRA C:2023

`misch` analyses `include/` and `src/` and reports no findings. The deviations are recorded at the point of use with `cppcheck-suppress ... @deviation` comments, and Rule 15.5 is deviated project-wide in `misra-deviations.txt` for guard-clause returns.

The only required-rule deviations are three uses of Rule 21.15: the type-erased member copies in `ppack_member_write` and `ppack_member_read`, which every field type goes through, and the `float` to `uint32_t` copy behind `PPACK_TYPE_F32`. The remaining deviations are advisory: the `void *` to element-pointer conversions for the payload and the structure (11.5), the `ptr_offset` arithmetic (18.4), the public macros not used inside the library (2.5), and the external linkage of the two public functions (8.7).

## Verification

- `test_ppack` covers validation, every type in raw and scaled form, sign extension, saturation, non-finite and zero-scale handling, layout and alignment patterns, the `uint8_t` storage contract, golden byte arrays that fix the wire format, and seeded fuzz cases. CI runs it natively and with `PPACK_SIMULATE_16BIT_MAU`, both under ASan, UBSan and `-fsanitize=float-cast-overflow` with GCC, and with `PPACK_PAYLOAD_BITS` overridden to 512 and 65,528.
- `tests/test_large_payload.c` checks the codec against an independent bit-at-a-time model that shares none of its index or copy code. In each storage model it covers all 2,096,400 legal start and width pairs at the smallest payload that holds each one; all 8,191 legal payload sizes with a field ending on the boundary; every source-bit position and fixed patterns at every alignment near bit 512, at high offsets, and at the maximum; every type in large unaligned layouts; buffer guards, complete clearing, unpack input preservation, and untouched neighbouring members; malformed sizes, widths and starts near `UINT16_MAX`; and a 496-bit prefix that is unchanged when fields are appended beyond bit 512.
- Configuring the tests checks that valid `PPACK_PAYLOAD_BITS` values build and invalid ones fail.
- `tests/compatibility` builds the current codec and the 2.4.0 codec, the last release limited to 512 bits, into one process under separate names, and compares pack bytes and unpacked members for every 2.4.0 payload size, start and width, across seven raw and four scaled types: 5,512,760 cases per storage model, with seeded values. CI pins the 2.4.0 source to commit `fecf8778423c353a54b7387aa0954887a75fbe49` and runs both models under sanitisers; the command is in [CONTRIBUTING.md](../CONTRIBUTING.md#payload-size-changes).
- A coverage job enforces 100% line and branch coverage of `src/` and `include/`.
- The documentation examples are compiled by the `ppack docs examples` test.
- CI compiles the library with TI C2000 CGT 22.6.2 and 25.11.1 under COFF and EABI, with `--float_support=fpu32` and warnings as errors.
- `scripts/format.sh` enforces the pinned clang-format, and `misch` checks MISRA C:2023.

Not verified here: execution on a 16-bit-MAU target. The host simulation reproduces the storage model but not that target's integer promotions or its `size_t` width, which the range argument covers by construction. Consumers should run their own target tests, including stack depth with their largest payload buffer.
