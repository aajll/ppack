/**
 * @copyright MIT Licence
 *
 * @file: ppack.h
 *
 * @brief
 *    Public API for the ppack library - a generic payload serialisation
 *    library for bit-aligned data fields.
 */

#ifndef PPACK_H_
#define PPACK_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ================ INCLUDES ================================================ */

#include <stddef.h>
#include <stdint.h>

#include "ppack_platform.h"

/*
 * Re-export the platform abstraction so users picking up ppack.h get
 * ppack_byte_t and PPACK_PAYLOAD_UNITS for portable buffer declarations
 * without needing a separate include.
 */
_Static_assert((PPACK_PAYLOAD_UNITS) * (PPACK_ADDR_UNIT_BITS) == 64u,
               "ppack payload must be exactly 64 bits");

/**
 * @defgroup ppack_api Ppack Library
 *
 * @brief Generic payload serialisation/deserialisation for bit-aligned fields.
 *
 * @details
 *    The ppack library serialises and deserialises C structures into a
 *    fixed 64-bit (8 logical-byte) payload using arbitrary bit-aligned
 *    field descriptors. It supports @c uint8_t, @c uint16_t, @c int16_t,
 *    @c uint32_t, @c int32_t, IEEE-754 @c float, and raw bitfields up to
 *    32 bits wide.
 *
 *    The @c base_ptr parameter for pack/unpack operations should point
 *    to the structure that the @c ptr_offset members refer to. The
 *    unpack operation overwrites the structure's members with the
 *    decoded values.
 *
 *    Requires C11 (uses @c _Static_assert in @c ppack_platform.h).
 *
 *    ## Wire format
 *
 *    The payload is treated as 64 bits numbered 0..63. Bit @c N maps to
 *    bit @c (N mod 8) of logical byte @c (N / 8), where logical byte 0
 *    is the first byte on the wire.
 *
 *    Within each logical byte, bit 0 is the least significant bit.
 *    Multi-byte fields are little-endian (Intel ordering, equivalent
 *    to DBC @c byte_order=1). A field at @c start_bit=0, @c bit_length=16
 *    with value @c 0x1234 produces @c 0x34 in byte 0 and @c 0x12 in byte 1.
 *
 *    @c PPACK_TYPE_F32 is a raw 32-bit IEEE-754 bit-copy. The wire
 *    representation is host-endian @c uint32_t. This is interoperable
 *    between any two little-endian IEEE-754 hosts (TI C2000, x86_64,
 *    ARM Cortex-M, AArch64 in default mode). It is NOT safe between a
 *    little-endian and a big-endian host without explicit byte-swapping
 *    in user code.
 *
 *    The payload buffer model is portable across MAU sizes: declare it
 *    as @c ppack_byte_t @c payload[PPACK_PAYLOAD_UNITS]. On byte-
 *    addressable targets this is @c uint8_t[8]; on TI C2000 it is
 *    @c uint16_t[4]. Both produce the same logical byte sequence on
 *    the wire.
 *
 *    ## Scaled-field semantics
 *
 *    When @c behaviour is @c PPACK_BEHAVIOUR_SCALED the library applies:
 *    @code
 *      pack:    raw      = (physical - offset) / scale  -> integer
 *      unpack:  physical = (float)raw * scale + offset
 *    @endcode
 *
 *    @b Saturation: pack silently clamps the raw integer to the
 *    destination type's representable range (e.g. 0..65535 for
 *    @c PPACK_TYPE_UINT16). For safety-critical applications where an
 *    out-of-range physical value MUST be detected, validate the
 *    input before calling @c ppack_pack.
 *
 *    @b Quantization: the float-to-integer cast truncates toward
 *    zero (C standard cast semantics, not round-to-nearest). The
 *    maximum round-trip error is one LSB (one unit of @c scale).
 *    For round-to-nearest behaviour, pre-add
 *    @c 0.5f*scale*sign(physical) at the call site before pack.
 *
 *    @b Float-range clamp: for @c PPACK_TYPE_UINT32 / @c PPACK_TYPE_INT32
 *    scaled fields the upper/lower clamp is the largest float that
 *    is exactly representable AND fits in the destination integer.
 *    @c UINT32_MAX (4294967295) and @c INT32_MAX (2147483647) round
 *    UP to @c 2^32 / @c 2^31 as floats, which would overflow the
 *    destination cast. The library clamps to @c 4294967040 and
 *    @c 2147483520 respectively (the next representable float
 *    below the type's true maximum).
 *
 *    ## TI C2000 / 16-bit MAU notes
 *
 *    On the TI C2000 family (e.g. F28379D) @c uint8_t is aliased to
 *    @c uint16_t and @c CHAR_BIT is 16. ppack auto-detects this and
 *    selects a 16-bit storage unit internally. The wire format is
 *    unchanged. Two contract points specific to this platform:
 *
 *    @li A @c PPACK_TYPE_UINT8 field reads and writes only the LOW 8
 *        bits of its struct member's storage. On C2000 the member's
 *        underlying @c uint16_t can technically hold 0..65535, but
 *        only the low 8 bits round-trip through the wire.
 *
 *    @li The @c ptr_offset value comes from @c offsetof(), which
 *        returns @c char-units. On C2000 a @c char is 16 bits, so
 *        the value is naturally in 16-bit units; the library does
 *        not need to translate it. Always use @c offsetof() rather
 *        than computing offsets by hand.
 *
 * @{
 */

