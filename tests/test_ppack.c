/*
 * @file: test_ppack.c
 * @brief Unit tests for the ppack library.
 */

#include "ppack.h"
#include <ppack_platform.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PPACK_LOGICAL_BYTES_PER_UNIT (PPACK_ADDR_UNIT_BITS / 8)

/*
 * Read a logical byte from the payload buffer, abstracting away
 * the underlying addressable unit (8-bit byte vs 16-bit word).
 */
#define READ_LOGICAL_BYTE(payload, byte_index)                                 \
        ((((ppack_byte_t *)(payload))[(byte_index)                             \
                                      / PPACK_LOGICAL_BYTES_PER_UNIT]          \
          >> (((byte_index) % PPACK_LOGICAL_BYTES_PER_UNIT) * 8))              \
         & 0xFFu)

/*
 * Compile-time verification that ppack_byte_t resolves correctly.
 * On byte-addressable platforms: ppack_byte_t == uint8_t, PPACK_PAYLOAD_UNITS
 * == 8 On word-addressable platforms: ppack_byte_t == uint16_t,
 * PPACK_PAYLOAD_UNITS == 4
 */
_Static_assert(sizeof(ppack_byte_t) == 1 || sizeof(ppack_byte_t) == 2,
               "ppack_byte_t must be 8 or 16 bits");

_Static_assert(PPACK_PAYLOAD_UNITS == 8 || PPACK_PAYLOAD_UNITS == 4,
               "PPACK_PAYLOAD_UNITS must be 4 or 8");

/* ------------------------------------------------------------------ */
/* Test infrastructure                                                */
/* ------------------------------------------------------------------ */

#define TEST_ASSERT(expr)                                                      \
        do {                                                                   \
                if (!(expr)) {                                                 \
                        fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__,         \
                                __LINE__, #expr);                              \
                        exit(EXIT_FAILURE);                                    \
                }                                                              \
        } while (0)

#define TEST_PASS(name) fprintf(stdout, "PASS  %s\n", (name))

#define TEST_CASE(name)                                                        \
        static void name(void);                                                \
        static void name(void)

/* ------------------------------------------------------------------ */
/* Test fixtures                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
        uint16_t field_u16;
        int16_t field_s16;
        uint32_t field_u32;
        int32_t field_s32;
        float field_f32;
        ppack_u8_t field_u8; /* Mirrors target-side uint8_t (16-bit on TI) */
        uint32_t field_bits;
} test_struct_t;

typedef struct {
        float field_u16_scaled;
        float field_s16_scaled;
        float field_u32_scaled;
        float field_s32_scaled;
} test_struct_scaled_t;

/* ------------------------------------------------------------------ */
/* NULL / invalid-argument tests                                      */
/* ------------------------------------------------------------------ */

TEST_CASE(test_pack_null_base_ptr)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(NULL, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NULLPTR);
}

TEST_CASE(test_pack_null_payload)
{
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };
        test_struct_t data = {.field_u16 = 0x1234};

        int ret = ppack_pack(&data, NULL, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NULLPTR);
}

TEST_CASE(test_pack_null_fields)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        int ret = ppack_pack(&data, payload, NULL, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_unpack_null_base_ptr)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(NULL, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NULLPTR);
}

TEST_CASE(test_unpack_null_payload)
{
        test_struct_t data = {0};
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(&data, NULL, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NULLPTR);
}

TEST_CASE(test_unpack_null_fields)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};

        int ret = ppack_unpack(&data, payload, NULL, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

/* ------------------------------------------------------------------ */
/* Validation: overflow and bit_length bounds                         */
/* ------------------------------------------------------------------ */

TEST_CASE(test_pack_overflow_start_plus_len)
{
        /* start_bit=56, bit_length=16 → 56+16=72 > 64: must be rejected */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 56,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_unpack_overflow_start_plus_len)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 56,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_exact_boundary_passes)
{
        /* start_bit=48, bit_length=16 → 48+16=64: exactly on the limit, OK */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0xABCD};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 48,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 6) == 0xCD);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 7) == 0xAB);
}

TEST_CASE(test_pack_bit_length_zero_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 0,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_pack_bit_length_33_rejected)
{
        /* bit_length > 32 must be rejected (would cause UB shift) */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u32 = 0xDEADBEEF};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U32,
             .start_bit = 0,
             .bit_length = 33,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

/* ------------------------------------------------------------------ */
/* Validation: scale == 0 on SCALED fields                           */
/* ------------------------------------------------------------------ */

TEST_CASE(test_pack_scale_zero_u16_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_u16_scaled = 1.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_u16_scaled),
             .scale = 0.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_scale_zero_s16_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_s16_scaled = -1.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_s16_scaled),
             .scale = 0.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_scale_zero_u32_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_u32_scaled = 1.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_u32_scaled),
             .scale = 0.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

TEST_CASE(test_pack_scale_zero_s32_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_s32_scaled = 1.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_s32_scaled),
             .scale = 0.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_OVERFLOW);
}

/* ------------------------------------------------------------------ */
/* Raw round-trip tests: full-width types                             */
/* ------------------------------------------------------------------ */

TEST_CASE(test_pack_unpack_u16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x34);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x12);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
}

TEST_CASE(test_pack_unpack_s16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s16 = -1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s16 == -1234);
}

TEST_CASE(test_pack_unpack_u32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u32 = 0x12345678};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u32 == 0x12345678);
}

TEST_CASE(test_pack_unpack_s32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s32 = -12345678};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_s32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s32 == -12345678);
}

TEST_CASE(test_pack_unpack_f32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_f32 = 3.14159f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_F32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_f32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_f32 - data.field_f32;
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.0001f);
}

TEST_CASE(test_pack_unpack_u8)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u8 = 0xAB};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 0xAB);
}

TEST_CASE(test_pack_unpack_bits)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_bits = 0xDEADBEEF};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_BITS,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_bits == 0xDEADBEEF);
}

/* ------------------------------------------------------------------ */
/* Sign-extension: sub-width signed fields                            */
/* ------------------------------------------------------------------ */

TEST_CASE(test_s16_12bit_negative)
{
        /*
         * Pack -100 into a 12-bit signed field and verify it comes back
         * as -100.  Previously the library would return 3996 (no sign-extend).
         */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s16 = -100};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 12,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s16 == -100);
}

TEST_CASE(test_s16_12bit_positive)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s16 = 100};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 12,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s16 == 100);
}

TEST_CASE(test_s32_20bit_negative)
{
        /* -500 in a 20-bit signed field */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s32 = -500};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S32,
             .start_bit = 0,
             .bit_length = 20,
             .ptr_offset = offsetof(test_struct_t, field_s32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s32 == -500);
}

TEST_CASE(test_s32_1bit_negative)
{
        /* A 1-bit signed field can only hold 0 or -1 */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s32 = -1};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S32,
             .start_bit = 0,
             .bit_length = 1,
             .ptr_offset = offsetof(test_struct_t, field_s32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s32 == -1);
}

/* ------------------------------------------------------------------ */
/* Layout and multi-field tests                                       */
/* ------------------------------------------------------------------ */

TEST_CASE(test_pack_unpack_u16_offset)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 2) == 0x34);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 3) == 0x12);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
}

TEST_CASE(test_pack_unpack_spanning_boundary)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u32 = 0x12345678};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U32,
             .start_bit = 8,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u32 == 0x12345678);
}

TEST_CASE(test_pack_multiple_fields)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_u16 = 0x1234,
            .field_s16 = -5678,
            .field_u8 = 0xAB,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U8,
             .start_bit = 32,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 3);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x34);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x12);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 2) == 0xd2);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 3) == 0xe9);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 3);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
        TEST_ASSERT(unpacked.field_s16 == -5678);
        TEST_ASSERT(unpacked.field_u8 == 0xAB);
}

/* ------------------------------------------------------------------ */
/* Scaled field tests                                                 */
/* ------------------------------------------------------------------ */

TEST_CASE(test_pack_scaled_u16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        float input_val = 123.45f;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_u16_scaled),
             .scale = 0.01f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        test_struct_scaled_t data = {.field_u16_scaled = input_val};
        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_u16_scaled - 123.45f;
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.1f);
}

TEST_CASE(test_pack_scaled_s16_with_offset)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        float input_val = -25.5f;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_s16_scaled),
             .scale = 0.1f,
             .offset = 10.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        test_struct_scaled_t data = {.field_s16_scaled = input_val};
        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_s16_scaled - (-25.5f);
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.2f);
}

