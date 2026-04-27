/*
 * @file: test_partial_coverage.c
 * @brief Verify struct-member survival semantics when fields only partially
 *        cover a multi-byte destination member.
 *
 * Tests the README contract: "Existing bytes of struct members not covered by
 * any field are left untouched."
 */

#include "ppack.h"
#include "test_harness.h"
#include <ppack_platform.h>
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Test 1: struct member with NO field targeting it must survive unpack       */
/* -------------------------------------------------------------------------- */

typedef struct {
        uint32_t field_a; /* Covered by an INT16 field (writes 2 bytes)    */
        uint32_t field_b; /* No field targets this member at all            */
} test_no_cover_t;

TEST_CASE(test_unpack_member_not_targeted_survives)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};

        /* Pack only into bits 0-15 (INT16 field). field_b is never touched */
        const int16_t src_val = -42;

        const struct ppack_field fields[] = {
            {.type       = PPACK_TYPE_INT16,
             .start_bit  = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_no_cover_t, field_a),
             .behaviour  = PPACK_BEHAVIOUR_RAW},
        };

        test_no_cover_t src = {
            .field_a = (uint32_t)(uint16_t)src_val, /* raw bit pattern   */
            .field_b = 0xDEADBEEFu,                  /* sentinel value    */
        };

        int ret = ppack_pack(&src, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_no_cover_t dst = {
            .field_a = 0xAAAAAAAAu, /* Will be overwritten                 */
            .field_b = 0xBEEFCAFEu, /* Must survive unpack unchanged       */
        };

        int unpack_ret = ppack_unpack(&dst, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        /* field_a: lower 16 bits contain sign-extended decoded value.
         * -42 as int16 has bit pattern 0xFFD6. Upper 16 bits of field_a
         * are NOT overwritten by the INT16 memcpy (writes only 2 bytes).*/
        uint16_t lower_a = (uint16_t)(dst.field_a & 0xFFFFu);
        TEST_ASSERT(lower_a == (uint16_t)src_val); /* 0xFFD6              */

        /* field_b was NOT targeted by any field — must survive untouched */
        TEST_ASSERT(dst.field_b == 0xBEEFCAFEu);
}

/* -------------------------------------------------------------------------- */
/* Test 2: INT16 field targeting lower half of uint32_t; upper half survives */
/* -------------------------------------------------------------------------- */

typedef struct {
        uint32_t field_u32; /* INT16 targets offset+0..offset+1 (2 bytes)  */
} test_int16_into_u32_t;

TEST_CASE(test_unpack_int16_into_uint32_upper_bytes_survive)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};

        const int16_t src_val = -42;

        const struct ppack_field fields[] = {
            {.type       = PPACK_TYPE_INT16,
             .start_bit  = 0,
             .bit_length = 16,
             .ptr_offset = offsetof(test_int16_into_u32_t, field_u32),
             .behaviour  = PPACK_BEHAVIOUR_RAW},
        };

        test_int16_into_u32_t src = {
            .field_u32 = (uint32_t)(uint16_t)src_val,
        };

        int ret = ppack_pack(&src, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        /* Pre-fill with known value: upper 16 bits = 0xBEEF, lower = 0xCAFE */
        test_int16_into_u32_t dst = {.field_u32 = 0xBEEFCAFEu};

        int unpack_ret = ppack_unpack(&dst, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        /* INT16 unpack writes sizeof(int16_t) == 2 bytes at offset+0.
         * Lower 16 bits must contain the decoded value; upper 16 bits
         * (bytes 2-3 of field_u32) should survive unchanged because no
         * field targets them.                                         */
        uint16_t lower = (uint16_t)(dst.field_u32 & 0xFFFFu);
        uint16_t upper = (uint16_t)((dst.field_u32 >> 16) & 0xFFFFu);

        /* Lower half: decoded INT16 value (-42 == 0xFFD6 as uint16) */
        TEST_ASSERT(lower == (uint16_t)src_val);

        /* Upper half: untouched by any field — must survive          */
        TEST_ASSERT(upper == 0xBEEFu);
}

/* -------------------------------------------------------------------------- */
/* Test 3: UINT8 partial coverage of uint16_t on native (8-bit MAU).         */
/* On native, sizeof(ppack_u8_t) == 1 so only the low byte is written.       */
/* The upper byte must survive unchanged.                                    */
/* On simulated 16-bit MAU, sizeof(ppack_u8_t) == 2 and both bytes are      */
/* overwritten — that is documented contract behaviour for C2000 targets.    */
/* -------------------------------------------------------------------------- */

typedef struct {
        uint16_t field_u16; /* UINT8 targets offset+0 only (1 byte native) */
} test_u8_into_u16_t;

TEST_CASE(test_unpack_uint8_into_uint16_upper_byte_survives_native)
{
#if PPACK_ADDR_UNIT_BITS == 8
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};

        const uint8_t src_val = 0x5Au;

        const struct ppack_field fields[] = {
            {.type       = PPACK_TYPE_UINT8,
             .start_bit  = 0,
             .bit_length = 8,
             .ptr_offset = offsetof(test_u8_into_u16_t, field_u16),
             .behaviour  = PPACK_BEHAVIOUR_RAW},
        };

        test_u8_into_u16_t src = {
            .field_u16 = (uint16_t)src_val,
        };

        int ret = ppack_pack(&src, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        /* Pre-fill: upper byte = 0xDE, lower byte = 0xAD */
        test_u8_into_u16_t dst = {.field_u16 = 0xDEADu};

        int unpack_ret = ppack_unpack(&dst, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        /* On native (8-bit MAU), UINT8 unpack writes sizeof(ppack_u8_t) == 1
         * byte at offset+0. The low byte must be the decoded value; the
         * high byte (offset+1) should survive untouched because no field
         * targets it.                                                  */
        uint8_t lo = (uint8_t)(dst.field_u16 & 0xFFu);
        uint8_t hi = (uint8_t)((dst.field_u16 >> 8) & 0xFFu);

        TEST_ASSERT(lo == src_val);
        TEST_ASSERT(hi == 0xDEu);
#else
        /* On simulated 16-bit MAU, sizeof(ppack_u8_t) == 2 so both bytes are
         * overwritten. This is documented contract behaviour for C2000 where
         * uint8_t aliases to uint16_t. No assertion needed — the unpack
         * writes sizeof(ppack_u8_t) == 2 and clears upper bits per design.   */
#endif
}

/* -------------------------------------------------------------------------- */
/* Test 4: UINT32/INT32 raw field targeting uint32_t member — full coverage   */
/* All 4 bytes are overwritten. No survival expected.                         */
/* -------------------------------------------------------------------------- */

typedef struct {
        uint32_t field_u32; /* UINT32 targets all 4 bytes                   */
} test_u32_full_cover_t;

TEST_CASE(test_unpack_uint32_overwrites_all_bytes)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};

        const uint32_t src_val = 0x12345678u;

        const struct ppack_field fields[] = {
            {.type       = PPACK_TYPE_UINT32,
             .start_bit  = 0,
             .bit_length = 32,
             .ptr_offset = offsetof(test_u32_full_cover_t, field_u32),
             .behaviour  = PPACK_BEHAVIOUR_RAW},
        };

        test_u32_full_cover_t src = {
            .field_u32 = src_val,
        };

        int ret = ppack_pack(&src, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        test_u32_full_cover_t dst = {.field_u32 = 0xDEADBEEFu};

        int unpack_ret = ppack_unpack(&dst, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        /* UINT32 unpack writes sizeof(uint32_t) == 4 bytes. Full overwrite */
        TEST_ASSERT(dst.field_u32 == src_val);
}

/* -------------------------------------------------------------------------- */
/* Test 5: Narrow bit_length (8 bits) on UINT16 type — still writes full     */
/* uint16_t (2 bytes), not just the narrow portion.                          */
/* -------------------------------------------------------------------------- */

typedef struct {
        uint32_t field_u32; /* UINT16 with bit_length=8 targets offset+0..1  */
} test_narrow_bitlen_t;

TEST_CASE(test_unpack_uint16_narrow_still_writes_full_type)
{
        ppack_byte_t payload[PPACK_PAYLOAD_UNITS] = {0};

        const uint16_t src_val = 0x42u; /* Only low 8 bits meaningful       */

        const struct ppack_field fields[] = {
            {.type       = PPACK_TYPE_UINT16,
             .start_bit  = 0,
             .bit_length = 8,  /* Narrow: only 8 bits on wire              */
             .ptr_offset = offsetof(test_narrow_bitlen_t, field_u32),
             .behaviour  = PPACK_BEHAVIOUR_RAW},
        };

        test_narrow_bitlen_t src = {
            .field_u32 = (uint32_t)src_val,
        };

        int ret = ppack_pack(&src, payload, fields, 1);
        TEST_ASSERT(ret == PPACK_SUCCESS);

        /* Pre-fill: upper 16 bits = 0xBEEF, lower 16 bits = 0xCAFE */
        test_narrow_bitlen_t dst = {.field_u32 = 0xBEEFCAFEu};

        int unpack_ret = ppack_unpack(&dst, payload, fields, 1);
        TEST_ASSERT(unpack_ret == PPACK_SUCCESS);

        /* UINT16 unpack writes sizeof(uint16_t) == 2 bytes at offset+0.
         * Even though bit_length=8 (only 8 bits on wire), the full uint16_t
         * is written to the destination. Lower 16 bits contain the decoded
         * value; upper 16 bits survive untouched.                        */
        uint16_t lower = (uint16_t)(dst.field_u32 & 0xFFFFu);
        uint16_t upper = (uint16_t)((dst.field_u32 >> 16) & 0xFFFFu);

        /* Only 8 bits came from payload, upper byte of the uint16 is zeroed
         * by the cast (uint16_t)raw where raw has only 8 valid bits.     */
        TEST_ASSERT(lower == (uint16_t)src_val);

        /* Upper half: untouched — no field targets bytes 2-3             */
        TEST_ASSERT(upper == 0xBEEFu);
}

/* -------------------------------------------------------------------------- */
/* Test runner entry point                                                    */
/* -------------------------------------------------------------------------- */

void
run_partial_coverage_tests(void)
{
        run_test(test_unpack_member_not_targeted_survives,
                 "test_unpack_member_not_targeted_survives");
        run_test(test_unpack_int16_into_uint32_upper_bytes_survive,
                 "test_unpack_int16_into_uint32_upper_bytes_survive");
        run_test(test_unpack_uint8_into_uint16_upper_byte_survives_native,
                 "test_unpack_uint8_into_uint16_upper_byte_survives_native");
        run_test(test_unpack_uint32_overwrites_all_bytes,
                 "test_unpack_uint32_overwrites_all_bytes");
        run_test(test_unpack_uint16_narrow_still_writes_full_type,
                 "test_unpack_uint16_narrow_still_writes_full_type");
}
