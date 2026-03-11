/**
 * @copyright MIT Licence
 *
 * @file: ppack.c
 *
 * @brief
 *    Implementation of the ppack library - generic payload serialisation.
 */

/* ================ INCLUDES ================================================ */

#include <stdint.h>
#include <string.h>

#include "ppack.h"

/* ================ DEFINES ================================================= */

/** Maximum payload size in bits (fixed 8-byte buffer). */
#define PPACK_PAYLOAD_BITS   64u

/** Maximum supported field width in bits. */
#define PPACK_MAX_FIELD_BITS 32u

/* ================ STRUCTURES ============================================== */

/* ================ TYPEDEFS ================================================ */

/* ================ STATIC PROTOTYPES ======================================= */

/* ================ STATIC VARIABLES ======================================== */

/* ================ MACROS ================================================== */

/* ================ STATIC FUNCTIONS ======================================== */

/**
 * @brief Write a bitfield of arbitrary length into a payload buffer.
 *
 * @param[in,out] payload   Output buffer (8 bytes)
 * @param[in]     start_bit Starting bit position (0-63)
 * @param[in]     bit_len   Number of bits to write (1-32)
 * @param[in]     value     Raw 32-bit value (lower bit_len bits written)
 *
 * @pre  start_bit + bit_len <= 64  (caller must validate)
 * @pre  bit_len <= 32              (caller must validate)
 */
static void
write_bits(void *payload, uint16_t start_bit, uint16_t bit_len, uint32_t value)
{
        uint8_t *bytes = (uint8_t *)payload;

        for (uint16_t i = 0; i < bit_len; ++i) {
                uint16_t bit_pos = start_bit + i;
                uint16_t byte_index = bit_pos / 8u;
                uint16_t bit_index = bit_pos % 8u;

                uint32_t bit_val = (value >> i) & 1u;
                if (bit_val) {
                        bytes[byte_index] |= (uint8_t)(1u << bit_index);
                } else {
                        bytes[byte_index] &= (uint8_t)(~(1u << bit_index));
                }
        }
}

/**
 * @brief Read a bitfield of arbitrary length from a payload buffer.
 *
 * @param[in] payload   Input buffer (8 bytes)
 * @param[in] start_bit Starting bit position (0-63)
 * @param[in] bit_len   Number of bits to read (1-32)
 *
 * @return 32-bit value with extracted bits in LSB positions
 *
 * @pre  start_bit + bit_len <= 64  (caller must validate)
 * @pre  bit_len <= 32              (caller must validate)
 */
static uint32_t
read_bits(const void *payload, uint16_t start_bit, uint16_t bit_len)
{
        const uint8_t *bytes = (const uint8_t *)payload;
        uint32_t result = 0;

        for (uint16_t i = 0; i < bit_len; ++i) {
                uint16_t bit_pos = start_bit + i;
                uint16_t byte_index = bit_pos / 8u;
                uint16_t bit_index = bit_pos % 8u;

                uint32_t bit =
                    ((uint32_t)(bytes[byte_index]) >> bit_index) & 1u;
                result |= (bit << i);
        }

        return result;
}

/**
 * @brief Sign-extend a value from @p width bits to 32 bits.
 *
 * @param[in] value Raw unsigned bit pattern (lower @p width bits are valid)
 * @param[in] width Number of bits in the original signed field (1-32)
 *
 * @return Sign-extended 32-bit signed integer
 *
 * @note  Uses a well-defined arithmetic shift via a mask + signed cast
 *        (implementation-defined only for the shift itself, which is safe on
 *        all two's-complement targets; explicitly documented as the intended
 *        behaviour).
 */
static int32_t
sign_extend(uint32_t value, uint16_t width)
{
        uint32_t sign_bit = (uint32_t)1u << (width - 1u);

        /* If sign bit is set, fill upper bits with 1s */
        if (value & sign_bit) {
                return (int32_t)(value | ~(sign_bit - 1u));
        }
        return (int32_t)value;
}

/**
 * @brief Validate a field descriptor for use with an 8-byte payload.
 *
 * @param[in] f  Pointer to the field descriptor to check
 *
 * @return PPACK_SUCCESS on success
 * @return -PPACK_ERR_INVALARG  if bit_length is 0 or > 32
 * @return -PPACK_ERR_OVERFLOW  if start_bit + bit_length exceeds 64
 */
static int
validate_field(const struct ppack_field *f)
{
        if (f->bit_length == 0u || f->bit_length > PPACK_MAX_FIELD_BITS) {
                return -PPACK_ERR_INVALARG;
        }
        if ((uint32_t)f->start_bit + (uint32_t)f->bit_length
            > PPACK_PAYLOAD_BITS) {
                return -PPACK_ERR_OVERFLOW;
        }
        return PPACK_SUCCESS;
}

/* ================ GLOBAL PROTOTYPES ======================================= */