TEST_CASE(test_pack_scaled_u32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        float input_val = 12345.678f;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_u32_scaled),
             .scale = 0.001f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        test_struct_scaled_t data = {.field_u32_scaled = input_val};
        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_u32_scaled - 12345.678f;
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 1.0f);
}

TEST_CASE(test_pack_scaled_s32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        float input_val = -500.0f;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_s32_scaled),
             .scale = 0.1f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        test_struct_scaled_t data = {.field_s32_scaled = input_val};
        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_s32_scaled - (-500.0f);
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.2f);
}

TEST_CASE(test_pack_scale_u8_rejected)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .scale = 1.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);

        int unpack_ret = ppack_unpack(&data, payload, fields, 1);
        TEST_ASSERT(unpack_ret == -PPACK_ERR_INVALARG);
}

/* ------------------------------------------------------------------ */
/* Misc / edge cases                                                  */
/* ------------------------------------------------------------------ */

TEST_CASE(test_invalid_field_type)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        const struct ppack_field fields[] = {
            {.type = (enum ppack_type)99,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NOTFOUND);
}

TEST_CASE(test_unpack_invalid_field_type)
{
        /* Unpack-side mirror of test_invalid_field_type — covers the
         * default switch arm in ppack_unpack. */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};

        const struct ppack_field fields[] = {
            {.type = (enum ppack_type)99,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(&data, payload, fields, 1);
        TEST_ASSERT(ret == -PPACK_ERR_NOTFOUND);
}

TEST_CASE(test_empty_field_list)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        int ret = ppack_pack(&data, payload, NULL, 0);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_unpack_zero_field_count)
{
        /* Covers the field_count == 0 short-circuit in ppack_unpack
         * (the first half of the OR; the NULL fields half is covered
         * by test_unpack_null_fields). */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_unpack(&data, payload, fields, 0);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_small_bit_field)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u8 = 0x0F};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 4,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 0x0F);
}

TEST_CASE(test_6bit_field)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u8 = 0x3F};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 6,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 0x3F);
}

TEST_CASE(test_adjacent_bit_fields)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_u8 = 0x55,
            .field_bits = 0xAA,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 8,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 2);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 2);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 0x55);
        TEST_ASSERT(unpacked.field_bits == 0xAA);
}

TEST_CASE(test_negative_s32)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s32 = -1};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_s32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s32 == -1);
}

TEST_CASE(test_max_u16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0xFFFF};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0xFFFF);
}

TEST_CASE(test_max_s16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s16 = 32767};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s16 == 32767);
}

TEST_CASE(test_min_s16)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_s16 = -32768};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s16 == -32768);
}

TEST_CASE(test_zero_values)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0);
}

TEST_CASE(test_negative_float)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_f32 = -3.14159f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_F32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_f32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_f32 - (-3.14159f);
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.0001f);
}

TEST_CASE(test_float_zero)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_f32 = 0.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_F32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_f32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_f32 == 0.0f);
}

TEST_CASE(test_1bit_field)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u8 = 1};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 1,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 1);
}

TEST_CASE(test_1bit_field_zero)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u8 = 0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 5,
             .bit_length = 1,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 0);
}

TEST_CASE(test_overlapping_fields)
{
        /* Overlapping fields are defined behaviour: last write wins. */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u8 = 0xFF};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U8,
             .start_bit = 4,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 2);
        TEST_ASSERT(ret == PPACK_SUCCESS);
}

TEST_CASE(test_full_payload)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_u16 = 0x1234,
            .field_s16 = 0x5678,
            .field_u32 = 0x9ABCDEF0,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U32,
             .start_bit = 32,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 3);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x34);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x12);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 2) == 0x78);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 3) == 0x56);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 4) == 0xF0);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 5) == 0xDE);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 6) == 0xBC);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 7) == 0x9A);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 3);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
        TEST_ASSERT(unpacked.field_s16 == 0x5678);
        TEST_ASSERT(unpacked.field_u32 == 0x9ABCDEF0);
}

/* ------------------------------------------------------------------ */
/* Exhaustive tests - bit-position and boundary coverage              */
/* ------------------------------------------------------------------ */

/* Helper: assert a specific bit in the payload has the expected value */
#define ASSERT_PAYLOAD_BIT(payload, bit_pos, expected)                         \
        do {                                                                   \
                uint16_t byte_idx = (bit_pos) / 8;                             \
                uint8_t bit_idx = (bit_pos) % 8;                               \
                uint32_t byte_val = READ_LOGICAL_BYTE(payload, byte_idx);      \
                uint32_t bit = (byte_val >> bit_idx) & 1u;                     \
                TEST_ASSERT(bit == ((expected) ? 1u : 0u));                    \
        } while (0)

/* Helper: assert a range of logical bytes in the payload */
#define ASSERT_PAYLOAD_BYTES_EQ(payload, start_byte, count, ...)               \
        do {                                                                   \
                const uint8_t _expected[] = {__VA_ARGS__};                     \
                for (int _i = 0; _i < (count); ++_i) {                         \
                        TEST_ASSERT(                                           \
                            READ_LOGICAL_BYTE(payload, (start_byte) + _i)      \
                            == _expected[_i]);                                 \
                }                                                              \
        } while (0)

/* ---- Every start_bit position (0-63) for each type ---- */

TEST_CASE(test_all_bit_positions_u8)
{
        for (uint16_t start_bit = 0; start_bit <= 56; ++start_bit) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_u8 = 0xAB};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U8,
                     .start_bit = start_bit,
                     .bit_length = 8,
                     .ptr_offset = offsetof(test_struct_t, field_u8),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_u8 == 0xAB);

                /* Verify no bits outside the field are modified */
                for (uint16_t i = 0; i < 64; ++i) {
                        if (i < start_bit || i >= start_bit + 8) {
                                ASSERT_PAYLOAD_BIT(payload, i, 0);
                        }
                }
        }
}

TEST_CASE(test_all_bit_positions_u16)
{
        for (uint16_t start_bit = 0; start_bit <= 48; ++start_bit) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_u16 = 0x1234};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U16,
                     .start_bit = start_bit,
                     .bit_length = 16,
                     .ptr_offset = offsetof(test_struct_t, field_u16),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_u16 == 0x1234);

                for (uint16_t i = 0; i < 64; ++i) {
                        if (i < start_bit || i >= start_bit + 16) {
                                ASSERT_PAYLOAD_BIT(payload, i, 0);
                        }
                }
        }
}

TEST_CASE(test_all_bit_positions_s16)
{
        for (uint16_t start_bit = 0; start_bit <= 48; ++start_bit) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_s16 = -5678};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S16,
                     .start_bit = start_bit,
                     .bit_length = 16,
                     .ptr_offset = offsetof(test_struct_t, field_s16),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_s16 == -5678);

                for (uint16_t i = 0; i < 64; ++i) {
                        if (i < start_bit || i >= start_bit + 16) {
                                ASSERT_PAYLOAD_BIT(payload, i, 0);
                        }
                }
        }
}

TEST_CASE(test_all_bit_positions_u32)
{
        for (uint16_t start_bit = 0; start_bit <= 32; ++start_bit) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_u32 = 0x12345678};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U32,
                     .start_bit = start_bit,
                     .bit_length = 32,
                     .ptr_offset = offsetof(test_struct_t, field_u32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_u32 == 0x12345678);

                for (uint16_t i = 0; i < 64; ++i) {
                        if (i < start_bit || i >= start_bit + 32) {
                                ASSERT_PAYLOAD_BIT(payload, i, 0);
                        }
                }
        }
}

TEST_CASE(test_all_bit_positions_s32)
{
        for (uint16_t start_bit = 0; start_bit <= 32; ++start_bit) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_s32 = -12345678};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S32,
                     .start_bit = start_bit,
                     .bit_length = 32,
                     .ptr_offset = offsetof(test_struct_t, field_s32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_s32 == -12345678);

                for (uint16_t i = 0; i < 64; ++i) {
                        if (i < start_bit || i >= start_bit + 32) {
                                ASSERT_PAYLOAD_BIT(payload, i, 0);
                        }
                }
        }
}

/* ---- Every bit_length (1-max) for each type ---- */

TEST_CASE(test_all_bit_lengths_u8)
{
        for (uint16_t bit_len = 1; bit_len <= 8; ++bit_len) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_u8 = 0xAB};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U8,
                     .start_bit = 0,
                     .bit_length = bit_len,
                     .ptr_offset = offsetof(test_struct_t, field_u8),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                /* Only the low bit_len bits should match after truncation */
                uint8_t mask = (uint8_t)((1u << bit_len) - 1u);
                TEST_ASSERT((unpacked.field_u8 & mask)
                            == (data.field_u8 & mask));
        }
}

