/*
 * @file: test_fuzz.h
 * @brief Deterministic xorshift32 PRNG and helpers for the fuzz tests.
 *
 * Each fuzz TEST_CASE seeds the generator from a constant so failures
 * are reproducible from the test name alone.
 */

#ifndef TEST_FUZZ_H_
#define TEST_FUZZ_H_

#include <stdint.h>

#define FUZZ_ITERATIONS 5000u

void fuzz_seed(uint32_t s);
uint32_t fuzz_next(void);
uint32_t fuzz_range(uint32_t n);
uint32_t fuzz_unsigned_in_bits(uint16_t bit_length);
int32_t fuzz_signed_in_bits(uint16_t bit_length);
float test_fabsf(float x);

#endif /* TEST_FUZZ_H_ */
