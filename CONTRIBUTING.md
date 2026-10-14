# Contributing to ppack

`ppack` is a small C11 serialisation primitive for fixed binary payloads. Firmware on different nodes and different addressing models exchanges bytes through it, so the wire format is the contract that carries most of the design weight: a change that moves one bit of an existing field breaks every node that has not been rebuilt. Keep that in mind when you change anything.

## Getting started

Install a C11 compiler and install the Python tools with [uv](https://docs.astral.sh/uv/), as CI does. The MISRA analysis also needs a current `cppcheck`; CI installs it from Homebrew because the 2.13 release in Ubuntu 24.04 reports different findings. The same commands CI runs, locally:

```sh
# Python tools
uv tool install meson
uv tool install ninja
uv tool install gcovr
uv tool install misch

# Configure with tests and sanitisers (CI default)
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined "-Dc_args=-fsanitize=float-cast-overflow" "-Dc_link_args=-fsanitize=float-cast-overflow"
meson compile -C build
meson test -C build --verbose

# Release build (the configuration a downstream consumer uses)
meson setup build_rel --buildtype=release -Dbuild_tests=false
meson compile -C build_rel

# Coverage (CI gate is 100% line and branch for src/ and include/)
meson setup build_cov --buildtype=debug -Dbuild_tests=true -Db_coverage=true
meson compile -C build_cov && meson test -C build_cov
gcovr --root . --filter 'src/' --filter 'include/' --print-summary

# MISRA C:2023 (reads build/compile_commands.json)
env -u MISRA_RULE_TEXTS misch run
misch deviations

# Formatting (pinned clang-format through uvx)
scripts/format.sh --fix
```

`-fsanitize=float-cast-overflow` is added explicitly because GCC leaves it out of `-fsanitize=undefined`, and it is the check that catches an undefined float-to-integer conversion in the scaled paths.

### Payload-size changes

A change to payload validation, bit indexing or buffer clearing also needs the buffer-size overrides and the comparison with the 2.4.0 codec, which CI runs:

```sh
# Default-buffer overrides (CI runs 512 and 65528)
meson setup build_max --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined "-Dc_args=-DPPACK_PAYLOAD_BITS=65528 -fsanitize=float-cast-overflow" "-Dc_link_args=-fsanitize=float-cast-overflow"
meson compile -C build_max && meson test -C build_max

# Exact comparison with the unmodified 2.4.0 codec, from the repository root
legacy_dir=$(mktemp -d)
git archive fecf8778423c353a54b7387aa0954887a75fbe49 include src | tar -x -C "$legacy_dir"
meson setup build_compat tests/compatibility --buildtype=debug "-Dlegacy_root=$legacy_dir" -Db_sanitize=address,undefined -Dc_args=-fsanitize=float-cast-overflow -Dc_link_args=-fsanitize=float-cast-overflow
meson test -C build_compat --verbose
```

Keep the extracted source until the build directory is no longer needed. A shallow clone must fetch the pinned commit first. Never point `legacy_root` at the branch under test. [design.md](docs/design.md#verification) describes what each check covers and what it does not.

## Source style

- Formatting is mandatory. Run `scripts/format.sh --fix` before submitting, or `scripts/format.sh` to check without writing, as CI does. The script pins the clang-format release and runs it through `uvx`, so an editor formatter gives the same result only when it runs the same release.
- 8-space indent, Linux brace style, 80-column limit. Match the existing conventions and do not reformat unrelated code.
- The Meson build system is the single source of truth. Update `meson.build` and `tests/meson.build` when you add or remove files. No CMake, no Make, and no other build systems.

### Section banners

Every `.h` file contains these section banners, in order. `ppack_platform.h` is the one exception: its definitions depend on the addressing model it selects first, so it is ordered by that selection instead.

<!-- snippet: skip -->
```c
/* ================ INCLUDES ================================================ */
/* ================ DEFINES ================================================= */
/* ================ STRUCTURES ============================================== */
/* ================ TYPEDEFS ================================================ */
/* ================ MACROS ================================================== */
/* ================ GLOBAL VARIABLES ======================================== */
/* ================ GLOBAL PROTOTYPES ======================================= */
```

Every `.c` file contains these section banners, in order:

<!-- snippet: skip -->
```c
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

Empty sections stay: an empty `STATIC VARIABLES` banner records the deliberate absence of file-scope mutable state. Do not invent other section styles.

## C language rules

- C11 only.
- Use fixed-width types from `<stdint.h>`, and never plain `int` for a fixed-width quantity. Inside the library, use `ppack_byte_t` for payload storage and `ppack_u8_t` for an 8-bit member, because `uint8_t` is backed by 16-bit storage where `CHAR_BIT` is 16.
- No heap allocation (`malloc`, `free`, VLAs), no recursion, and no global state.
- Validate every pointer argument at the public API boundary.
- Public functions return `int`: `PPACK_SUCCESS` or a negated `PPACK_ERR_*` code. There is no `errno` and no exceptions.
- Bit indices are `uint16_t`. Keep every new index computation inside the [range argument](docs/design.md#range-argument), and check before subtracting, so that it holds where `size_t` is 16 bits.

## MISRA C:2023

The library is analysed with `misch` (cppcheck-backed MISRA C:2023 analysis), configured by `misra.toml`, over `include/` and `src/`. It reads the compilation database from a Meson `build/` directory, so configure one first. The analysis is clean: `misch run` must report zero findings, so there is no baseline. If you have `MISRA_RULE_TEXTS` set, unset it for `misch run`, as CI does. If your change introduces a finding:

1. Prefer restructuring the code so the rule is satisfied. Deviate only when compliance would make the code genuinely worse, and never restructure purely to hide a finding from the checker; an honest deviation beats evasion.
2. Suppress at the point of use with `/* cppcheck-suppress misra-c2012-<rule> ; @deviation <rationale> */`, or add a justified project-wide entry to `misra-deviations.txt` for a house-style rule.
3. Verify with `misch run` and `misch deviations`, which requires every suppression to carry a rationale, and justify the deviation in the pull request description.

A `required` rule deviation faces a higher bar than an `advisory` one. The library carries exactly three, all Rule 21.15: the type-erased member copies in `ppack_member_write` and `ppack_member_read`, and the `float` to `uint32_t` copy for `PPACK_TYPE_F32`. Route new member copies through the existing helpers rather than adding another.

Full tool-driven compliance beyond this analysis needs a certified static analyser, which this repository does not vendor.

## Tests and coverage

- Add a test for every bug fix and every new feature.
- Every test must pass in both storage models: native, and with `PPACK_SIMULATE_16BIT_MAU`. `tests/meson.build` builds the suite twice for this.
- Tests must be deterministic. Fuzz seeds are fixed and recorded in each test, so a failure reproduces from its name alone.
- Tests live in `tests/test_*.c`, written with the `TEST_CASE` and `TEST_ASSERT` macros from `tests/test_harness.h`. Each file's `run_*_tests` function is called from `tests/test_ppack_main.c`, and each new file is listed in `tests/meson.build`.
- CI enforces 100 percent line and branch coverage of `src/` and `include/`. Every new branch needs a test that fails when the branch is broken.
- The `ppack docs examples` test compiles every C code block in `README.md`, `CONTRIBUTING.md`, and `docs/` against the public headers, through `tests/docs/check_snippets.py`. A block compiles at file scope by default. On the line directly above the fence, put `<!-- snippet: body -->` for a block of statements, or `<!-- snippet: skip -->` for pseudo-code or a copy of a header definition.

## Documentation

| File | Purpose |
| --- | --- |
| `README.md` | Landing page: quick start, description, validated toolchains, and links. Keep it short. |
| `docs/integration.md` | Requirements, installation, buffer sizing, building, and cross builds. |
| `docs/api.md` | Reference for every type and function, error codes, scaled-field semantics, and recipes. |
| `docs/design.md` | The wire format, addressing models, range argument, failure behaviour, cost, MISRA position, and verification. |

Update `docs/api.md` and the header Doxygen together when an interface changes, and check every value an example states against a build. Update `docs/design.md` when behaviour or verification changes. Keep it standalone: describe how the library works, not how it was built or which other projects use it. Write prose in British English, one line per paragraph.

## Wire format and API stability

The wire format and the public API in `include/ppack.h` and `include/ppack_platform.h` are the project's contract:

- The wire format is fixed. The golden byte arrays in `tests/test_wire_lockdown.c` record it, and any change to them is a breaking change to the protocol.
- Changing a function signature, the layout of `struct ppack_field`, the value of an enumerator or error code, or the meaning of a configuration macro is a breaking change and needs a major release.
- Renaming or removing a public symbol is a breaking change.
- Accepting input that was rejected, such as a larger payload, is a minor release when the bytes for every previously valid input are unchanged; the legacy comparison is the evidence.

## Commits

Use Conventional Commits:

- `feat: ...` new feature
- `fix: ...` bug fix
- `docs: ...` documentation only
- `test: ...` test-only changes
- `chore: ...` build, CI, release work
- `refactor: ...` a change that neither fixes a bug nor adds a feature

Keep the subject under about 70 characters. Use the body to explain why the change is needed.

## Pull requests

- Open an issue first for non-trivial changes so the design can be agreed before implementation.
- Keep pull requests focused: one feature or one fix.
- All CI checks must pass.
- Add a `CHANGELOG.md` entry under `## [Unreleased]` for a user-visible change.
- Flag any new MISRA deviation, and any change to the bytes a valid input produces, in the description.

## When in doubt

Open an issue. The library is small enough that a modest design change has an outsized effect on every node that shares a payload.
