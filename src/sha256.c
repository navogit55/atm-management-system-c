#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} Sha256Ctx;

static void sha256_transform(Sha256Ctx *ctx, const uint8_t data[]) {
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    int i;

    for (i = 0; i < 16; i++) {
        W[i] = ((uint32_t) data[i * 4] << 24) |
               ((uint32_t) data[i * 4 + 1] << 16) |
               ((uint32_t) data[i * 4 + 2] << 8) |
               ((uint32_t) data[i * 4 + 3]);
    }

    for (i = 16; i < 64; i++) {
        W[i] = SIG1(W[i - 2]) + W[i - 7] + SIG0(W[i - 15]) + W[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        T1 = h + EP1(e) + CH(e, f, g) + K[i] + W[i];
        T2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(Sha256Ctx *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(Sha256Ctx *ctx, const uint8_t data[], size_t len) {
    size_t i;

    for (i = 0; i < len; i++) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(Sha256Ctx *ctx, uint8_t hash[SHA256_DIGEST_SIZE]) {
    uint64_t i;

    i = ctx->datalen;
    ctx->data[i] = 0x80;
    i++;

    if (ctx->datalen < 56) {
        memset(ctx->data + i, 0, 56 - i);
    } else {
        memset(ctx->data + i, 0, 64 - i);
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = (uint8_t) (ctx->bitlen);
    ctx->data[62] = (uint8_t) (ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t) (ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t) (ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t) (ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t) (ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t) (ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t) (ctx->bitlen >> 56);

    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 8; i++) {
        hash[i * 4]     = (uint8_t) (ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t) (ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t) (ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t) (ctx->state[i]);
    }
}

void sha256(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE]) {
    Sha256Ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

void sha256_to_hex(const uint8_t hash[SHA256_DIGEST_SIZE],
                   char hex[SHA256_HEX_SIZE]) {
    static const char digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < SHA256_DIGEST_SIZE; i++) {
        hex[i * 2]     = digits[(hash[i] >> 4) & 0xf];
        hex[i * 2 + 1] = digits[hash[i] & 0xf];
    }
    hex[SHA256_DIGEST_SIZE * 2] = '\0';
}

unsigned long long generate_salt(void) {
    unsigned long long salt = 0;
    int i;
    FILE *urandom = fopen("/dev/urandom", "rb");

    if (urandom != NULL) {
        size_t read_bytes = fread(&salt, sizeof(salt), 1, urandom);
        fclose(urandom);
        if (read_bytes == 1 && salt != 0ULL) {
            return salt;
        }
    }

    srand((unsigned int) (time(NULL) ^ (uintptr_t) &salt));
    for (i = 0; i < 4; i++) {
        salt = (salt << 16) ^ (unsigned long long) (rand() & 0xFFFF);
    }

    if (salt == 0ULL) {
        salt = 0xdeadbeefcafebabeULL;
    }

    return salt;
}
