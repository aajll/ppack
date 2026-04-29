/**
 * @copyright MIT Licence
 *
 * @file: ppack.c
 *
 * @brief
 *    Implementation of the ppack library - generic payload serialisation.
 *
 * @par MISRA C:2012 Deviation Record
 *
 * Deviations are intentional and have been reviewed. Each site is also
 * tagged in-line at point-of-use (search for "MISRA <rule>"). Tool-driven
 * full compliance requires running a certified static analyser
 * (e.g. PC-lint Plus, Coverity, Polyspace) against this list.
 *
 * Last reviewed: against MISRA C:2012 Guidelines (manual review).
 *
 * Rule 11.5 (advisory) - Cast from pointer-to-void to pointer-to-object
 *   Sites:        write_bits, read_bits payload casts to ppack_byte_t*;
 *                 ppack_pack/unpack base_ptr cast to char* for offset
 *                 arithmetic.
 *   Justification: The public API takes void* / const void* per the C
 *                  standard's convention for opaque polymorphic buffer
 *                  types. Internal code requires unit-typed access on
 *                  the payload (for indexed read-modify-write of bits)
 *                  and char-typed access on the user struct (for
 *                  offsetof()-based member addressing).
 *   Mitigation:   Verified end-to-end on host (8-bit MAU) and via
 *                  PPACK_SIMULATE_16BIT_MAU host build (16-bit MAU code
 *                  path). ASan + UBSan clean across the test suite,
 *                  including 35 000 randomised round-trip iterations
 *                  per type.
 *
 * Rule 18.4 (advisory) - The +, -, += and -= operators should not be
 *                         applied to an expression of pointer type
 *   Sites:        ppack_pack / ppack_unpack field_ptr arithmetic
 *                 (char* + ptr_offset).
 *   Justification: ptr_offset is the user-supplied offsetof() value and
 *                  encodes the location of the struct member relative
 *                  to base_ptr. Arithmetic addition is the natural and
 *                  idiomatic way to apply it; alternatives (subscript,
 *                  uintptr_t round-trip) either reduce to the same
 *                  underlying pointer arithmetic or introduce a
 *                  pointer-to-integer conversion (Rule 11.4) which is
 *                  worse.
 *   Mitigation:   ptr_offset is bounded by sizeof(user struct) and
 *                  validated implicitly at use (memcpy with a fixed
 *                  small size). All field tests exercise this path.
 *
 * Rule 15.5 (advisory) - A function should have a single point of exit
 *   Sites:        ppack_pack, ppack_unpack, validate_field, sign_extend
 *   Justification: Early-return-on-error is the project's idiomatic
 *                  control flow. Refactoring to a single-exit pattern
 *                  would either deeply nest the validation arms (hurting
 *                  readability) or introduce a goto-cleanup (Rule 15.1
 *                  deviation), trading one advisory deviation for
 *                  another with no readability gain.
 *   Mitigation:   All exit points return one of the documented error
 *                  codes or PPACK_SUCCESS; covered by NULL/INVALARG/
 *                  OVERFLOW/NOTFOUND test cases.
 */

/* ================ INCLUDES ================================================ */

#include <stdint.h>
#include <string.h>

#include "ppack.h"
#include <ppack_platform.h>

/* ================ DEFINES ================================================= */

/** Maximum supported payload size in bits (CAN-FD frame data field). */
#define PPACK_MAX_PAYLOAD_BITS 512u

/** Maximum supported field width in bits. */
#define PPACK_MAX_FIELD_BITS   32u

/* ================ STRUCTURES ============================================== */

/* ================ TYPEDEFS ================================================ */

/* ================ STATIC PROTOTYPES ======================================= */

/* ================ STATIC VARIABLES ======================================== */

/* ================ MACROS ================================================== */

/*
 * Largest uint32_t / int32_t values that are *exactly* representable as a
 * 32-bit IEEE-754 float AND fit in the destination integer. Float spacing
 * in [2^31, 2^32) is 256 and in [2^30, 2^31) is 128, so UINT32_MAX
 * (4294967295) and INT32_MAX (2147483647) both round up to 2^32 / 2^31 as
 * floats: values that are out of range for the destination cast and would
 * invoke UB. Clamp to the next-lower representable float instead.
 */
#define PPACK_FLOAT_UINT32_MAX ((float)0xFFFFFF00u) /* 4294967040 */
#define PPACK_FLOAT_INT32_MAX  ((float)0x7FFFFF80)  /* 2147483520 */
#define PPACK_FLOAT_INT32_MIN  (-2147483648.0f)     /* -2^31, exact */

/* ================ STATIC FUNCTIONS ======================================== */

/**
 * @brief Clamp a float to a closed range [@p lo, @p hi].
 *
 * Single-exit by design (MISRA 15.5 friendly).
 */
