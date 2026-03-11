/*
 * @file: test_ppack.c
 * @brief Unit tests for the ppack library.
 */

#include "ppack.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        uint8_t field_u8; /* U8 maps to uint8_t, not uint16_t */
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        int ret = ppack_pack(&data, payload, NULL, 1);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_unpack_null_base_ptr)
{
        uint8_t payload[8] = {0x34, 0x12, 0, 0, 0, 0, 0, 0};
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
        uint8_t payload[8] = {0x34, 0x12, 0, 0, 0, 0, 0, 0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        TEST_ASSERT(payload[6] == 0xCD);
        TEST_ASSERT(payload[7] == 0xAB);
}

TEST_CASE(test_pack_bit_length_zero_rejected)
{
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        TEST_ASSERT(payload[0] == 0x34);
        TEST_ASSERT(payload[1] == 0x12);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
}

TEST_CASE(test_pack_unpack_s16)
{
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        TEST_ASSERT(payload[2] == 0x34);
        TEST_ASSERT(payload[3] == 0x12);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
}

TEST_CASE(test_pack_unpack_spanning_boundary)
{
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        TEST_ASSERT(payload[0] == 0x34);
        TEST_ASSERT(payload[1] == 0x12);
        TEST_ASSERT(payload[2] == 0xd2);
        TEST_ASSERT(payload[3] == 0xe9);

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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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

/* ------------------------------------------------------------------ */
/* Misc / edge cases                                                  */
/* ------------------------------------------------------------------ */

TEST_CASE(test_invalid_field_type)
{
        uint8_t payload[8] = {0};
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

TEST_CASE(test_empty_field_list)
{
        uint8_t payload[8] = {0};
        test_struct_t data = {.field_u16 = 0x1234};

        int ret = ppack_pack(&data, payload, NULL, 0);
        TEST_ASSERT(ret == -PPACK_ERR_INVALARG);
}

TEST_CASE(test_small_bit_field)
{
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        uint8_t payload[8] = {0};
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
        TEST_ASSERT(payload[0] == 0x34);
        TEST_ASSERT(payload[1] == 0x12);
        TEST_ASSERT(payload[2] == 0x78);
        TEST_ASSERT(payload[3] == 0x56);
        TEST_ASSERT(payload[4] == 0xF0);
        TEST_ASSERT(payload[5] == 0xDE);
        TEST_ASSERT(payload[6] == 0xBC);
        TEST_ASSERT(payload[7] == 0x9A);

        test_struct_t unpacked = {0};
        int unpack_ret = ppack_unpack(&unpacked, payload, fields, 3);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u16 == 0x1234);
        TEST_ASSERT(unpacked.field_s16 == 0x5678);
        TEST_ASSERT(unpacked.field_u32 == 0x9ABCDEF0);
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
        run_test(test_empty_field_list, "test_empty_field_list");
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

        fprintf(stdout, "\n=== All tests passed ===\n\n");
        return EXIT_SUCCESS;
}
