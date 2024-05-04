/**
 * @file key_exchange.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Example usage of x25519
 * @date 2022-04-06
 *
 * @copyright This example is free and unencumbered software released into the public domain.
 *
 */

#include <inttypes.h>
#include <stdio.h>

#if _WIN32
    #include <windows.h>
    #include <ntsecapi.h>
#endif

#include "x25519.h"

void xgetrandom(void *dst, size_t len)
{
#if __unix__ || __APPLE__
    FILE *random = fopen("/dev/urandom", "rb");
    if (!random) {
        exit(-1);
    }
    if (!fread(dst, len, 1, random)) {
        exit(-1);
    }
    (void)fclose(random);
#elif _WIN32
    if (!RtlGenRandom(dst, len)) {
        exit(-1);
    }
#endif
}

// ECDH private key ( {d ∈ ℕ | d < n} )
static void point_d(uint8_t *dst)
{
    xgetrandom(dst, 32);
    dst[0x00] &= 0xf8;
    dst[0x1f] &= 0x7f;
    dst[0x1f] |= 0x40;
}

// ECDH public key (Q = d * G)
static void point_q(const uint8_t *secret_key, uint8_t *public_key)
{
    const uint8_t basepoint[32] = { 9 };
    x25519(public_key, secret_key, basepoint);
}

// EDCH shared secret
static void point_kx(uint8_t *shared_key, const uint8_t *secret_key, const uint8_t *public_key)
{
    x25519(shared_key, secret_key, public_key);
}

static void print_key(const char *str, const uint8_t *key)
{
    printf("%-36s", str);
    for (size_t i = 0; i < 32; i += 4) {
        char hex[] = "---------";
        for (size_t j = 0; j < 4; j++) {
            hex[2 * j + 0] = "0123456789abcdef"[key[j + i] >> 4];
            hex[2 * j + 1] = "0123456789abcdef"[key[j + i] & 15];
        }
        if (i + 4 >= 32) {
            hex[8] = '\n';
        }
        printf("%s", hex);
    }
}

int main(void)
{
    // Our keys
    uint8_t our_secret_key[32];
    uint8_t our_public_key[32];
    // Shared secret (our side)
    uint8_t our_shared_key[32];

    // Generate our secret key and use it to generate a public key
    point_d(our_secret_key);
    point_q(our_secret_key, our_public_key);

    print_key("Our secret key is:", our_secret_key);
    print_key("and our public key is:", our_public_key);

    // The other party does the same
    uint8_t their_secret_key[32];
    uint8_t their_public_key[32];
    // Shared secret (their side)
    uint8_t their_shared_key[32];

    // Other party would keep their secret to themselves (hopefully)
    point_d(their_secret_key);
    point_q(their_secret_key, their_public_key);

    print_key("The other party's secret key is:", their_secret_key);
    print_key("and their public key is:", their_public_key);

    // We can now exchange our public keys, even if the channel is unsecure
    // On our side, we use our secret key and their public key to calculate the shared secret,
    // likewise, the other party uses their secret key and our public key
    // Note that it is good practice to hash the shared key before using it

    point_kx(our_shared_key, our_secret_key, their_public_key);
    point_kx(their_shared_key, their_secret_key, our_public_key);

    // We should now share a secret key
    print_key("We computed our shared key as:", our_shared_key);
    print_key("and they computed theirs as:", their_shared_key);

    return 0;
}
