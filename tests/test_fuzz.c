/*
 * @file: test_fuzz.c
 * @brief PRNG + small math helpers shared by the fuzz tests.
 */

#include "test_fuzz.h"

static uint32_t fuzz_rng_state;

void
fuzz_seed(uint32_t s)
{
        fuzz_rng_state = s ? s : 1u;
}

uint32_t
fuzz_next(void)
{
        uint32_t x = fuzz_rng_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        fuzz_rng_state = x;
        return x;
}

uint32_t
fuzz_range(uint32_t n)
{
        return (n == 0u) ? 0u : (fuzz_next() % n);
}

uint32_t
fuzz_unsigned_in_bits(uint16_t bit_length)
{
        if (bit_length >= 32u) {
                return fuzz_next();
        }
        return fuzz_next() & (((uint32_t)1u << bit_length) - 1u);
}

int32_t
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

float
test_fabsf(float x)
{
        return x < 0.0f ? -x : x;
}
