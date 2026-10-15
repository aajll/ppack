# Integration guide

This guide shows how to add `ppack` to a project, size its buffers, build it, and run its tests. The API is described in [api.md](api.md), and the wire format and its guarantees in [design.md](design.md).

## Requirements

- A C11 compiler; `ppack_platform.h` checks its configuration with `_Static_assert`.
- A conformant `<stdint.h>` and `<limits.h>`, and `CHAR_BIT` equal to 8 or 16. Other values are rejected at compile time.
- Two's-complement signed integers, which the sign extension and the unsigned-to-signed conversions rely on.
- IEEE 754 binary32 `float`, the same size as `uint32_t`. `PPACK_TYPE_F32` and scaled fields depend on it, and a compile-time check enforces the size.
- For the tests: Meson, Ninja, and Python 3.

The library has no chip-specific code paths. It selects its addressing model from `CHAR_BIT` and `UCHAR_MAX`, so any target that meets these requirements is expected to work.

## Copy-in

Copy-in is the recommended method for embedded targets. Copy three files into your source tree:

```
include/ppack.h
include/ppack_platform.h
src/ppack.c
```

Keep both headers on the same include path, because `ppack.h` includes `ppack_platform.h`. Then include the public header:

```c
#include "ppack.h"
```

`ppack.h` defines the version macros described in [api.md](api.md#version).

## Meson subproject

Put the repository in `subprojects/ppack`, as a checkout or through a wrap file, then declare the dependency:

```meson
ppack_dep = dependency('ppack', fallback: ['ppack', 'ppack_dep'])
```

The build calls `meson.override_dependency('ppack', ...)`, so other subprojects in the same build find `ppack` by name. Include the header as `"ppack.h"`. Tests are off unless the `build_tests` option is set.

| Option | Default | Effect |
| --- | --- | --- |
| `build_tests` | `false` | Builds the unit tests, both addressing models, and the documentation example test. |

## Installed package

```sh
meson setup build --buildtype=release -Dbuild_tests=false --prefix=/usr/local
meson compile -C build
meson install -C build
cc app.c $(pkg-config --cflags --libs ppack) -o app
```

The headers install to a `ppack/` subdirectory, and the `pkg-config` flags add that directory to the include path, so the same include works as in a copy-in build. Without `pkg-config`, include the namespaced path instead:

<!-- snippet: skip -->
```c
#include <ppack/ppack.h>
```

`ppack_version.h` is installed beside it as a deprecated header that includes `ppack.h`.

## Buffer sizing

A payload buffer is an array of `ppack_byte_t` with one element per logical octet. `ppack_byte_t` is `uint8_t` on a byte-addressable target and `uint16_t` where `CHAR_BIT` is 16, so the element count is the same on both and only the storage per element differs.

`PPACK_PAYLOAD_UNITS` is the element count for the default size, `PPACK_PAYLOAD_BITS`, which is 64. For any other size, declare `payload_bits / PPACK_ADDR_UNIT_BITS` elements; `PPACK_ADDR_UNIT_BITS` is 8 on every target.

```c
ppack_byte_t classic_frame[PPACK_PAYLOAD_UNITS];      /* 64 bits */
ppack_byte_t fd_frame[512u / PPACK_ADDR_UNIT_BITS];   /* 512 bits */
```

To change the default, define `PPACK_PAYLOAD_BITS` with `-D`, identically for the library source and every file that includes the header. It must be a positive multiple of 8 and no greater than `PPACK_MAX_PAYLOAD_BITS`, 65,528, or the build fails. The default only sizes `PPACK_PAYLOAD_UNITS`: every call still takes the payload size as an argument.

```sh
meson setup build "-Dc_args=-DPPACK_PAYLOAD_BITS=512"
```

The codec cannot see the size of the buffer behind a pointer. The caller must supply at least `payload_bits / 8` elements, and a larger `payload_bits` does not resize a buffer. Do not allocate a maximum-size buffer on a small stack.

## Build and test

```sh
# Library only
meson setup build --buildtype=release -Dbuild_tests=false
meson compile -C build

# Unit tests and the documentation test under sanitisers
meson setup build --buildtype=debug -Dbuild_tests=true -Db_sanitize=address,undefined
meson compile -C build
meson test -C build --verbose
```

The unit tests run twice: natively, and with `PPACK_SIMULATE_16BIT_MAU` defined, which selects the 16-bit storage model on a byte-addressable host. That macro is for host testing only; production builds leave it undefined and rely on detection. Configuring the tests also checks that the build accepts valid `PPACK_PAYLOAD_BITS` values and rejects invalid ones.

Coverage, formatting, MISRA and legacy-comparison commands are in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Cross builds

The consuming firmware owns its cross file, linker command file, and memory placement. No cross file ships with this project.

CI compiles the library with warnings as errors using two releases of a vendor C11 compiler for a 16-bit-MAU DSP target, where `CHAR_BIT` is 16, under both of its object formats and with its hardware single-precision floating point enabled. This covers the source dialect and type assumptions only. Linking, execution, and the final map-file footprint remain checks for the consumer, which [design.md](design.md#verification) sets out.

On a 16-bit-MAU target, a `PPACK_TYPE_UINT8` struct member occupies a 16-bit storage unit, and `offsetof` counts in 16-bit units. Both are handled by the library, provided every `ptr_offset` comes from `offsetof`; see [api.md](api.md#field-descriptors).