TEST_CASE(test_all_bit_lengths_u16)
{
        for (uint16_t bit_len = 1; bit_len <= 16; ++bit_len) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_u16 = 0x1234};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U16,
                     .start_bit = 0,
                     .bit_length = bit_len,
                     .ptr_offset = offsetof(test_struct_t, field_u16),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                uint16_t mask = (uint16_t)((1u << bit_len) - 1u);
                TEST_ASSERT((unpacked.field_u16 & mask)
                            == (data.field_u16 & mask));
        }
}

TEST_CASE(test_all_bit_lengths_s16)
{
        for (uint16_t bit_len = 1; bit_len <= 16; ++bit_len) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_s16 = -5678};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S16,
                     .start_bit = 0,
                     .bit_length = bit_len,
                     .ptr_offset = offsetof(test_struct_t, field_s16),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                /* After sign-extension, low bit_len bits must match */
                int16_t orig = data.field_s16;
                int16_t result = unpacked.field_s16;
                uint16_t mask = (uint16_t)((1u << bit_len) - 1u);
                TEST_ASSERT(((uint16_t)(orig & (int16_t)mask))
                            == ((uint16_t)(result & (int16_t)mask)));
        }
}

TEST_CASE(test_all_bit_lengths_u32)
{
        for (uint16_t bit_len = 1; bit_len <= 32; ++bit_len) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_u32 = 0x12345678};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U32,
                     .start_bit = 0,
                     .bit_length = bit_len,
                     .ptr_offset = offsetof(test_struct_t, field_u32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                uint32_t mask = (uint32_t)(((uint64_t)1u << bit_len) - 1u);
                TEST_ASSERT((unpacked.field_u32 & mask)
                            == (data.field_u32 & mask));
        }
}

TEST_CASE(test_all_bit_lengths_s32)
{
        for (uint16_t bit_len = 1; bit_len <= 32; ++bit_len) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_s32 = -12345678};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S32,
                     .start_bit = 0,
                     .bit_length = bit_len,
                     .ptr_offset = offsetof(test_struct_t, field_s32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                int32_t orig = data.field_s32;
                int32_t result = unpacked.field_s32;
                uint32_t mask = (uint32_t)(((uint64_t)1u << bit_len) - 1u);
                TEST_ASSERT(((uint32_t)(orig & (int32_t)mask))
                            == ((uint32_t)(result & (int32_t)mask)));
        }
}

/* ---- Cross-boundary fields ---- */

TEST_CASE(test_cross_byte_boundary_u16)
{
        /* U16 field spanning bits 7-22 (crosses byte 0→1 and 1→2) */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {.field_u16 = 0x5678};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 7,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x5678);
}

TEST_CASE(test_cross_byte_boundary_u8_pair)
{
        /* Two U8 fields at bits 4-11 and 12-19 (cross byte boundaries) */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_u8 = 0xCC,
            .field_bits = 0xDD,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 4,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 12,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 2);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 2);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 0xCC);
        TEST_ASSERT((unpacked.field_bits & 0xFFu) == 0xDD);
}

/* ---- Adjacent U8 fields sharing an addressable unit ---- */

TEST_CASE(test_adjacent_u8_same_word)
{
        /* Two U8 fields at bits 0-7 and 8-15 - same logical byte pair */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_u8 = 0x55,
            .field_bits = 0xAA,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 8,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 2);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        /* Verify bytes are correct without cross-contamination */
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x55);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0xAA);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 2);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 0x55);
        TEST_ASSERT((unpacked.field_bits & 0xFFu) == 0xAA);
}

/* ---- Odd-aligned U8 fields ---- */

TEST_CASE(test_odd_aligned_u8)
{
        /* U8 field at every odd start_bit position (1, 3, 5, …, 55)
         * 55 + 8 = 63 <= 64; 57 + 8 = 65 > 64 would overflow */
        for (uint16_t start_bit = 1; start_bit <= 55; start_bit += 2) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_u8 = 0xFF};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U8,
                     .start_bit = start_bit,
                     .bit_length = 8,
                     .ptr_offset = offsetof(test_struct_t, field_u8),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_u8 == 0xFF);

                /* Verify no bleed into neighbouring positions */
                for (uint16_t i = 0; i < 64; ++i) {
                        if (i < start_bit || i >= start_bit + 8) {
                                ASSERT_PAYLOAD_BIT(payload, i, 0);
                        }
                }
        }
}

/* ---- Full-payload multi-field layouts ---- */

TEST_CASE(test_full_layout_u8x8)
{
        /* Eight U8 fields filling all 64 bits */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_u8 = 0x11,
            .field_bits = 0x22334455,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 8,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 16,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 24,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 32,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 40,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 48,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 56,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 8);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        /* Each PPACK_TYPE_BITS reads full field_bits (0x22334455) and writes
         * the lower bit_length bits - all write 0x55 (LSB of 0x22334455).    */
        ASSERT_PAYLOAD_BYTES_EQ(payload, 0, 8, 0x11, 0x55, 0x55, 0x55, 0x55,
                                0x55, 0x55, 0x55);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 8);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u8 == 0x11);
        /* Each PPACK_TYPE_BITS unpack writes full 32-bit raw into field_bits;
         * last write (bits 56-63, raw=0x55) overwrites all previous.         */
        TEST_ASSERT(unpacked.field_bits == 0x55);
}

TEST_CASE(test_full_layout_s16x4)
{
        /* Four S16 fields filling all 64 bits */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_s16 = 0x1122,
            .field_u16 = 0x3344,
            .field_bits = 0x55667788,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 32,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 48,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 4);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 4);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s16 == 0x1122);
        TEST_ASSERT(unpacked.field_u16 == 0x3344);
        /* Each PPACK_TYPE_BITS unpack writes full 32-bit raw; last write wins.
         * Both Bits fields read lower 16 bits of 0x55667788 = 0x7788.        */
        TEST_ASSERT(unpacked.field_bits == 0x7788);
}

TEST_CASE(test_full_layout_mixed)
{
        /* Mixed types: U16 + S16 + U8×4 filling 64 bits */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {
            .field_u16 = 0x1234,
            .field_s16 = 0x5678,
            .field_u8 = 0x9A,
            .field_bits = 0xBCDEF0,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U8,
             .start_bit = 32,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 40,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 48,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 56,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 6);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 6);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
        TEST_ASSERT(unpacked.field_s16 == 0x5678);
        TEST_ASSERT(unpacked.field_u8 == 0x9A);
        /* Each PPACK_TYPE_BITS writes full 32-bit raw; last write wins.
         * All Bits fields read lower 8 bits of 0xBCDEF0 = 0xF0.           */
        TEST_ASSERT(unpacked.field_bits == 0xF0);
}

/* ---- Scaled fields at every type ---- */

/* Note: PPACK_TYPE_U8 does not support PPACK_BEHAVIOUR_SCALED in the
 * current library implementation; scaled tests cover U16/S16/U32/S32. */

TEST_CASE(test_scaled_s16_negative)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_s16_scaled = -50.25f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_scaled_t, field_s16_scaled),
             .scale = 0.25f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_s16_scaled - (-50.25f);
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.5f);
}

TEST_CASE(test_scaled_u32_large)
{
        /* Large scaled U32 - value * scale must fit in uint32_t. */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_u32_scaled = 42949.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_u32_scaled),
             .scale = 1.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_u32_scaled - 42949.0f;
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 0.5f);
}

TEST_CASE(test_scaled_s32_boundary)
{
        /* Scaled S32 with large negative value - tests float-to-int round-trip
         * at the extreme end of the range. */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_s32_scaled = -214700000.0f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_S32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_scaled_t, field_s32_scaled),
             .scale = 1.0f,
             .offset = 0.0f,
             .behaviour = PPACK_BEHAVIOUR_SCALED},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_struct_scaled_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        float diff = unpacked.field_s32_scaled - (-214700000.0f);
        if (diff < 0.0f) {
                diff = -diff;
        }
        TEST_ASSERT(diff < 100000.0f);
}

/* ---- Sign-extension edge cases ---- */

TEST_CASE(test_sign_ext_min_negative)
{
        /* Test minimum negative value for widths 1-16 */
        for (uint16_t bit_len = 1; bit_len <= 16; ++bit_len) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                uint16_t u_min = (uint16_t)(1u << (bit_len - 1));
                int16_t min_val;
                memcpy(&min_val, &u_min, sizeof(min_val));
                if (bit_len == 1) {
                        min_val = -1;
                } else if (bit_len == 16) {
                        /* Force precise standard representation */
                        min_val = (int16_t)(-32767 - 1);
                } else {
                        min_val = -(int16_t)(1u << (bit_len - 1));
                }
                test_struct_t data = {.field_s16 = min_val};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S16,
                     .start_bit = 0,
                     .bit_length = bit_len,
                     .ptr_offset = offsetof(test_struct_t, field_s16),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_s16 == min_val);
        }
}