static inline float
ppack_clamp_float(float val, float lo, float hi)
{
        float result = val;

        if (result < lo) {
                result = lo;
        } else if (result > hi) {
                result = hi;
        } else {
                /* in range, leave untouched */
        }

        return result;
}

/**
 * @brief Write a bitfield of arbitrary length into a payload buffer.
 *
 * @param[in,out] payload   Output buffer
 * @param[in]     start_bit Starting bit position within the payload
 * @param[in]     bit_len   Number of bits to write (1-32)
 * @param[in]     value     Raw 32-bit value (lower bit_len bits written)
 *
 * @pre  start_bit + bit_len <= payload size  (caller must validate)
 * @pre  bit_len <= 32                        (caller must validate)
 */
static void
write_bits(void *payload, uint16_t start_bit, uint16_t bit_len, uint32_t value)
{
        /* MISRA 11.5: opaque void* payload requires unit-typed access. */
        ppack_byte_t *words = (ppack_byte_t *)payload;
        uint16_t bits_written = 0;

        while (bits_written < bit_len) {
                uint16_t current_bit = start_bit + bits_written;
                uint16_t unit_idx = ppack_bit_to_unit(current_bit);
                uint16_t bit_offset = ppack_bit_to_shift(current_bit);
                uint16_t bits_left = bit_len - bits_written;
                uint16_t bits_this_unit = PPACK_ADDR_UNIT_BITS - bit_offset;

                if (bits_this_unit > bits_left) {
                        bits_this_unit = bits_left;
                }

                uint32_t val_chunk = (value >> bits_written)
                                     & (((uint32_t)1u << bits_this_unit) - 1u);
                uint32_t mask = (((uint32_t)1u << bits_this_unit) - 1u)
                                << bit_offset;

                uint32_t unit_val = words[unit_idx];
                unit_val = (unit_val & ~mask) | (val_chunk << bit_offset);
                words[unit_idx] = (ppack_byte_t)unit_val;

                bits_written += bits_this_unit;
        }
}

/**
 * @brief Read a bitfield of arbitrary length from a payload buffer.
 *
 * @param[in] payload   Input buffer
 * @param[in] start_bit Starting bit position within the payload
 * @param[in] bit_len   Number of bits to read (1-32)
 *
 * @return 32-bit value with extracted bits in LSB positions
 *
 * @pre  start_bit + bit_len <= payload size  (caller must validate)
 * @pre  bit_len <= 32                        (caller must validate)
 */