/* ================ DEFINES ================================================= */

/* ---------------- Error Codes --------------------------------------------- */

/** @brief Success */
#define PPACK_SUCCESS      0

/** @brief Invalid arguments (NULL pointer, zero field count, etc.) */
#define PPACK_ERR_INVALARG 1

/* Note: error code 2 is reserved for future use. */

/** @brief Requested item not found */
#define PPACK_ERR_NOTFOUND 3

/** @brief Null pointer detected */
#define PPACK_ERR_NULLPTR  4

/** @brief Field descriptor overflows the 64-bit payload boundary */
#define PPACK_ERR_OVERFLOW 5

/* ================ STRUCTURES ============================================== */

/**
 * @brief Data types supported by the library.
 *
 * The struct member type below indicates the C type the user must
 * declare in their structure for the corresponding @c PPACK_TYPE_*
 * value, when @c behaviour is @c PPACK_BEHAVIOUR_RAW. For
 * @c PPACK_BEHAVIOUR_SCALED the struct member must always be @c float.
 */
enum ppack_type {
        PPACK_TYPE_UINT8 = 0, /**< 8-bit unsigned  (struct member: uint8_t)  */
        PPACK_TYPE_UINT16,    /**< 16-bit unsigned (struct member: uint16_t) */
        PPACK_TYPE_INT16,     /**< 16-bit signed   (struct member: int16_t)  */
        PPACK_TYPE_INT32,     /**< 32-bit signed   (struct member: int32_t)  */
        PPACK_TYPE_UINT32,    /**< 32-bit unsigned (struct member: uint32_t) */
        PPACK_TYPE_F32,       /**< 32-bit float    (struct member: float)    */
        PPACK_TYPE_BITS,      /**< Raw bitfield    (struct member: uint32_t) */
};

/**
 * @brief Field behaviour mode.
 *
 * @note  @c PPACK_TYPE_F32 ignores @c behaviour and always performs a
 *        raw bit-copy. @c PPACK_TYPE_BITS also ignores @c behaviour
 *        and always treats its source as a raw 32-bit pattern.
 *        @c PPACK_TYPE_UINT8 rejects @c PPACK_BEHAVIOUR_SCALED with
 *        @c PPACK_ERR_INVALARG.
 */
enum ppack_behaviour {
        PPACK_BEHAVIOUR_RAW,    /**< Raw value, no scaling */
        PPACK_BEHAVIOUR_SCALED, /**< Apply scale and offset */
};

/**
 * @brief Describes a field within a payload.
 *
 * @note  Set @c bit_length to match the natural width of the @c type
 *        for predictable behaviour. Values larger than the type's
 *        natural width are accepted (the validation only enforces
 *        @c bit_length <= 32 and @c start_bit + bit_length <= 64) but
 *        only the low bits of the source are meaningful, so the
 *        upper wire bits will be zero or sign-extended.
 *
 * @note  @c PPACK_TYPE_F32 always uses raw bit-copy semantics; @c behaviour
 *        and @c scale / @c offset are ignored. This assumes matching
 *        IEEE-754 layout and endianness across nodes.
 *
 * @note  For @c PPACK_BEHAVIOUR_SCALED the struct member must be @c float.
 *        For @c PPACK_BEHAVIOUR_RAW  the struct member must match the
 *        native C type implied by @c type (see @c ppack_type).
 */
