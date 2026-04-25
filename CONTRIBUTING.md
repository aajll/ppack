# Contributing to ppack

ppack is a small C library used as a serialisation primitive for
fixed-size payloads. It is designed to be safe to drop into
embedded firmware and audited environments.

## Getting started

The same commands CI runs, locally:

```sh
# Configure with tests + sanitisers (CI default)
meson setup build --buildtype=debug -Dbuild_tests=true \
                  -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose

# Coverage (CI gate is 100% line + 100% branch)
meson setup build_cov --buildtype=debug -Dbuild_tests=true \
                      -Db_coverage=true
meson compile -C build_cov && meson test -C build_cov
gcovr --root . --filter 'src/' --filter 'include/' --print-summary
```

## Source style

- `.clang-format` is mandatory. Run `clang-format -i` on every modified
  `.c` / `.h` file before submitting.
- 8-space indent, Linux brace style, 80-column limit. Match the existing
  conventions; do not reformat unrelated code.
- The Meson build system is the single source of truth. Update
  `meson.build` / `tests/meson.build` when adding or removing source files.
- No CMake, no Make, no other build systems.

## C language rules

- C11 only (uses `_Static_assert`).
- Use fixed-width types from `<stdint.h>` and `<stdbool.h>`. Never plain
  `int` for fixed-width fields.
- No heap allocation (`malloc`, `free`, VLAs).
- Validate pointer arguments at every public-API boundary.
- Public functions return `int` (negated `PPACK_ERR_*` on failure). No
  `errno`, no exceptions.

## MISRA C:2012

The library is reviewed against MISRA C:2012. The full deviation record
lives in the file header of `src/ppack.c`. If your change introduces a
new deviation:

1. Add an entry to the file-header deviation record (rule, sites,
   justification, mitigation).
2. Add an inline `/* MISRA <rule> */` marker at the deviation site.
3. Justify the deviation in the PR description.

`required` rule deviations face a higher bar than `advisory`. We
currently have zero required-rule deviations and want to keep it that way.

## Tests and coverage

- Add a test for every bug fix.
- Add a test for every new feature.
- All tests must pass on both build configurations: 8-bit native and
  16-bit MAU simulated (`-DPPACK_SIMULATE_16BIT_MAU`).
- 100% line and branch coverage is enforced by CI. New code without
  tests will fail the coverage gate.
- Tests live in `tests/test_*.c`.
- Fuzz seeds are fixed (`0xBEEF000N`) so failures are reproducible from
  the test name alone. Do not introduce non-deterministic tests.

## Wire format and API stability

ppack defines a public binary wire format. Once a major version is
released, the format is part of the contract:

- The wire format is locked from v1.0.0 onwards. See
  `test_wire_v1_lockdown_*`. Any change to those expected byte arrays is
  a breaking change to the protocol.
- The public API in `ppack.h` is stable across the v1.x line.
- Breaking changes to either go in a new major release (v2.0.0) and
  require a deprecation period.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `doc: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` code change that neither fixes a bug nor adds a feature

Keep the subject under ~70 characters. Use the body to explain _why_ the
change is needed, not _what_ the diff already shows.

## Pull requests

- Open an issue first for non-trivial changes so the design can be agreed
  before implementation.
- Keep PRs focused. One feature or one fix per PR.
- The PR description should explain _why_ the change is needed.
- All CI checks must pass: tests on Linux+macOS, ASan+UBSan, release
  build, coverage gate.
- Any new MISRA deviation must be flagged in the PR description.

## When in doubt

Open an issue and discuss before writing code. The library is small
enough that even modest design changes have outsized implications.