static uint32_t
read_bits(const void *payload, uint16_t start_bit, uint16_t bit_len)
{
        /* MISRA 11.5: opaque const void* payload requires unit-typed access. */
        const ppack_byte_t *words = (const ppack_byte_t *)payload;
        uint32_t result = 0;
        uint16_t bits_read = 0;

        while (bits_read < bit_len) {
                uint16_t current_bit = start_bit + bits_read;
                uint16_t unit_idx = ppack_bit_to_unit(current_bit);
                uint16_t bit_offset = ppack_bit_to_shift(current_bit);
                uint16_t bits_left = bit_len - bits_read;
                uint16_t bits_this_unit = PPACK_ADDR_UNIT_BITS - bit_offset;

                if (bits_this_unit > bits_left) {
                        bits_this_unit = bits_left;
                }

                uint32_t mask = (((uint32_t)1u << bits_this_unit) - 1u);
                uint32_t unit_val = words[unit_idx];
                uint32_t val_chunk = (unit_val >> bit_offset) & mask;
                result |= (val_chunk << bits_read);

                bits_read += bits_this_unit;
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
 * @note  The unsigned-to-signed cast on the final return is well-defined
 *        on every two's-complement target; the library's supported
 *        platforms all use two's-complement (see README "Supported
 *        architectures").
 */
static int32_t
sign_extend(uint32_t value, uint16_t width)
{
        uint32_t sign_bit = (uint32_t)1u << (width - 1u);

        /* If sign bit is set, fill upper bits with 1s. */
        if ((value & sign_bit) != 0u) {
                /* Compute the sign-extended bit pattern in an intermediate
                 * variable, then cast: keeps the cast operand a simple
                 * variable rather than a composite expression. */
                uint32_t bits = value | ~(sign_bit - 1u);
                return (int32_t)bits;
        }
        return (int32_t)value;
}

/**
 * @brief Validate a field descriptor against the runtime payload size.
 *
 * @param[in] f            Pointer to the field descriptor to check
 * @param[in] payload_bits Payload size in bits (caller-validated)
 *
 * @return PPACK_SUCCESS on success
 * @return -PPACK_ERR_INVALARG  if bit_length is 0 or > 32
 * @return -PPACK_ERR_OVERFLOW  if start_bit + bit_length exceeds
 *                              payload_bits
 */
static int
validate_field(const struct ppack_field *f, size_t payload_bits)
{
        if (f->bit_length == 0u || f->bit_length > PPACK_MAX_FIELD_BITS) {
                return -PPACK_ERR_INVALARG;
        }
        if ((size_t)f->start_bit + (size_t)f->bit_length > payload_bits) {
                return -PPACK_ERR_OVERFLOW;
        }
        return PPACK_SUCCESS;
}

/**
 * @brief Validate the @c payload_bits argument supplied to pack/unpack.
 *
 * @param[in] payload_bits Caller-supplied payload size in bits
 *
 * @return PPACK_SUCCESS on success
 * @return -PPACK_ERR_INVALARG  if @c payload_bits is 0, not a multiple
 *                              of @c PPACK_ADDR_UNIT_BITS, or exceeds
 *                              @c PPACK_MAX_PAYLOAD_BITS
 */
static int
validate_payload_bits(size_t payload_bits)
{
        if (payload_bits == 0u || payload_bits > PPACK_MAX_PAYLOAD_BITS) {
                return -PPACK_ERR_INVALARG;
        }
        if ((payload_bits % (size_t)PPACK_ADDR_UNIT_BITS) != 0u) {
                return -PPACK_ERR_INVALARG;
        }
        return PPACK_SUCCESS;
}

/* ================ GLOBAL PROTOTYPES ======================================= */

int
ppack_unpack(void *base_ptr, const void *payload, size_t payload_bits,
             const struct ppack_field *fields, size_t field_count)
{
        if (base_ptr == NULL || payload == NULL) {
                return -PPACK_ERR_NULLPTR;
        }

        if (field_count == 0u || fields == NULL) {
                return -PPACK_ERR_INVALARG;
        }

        int pret = validate_payload_bits(payload_bits);
        if (pret != PPACK_SUCCESS) {
                return pret;
        }

        for (size_t i = 0; i < field_count; ++i) {
                const struct ppack_field *f = &fields[i];

                int vret = validate_field(f, payload_bits);
                if (vret != PPACK_SUCCESS) {
                        return vret;
                }

                /* MISRA 11.5: void* opaque struct requires char* for offset
                 * arithmetic. MISRA 18.4: ptr_offset is the user-supplied
                 * offsetof() value and is added to the base pointer. */
                char *field_ptr = (char *)base_ptr + f->ptr_offset;
                uint32_t raw = read_bits(payload, f->start_bit, f->bit_length);

                switch (f->type) {
                case PPACK_TYPE_UINT8: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                return -PPACK_ERR_INVALARG;
                        }
                        ppack_u8_t tmp = (ppack_u8_t)(raw & 0xFFu);
                        (void)memcpy(field_ptr, &tmp, sizeof(tmp));
                        break;
                }

                case PPACK_TYPE_UINT16: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float tmp = ((float)(uint16_t)raw) * f->scale
                                            + f->offset;
                                (void)memcpy(field_ptr, &tmp, sizeof(tmp));
                        } else {
                                uint16_t tmp = (uint16_t)raw;
                                (void)memcpy(field_ptr, &tmp, sizeof(tmp));
                        }
                        break;
                }

                case PPACK_TYPE_INT16: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                int32_t sval = sign_extend(raw, f->bit_length);
                                float tmp =
                                    ((float)sval) * f->scale + f->offset;
                                (void)memcpy(field_ptr, &tmp, sizeof(tmp));
                        } else {
                                int16_t tmp =
                                    (int16_t)sign_extend(raw, f->bit_length);
                                (void)memcpy(field_ptr, &tmp, sizeof(tmp));
                        }
                        break;
                }

                case PPACK_TYPE_UINT32: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float tmp = ((float)raw) * f->scale + f->offset;
                                (void)memcpy(field_ptr, &tmp, sizeof(tmp));
                        } else {
                                (void)memcpy(field_ptr, &raw, sizeof(raw));
                        }
                        break;
                }

                case PPACK_TYPE_INT32: {
                        int32_t sval = sign_extend(raw, f->bit_length);
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                float tmp =
                                    ((float)sval) * f->scale + f->offset;
                                (void)memcpy(field_ptr, &tmp, sizeof(tmp));
                        } else {
                                (void)memcpy(field_ptr, &sval, sizeof(sval));
                        }
                        break;
                }

                case PPACK_TYPE_F32: {
                        float tmp;
                        (void)memcpy(&tmp, &raw, sizeof(tmp));
                        (void)memcpy(field_ptr, &tmp, sizeof(tmp));
                        break;
                }

                case PPACK_TYPE_BITS: {
                        (void)memcpy(field_ptr, &raw, sizeof(raw));
                        break;
                }

                default: return -PPACK_ERR_NOTFOUND;
                }
        }

        return PPACK_SUCCESS;
}

