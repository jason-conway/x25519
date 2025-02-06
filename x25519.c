/**
 * @file x25519.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Curve25519 / X25519 as described by Daniel Bernstein of ChaCha20 fame
 * @ref https://en.wikipedia.org/wiki/Elliptic_curve_point_multiplication
 * @ref https://en.wikipedia.org/wiki/Curve25519
 * @ref https://en.wikipedia.org/wiki/Montgomery_curve
 * @ref https://neilmadden.blog/2020/05/28/whats-the-curve25519-clamping-all-about/
 * @ref https://martin.kleppmann.com/papers/curve25519.pdf
 * @ref https://cr.yp.to/ecdh.html
 * @ref https://cr.yp.to/ecdh/curve25519-20060209.pdf
 * @version 0.9.4
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2025 Jason Conway. All rights reserved.
 *
 */

#include "x25519.h"

typedef union field_t {
    int64_t q[16];
} field_t;

// u0 + u1x + u2x^2 + ... + u9x^9 with u0/2^0, u1/2^26, u2/2^51, u3/2^77, u4/2^102, u5/2^128, u6/2^153,
// u7/2^179, u8/2^204, u9/2^230 all in {−2^25, −2^25 + 1, ..., −1, 0, 1, ..., 2^25 − 1, 2^25}
static inline void carry_reduce(field_t *dst)
{
    for (size_t i = 0;; i++) {
        // Subtract the top 48 bits from each element to yield [0, 2^16 - 1]
        int64_t carry = dst->q[i] >> 16;
        dst->q[i + 0] -= (uint64_t)carry << 16;
        if (i == 15) {
            dst->q[0] += 38 * carry; // Reduction modulo 2p
            break;
        }
        dst->q[i + 1] += carry;
    }
}

static inline void multiply(field_t *dst, const field_t *a, const field_t *b)
{
    int64_t product[31] = { 0 };
    for (size_t i = 0; i < 16; i++) {
        for (size_t j = 0; j < 16; j++) {
            product[i + j] += a->q[i] * b->q[j];
        }
    }

    // Reduce the 510-bit result mod 2^256 - 38
    for (size_t i = 0; i < 15; i++) {
        product[i] += 38 * product[i + 16];
    }

    // Upper 120 bytes aren't copied
    memcpy(dst, &product[0], 128);
    carry_reduce(dst);
    carry_reduce(dst);
}

static inline void square(field_t *dst, const field_t *src)
{
    multiply(dst, src, src);
}

// a^2i = a^i· a^i and a^2i+1 = a·a^i· ai
// Compute a^i recursively, and then square a^i to obtain a^2i
// If the exponent is odd, we additionally multiply the result with another copy of a to obtain a^2i+1
static inline void inverse(field_t *dst, const field_t *src)
{
    field_t elements = *src; // Initialize with src allows starting at bit 253

    for (size_t i = 0; i < 254; i++) {
        square(&elements, &elements);
        if (i != 251 && i != 249) {
            multiply(&elements, &elements, src);
        }
    }
    *dst = elements;
}

static inline void add(field_t *dst, const field_t *a, const field_t *b)
{
    for (size_t i = 0; i < 16; i++) {
        dst->q[i] = a->q[i] + b->q[i];
    }
}

static inline void add_inplace(field_t *restrict a, const field_t *restrict b)
{
    for (size_t i = 0; i < 16; i++) {
        a->q[i] += b->q[i];
    }
}

static inline void subtract(field_t *dst, const field_t *a, const field_t *b)
{
    for (size_t i = 0; i < 16; i++) {
        dst->q[i] = a->q[i] - b->q[i];
    }
}

static inline void subtract_inplace(field_t *restrict a, const field_t *restrict b)
{
    for (size_t i = 0; i < 16; i++) {
        a->q[i] -= b->q[i];
    }
}

// Conditionally swap the contents of a and b, must be constant time
static inline void swap(field_t *restrict a, field_t *restrict b, uint8_t bit)
{
    for (size_t i = 0; i < 16; i++) {
        const int64_t val = ~(bit - 1ul) & (a->q[i] ^ b->q[i]);
        a->q[i] ^= val;
        b->q[i] ^= val;
    }
}