struct ppack_field {
        enum ppack_type type; /**< Data type of the field */
        uint16_t start_bit;   /**< Starting bit position in payload (0-63) */
        uint16_t bit_length;  /**< Number of bits (1-32) */
        size_t ptr_offset;    /**< Offset returned by offsetof() into base */
        float scale;          /**< Scaling factor (ignored for F32 / BITS) */
        float offset; /**< Offset after scaling (ignored for F32 / BITS) */
        enum ppack_behaviour
            behaviour; /**< Raw or scaled (see @c ppack_behaviour) */
};

/* ================ TYPEDEFS ================================================ */

/* ================ MACROS ================================================== */

/* ================ GLOBAL VARIABLES ======================================== */

/* ================ GLOBAL PROTOTYPES ======================================= */

/**
 * @brief Pack a payload from given fields into a buffer.
 *
 * Writes a 64-bit payload to @c payload by reading each field's source
 * value from the @c base_ptr structure, applying any scaling, and
 * placing the bits at the field's @c start_bit. Bits outside the
 * union of the field ranges are cleared to zero.
 *
 * @param[in]  base_ptr    Pointer to source structure
 * @param[out] payload     Destination buffer of exactly 64 bits.
 *                         Declare as @c ppack_byte_t @c [PPACK_PAYLOAD_UNITS]
 *                         for portability across MAU sizes.
 * @param[in]  fields      Array of field descriptors
 * @param[in]  field_count Number of fields
 *
 * @return PPACK_SUCCESS on success
 * @return -PPACK_ERR_NULLPTR   if @c base_ptr or @c payload is NULL
 * @return -PPACK_ERR_INVALARG  if @c field_count is 0, @c fields is NULL,
 *                               @c bit_length is 0 or > 32, or scaling is
 *                               requested for @c PPACK_TYPE_UINT8
 * @return -PPACK_ERR_OVERFLOW  if @c start_bit + bit_length exceeds 64,
 *                               or @c scale is 0.0 on a SCALED field
 * @return -PPACK_ERR_NOTFOUND  if an unknown field type is encountered
 *
 * @note  Not thread-safe. Callers sharing @c base_ptr or @c payload
 *        across threads or ISRs must provide their own mutual exclusion.
 *
 * @note  Out-of-range scaled values clamp silently to the destination
 *        type's representable range. See "Scaled-field semantics" in
 *        the file-level docs.
 */
int ppack_pack(const void *base_ptr, void *payload,
               const struct ppack_field *fields, size_t field_count);

/**
 * @brief Unpack a payload from a buffer into specified field locations.
 *
 * Reads a 64-bit payload from @c payload and writes each field's
 * decoded value to the corresponding offset within the @c base_ptr
 * structure. Existing bytes of struct members not covered by any
 * field are left untouched.
 *
 * @param[out] base_ptr    Pointer to destination structure
 * @param[in]  payload     Source buffer of exactly 64 bits.
 *                         Declare as @c ppack_byte_t @c [PPACK_PAYLOAD_UNITS]
 *                         for portability across MAU sizes.
 * @param[in]  fields      Array of field descriptors
 * @param[in]  field_count Number of fields
 *
 * @return PPACK_SUCCESS on success
 * @return -PPACK_ERR_NULLPTR   if @c base_ptr or @c payload is NULL
 * @return -PPACK_ERR_INVALARG  if @c field_count is 0, @c fields is NULL,
 *                               @c bit_length is 0 or > 32, or scaling is
 *                               requested for @c PPACK_TYPE_UINT8
 * @return -PPACK_ERR_OVERFLOW  if @c start_bit + bit_length exceeds 64
 * @return -PPACK_ERR_NOTFOUND  if an unknown field type is encountered
 *
 * @note  Not thread-safe. Callers sharing @c base_ptr or @c payload
 *        across threads or ISRs must provide their own mutual exclusion.
 */
int ppack_unpack(void *base_ptr, const void *payload,
                 const struct ppack_field *fields, size_t field_count);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* PPACK_H_ */