TEST_CASE(test_sign_ext_max_positive)
{
        /* Test maximum positive value for widths 1-16 */
        for (uint16_t bit_len = 1; bit_len <= 16; ++bit_len) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                int16_t max_val = (int16_t)((1u << (bit_len - 1)) - 1);
                test_struct_t data = {.field_s16 = max_val};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S16,
                     .start_bit = 0,
                     .bit_length = bit_len,
                     .ptr_offset = offsetof(test_struct_t, field_s16),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_s16 == max_val);
        }
}

TEST_CASE(test_sign_ext_s32_boundary)
{
        /* S32 sign-extension at widths 1-32 */
        for (uint16_t bit_len = 1; bit_len <= 32; ++bit_len) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                int32_t min_val;
                if (bit_len == 1) {
                        min_val = -1;
                } else if (bit_len == 32) {
                        /* Use -2147483647 - 1 to avoid C macro parser UB */
                        min_val = (int32_t)(-2147483647L - 1L);
                } else {
                        min_val = -(int32_t)(1u << (bit_len - 1));
                }
                test_struct_t data = {.field_s32 = min_val};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S32,
                     .start_bit = 0,
                     .bit_length = bit_len,
                     .ptr_offset = offsetof(test_struct_t, field_s32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_s32 == min_val);
        }
}

/* ---- Zero and all-ones patterns ---- */

TEST_CASE(test_payload_all_zeros)
{
        /* Round-trip with all-zero payload through various field configs */
        const ppack_byte_t zero_payload[PPACK_PAYLOAD_UNITS] = {0};

        /* U16 at position 0 */
        const struct ppack_field f1 = {
            .type = PPACK_TYPE_U16,
            .start_bit = 0,
            .bit_length = 16,
            .ptr_offset = offsetof(test_struct_t, field_u16),
            .behaviour = PPACK_BEHAVIOUR_RAW,
        };
        test_struct_t unpacked = {0};
        TEST_ASSERT(ppack_unpack(&unpacked, zero_payload, &f1, 1)
                    == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0);

        /* S32 at position 32 */
        const struct ppack_field f2 = {
            .type = PPACK_TYPE_S32,
            .start_bit = 32,
            .bit_length = 32,
            .ptr_offset = offsetof(test_struct_t, field_s32),
            .behaviour = PPACK_BEHAVIOUR_RAW,
        };
        unpacked.field_s32 = -1;
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        TEST_ASSERT(ppack_pack(&unpacked, payload, &f2, 1) == PPACK_SUCCESS);
        /* All bits should be 1 in the S32 field, 0 elsewhere */
        for (uint16_t i = 0; i < 32; ++i) {
                ASSERT_PAYLOAD_BIT(payload, 32 + i, 1);
        }
        for (uint16_t i = 0; i < 32; ++i) {
                ASSERT_PAYLOAD_BIT(payload, i, 0);
        }
}

TEST_CASE(test_payload_all_ones)
{
        /* Round-trip with all-ones payload */
        ppack_byte_t ones_payload[PPACK_PAYLOAD_UNITS];
        for (uint16_t i = 0; i < PPACK_PAYLOAD_UNITS; ++i) {
                ones_payload[i] = (ppack_byte_t)~0u;
        }

        test_struct_t unpacked = {0};
        const struct ppack_field f1 = {
            .type = PPACK_TYPE_U32,
            .start_bit = 0,
            .bit_length = 32,
            .ptr_offset = offsetof(test_struct_t, field_u32),
            .behaviour = PPACK_BEHAVIOUR_RAW,
        };
        TEST_ASSERT(ppack_unpack(&unpacked, ones_payload, &f1, 1)
                    == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u32 == 0xFFFFFFFF);

        const struct ppack_field f2 = {
            .type = PPACK_TYPE_S32,
            .start_bit = 0,
            .bit_length = 32,
            .ptr_offset = offsetof(test_struct_t, field_s32),
            .behaviour = PPACK_BEHAVIOUR_RAW,
        };
        unpacked.field_s32 = 0;
        TEST_ASSERT(ppack_unpack(&unpacked, ones_payload, &f2, 1)
                    == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_s32 == -1);
}

/* ---- Single-bit fields (PPACK_TYPE_BITS) ---- */

TEST_CASE(test_single_bit_bits_type)
{
        /* Each individual bit at positions 0-63 set independently */
        for (uint16_t bit_pos = 0; bit_pos < 64; ++bit_pos) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_bits = 1u};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_BITS,
                     .start_bit = bit_pos,
                     .bit_length = 1,
                     .ptr_offset = offsetof(test_struct_t, field_bits),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                int ret = ppack_pack(&data, payload, fields, 1);
                TEST_ASSERT(ret == PPACK_SUCCESS);

                /* Verify only bit_pos is set */
                ASSERT_PAYLOAD_BIT(payload, bit_pos, 1);
                for (uint16_t i = 0; i < 64; ++i) {
                        if (i != bit_pos) {
                                ASSERT_PAYLOAD_BIT(payload, i, 0);
                        }
                }

                test_struct_t unpacked = {0};
                int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
                TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
                TEST_ASSERT(unpacked.field_bits == 1u);
        }
}

/* ------------------------------------------------------------------ */
/* TI C2000 uint8_t alias shim tests                                  */
/* ------------------------------------------------------------------ */
/*
 * These tests exercise the 16-bit-MAU U8 path where a user's
 * `uint8_t` struct member is 16-bit storage (on TI) or simulated as
 * such via PPACK_SIMULATE_16BIT_MAU + the ppack_u8_t alias.
 */

TEST_CASE(test_u8_pack_masks_oversized_source)
{
        /* Source holds bits above the low 8; pack must emit only 0x34 */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};
        data.field_u8 = (ppack_u8_t)0x1234u;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x34u);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x00u);
}

TEST_CASE(test_u8_pack_bitlen16_masks_upper)
{
        /*
         * bit_length > 8 is where write_bits no longer masks internally,
         * so the library's explicit `& 0xFFu` is the only guard. Without
         * it on TI, an out-of-range source leaks its upper 8 bits onto
         * the wire.
         */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t data = {0};
        data.field_u8 = (ppack_u8_t)0xABCDu;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0xCDu);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x00u);
}

TEST_CASE(test_u8_unpack_clears_upper_bits)
{
        /*
         * On TI, field_u8 is 16-bit storage. Unpack must overwrite the
         * whole storage unit, not just the low 8 bits - otherwise junk
         * left over from a prior write would leak into subsequent reads.
         */
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_t source = {0};
        source.field_u8 = (ppack_u8_t)0x33u;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int pack_ret = ppack_pack(&source, payload, fields, 1);
        TEST_ASSERT(pack_ret == PPACK_SUCCESS);

        test_struct_t dest = {0};
        dest.field_u8 = (ppack_u8_t)0xAA55u; /* Pre-existing junk */

        int unpack_ret = ppack_unpack(&dest, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(dest.field_u8 == (ppack_u8_t)0x33u);
        TEST_ASSERT(((uint16_t)dest.field_u8 & 0xFF00u) == 0u);
}

TEST_CASE(test_u8_adjacent_oversized_inputs)
{
        /*
         * Two adjacent U8 fields, both with source values > 255. On a
         * 16-bit-MAU target both members sit in separate 16-bit words
         * but the wire must still contain only the low bytes.
         */
        typedef struct {
                ppack_u8_t a;
                ppack_u8_t b;
        } u8_pair_t;

        u8_pair_t data = {
            .a = (ppack_u8_t)0xAB12u,
            .b = (ppack_u8_t)0xCD34u,
        };
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U8,
             .start_bit = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(u8_pair_t, a),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U8,
             .start_bit = 8,
             .bit_length = 8,
             .ptr_offset = offsetof(u8_pair_t, b),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        int ret = ppack_pack(&data, payload, fields, 2);
        TEST_ASSERT(ret == PPACK_SUCCESS);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 0) == 0x12u);
        TEST_ASSERT(READ_LOGICAL_BYTE(payload, 1) == 0x34u);
}

/* ------------------------------------------------------------------ */
/* Stress / fuzz / property tests                                     */
/* ------------------------------------------------------------------ */