// Convert to byte array
static inline void pack(uint8_t *dst, const field_t *src)
{
    field_t elements = *src;

    // Ensure all elements are in [0, 2^16 − 1]
    carry_reduce(&elements);
    carry_reduce(&elements);
    carry_reduce(&elements);

    // Ensure all elements are in [0, p − 1]
    for (size_t j = 0; j < 2; j++) {
        field_t m = { .q[0] = elements.q[0] - 0xffed }; // Subtract least-significant 16 bits of p

        for (size_t i = 1; i < 16; i++) {
            const int64_t dim = (i == 15) ? 0x7fff : 0xffff; // 0x7fff being the most-significant 16 bits of p
            m.q[i - 0] = elements.q[i] - dim - ((m.q[i - 1] >> 16) & 1ul);
            m.q[i - 1] &= 0xffff; // Put back into [0, 2^16 − 1] in case it became negative
        }

        const uint8_t carry_bit = (m.q[15] >> 16) & 1ul;
        swap(&elements, &m, 1 - carry_bit); // 1 when negative, 0 when positive
    }

    // Spit into two bytes and place in adjacent elements
    for (size_t i = 0; i < 16; i++) {
        dst[2 * i + 0] = (uint8_t)((elements.q[i] >> 0) & 0xff);
        dst[2 * i + 1] = (uint8_t)((elements.q[i] >> 8) & 0xff);
    }
}

static inline void unpack(field_t *dst, const uint8_t *src)
{
    for (size_t i = 0; i < 16; i++) {
        dst->q[i] = src[2 * i] + ((int64_t)src[2 * i + 1] << 8);
    }
    dst->q[15] &= 0x7fff;
}

void x25519(uint8_t *public, const uint8_t *secret, const uint8_t *basepoint)
{
    // Using a local copy of the secret key
    uint8_t private_key[32];
    memcpy(private_key, secret, 32);

    int64_t x[80];
    unpack((field_t *)&x[0], basepoint);

    field_t inputs[4] = { 0 }; // a, b, c, and d

    inputs[0].q[0] = 1;
    memcpy(&inputs[1], &x[0], 128);
    inputs[3].q[0] = 1;

    field_t intermediate[2]; // e and f

    static const field_t c1db41 = { .q = { 0xdb41, 0x0001 } }; // 121665

    // Infamous Montgomery ladder
    for (int64_t i = 254; i >= 0; i--) {
        const uint8_t bit = (private_key[i / 8] >> (i & 7ul)) & 1ul;

        // Swap the values in [0] with [1] and [2] with [3] when bit == 1
        swap(&inputs[0], &inputs[1], bit);
        swap(&inputs[2], &inputs[3], bit);

        // v1 = a + c
        add(&intermediate[0], &inputs[0], &inputs[2]);

        // v2 = a - c
        subtract_inplace(&inputs[0], &inputs[2]);

        // v3 = b + d
        add(&inputs[2], &inputs[1], &inputs[3]);

        // v4  = b - d
        subtract_inplace(&inputs[1], &inputs[3]);

        // v5 = v1**2 = (a + c)**2
        square(&inputs[3], &intermediate[0]);

        // v6 = v2**2 = (a - c)**2
        square(&intermediate[1], &inputs[0]);

        // v7 = v3*v2 = ab - bc + ad - cd
        multiply(&inputs[0], &inputs[2], &inputs[0]);

        // v8 = v4*v1 = ab + bc - ad - cd
        multiply(&inputs[2], &inputs[1], &intermediate[0]);

        // v9 = v7 + v8 = 2(ab - cd)
        add(&intermediate[0], &inputs[0], &inputs[2]);

        // v10 = v7 - v8 = 2(ad - bc)
        subtract_inplace(&inputs[0], &inputs[2]);

        // v11 = v10**2 = 4(ad - bc)**2
        square(&inputs[1], &inputs[0]);

        // v12 = v5 - v6 = (a + c)**2 - (a - c)**2 = 4ac
        subtract(&inputs[2], &inputs[3], &intermediate[1]);

        // v13 = 121665*v12 = 486660ac  = (A - 2)ac
        multiply(&inputs[0], &inputs[2], &c1db41);

        // v14 = v13 + v5 = a**2 + Aac + c**2
        add_inplace(&inputs[0], &inputs[3]);

        // v15 = v12*v14 = 4ac(a**2 + Aac + c**2)
        multiply(&inputs[2], &inputs[2], &inputs[0]);

        // v16 = v5*v6 = a**4 - 2a**2c**2 + c**4 = (a**2 - c**2)**2
        multiply(&inputs[0], &inputs[3], &intermediate[1]);

        // v17 = v11*x = 4x(ad - bc)**2
        multiply(&inputs[3], &inputs[1], (field_t *)&x[0]);

        // v18 = v9**2 = 4(ab - cd)**2
        square(&inputs[1], &intermediate[0]);

        swap(&inputs[0], &inputs[1], bit);
        swap(&inputs[2], &inputs[3], bit);
    }

    memcpy(&x[0x10], &inputs[0], 128);
    memcpy(&x[0x20], &inputs[2], 128);
    memcpy(&x[0x30], &inputs[1], 128);
    memcpy(&x[0x40], &inputs[3], 128);

    inverse((field_t *)&x[0x20], (field_t *)&x[0x20]);

    multiply((field_t *)&x[0x10], (field_t *)&x[0x10], (field_t *)&x[0x20]);

    pack(public, (field_t *)&x[0x10]);
}
