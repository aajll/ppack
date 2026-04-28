/**
 * @copyright MIT Licence
 *
 * @file: ppack_platform.h
 *
 * @brief
 *    Platform abstraction for ppack. Detects whether the target's
 *    minimum addressable unit (MAU) is 8 or 16 bits and exposes the
 *    type aliases and compile-time constants the library needs.
 *
 *    On byte-addressable targets (x86_64, ARM, AArch64, RISC-V, AVR, etc.)
 *    @c ppack_byte_t is @c uint8_t.
 *
 *    On word-addressable targets where @c CHAR_BIT is 16 (e.g. the
 *    Texas Instruments C2000 family, including the F28379D),
 *    @c ppack_byte_t is @c uint16_t. All internal pack/unpack operations
 *    then work in 16-bit units.
 *
 *    @c PPACK_PAYLOAD_BITS is a user-overridable convenience macro that
 *    sizes @c PPACK_PAYLOAD_UNITS for stack buffer declarations. Define
 *    it before including the header (e.g. @c -DPPACK_PAYLOAD_BITS=128
 *    or @c -DPPACK_PAYLOAD_BITS=512) to change the default. The runtime
 *    @c ppack_pack / @c ppack_unpack API takes the payload size as an
 *    explicit argument and does not depend on this macro.
 *
 *    The library's wire format is identical across both addressing
 *    models: a sequence of N bits (where N is the @c payload_bits
 *    argument, a multiple of 8 between 8 and 512), with bit 0
 *    corresponding to the least significant bit of the first logical
 *    8-bit byte. See the "Wire format" section in the README for the
 *    full contract.
 *
 *    Define @c PPACK_SIMULATE_16BIT_MAU at compile time on a byte-
 *    addressable host to exercise the word-addressable code path
 *    against host unit tests. This is for library development and
 *    test infrastructure; production builds should leave it undefined
 *    and rely on auto-detection.
 */

#ifndef PPACK_PLATFORM_H_
#define PPACK_PLATFORM_H_

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Auto-detection: a target is treated as 16-bit MAU when CHAR_BIT > 8
 * or when UCHAR_MAX exceeds 255. PPACK_SIMULATE_16BIT_MAU forces this
 * branch on byte-addressable hosts for unit-test simulation.
 */
#if defined(PPACK_SIMULATE_16BIT_MAU) || CHAR_BIT > 8 || UCHAR_MAX > 255u
/** @brief Bits per minimum addressable unit on this target. */
#define PPACK_ADDR_UNIT_BITS 16u
/** @brief Storage unit used for the payload buffer. */
typedef uint16_t ppack_byte_t;
#else
#define PPACK_ADDR_UNIT_BITS 8u
typedef uint8_t ppack_byte_t;
#endif

/**
 * @brief Storage type that a user's `uint8_t` struct member resolves
 *        to on the current target.
 *
 * On byte-addressable hosts this is @c uint8_t. On TI C2000 the
 * compiler aliases @c uint8_t to @c uint16_t, and this typedef
 * mirrors that aliasing.
 *
 * @note  This is a library-internal alias used by the implementation
 *        and the unit tests. Application code on real targets should
 *        continue to declare struct members as @c uint8_t for
 *        @c PPACK_TYPE_UINT8 fields. @c ppack_u8_t exists so that host
 *        builds with @c PPACK_SIMULATE_16BIT_MAU faithfully reproduce
 *        the C2000 storage layout.
 */
typedef ppack_byte_t ppack_u8_t;

/**
 * @brief Default payload size in bits for the @c PPACK_PAYLOAD_UNITS
 *        convenience macro.
 *
 * Override at the toolchain level (e.g. @c -DPPACK_PAYLOAD_BITS=128)
 * to size stack buffer declarations to a non-default payload. Must
 * be a positive multiple of @c PPACK_ADDR_UNIT_BITS and no greater
 * than 512.
 *
 * The runtime @c ppack_pack / @c ppack_unpack API takes the payload
 * size as an explicit argument and is independent of this macro.
 */
#ifndef PPACK_PAYLOAD_BITS
#define PPACK_PAYLOAD_BITS 64u
#endif

/**
 * @brief Number of addressable units occupied by a payload of
 *        @c PPACK_PAYLOAD_BITS bits.
 *
 * Defaults to @c 8 on byte-addressable targets and @c 4 on 16-bit MAU
 * targets, matching the historical 64-bit payload size.
 *
 * Use this to declare a portable payload buffer:
 * @code
 * ppack_byte_t payload[PPACK_PAYLOAD_UNITS];
 * @endcode
 */
#define PPACK_PAYLOAD_UNITS (PPACK_PAYLOAD_BITS / PPACK_ADDR_UNIT_BITS)

/** @brief Bitmask covering one addressable unit. */
#define PPACK_WORD_MASK     (((uint32_t)1u << PPACK_ADDR_UNIT_BITS) - 1u)

/**
 * @brief Convert a payload bit index to its addressable-unit index.
 *
 * @param[in] bit Bit position within the payload.
 * @return        Index into a @c ppack_byte_t array.
 */
static inline uint16_t
ppack_bit_to_unit(uint16_t bit)
{
        return (uint16_t)((uint32_t)bit / PPACK_ADDR_UNIT_BITS);
}

/**
 * @brief Convert a payload bit index to its intra-unit bit offset.
 *
 * @param[in] bit Bit position within the payload.
 * @return        Bit offset (0..PPACK_ADDR_UNIT_BITS-1) within the
 *                addressable unit returned by @c ppack_bit_to_unit.
 */
static inline uint16_t
ppack_bit_to_shift(uint16_t bit)
{
        return (uint16_t)((uint32_t)bit % PPACK_ADDR_UNIT_BITS);
}

/* Catch unsupported configurations at compile time. */
_Static_assert(PPACK_ADDR_UNIT_BITS == 8 || PPACK_ADDR_UNIT_BITS == 16,
               "ppack only supports 8-bit or 16-bit addressable units");

_Static_assert(PPACK_PAYLOAD_BITS > 0u
                   && PPACK_PAYLOAD_BITS % PPACK_ADDR_UNIT_BITS == 0u,
               "PPACK_PAYLOAD_BITS must be a positive multiple of "
               "PPACK_ADDR_UNIT_BITS");

_Static_assert(PPACK_PAYLOAD_BITS <= 512u,
               "PPACK_PAYLOAD_BITS must be at most 512 "
               "(CAN-FD frame data field ceiling)");

#endif /* PPACK_PLATFORM_H_ */
