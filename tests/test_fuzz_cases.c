/*
 * @file: test_fuzz_cases.c
 * @brief Stress / fuzz / property tests using the deterministic PRNG
 *        from test_fuzz.h: per-type round-trips, pack/unpack/pack
 *        idempotency, determinism, scaled quantization bounds, scaled
 *        saturation, and F32 special-value bit-copy.
 */

#include "ppack.h"
#include "test_fixtures.h"
#include "test_fuzz.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
                    {.type = PPACK_TYPE_UINT8,
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
                    {.type = PPACK_TYPE_UINT16,
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
                    {.type = PPACK_TYPE_INT16,
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
                    {.type = PPACK_TYPE_UINT32,
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
                    {.type = PPACK_TYPE_INT32,
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
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 12,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_INT16,
             .start_bit = 12,
             .bit_length = 10,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 22,
             .bit_length = 18,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_INT32,
             .start_bit = 40,
             .bit_length = 14,
             .ptr_offset = offsetof(test_struct_t, field_s32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT8,
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
            {.type = PPACK_TYPE_UINT16,
             .start_bit = 0,
             .bit_length = 12,
             .ptr_offset = offsetof(test_struct_t, field_u16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_INT16,
             .start_bit = 12,
             .bit_length = 10,
             .ptr_offset = offsetof(test_struct_t, field_s16),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT32,
             .start_bit = 22,
             .bit_length = 18,
             .ptr_offset = offsetof(test_struct_t, field_u32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_INT32,
             .start_bit = 40,
             .bit_length = 14,
             .ptr_offset = offsetof(test_struct_t, field_s32),
             .behaviour = PPACK_BEHAVIOUR_RAW},
            {.type = PPACK_TYPE_UINT8,
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
            {.type = PPACK_TYPE_UINT32,
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
            {PPACK_TYPE_UINT16, 0.01f, 0.0f, 16,
             offsetof(test_struct_scaled_t, field_u16_scaled), 0.0f, 650.0f},
            /* Current -2000..2000A at 0.1A */
            {PPACK_TYPE_INT16, 0.1f, 0.0f, 16,
             offsetof(test_struct_scaled_t, field_s16_scaled), -2000.0f,
             2000.0f},
            /* Temperature -40..125C at 0.25C, offset-encoded */
            {PPACK_TYPE_UINT16, 0.25f, -40.0f, 10,
             offsetof(test_struct_scaled_t, field_u16_scaled), -40.0f, 125.0f},
            /* Energy counter 0..1e6 kWh at 0.001 */
            {PPACK_TYPE_UINT32, 0.001f, 0.0f, 32,
             offsetof(test_struct_scaled_t, field_u32_scaled), 0.0f, 1.0e6f},
            /* Power -2e6..2e6 W at 1W */
            {PPACK_TYPE_INT32, 1.0f, 0.0f, 24,
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
                    {.type = PPACK_TYPE_UINT16,
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
                    {.type = PPACK_TYPE_INT16,
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
                    {.type = PPACK_TYPE_UINT32,
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
                    {.type = PPACK_TYPE_INT32,
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

void
run_fuzz_tests(void)
{
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
}
