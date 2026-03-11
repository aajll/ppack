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

/**
 * @defgroup ppack_api Ppack Library
 *
 * @brief Generic payload serialisation/deserialisation for bit-aligned fields.
 *
 * @details
 *    The ppack library provides functions to serialise and deserialise
 *    structured data into fixed-size payloads with arbitrary bit-aligned
 *    fields. It supports uint8_t, uint16_t, int16_t, uint32_t, int32_t,
 *    and float types.
 *
 *    The `base_ptr` parameter for pack/unpack operations should point to the
 *    structure that the `payload_field.ptr_offset` member refers to. The unpack
 *    operation updates the structure's members with the unpacked values.
 *
 * @{
 */

/* ================ DEFINES ================================================= */

/* ---------------- Error Codes --------------------------------------------- */

/** @brief Success */
#define PPACK_SUCCESS      0

/** @brief Invalid arguments (NULL pointer, zero field count, etc.) */
#define PPACK_ERR_INVALARG 1

/** @brief Requested item not found */
#define PPACK_ERR_NOTFOUND 3

/** @brief Null pointer detected */
#define PPACK_ERR_NULLPTR  4

/** @brief Field descriptor overflows the 64-bit payload boundary */
#define PPACK_ERR_OVERFLOW 5

/* ================ STRUCTURES ============================================== */

/**
 * @brief Data types supported by the library.
 */
enum ppack_type {
        PPACK_TYPE_U8 = 0, /**< 8-bit unsigned  (struct member: uint8_t)  */
        PPACK_TYPE_U16,    /**< 16-bit unsigned (struct member: uint16_t) */
        PPACK_TYPE_S16,    /**< 16-bit signed   (struct member: int16_t)  */
        PPACK_TYPE_S32,    /**< 32-bit signed   (struct member: int32_t)  */
        PPACK_TYPE_U32,    /**< 32-bit unsigned (struct member: uint32_t) */
        PPACK_TYPE_F32,    /**< 32-bit float    (struct member: float)    */
        PPACK_TYPE_BITS,   /**< Raw bitfield    (struct member: uint32_t) */
};

/**
 * @brief Field behavior mode.
 */
enum ppack_behaviour {
        PPACK_BEHAVIOUR_RAW,    /**< Raw value, no scaling */
        PPACK_BEHAVIOUR_SCALED, /**< Apply scale and offset */
};

/**
 * @brief Describes a field within a payload.
 *
 * @note  PPACK_TYPE_F32 always uses raw bit-copy semantics; @c behaviour and
 *        @c scale / @c offset are ignored for that type.
 *
 * @note  For PPACK_BEHAVIOUR_SCALED the struct member must be @c float.
 *        For PPACK_BEHAVIOUR_RAW  the struct member must match the native
 *        C type implied by @c type (e.g. @c uint16_t for PPACK_TYPE_U16).
 */
struct ppack_field {
        enum ppack_type type; /**< Data type of the field */
        uint16_t start_bit;   /**< Starting bit position in payload (0-63) */
        uint16_t bit_length;  /**< Number of bits (1-32) */
        size_t ptr_offset;    /**< Byte offset from base_ptr (use offsetof()) */
        float scale;          /**< Scaling factor (ignored for F32) */
        float offset;         /**< Offset after scaling (ignored for F32) */
        enum ppack_behaviour behaviour; /**< Raw or scaled (ignored for F32) */
};

/* ================ TYPEDEFS ================================================ */

/* ================ MACROS ================================================== */

/* ================ GLOBAL VARIABLES ======================================== */

/* ================ GLOBAL PROTOTYPES ======================================= */

/**
 * @brief Pack a payload from given fields into a buffer.
 *
 * @param[in]  base_ptr    Pointer to source structure
 * @param[out] payload     Destination buffer (exactly 8 bytes)
 * @param[in]  fields      Array of field descriptors
 * @param[in]  field_count Number of fields
 *
 * @return PPACK_SUCCESS on success
 * @return -PPACK_ERR_NULLPTR   if a NULL pointer is passed
 * @return -PPACK_ERR_INVALARG  if field_count is 0 or fields is NULL, or
 *                               bit_length is 0 or > 32
 * @return -PPACK_ERR_OVERFLOW  if start_bit + bit_length exceeds 64, or
 *                               scale is 0.0 on a SCALED field
 * @return -PPACK_ERR_NOTFOUND  if an unknown field type is encountered
 */
int ppack_pack(const void *base_ptr, void *payload,
               const struct ppack_field *fields, size_t field_count);

/**
 * @brief Unpack a payload from a buffer into specified field locations.
 *
 * @param[out] base_ptr    Pointer to destination structure
 * @param[in]  payload     Source buffer (exactly 8 bytes)
 * @param[in]  fields      Array of field descriptors
 * @param[in]  field_count Number of fields
 *
 * @return PPACK_SUCCESS on success
 * @return -PPACK_ERR_NULLPTR   if a NULL pointer is passed
 * @return -PPACK_ERR_INVALARG  if field_count is 0 or fields is NULL, or
 *                               bit_length is 0 or > 32
 * @return -PPACK_ERR_OVERFLOW  if start_bit + bit_length exceeds 64
 * @return -PPACK_ERR_NOTFOUND  if an unknown field type is encountered
 */
int ppack_unpack(void *base_ptr, const void *payload,
                 const struct ppack_field *fields, size_t field_count);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* PPACK_H_ */