int
ppack_unpack(void *base_ptr, const void *payload,
             const struct ppack_field *fields, size_t field_count)
{
        if (base_ptr == NULL || payload == NULL) {
                return -PPACK_ERR_NULLPTR;
        }

        if (field_count == 0u || fields == NULL) {
                return -PPACK_ERR_INVALARG;
        }

        for (size_t i = 0; i < field_count; ++i) {
                const struct ppack_field *f = &fields[i];

                int vret = validate_field(f);
                if (vret != PPACK_SUCCESS) {
                        return vret;
                }

                uint8_t *field_ptr = (uint8_t *)base_ptr + f->ptr_offset;
                uint32_t raw = read_bits(payload, f->start_bit, f->bit_length);

                switch (f->type) {
                case PPACK_TYPE_U8: {
                        uint8_t tmp = (uint8_t)raw;
                        memcpy(field_ptr, &tmp, sizeof(tmp));
                        break;
                }

                case PPACK_TYPE_U16: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float tmp = ((float)(uint16_t)raw) * f->scale
                                            + f->offset;
                                memcpy(field_ptr, &tmp, sizeof(tmp));
                        } else {
                                uint16_t tmp = (uint16_t)raw;
                                memcpy(field_ptr, &tmp, sizeof(tmp));
                        }
                        break;
                }

                case PPACK_TYPE_S16: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                int32_t sval = sign_extend(raw, f->bit_length);
                                float tmp =
                                    ((float)sval) * f->scale + f->offset;
                                memcpy(field_ptr, &tmp, sizeof(tmp));
                        } else {
                                int16_t tmp =
                                    (int16_t)sign_extend(raw, f->bit_length);
                                memcpy(field_ptr, &tmp, sizeof(tmp));
                        }
                        break;
                }

                case PPACK_TYPE_U32: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float tmp = ((float)raw) * f->scale + f->offset;
                                memcpy(field_ptr, &tmp, sizeof(tmp));
                        } else {
                                memcpy(field_ptr, &raw, sizeof(raw));
                        }
                        break;
                }

                case PPACK_TYPE_S32: {
                        int32_t sval = sign_extend(raw, f->bit_length);
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float tmp =
                                    ((float)sval) * f->scale + f->offset;
                                memcpy(field_ptr, &tmp, sizeof(tmp));
                        } else {
                                memcpy(field_ptr, &sval, sizeof(sval));
                        }
                        break;
                }

                case PPACK_TYPE_F32: {
                        float tmp;
                        memcpy(&tmp, &raw, sizeof(tmp));
                        memcpy(field_ptr, &tmp, sizeof(tmp));
                        break;
                }

                case PPACK_TYPE_BITS: {
                        memcpy(field_ptr, &raw, sizeof(raw));
                        break;
                }

                default: return -PPACK_ERR_NOTFOUND;
                }
        }

        return PPACK_SUCCESS;
}

int
ppack_pack(const void *base_ptr, void *payload,
           const struct ppack_field *fields, size_t field_count)
{
        if (base_ptr == NULL || payload == NULL) {
                return -PPACK_ERR_NULLPTR;
        }

        if (field_count == 0u || fields == NULL) {
                return -PPACK_ERR_INVALARG;
        }

        memset(payload, 0, 8); /* Clear 8-byte payload */

        for (size_t i = 0; i < field_count; ++i) {
                const struct ppack_field *f = &fields[i];

                int vret = validate_field(f);
                if (vret != PPACK_SUCCESS) {
                        return vret;
                }

                const uint8_t *field_ptr =
                    (const uint8_t *)base_ptr + f->ptr_offset;
                uint32_t raw = 0u;

                switch (f->type) {
                case PPACK_TYPE_U8: {
                        uint8_t tmp;
                        memcpy(&tmp, field_ptr, sizeof(tmp));
                        raw = (uint32_t)tmp;
                        break;
                }

                case PPACK_TYPE_U16: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                if (f->scale == 0.0f) {
                                        return -PPACK_ERR_OVERFLOW;
                                }
                                float val;
                                memcpy(&val, field_ptr, sizeof(val));
                                raw = (uint32_t)(uint16_t)((val - f->offset)
                                                           / f->scale);
                        } else {
                                uint16_t tmp;
                                memcpy(&tmp, field_ptr, sizeof(tmp));
                                raw = (uint32_t)tmp;
                        }
                        break;
                }

                case PPACK_TYPE_S16: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                if (f->scale == 0.0f) {
                                        return -PPACK_ERR_OVERFLOW;
                                }
                                float val;
                                memcpy(&val, field_ptr, sizeof(val));
                                raw = (uint32_t)(int16_t)((val - f->offset)
                                                          / f->scale);
                        } else {
                                int16_t tmp;
                                memcpy(&tmp, field_ptr, sizeof(tmp));
                                raw = (uint32_t)tmp;
                        }
                        break;
                }

                case PPACK_TYPE_U32: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                if (f->scale == 0.0f) {
                                        return -PPACK_ERR_OVERFLOW;
                                }
                                float val;
                                memcpy(&val, field_ptr, sizeof(val));
                                raw = (uint32_t)((val - f->offset) / f->scale);
                        } else {
                                memcpy(&raw, field_ptr, sizeof(raw));
                        }
                        break;
                }

                case PPACK_TYPE_S32: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                if (f->scale == 0.0f) {
                                        return -PPACK_ERR_OVERFLOW;
                                }
                                float val;
                                memcpy(&val, field_ptr, sizeof(val));
                                raw = (uint32_t)(int32_t)((val - f->offset)
                                                          / f->scale);
                        } else {
                                int32_t tmp;
                                memcpy(&tmp, field_ptr, sizeof(tmp));
                                raw = (uint32_t)tmp;
                        }
                        break;
                }

                case PPACK_TYPE_F32: {
                        uint32_t tmp;
                        memcpy(&tmp, field_ptr, sizeof(tmp));
                        raw = tmp;
                        break;
                }

                case PPACK_TYPE_BITS: {
                        memcpy(&raw, field_ptr, sizeof(raw));
                        break;
                }

                default: return -PPACK_ERR_NOTFOUND;
                }

                write_bits(payload, f->start_bit, f->bit_length, raw);
        }

        return PPACK_SUCCESS;
}