/*
 * Deterministic xorshift32 PRNG. Seeded per test so failures are
 * reproducible from the test name alone.
 */
static uint32_t fuzz_rng_state;

static void
fuzz_seed(uint32_t s)
{
        fuzz_rng_state = s ? s : 1u;
}

static uint32_t
fuzz_next(void)
{
        uint32_t x = fuzz_rng_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        fuzz_rng_state = x;
        return x;
}

static uint32_t
fuzz_range(uint32_t n)
{
        return (n == 0u) ? 0u : (fuzz_next() % n);
}

static uint32_t
fuzz_unsigned_in_bits(uint16_t bit_length)
{
        if (bit_length >= 32u) {
                return fuzz_next();
        }
        return fuzz_next() & (((uint32_t)1u << bit_length) - 1u);
}

static int32_t
fuzz_signed_in_bits(uint16_t bit_length)
{
        if (bit_length >= 32u) {
                return (int32_t)fuzz_next();
        }
        uint32_t mask = ((uint32_t)1u << bit_length) - 1u;
        uint32_t bits = fuzz_next() & mask;
        if (bits & ((uint32_t)1u << (bit_length - 1u))) {
                return (int32_t)(bits | ~mask);
        }
        return (int32_t)bits;
}

static float
test_fabsf(float x)
{
        return x < 0.0f ? -x : x;
}

#define FUZZ_ITERATIONS 5000u

/* ---- Per-type fuzz round-trip ---- */

TEST_CASE(test_fuzz_u8_round_trip)
{
        fuzz_seed(0xBEEF0001u);
        for (uint32_t iter = 0; iter < FUZZ_ITERATIONS; ++iter) {
                uint16_t bit_length = 1u + (uint16_t)fuzz_range(8u);
                uint16_t start_bit =
                    (uint16_t)fuzz_range(65u - (uint32_t)bit_length);
                uint8_t value = (uint8_t)fuzz_unsigned_in_bits(bit_length);

                test_struct_t src = {0};
                src.field_u8 = (ppack_u8_t)value;

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U8,
                     .start_bit = start_bit,
                     .bit_length = bit_length,
                     .ptr_offset = offsetof(test_struct_t, field_u8),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                            == PPACK_SUCCESS);
                TEST_ASSERT((uint8_t)dst.field_u8 == value);
        }
}

TEST_CASE(test_fuzz_u16_round_trip)
{
        fuzz_seed(0xBEEF0002u);
        for (uint32_t iter = 0; iter < FUZZ_ITERATIONS; ++iter) {
                uint16_t bit_length = 1u + (uint16_t)fuzz_range(16u);
                uint16_t start_bit =
                    (uint16_t)fuzz_range(65u - (uint32_t)bit_length);
                uint16_t value = (uint16_t)fuzz_unsigned_in_bits(bit_length);

                test_struct_t src = {.field_u16 = value};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U16,
                     .start_bit = start_bit,
                     .bit_length = bit_length,
                     .ptr_offset = offsetof(test_struct_t, field_u16),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                            == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_u16 == value);
        }
}

TEST_CASE(test_fuzz_s16_round_trip)
{
        fuzz_seed(0xBEEF0003u);
        for (uint32_t iter = 0; iter < FUZZ_ITERATIONS; ++iter) {
                uint16_t bit_length = 1u + (uint16_t)fuzz_range(16u);
                uint16_t start_bit =
                    (uint16_t)fuzz_range(65u - (uint32_t)bit_length);
                int16_t value = (int16_t)fuzz_signed_in_bits(bit_length);

                test_struct_t src = {.field_s16 = value};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S16,
                     .start_bit = start_bit,
                     .bit_length = bit_length,
                     .ptr_offset = offsetof(test_struct_t, field_s16),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                            == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_s16 == value);
        }
}

TEST_CASE(test_fuzz_u32_round_trip)
{
        fuzz_seed(0xBEEF0004u);
        for (uint32_t iter = 0; iter < FUZZ_ITERATIONS; ++iter) {
                uint16_t bit_length = 1u + (uint16_t)fuzz_range(32u);
                uint16_t start_bit =
                    (uint16_t)fuzz_range(65u - (uint32_t)bit_length);
                uint32_t value = fuzz_unsigned_in_bits(bit_length);

                test_struct_t src = {.field_u32 = value};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_U32,
                     .start_bit = start_bit,
                     .bit_length = bit_length,
                     .ptr_offset = offsetof(test_struct_t, field_u32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                            == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_u32 == value);
        }
}

TEST_CASE(test_fuzz_s32_round_trip)
{
        fuzz_seed(0xBEEF0005u);
        for (uint32_t iter = 0; iter < FUZZ_ITERATIONS; ++iter) {
                uint16_t bit_length = 1u + (uint16_t)fuzz_range(32u);
                uint16_t start_bit =
                    (uint16_t)fuzz_range(65u - (uint32_t)bit_length);
                int32_t value = fuzz_signed_in_bits(bit_length);

                test_struct_t src = {.field_s32 = value};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_S32,
                     .start_bit = start_bit,
                     .bit_length = bit_length,
                     .ptr_offset = offsetof(test_struct_t, field_s32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                            == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_s32 == value);
        }
}

TEST_CASE(test_fuzz_bits_round_trip)
{
        fuzz_seed(0xBEEF0006u);
        for (uint32_t iter = 0; iter < FUZZ_ITERATIONS; ++iter) {
                uint16_t bit_length = 1u + (uint16_t)fuzz_range(32u);
                uint16_t start_bit =
                    (uint16_t)fuzz_range(65u - (uint32_t)bit_length);
                uint32_t value = fuzz_unsigned_in_bits(bit_length);

                test_struct_t src = {.field_bits = value};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_BITS,
                     .start_bit = start_bit,
                     .bit_length = bit_length,
                     .ptr_offset = offsetof(test_struct_t, field_bits),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                            == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_bits == value);
        }
}

TEST_CASE(test_fuzz_f32_round_trip)
{
        fuzz_seed(0xBEEF0007u);
        for (uint32_t iter = 0; iter < FUZZ_ITERATIONS; ++iter) {
                /* F32 is a raw 32-bit copy; bit_length is always 32. */
                uint16_t start_bit = (uint16_t)fuzz_range(33u);
                uint32_t bits = fuzz_next();

                test_struct_t src = {0};
                /* Use memcpy to set the float bits - avoids FP signalling
                 * behaviour when the random pattern happens to be a sNaN. */
                memcpy(&src.field_f32, &bits, sizeof(src.field_f32));

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_F32,
                     .start_bit = start_bit,
                     .bit_length = 32,
                     .ptr_offset = offsetof(test_struct_t, field_f32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                            == PPACK_SUCCESS);

                uint32_t out_bits;
                memcpy(&out_bits, &dst.field_f32, sizeof(out_bits));
                TEST_ASSERT(out_bits == bits);
        }
}

/* ---- Multi-field mixed-type fuzz ---- */

TEST_CASE(test_fuzz_multi_field_mixed)
{
        /* Fixed 64-bit layout exercising six types; random values. */
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 12,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S16,
             .start_bit = 12,
             .bit_length = 10,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U32,
             .start_bit = 22,
             .bit_length = 18,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S32,
             .start_bit = 40,
             .bit_length = 14,
             .ptr_offset = offsetof(test_struct_t, field_s32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U8,
             .start_bit = 54,
             .bit_length = 6,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 60,
             .bit_length = 4,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };
        const size_t n_fields = sizeof(fields) / sizeof(fields[0]);

        fuzz_seed(0xBEEF0008u);
        for (uint32_t iter = 0; iter < 2000u; ++iter) {
                test_struct_t src = {0};
                src.field_u16 = (uint16_t)fuzz_unsigned_in_bits(12);
                src.field_s16 = (int16_t)fuzz_signed_in_bits(10);
                src.field_u32 = fuzz_unsigned_in_bits(18);
                src.field_s32 = fuzz_signed_in_bits(14);
                src.field_u8 = (ppack_u8_t)fuzz_unsigned_in_bits(6);
                src.field_bits = fuzz_unsigned_in_bits(4);

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, n_fields)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, n_fields)
                            == PPACK_SUCCESS);

                TEST_ASSERT(dst.field_u16 == src.field_u16);
                TEST_ASSERT(dst.field_s16 == src.field_s16);
                TEST_ASSERT(dst.field_u32 == src.field_u32);
                TEST_ASSERT(dst.field_s32 == src.field_s32);
                TEST_ASSERT((uint8_t)dst.field_u8 == (uint8_t)src.field_u8);
                TEST_ASSERT(dst.field_bits == src.field_bits);
        }
}