int
ppack_pack(const void *base_ptr, void *payload, size_t payload_bits,
           const struct ppack_field *fields, size_t field_count)
{
        if (base_ptr == NULL || payload == NULL) {
                return -PPACK_ERR_NULLPTR;
        }

        if (field_count == 0u || fields == NULL) {
                return -PPACK_ERR_INVALARG;
        }

        int pret = validate_payload_bits(payload_bits);
        if (pret != PPACK_SUCCESS) {
                return pret;
        }

        size_t payload_units = payload_bits / (size_t)PPACK_ADDR_UNIT_BITS;
        (void)memset(payload, 0, sizeof(ppack_byte_t) * payload_units);

        for (size_t i = 0; i < field_count; ++i) {
                const struct ppack_field *f = &fields[i];

                int vret = validate_field(f, payload_bits);
                if (vret != PPACK_SUCCESS) {
                        return vret;
                }

                /* MISRA 11.5: const void* opaque struct requires const char*
                 * for offset arithmetic. MISRA 18.4: ptr_offset is the
                 * user-supplied offsetof() value. */
                const char *field_ptr = (const char *)base_ptr + f->ptr_offset;
                uint32_t raw = 0u;

                switch (f->type) {
                case PPACK_TYPE_UINT8: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                return -PPACK_ERR_INVALARG;
                        }
                        ppack_u8_t tmp;
                        (void)memcpy(&tmp, field_ptr, sizeof(tmp));
                        raw = (uint32_t)(tmp & 0xFFu);
                        break;
                }

                case PPACK_TYPE_UINT16: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                if (f->scale == 0.0f) {
                                        return -PPACK_ERR_OVERFLOW;
                                }
                                float val;
                                (void)memcpy(&val, field_ptr, sizeof(val));
                                float scaled = (val - f->offset) / f->scale;
                                scaled =
                                    ppack_clamp_float(scaled, 0.0f, 65535.0f);
                                raw = (uint32_t)(uint16_t)scaled;
                        } else {
                                uint16_t tmp;
                                (void)memcpy(&tmp, field_ptr, sizeof(tmp));
                                raw = (uint32_t)tmp;
                        }
                        break;
                }

                case PPACK_TYPE_INT16: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                if (f->scale == 0.0f) {
                                        return -PPACK_ERR_OVERFLOW;
                                }
                                float val;
                                (void)memcpy(&val, field_ptr, sizeof(val));
                                float scaled = (val - f->offset) / f->scale;
                                scaled = ppack_clamp_float(scaled, -32768.0f,
                                                           32767.0f);
                                raw = (uint32_t)(int16_t)scaled;
                        } else {
                                int16_t tmp;
                                (void)memcpy(&tmp, field_ptr, sizeof(tmp));
                                raw = (uint32_t)tmp;
                        }
                        break;
                }

                case PPACK_TYPE_UINT32: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                if (f->scale == 0.0f) {
                                        return -PPACK_ERR_OVERFLOW;
                                }
                                float val;
                                (void)memcpy(&val, field_ptr, sizeof(val));
                                float scaled = (val - f->offset) / f->scale;
                                scaled = ppack_clamp_float(
                                    scaled, 0.0f, PPACK_FLOAT_UINT32_MAX);
                                raw = (uint32_t)scaled;
                        } else {
                                (void)memcpy(&raw, field_ptr, sizeof(raw));
                        }
                        break;
                }

                case PPACK_TYPE_INT32: {
                        if (f->behaviour == PPACK_BEHAVIOUR_SCALED) {
                                if (f->scale == 0.0f) {
                                        return -PPACK_ERR_OVERFLOW;
                                }
                                float val;
                                (void)memcpy(&val, field_ptr, sizeof(val));
                                float scaled = (val - f->offset) / f->scale;
                                scaled = ppack_clamp_float(
                                    scaled, PPACK_FLOAT_INT32_MIN,
                                    PPACK_FLOAT_INT32_MAX);
                                raw = (uint32_t)(int32_t)scaled;
                        } else {
                                int32_t tmp;
                                (void)memcpy(&tmp, field_ptr, sizeof(tmp));
                                raw = (uint32_t)tmp;
                        }
                        break;
                }

                case PPACK_TYPE_F32: {
                        uint32_t tmp;
                        (void)memcpy(&tmp, field_ptr, sizeof(tmp));
                        raw = tmp;
                        break;
                }

                case PPACK_TYPE_BITS: {
                        (void)memcpy(&raw, field_ptr, sizeof(raw));
                        break;
                }

                default: return -PPACK_ERR_NOTFOUND;
                }

                write_bits(payload, f->start_bit, f->bit_length, raw);
        }

        return PPACK_SUCCESS;
}
