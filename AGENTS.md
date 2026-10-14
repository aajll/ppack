# AGENTS.md

---

## 1) Project-specific instructions

**Project:** `ppack`
**Primary goal:** A C11 table-driven codec that serialises C structures into fixed bit-aligned little-endian payloads of up to 65,528 bits and back, with scaled physical-unit fields, an identical wire format on 8-bit and 16-bit addressable targets, caller-owned storage and no allocation.

### 1.1 Essential commands

#### Configure and build (library only)

```sh
meson setup build --wipe --buildtype=release -Dbuild_tests=false
meson compile -C build
```

#### Configure, build, and run unit tests

```sh
meson setup build --wipe --buildtype=debug -Dbuild_tests=true
meson compile -C build
meson test -C build --verbose
```

#### MISRA C:2023 (reads `build/compile_commands.json`)

```sh
env -u MISRA_RULE_TEXTS misch run
misch deviations
```

#### Formatting (pinned clang-format through `uvx`)

```sh
scripts/format.sh          # check, as CI does
scripts/format.sh --fix    # rewrite in place
```

### 1.2 Notes

- `meson setup` generates the optional `ppack_version.h` into the build directory; `ppack.h` does not include it.
- `PPACK_PAYLOAD_BITS` (default 64) sizes only the `PPACK_PAYLOAD_UNITS` buffer macro; every call takes the payload size explicitly. `PPACK_SIMULATE_16BIT_MAU` selects the 16-bit storage model on a host, for tests only. The only Meson option is `build_tests`.
- The test suite builds twice, native and with `PPACK_SIMULATE_16BIT_MAU`. `tests/compatibility` is a separate Meson project that compares against the pinned 2.4.0 source; the command is in `CONTRIBUTING.md`.

---

## 2) Non-negotiable design invariants

- **Wire format is fixed.** LSB-first bit numbering, little-endian multi-octet fields, zero in every bit no field covers. Never change the expected bytes in `tests/test_wire_lockdown.c` without a major release.
- **Same bytes on both addressing models.** One logical octet per `ppack_byte_t` element, in its low 8 bits, whether `CHAR_BIT` is 8 or 16. `ptr_offset` is an `offsetof` value in `char` units.
- **16-bit index arithmetic.** Bit indices are `uint16_t` and `size_t` may be 16 bits. Check `start_bit <= payload_bits` before subtracting, and never form `start_bit + bit_length`.
- **Bounded work.** At most five loop iterations per field, every shift below 32, no recursion.
- **Explicit errors.** Every public call validates its pointers, table, size and fields and returns a negated `PPACK_ERR_*` code. Scaled conversions clamp before converting, so no float-to-integer conversion is undefined.
- **No exact 8-bit storage assumptions.** Use `ppack_byte_t` and `ppack_u8_t` inside the library; `uint8_t` is 16-bit storage where `CHAR_BIT` is 16.
- No heap, no recursion, no global state.

---

## 3) CI / source of truth

- CI definitions live in `.github/workflows/ci.yml`.
- Prefer running the same commands locally as CI runs (see §1.1 above).
- Install Python tools (Meson, Ninja, gcovr, misch) with `uv tool install`, never `pip`; CI does the same through `astral-sh/setup-uv`. CI installs `cppcheck` from Homebrew because Ubuntu's 2.13 reports different findings.
- `misch run` must report zero findings; there is no baseline. Run it with `MISRA_RULE_TEXTS` unset, as CI does.
- The CI jobs are: tests under ASan/UBSan and float-cast-overflow in both storage models plus the 2.4.0 comparison, `PPACK_PAYLOAD_BITS` overrides of 512 and 65528, a release build, a 100 percent line and branch coverage gate, a TI C2000 library build (CGT 22.6.2 and 25.11.1, COFF and EABI, warnings as errors), `clang-format`, and `misch`.

---

## 4) Docs / commit conventions

### Documentation layout

