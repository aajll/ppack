/**
 * @copyright MIT Licence
 *
 * @file: ppack.c
 *
 * @brief
 *    Implementation of the ppack library - generic payload serialisation.
 */

/* ================ INCLUDES ================================================ */

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ppack.h"

/* ================ DEFINES ================================================= */

/* ================ STRUCTURES ============================================== */

/* ================ TYPEDEFS ================================================ */

/* ================ STATIC PROTOTYPES ======================================= */

/* ================ STATIC VARIABLES ======================================== */

/* ================ MACROS ================================================== */

/* ================ STATIC FUNCTIONS ======================================== */

/**
 * @brief Write a bitfield of arbitrary length into a payload buffer.
 *
 * @param[in,out] payload  Output buffer (byte-addressable)
 * @param[in]     start_bit Starting bit position (0-63)
 * @param[in]     bit_len   Number of bits to write (must be <= 32)
 * @param[in]     value     Raw 32-bit value (lower bit_len bits written)
 */
static inline void
write_bits(void *payload, uint16_t start_bit, uint16_t bit_len, uint32_t value)
{
        uint8_t *bytes = (uint8_t *)payload;

        for (uint16_t i = 0; i < bit_len; ++i) {
                uint16_t bit_pos = start_bit + i;
                uint16_t byte_index = bit_pos / 8u;
                uint16_t bit_index = bit_pos % 8u;

                uint32_t bit_val = (value >> i) & 1u;
                if (bit_val) {
                        bytes[byte_index] |= (1u << bit_index);
                } else {
                        bytes[byte_index] &= ~(1u << bit_index);
                }
        }
}

/**
 * @brief Read a bitfield of arbitrary length from a payload buffer.
 *
 * @param[in] payload  Input buffer (byte-addressable)
 * @param[in] start_bit Starting bit position (0-63)
 * @param[in] bit_len   Number of bits to read (must be <= 32)
 *
 * @return 32-bit value with extracted bits in LSB positions
 */
static inline uint32_t
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

/* ================ GLOBAL PROTOTYPES ======================================= */

int
ppack_unpack(void *base_ptr, const void *payload,
             const struct ppack_field *fields, size_t field_count)
{
        if (base_ptr == NULL || payload == NULL) {
                return -PPACK_ERR_NULLPTR;
        }

        if (field_count == 0 || fields == NULL) {
                return -PPACK_ERR_INVALARG;
        }

        for (size_t i = 0; i < field_count; ++i) {
                const struct ppack_field *f = &fields[i];
                uint8_t *field_ptr = (uint8_t *)base_ptr + f->ptr_offset;
                uint32_t raw = read_bits(payload, f->start_bit, f->bit_length);

                switch (f->type) {
                case PPACK_TYPE_U8:
                        *(uint16_t *)field_ptr = (uint16_t)raw;
                        break;

                case PPACK_TYPE_U16:
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                *(float *)field_ptr =
                                    ((float)(uint16_t)raw) * f->scale
                                    + f->offset;
                        } else {
                                *(uint16_t *)field_ptr = (uint16_t)raw;
                        }
                        break;

                case PPACK_TYPE_S16:
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                *(float *)field_ptr =
                                    ((float)(int16_t)raw) * f->scale
                                    + f->offset;
                        } else {
                                *(int16_t *)field_ptr = (int16_t)raw;
                        }
                        break;

                case PPACK_TYPE_U32:
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                *(float *)field_ptr =
                                    ((float)raw) * f->scale + f->offset;
                        } else {
                                *(uint32_t *)field_ptr = raw;
                        }
                        break;

                case PPACK_TYPE_S32: {
                        int32_t val = (int32_t)raw;
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                *(float *)field_ptr =
                                    ((float)val) * f->scale + f->offset;
                        } else {
                                *(int32_t *)field_ptr = val;
                        }
                        break;
                }

                case PPACK_TYPE_F32: {
                        float temp;
                        memcpy(&temp, &raw, sizeof(temp));
                        *(float *)field_ptr = temp;
                        break;
                }

                case PPACK_TYPE_BITS: *(uint32_t *)field_ptr = raw; break;

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

        if (field_count == 0 || fields == NULL) {
                return -PPACK_ERR_INVALARG;
        }

        memset(payload, 0, 8); /* Clear 8-byte payload */

        for (size_t i = 0; i < field_count; ++i) {
                const struct ppack_field *f = &fields[i];
                const uint8_t *field_ptr =
                    (const uint8_t *)base_ptr + f->ptr_offset;
                uint32_t raw = 0;

                switch (f->type) {
                case PPACK_TYPE_U8: raw = *(const uint16_t *)field_ptr; break;

                case PPACK_TYPE_U16:
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float val = *(const float *)field_ptr;
                                raw = (uint32_t)((uint16_t)((val - f->offset)
                                                            / f->scale));
                        } else {
                                raw = (uint32_t)(*(const uint16_t *)field_ptr);
                        }
                        break;

                case PPACK_TYPE_S16:
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float val = *(const float *)field_ptr;
                                raw = (uint32_t)((int16_t)((val - f->offset)
                                                           / f->scale));
                        } else {
                                raw = (uint32_t)(*(const int16_t *)field_ptr);
                        }
                        break;

                case PPACK_TYPE_U32:
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float val = *(const float *)field_ptr;
                                raw = (uint32_t)((val - f->offset) / f->scale);
                        } else {
                                raw = *(const uint32_t *)field_ptr;
                        }
                        break;

                case PPACK_TYPE_S32:
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float val = *(const float *)field_ptr;
                                raw = (uint32_t)((int32_t)((val - f->offset)
                                                           / f->scale));
                        } else {
                                raw = (uint32_t)(*(const int32_t *)field_ptr);
                        }
                        break;

                case PPACK_TYPE_F32: {
                        uint32_t temp;
                        memcpy(&temp, field_ptr, sizeof(temp));
                        raw = temp;
                        break;
                }

                case PPACK_TYPE_BITS: raw = *(const uint32_t *)field_ptr; break;

                default: return -PPACK_ERR_NOTFOUND;
                }

                write_bits(payload, f->start_bit, f->bit_length, raw);
        }

        return PPACK_SUCCESS;
}
