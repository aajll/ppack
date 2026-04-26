/*
 * @file: test_exhaustive.c
 * @brief Brute-force sweeps over bit positions, bit lengths, cross-boundary
 *        layouts, adjacency, odd alignment, and full-payload multi-field
 *        configurations.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>
#include <stdint.h>

/* ---- Every start_bit position (0-63) for each type ---- */

TEST_CASE(test_all_bit_positions_u8)
{
        for (uint16_t start_bit = 0; start_bit <= 56; ++start_bit) {
                ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};
                test_struct_t data = {.field_u8 = 0xAB};

                const struct ppack_field fields[] = {
                    {.type = PPACK_TYPE_UINT8,
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
                    {.type = PPACK_TYPE_UINT16,
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
                    {.type = PPACK_TYPE_INT16,
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
                    {.type = PPACK_TYPE_UINT32,
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
                    {.type = PPACK_TYPE_INT32,
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
                    {.type = PPACK_TYPE_UINT8,
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
                    {.type = PPACK_TYPE_UINT16,
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
                    {.type = PPACK_TYPE_UINT32,
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
            {.type = PPACK_TYPE_UINT16,
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
            {.type = PPACK_TYPE_UINT8,
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
            {.type = PPACK_TYPE_UINT8,
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
                    {.type = PPACK_TYPE_UINT8,
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
            {.type = PPACK_TYPE_UINT8,
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
            {.type = PPACK_TYPE_INT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT16,
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
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_INT16,
             .start_bit = 16,
             .bit_length = 16,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT8,
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

void
run_exhaustive_tests(void)
{
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
}