/* ---- Pack / unpack / pack idempotency ---- */

TEST_CASE(test_fuzz_pack_unpack_pack_idempotent_raw)
{
        /*
         * Strong serialisation invariant: pack -> unpack -> pack must
         * produce a bit-identical second wire. Catches any subtle loss
         * of information through the unpack-then-repack cycle for raw
         * (non-scaled) types.
         */
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 12,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S16,
             .start_bit = 12,
             .bit_length = 10,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U32,
             .start_bit = 22,
             .bit_length = 18,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S32,
             .start_bit = 40,
             .bit_length = 14,
             .ptr_offset = offsetof(test_struct_t, field_s32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U8,
             .start_bit = 54,
             .bit_length = 6,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 60,
             .bit_length = 4,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };
        const size_t n_fields = sizeof(fields) / sizeof(fields[0]);

        fuzz_seed(0xBEEF000Au);
        for (uint32_t iter = 0; iter < 2000u; ++iter) {
                test_struct_t src = {0};
                src.field_u16 = (uint16_t)fuzz_unsigned_in_bits(12);
                src.field_s16 = (int16_t)fuzz_signed_in_bits(10);
                src.field_u32 = fuzz_unsigned_in_bits(18);
                src.field_s32 = fuzz_signed_in_bits(14);
                src.field_u8 = (ppack_u8_t)fuzz_unsigned_in_bits(6);
                src.field_bits = fuzz_unsigned_in_bits(4);

                ppack_byte_t wire1[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, wire1, fields, n_fields)
                            == PPACK_SUCCESS);

                test_struct_t mid = {0};
                TEST_ASSERT(ppack_unpack(&mid, wire1, fields, n_fields)
                            == PPACK_SUCCESS);

                ppack_byte_t wire2[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&mid, wire2, fields, n_fields)
                            == PPACK_SUCCESS);

                TEST_ASSERT(memcmp(wire1, wire2, sizeof(wire1)) == 0);
        }
}

/*
 * Note: an equivalent pack/unpack/pack idempotency test for SCALED
 * fields is intentionally NOT included. The scaled cycle is
 * raw1 -> y = raw1 * scale + offset -> raw2 = trunc((y - offset) / scale).
 * For arbitrary float scale factors (e.g. 0.01f, which is not exactly
 * representable in IEEE-754 binary32) the multiplication-then-division
 * does not always recover raw1 exactly, so the second wire can differ
 * by up to one LSB. The library does not promise scaled idempotency
 * on the wire, only the bounded round-trip error verified by
 * test_scaled_quantization_bounded.
 */

/* ---- Pack determinism ---- */

TEST_CASE(test_pack_is_deterministic)
{
        /* Identical inputs must produce bit-identical payloads. */
        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U32,
             .start_bit = 32,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        fuzz_seed(0xBEEF0009u);
        for (uint32_t iter = 0; iter < 1000u; ++iter) {
                test_struct_t src = {0};
                src.field_u16 = (uint16_t)fuzz_unsigned_in_bits(16);
                src.field_s16 = (int16_t)fuzz_signed_in_bits(16);
                src.field_u32 = fuzz_unsigned_in_bits(32);

                ppack_byte_t p1[PPACK_PAYLOAD_UNITS] = {0};
                ppack_byte_t p2[PPACK_PAYLOAD_UNITS] = {0};

                TEST_ASSERT(ppack_pack(&src, p1, fields, 3) == PPACK_SUCCESS);
                TEST_ASSERT(ppack_pack(&src, p2, fields, 3) == PPACK_SUCCESS);
                TEST_ASSERT(memcmp(p1, p2, sizeof(p1)) == 0);
        }
}

/* ---- Scaled quantization error bound ---- */

TEST_CASE(test_scaled_quantization_bounded)
{
        /*
         * For realistic CAN scale/offset/width combos, round-trip error
         * through the library must stay within one LSB of the scale.
         */
        struct quant_case {
                enum ppack_type type;
                float scale;
                float offset;
                uint16_t bit_length;
                size_t ptr_offset;
                float min_input;
                float max_input;
        };

        const struct quant_case cases[] = {
            /* Voltage 0..650V at 0.01V resolution */
            {PPACK_TYPE_U16, 0.01f, 0.0f, 16,
             offsetof(test_struct_scaled_t, field_u16_scaled), 0.0f, 650.0f},
            /* Current -2000..2000A at 0.1A */
            {PPACK_TYPE_S16, 0.1f, 0.0f, 16,
             offsetof(test_struct_scaled_t, field_s16_scaled), -2000.0f,
             2000.0f},
            /* Temperature -40..125C at 0.25C, offset-encoded */
            {PPACK_TYPE_U16, 0.25f, -40.0f, 10,
             offsetof(test_struct_scaled_t, field_u16_scaled), -40.0f, 125.0f},
            /* Energy counter 0..1e6 kWh at 0.001 */
            {PPACK_TYPE_U32, 0.001f, 0.0f, 32,
             offsetof(test_struct_scaled_t, field_u32_scaled), 0.0f, 1.0e6f},
            /* Power -2e6..2e6 W at 1W */
            {PPACK_TYPE_S32, 1.0f, 0.0f, 24,
             offsetof(test_struct_scaled_t, field_s32_scaled), -2.0e6f, 2.0e6f},
        };

        const size_t n_cases = sizeof(cases) / sizeof(cases[0]);
        for (size_t c = 0; c < n_cases; ++c) {
                for (uint32_t step = 0; step < 200u; ++step) {
                        float frac = (float)step / 199.0f;
                        float value =
                            cases[c].min_input
                            + (cases[c].max_input - cases[c].min_input) * frac;

                        test_struct_scaled_t src = {0};
                        memcpy((char *)&src + cases[c].ptr_offset, &value,
                               sizeof(value));

                        const struct ppack_field fields[] = {
                            {.type = cases[c].type,
                             .start_bit = 0,
                             .bit_length = cases[c].bit_length,
                             .ptr_offset = cases[c].ptr_offset,
                             .scale = cases[c].scale,
                             .offset = cases[c].offset,
                             .behaviour = PPACK_BEHAVIOUR_SCALED},
                        };

                        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                        TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                                    == PPACK_SUCCESS);

                        test_struct_scaled_t dst = {0};
                        TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                                    == PPACK_SUCCESS);

                        float unpacked;
                        memcpy(&unpacked,
                               (const char *)&dst + cases[c].ptr_offset,
                               sizeof(unpacked));

                        /* Library truncates, so error is within one LSB.
                         * Add a small epsilon for float rounding. */
                        float tol =
                            cases[c].scale + test_fabsf(value) * 1.0e-6f;
                        TEST_ASSERT(test_fabsf(unpacked - value) <= tol);
                }
        }
}

/* ---- Scaled saturation ---- */