- `README.md` is a short landing page (about 70 lines). Put detail in `docs/`, never in the README; link to the owning file instead.
- `docs/integration.md` owns requirements, install methods, buffer sizing, building, and cross builds.
- `docs/api.md` owns the API reference, error codes, scaled-field semantics and recipes. Change it together with the headers; keep signatures in fenced `c` blocks and describe them in prose, not function tables. Check every value an example states against a build.
- `docs/design.md` is the single authoritative design record: wire format, addressing models, range argument, failure behaviour, cost, MISRA position, and verification.
- `CONTRIBUTING.md` documents the contributor workflow; `CHANGELOG.md` holds release history (Keep a Changelog); `.github/SECURITY.md` covers vulnerability reporting.
- Code blocks in `README.md`, `CONTRIBUTING.md`, and `docs/` are compiled by the `ppack docs examples` test (`tests/docs/check_snippets.py`). Each document is compiled as one source file: a `c` fence is file-level code, `<!-- snippet: body -->` wraps statements, and `<!-- snippet: skip -->` excludes pseudo-code. Do not use `skip` to silence a broken example.
- Prose uses British spelling. Do not hard-wrap Markdown: write one line per paragraph. The 80-column limit applies to C source, not Markdown.

### Commits

- Use **Conventional Commits** format when asked to commit.
- Keep commits focused; explain *why* in the message body.

---

## 5) C style expectations

### Build & configuration

- Use the Meson build system. Do not introduce CMake, Make, or other systems.
- Update `meson.build` and `tests/meson.build` when adding or removing files.

### Formatting

- `.clang-format` is present and **mandatory**. Run `scripts/format.sh --fix` before committing. It runs the clang-format release pinned in the script through `uvx`. Do not use a system `clang-format`: another release can format the same code differently.
- Do not reformat unrelated code.
- Key settings: 8-space indent, `BreakBeforeBraces: Linux`, column limit 80.

### Section banners

Every `.h` file except `ppack_platform.h`, which is ordered by its addressing-model selection, contains these banners, in order:

```
/* ================ INCLUDES ================================================ */
/* ================ DEFINES ================================================= */
/* ================ STRUCTURES ============================================== */
/* ================ TYPEDEFS ================================================ */
/* ================ MACROS ================================================== */
/* ================ GLOBAL VARIABLES ======================================== */
/* ================ GLOBAL PROTOTYPES ======================================= */
```

Every `.c` file contains these banners, in order:

```
/* ================ INCLUDES ================================================ */
/* ================ DEFINES ================================================= */
/* ================ STRUCTURES ============================================== */
/* ================ TYPEDEFS ================================================ */
/* ================ STATIC PROTOTYPES ======================================= */
/* ================ STATIC VARIABLES ======================================== */
/* ================ MACROS ================================================== */
/* ================ STATIC FUNCTIONS ======================================== */
/* ================ GLOBAL FUNCTIONS ======================================== */
```

Use the exact banner format. Sections may be empty; an empty banner records a deliberate absence.

### Style & correctness

- Match conventions in the existing files (indentation, braces, naming).
- Keep public headers minimal and stable.
- Validate pointer arguments at every public API boundary.
- Use fixed-width types from `<stdint.h>`; never plain `int` for a fixed-width quantity.

### Error handling

- Public functions return `int`: `PPACK_SUCCESS` or a negated `PPACK_ERR_*` code. No `errno`; no exceptions.

### MISRA deviations

- Prefer compliant code; never restructure only to hide a finding. Deviate at the point of use with `/* cppcheck-suppress misra-c2012-<rule> ; @deviation <rationale> */`, or project-wide in `misra-deviations.txt` for a house-style rule.
- The only required-rule deviations are three Rule 21.15 sites. Route new member copies through `ppack_member_write` and `ppack_member_read` rather than adding another.

### Comment placement (Doxygen)

- Inline trailing annotations (`/**< ... */`) on `enum`/`struct` members are allowed only when the resulting line fits the 80-column limit.
- If any member's annotation would overrun, move **all** of that aggregate's member docs into a single structured Doxygen block above the type, as an `@details` list of `- ::SYMBOL  description` entries.
- Never mix inline and block forms within one aggregate, and never leave a trailing comment that clang-format would wrap onto a second line.
- After editing, run `scripts/format.sh` and an 80-column scan.

### Testing

- Run `meson test -C build` after every change; both storage models must pass.
- Add a test case for each bug fix and each new feature.
- Tests must be deterministic; fuzz seeds are fixed in each test.
- For payload-size, indexing or clearing changes, also run the 65528 override and the 2.4.0 comparison.
- Keep `src/` and `include/` at the coverage gate (100 percent line and branch).
