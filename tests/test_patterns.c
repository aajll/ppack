/*
 * @file: test_patterns.c
 * @brief Targeted edge-value tests — scaled extremes, sign-extension
 *        boundaries, all-zeros / all-ones payloads, single-bit fields.
 *
 * Note: PPACK_TYPE_UINT8 does not support PPACK_BEHAVIOUR_SCALED in the
 * current library implementation; scaled tests cover U16/S16/U32/S32.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

TEST_CASE(test_scaled_s16_negative)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
        test_struct_scaled_t data = {.field_s16_scaled = -50.25f};

        const struct ppack_field fields[] = {
            {.type = PPACK_TYPE_INT16,
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
            {.type = PPACK_TYPE_UINT32,
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
            {.type = PPACK_TYPE_INT32,
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
                    {.type = PPACK_TYPE_INT16,
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
                    {.type = PPACK_TYPE_INT16,
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
                    {.type = PPACK_TYPE_INT32,
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

TEST_CASE(test_payload_all_zeros)
{
        /* Round-trip with all-zero payload through various field configs */
        const ppack_byte_t zero_payload[PPACK_PAYLOAD_UNITS] = {0};

        /* U16 at position 0 */
        const struct ppack_field f1 = {
            .type = PPACK_TYPE_UINT16,
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
            .type = PPACK_TYPE_INT32,
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
            .type = PPACK_TYPE_UINT32,
            .start_bit = 0,
            .bit_length = 32,
            .ptr_offset = offsetof(test_struct_t, field_u32),
            .behaviour = PPACK_BEHAVIOUR_RAW,
        };
        TEST_ASSERT(ppack_unpack(&unpacked, ones_payload, &f1, 1)
                    == PPACK_SUCCESS);
        TEST_ASSERT(unpacked.field_u32 == 0xFFFFFFFF);

        const struct ppack_field f2 = {
            .type = PPACK_TYPE_INT32,
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

void
run_pattern_tests(void)
{
        run_test(test_scaled_s16_negative, "test_scaled_s16_negative");
        run_test(test_scaled_u32_large, "test_scaled_u32_large");
        run_test(test_scaled_s32_boundary, "test_scaled_s32_boundary");
        run_test(test_sign_ext_min_negative, "test_sign_ext_min_negative");
        run_test(test_sign_ext_max_positive, "test_sign_ext_max_positive");
        run_test(test_sign_ext_s32_boundary, "test_sign_ext_s32_boundary");
        run_test(test_payload_all_zeros, "test_payload_all_zeros");
        run_test(test_payload_all_ones, "test_payload_all_ones");
        run_test(test_single_bit_bits_type, "test_single_bit_bits_type");
}