TEST_CASE(test_scaled_saturation)
{
        /* U16: 0..65535, scale=1, offset=0 */
        {
                test_struct_scaled_t src = {.field_u16_scaled = 1.0e9f};
                const struct ppack_field f[] = {
                    {.type = PPACK_TYPE_U16,
                     .start_bit = 0,
                     .bit_length = 16,
                     .ptr_offset =
                         offsetof(test_struct_scaled_t, field_u16_scaled),
                     .scale = 1.0f,
                     .offset = 0.0f,
                     .behaviour = PPACK_BEHAVIOUR_SCALED},
                };
                ppack_byte_t p[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, p, f, 1) == PPACK_SUCCESS);
                test_struct_scaled_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_u16_scaled == 65535.0f);

                src.field_u16_scaled = -1.0e9f;
                TEST_ASSERT(ppack_pack(&src, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(ppack_unpack(&dst, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_u16_scaled == 0.0f);
        }

        /* S16: -32768..32767 */
        {
                test_struct_scaled_t src = {.field_s16_scaled = 1.0e9f};
                const struct ppack_field f[] = {
                    {.type = PPACK_TYPE_S16,
                     .start_bit = 0,
                     .bit_length = 16,
                     .ptr_offset =
                         offsetof(test_struct_scaled_t, field_s16_scaled),
                     .scale = 1.0f,
                     .offset = 0.0f,
                     .behaviour = PPACK_BEHAVIOUR_SCALED},
                };
                ppack_byte_t p[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, p, f, 1) == PPACK_SUCCESS);
                test_struct_scaled_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_s16_scaled == 32767.0f);

                src.field_s16_scaled = -1.0e9f;
                TEST_ASSERT(ppack_pack(&src, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(ppack_unpack(&dst, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_s16_scaled == -32768.0f);
        }

        /* U32: 0..4294967295 */
        {
                test_struct_scaled_t src = {.field_u32_scaled = 1.0e12f};
                const struct ppack_field f[] = {
                    {.type = PPACK_TYPE_U32,
                     .start_bit = 0,
                     .bit_length = 32,
                     .ptr_offset =
                         offsetof(test_struct_scaled_t, field_u32_scaled),
                     .scale = 1.0f,
                     .offset = 0.0f,
                     .behaviour = PPACK_BEHAVIOUR_SCALED},
                };
                ppack_byte_t p[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, p, f, 1) == PPACK_SUCCESS);
                test_struct_scaled_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, p, f, 1) == PPACK_SUCCESS);
                /* U32 scaled-unpack goes through uint32_t so float precision
                 * may not represent 4294967295 exactly; check within 1 LSB. */
                TEST_ASSERT(dst.field_u32_scaled >= 4.294e9f);

                src.field_u32_scaled = -1.0f;
                TEST_ASSERT(ppack_pack(&src, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(ppack_unpack(&dst, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_u32_scaled == 0.0f);
        }

        /* S32: -2147483648..2147483647 */
        {
                test_struct_scaled_t src = {.field_s32_scaled = 1.0e12f};
                const struct ppack_field f[] = {
                    {.type = PPACK_TYPE_S32,
                     .start_bit = 0,
                     .bit_length = 32,
                     .ptr_offset =
                         offsetof(test_struct_scaled_t, field_s32_scaled),
                     .scale = 1.0f,
                     .offset = 0.0f,
                     .behaviour = PPACK_BEHAVIOUR_SCALED},
                };
                ppack_byte_t p[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, p, f, 1) == PPACK_SUCCESS);
                test_struct_scaled_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_s32_scaled >= 2.1474e9f);

                src.field_s32_scaled = -1.0e12f;
                TEST_ASSERT(ppack_pack(&src, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(ppack_unpack(&dst, p, f, 1) == PPACK_SUCCESS);
                TEST_ASSERT(dst.field_s32_scaled <= -2.1474e9f);
        }
}

/* ---- F32 special values ---- */

TEST_CASE(test_f32_special_values)
{
        /* Bit patterns covering ±0, ±Inf, NaN, denormals, FLT_MIN/MAX */
        static const uint32_t specials[] = {
            0x00000000u, /* +0 */
            0x80000000u, /* -0 */
            0x3F800000u, /* +1.0 */
            0xBF800000u, /* -1.0 */
            0x7F800000u, /* +Inf */
            0xFF800000u, /* -Inf */
            0x7FC00000u, /* quiet NaN */
            0x7FA00000u, /* signalling NaN */
            0xFFC12345u, /* negative NaN with payload */
            0x7F7FFFFFu, /* FLT_MAX */
            0x00800000u, /* FLT_MIN (smallest normal) */
            0x007FFFFFu, /* largest denormal */
            0x00000001u, /* smallest denormal */
            0x80000001u, /* smallest negative denormal */
        };

        for (size_t i = 0; i < sizeof(specials) / sizeof(specials[0]); ++i) {
                test_struct_t src = {0};
                memcpy(&src.field_f32, &specials[i], sizeof(src.field_f32));

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_F32,
                     .start_bit = 16, /* non-zero start to stress alignment */
                     .bit_length = 32,
                     .ptr_offset = offsetof(test_struct_t, field_f32),
                     .behaviour = PPACK_BEHAVIOUR_RAW},
                };

                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                TEST_ASSERT(ppack_pack(&src, payload, fields, 1)
                            == PPACK_SUCCESS);

                test_struct_t dst = {0};
                TEST_ASSERT(ppack_unpack(&dst, payload, fields, 1)
                            == PPACK_SUCCESS);

                uint32_t out_bits;
                memcpy(&out_bits, &dst.field_f32, sizeof(out_bits));
                TEST_ASSERT(out_bits == specials[i]);
        }
}

/* ------------------------------------------------------------------ */
/* Wire format v1 lockdown                                            */
/* ------------------------------------------------------------------ */
/*
 * These tests assert the exact byte sequence produced by ppack_pack
 * for a fixed set of inputs. They define the wire-format contract for
 * the v1.x release line: any change to these expected bytes is a
 * BREAKING change to the protocol that downstream firmware nodes
 * (e.g. Power Module / Supervisory controller CAN bus) rely on.
 *
 * Do not edit the expected byte arrays in these tests. If the library
 * needs to produce a different wire layout, that is a v2 release.
 */

TEST_CASE(test_wire_v1_lockdown_byte_aligned)
{
        /* Layout:
         *   bits  0..15  PPACK_TYPE_U16  (uint16_t  = 0x1234)
         *   bits 16..31  PPACK_TYPE_S16  (int16_t   = -100   = 0xFF9C)
         *   bits 32..63  PPACK_TYPE_U32  (uint32_t  = 0xDEADBEEF)
         */
        test_struct_t data = {
            .field_u16 = 0x1234u,
            .field_s16 = -100,
            .field_u32 = 0xDEADBEEFu,
        };

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_S16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U32,
             .start_bit = 32,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        TEST_ASSERT(ppack_pack(&data, payload, fields, 3) == PPACK_SUCCESS);

        static const uint8_t expected[8] = {
            0x34u, 0x12u, 0x9Cu, 0xFFu, 0xEFu, 0xBEu, 0xADu, 0xDEu,
        };
        for (uint16_t i = 0; i < 8; ++i) {
                TEST_ASSERT(READ_LOGICAL_BYTE(payload, i) == expected[i]);
        }
}

TEST_CASE(test_wire_v1_lockdown_float_and_small_types)
{
        /* Layout:
         *   bits  0..31  PPACK_TYPE_F32  (float    = 1.0f = 0x3F800000)
         *   bits 32..47  PPACK_TYPE_U16  (uint16_t = 0xCAFE)
         *   bits 48..55  PPACK_TYPE_U8   (uint8_t  = 0xAB)
         *   bits 56..63  PPACK_TYPE_BITS (uint32_t = 0xCD, 8-bit field)
         */
        test_struct_t data = {0};
        data.field_f32 = 1.0f;
        data.field_u16 = 0xCAFEu;
        data.field_u8 = (ppack_u8_t)0xABu;
        data.field_bits = 0xCDu;

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_F32,
             .start_bit = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_struct_t, field_f32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U16,
             .start_bit = 32,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_U8,
             .start_bit = 48,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_u8),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_BITS,
             .start_bit = 56,
             .bit_length = 8,
             .ptr_offset = offsetof(test_struct_t, field_bits),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        TEST_ASSERT(ppack_pack(&data, payload, fields, 4) == PPACK_SUCCESS);

        static const uint8_t expected[8] = {
            0x00u, 0x00u, 0x80u, 0x3Fu, 0xFEu, 0xCAu, 0xABu, 0xCDu,
        };
        for (uint16_t i = 0; i < 8; ++i) {
                TEST_ASSERT(READ_LOGICAL_BYTE(payload, i) == expected[i]);
        }
}

TEST_CASE(test_wire_v1_lockdown_cross_byte_boundary)
{
        /* Layout: a single PPACK_TYPE_U16 = 0xABCD shifted left by 4 bits.
         * Verifies the LSB-first / little-endian bit ordering on a non-
         * byte-aligned start position. Bits 0..3 and 20..63 are zero. */
        test_struct_t data = {.field_u16 = 0xABCDu};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_U16,
             .start_bit = 4,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
        };

        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        TEST_ASSERT(ppack_pack(&data, payload, fields, 1) == PPACK_SUCCESS);

        static const uint8_t expected[8] = {
            0xD0u, 0xBCu, 0x0Au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        };
        for (uint16_t i = 0; i < 8; ++i) {
                TEST_ASSERT(READ_LOGICAL_BYTE(payload, i) == expected[i]);
        }
}

/* ------------------------------------------------------------------ */
/* Test runner                                                        */
/* ------------------------------------------------------------------ */

static void
run_test(void (*test_func)(void), const char *name)
{
        test_func();
        TEST_PASS(name);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int
main(void)
{
        fprintf(stdout, "\n=== Running ppack unit tests ===\n\n");

        /* NULL / invalid argument */
        run_test(test_pack_null_base_ptr, "test_pack_null_base_ptr");
        run_test(test_pack_null_payload, "test_pack_null_payload");
        run_test(test_pack_null_fields, "test_pack_null_fields");
        run_test(test_unpack_null_base_ptr, "test_unpack_null_base_ptr");
        run_test(test_unpack_null_payload, "test_unpack_null_payload");
        run_test(test_unpack_null_fields, "test_unpack_null_fields");

        /* Bounds / overflow validation */
        run_test(test_pack_overflow_start_plus_len,
                 "test_pack_overflow_start_plus_len");
        run_test(test_unpack_overflow_start_plus_len,
                 "test_unpack_overflow_start_plus_len");
        run_test(test_pack_exact_boundary_passes,
                 "test_pack_exact_boundary_passes");
        run_test(test_pack_bit_length_zero_rejected,
                 "test_pack_bit_length_zero_rejected");
        run_test(test_pack_bit_length_33_rejected,
                 "test_pack_bit_length_33_rejected");

        /* Scale == 0 rejection */
        run_test(test_pack_scale_zero_u16_rejected,
                 "test_pack_scale_zero_u16_rejected");
        run_test(test_pack_scale_zero_s16_rejected,
                 "test_pack_scale_zero_s16_rejected");
        run_test(test_pack_scale_zero_u32_rejected,
                 "test_pack_scale_zero_u32_rejected");
        run_test(test_pack_scale_zero_s32_rejected,
                 "test_pack_scale_zero_s32_rejected");
        run_test(test_pack_scale_u8_rejected, "test_pack_scale_u8_rejected");

        /* Raw round-trips */
        run_test(test_pack_unpack_u16, "test_pack_unpack_u16");
        run_test(test_pack_unpack_s16, "test_pack_unpack_s16");
        run_test(test_pack_unpack_u32, "test_pack_unpack_u32");
        run_test(test_pack_unpack_s32, "test_pack_unpack_s32");
        run_test(test_pack_unpack_f32, "test_pack_unpack_f32");
        run_test(test_pack_unpack_u8, "test_pack_unpack_u8");
        run_test(test_pack_unpack_bits, "test_pack_unpack_bits");

        /* Sign extension */
        run_test(test_s16_12bit_negative, "test_s16_12bit_negative");
        run_test(test_s16_12bit_positive, "test_s16_12bit_positive");
        run_test(test_s32_20bit_negative, "test_s32_20bit_negative");
        run_test(test_s32_1bit_negative, "test_s32_1bit_negative");

        /* Layout */
        run_test(test_pack_unpack_u16_offset, "test_pack_unpack_u16_offset");
        run_test(test_pack_unpack_spanning_boundary,
                 "test_pack_unpack_spanning_boundary");
        run_test(test_pack_multiple_fields, "test_pack_multiple_fields");

        /* Scaled */
        run_test(test_pack_scaled_u16, "test_pack_scaled_u16");
        run_test(test_pack_scaled_s16_with_offset,
                 "test_pack_scaled_s16_with_offset");
        run_test(test_pack_scaled_u32, "test_pack_scaled_u32");
        run_test(test_pack_scaled_s32, "test_pack_scaled_s32");

        /* Misc / edge */
        run_test(test_invalid_field_type, "test_invalid_field_type");
        run_test(test_unpack_invalid_field_type,
                 "test_unpack_invalid_field_type");
        run_test(test_empty_field_list, "test_empty_field_list");
        run_test(test_unpack_zero_field_count, "test_unpack_zero_field_count");
        run_test(test_small_bit_field, "test_small_bit_field");
        run_test(test_6bit_field, "test_6bit_field");
        run_test(test_adjacent_bit_fields, "test_adjacent_bit_fields");
        run_test(test_negative_s32, "test_negative_s32");
        run_test(test_max_u16, "test_max_u16");
        run_test(test_max_s16, "test_max_s16");
        run_test(test_min_s16, "test_min_s16");
        run_test(test_zero_values, "test_zero_values");
        run_test(test_negative_float, "test_negative_float");
        run_test(test_float_zero, "test_float_zero");
        run_test(test_1bit_field, "test_1bit_field");
        run_test(test_1bit_field_zero, "test_1bit_field_zero");
        run_test(test_overlapping_fields, "test_overlapping_fields");
        run_test(test_full_payload, "test_full_payload");

        /* Exhaustive: bit-position and boundary coverage */
        run_test(test_all_bit_positions_u8, "test_all_bit_positions_u8");
        run_test(test_all_bit_positions_u16, "test_all_bit_positions_u16");
        run_test(test_all_bit_positions_s16, "test_all_bit_positions_s16");
        run_test(test_all_bit_positions_u32, "test_all_bit_positions_u32");
        run_test(test_all_bit_positions_s32, "test_all_bit_positions_s32");
        run_test(test_all_bit_lengths_u8, "test_all_bit_lengths_u8");
        run_test(test_all_bit_lengths_u16, "test_all_bit_lengths_u16");
        run_test(test_all_bit_lengths_s16, "test_all_bit_lengths_s16");
        run_test(test_all_bit_lengths_u32, "test_all_bit_lengths_u32");
        run_test(test_all_bit_lengths_s32, "test_all_bit_lengths_s32");
        run_test(test_cross_byte_boundary_u16, "test_cross_byte_boundary_u16");
        run_test(test_cross_byte_boundary_u8_pair,
                 "test_cross_byte_boundary_u8_pair");
        run_test(test_adjacent_u8_same_word, "test_adjacent_u8_same_word");
        run_test(test_odd_aligned_u8, "test_odd_aligned_u8");
        run_test(test_full_layout_u8x8, "test_full_layout_u8x8");
        run_test(test_full_layout_s16x4, "test_full_layout_s16x4");
        run_test(test_full_layout_mixed, "test_full_layout_mixed");
        run_test(test_scaled_s16_negative, "test_scaled_s16_negative");
        run_test(test_scaled_u32_large, "test_scaled_u32_large");
        run_test(test_scaled_s32_boundary, "test_scaled_s32_boundary");
        run_test(test_sign_ext_min_negative, "test_sign_ext_min_negative");
        run_test(test_sign_ext_max_positive, "test_sign_ext_max_positive");
        run_test(test_sign_ext_s32_boundary, "test_sign_ext_s32_boundary");
        run_test(test_payload_all_zeros, "test_payload_all_zeros");
        run_test(test_payload_all_ones, "test_payload_all_ones");
        run_test(test_single_bit_bits_type, "test_single_bit_bits_type");

        /* TI C2000 uint8_t alias shim */
        run_test(test_u8_pack_masks_oversized_source,
                 "test_u8_pack_masks_oversized_source");
        run_test(test_u8_pack_bitlen16_masks_upper,
                 "test_u8_pack_bitlen16_masks_upper");
        run_test(test_u8_unpack_clears_upper_bits,
                 "test_u8_unpack_clears_upper_bits");
        run_test(test_u8_adjacent_oversized_inputs,
                 "test_u8_adjacent_oversized_inputs");

        /* Stress / fuzz / property */
        run_test(test_fuzz_u8_round_trip, "test_fuzz_u8_round_trip");
        run_test(test_fuzz_u16_round_trip, "test_fuzz_u16_round_trip");
        run_test(test_fuzz_s16_round_trip, "test_fuzz_s16_round_trip");
        run_test(test_fuzz_u32_round_trip, "test_fuzz_u32_round_trip");
        run_test(test_fuzz_s32_round_trip, "test_fuzz_s32_round_trip");
        run_test(test_fuzz_bits_round_trip, "test_fuzz_bits_round_trip");
        run_test(test_fuzz_f32_round_trip, "test_fuzz_f32_round_trip");
        run_test(test_fuzz_multi_field_mixed, "test_fuzz_multi_field_mixed");
        run_test(test_fuzz_pack_unpack_pack_idempotent_raw,
                 "test_fuzz_pack_unpack_pack_idempotent_raw");
        run_test(test_pack_is_deterministic, "test_pack_is_deterministic");
        run_test(test_scaled_quantization_bounded,
                 "test_scaled_quantization_bounded");
        run_test(test_scaled_saturation, "test_scaled_saturation");
        run_test(test_f32_special_values, "test_f32_special_values");

        /* Wire format v1 lockdown - DO NOT change expected bytes in v1.x */
        run_test(test_wire_v1_lockdown_byte_aligned,
                 "test_wire_v1_lockdown_byte_aligned");
        run_test(test_wire_v1_lockdown_float_and_small_types,
                 "test_wire_v1_lockdown_float_and_small_types");
        run_test(test_wire_v1_lockdown_cross_byte_boundary,
                 "test_wire_v1_lockdown_cross_byte_boundary");

        fprintf(stdout, "\n=== All tests passed ===\n\n");
        return EXIT_SUCCESS;
}
