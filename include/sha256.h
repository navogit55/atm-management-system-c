#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE 32
#define SHA256_HEX_SIZE 65

void sha256(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE]);
void sha256_to_hex(const uint8_t hash[SHA256_DIGEST_SIZE],
                   char hex[SHA256_HEX_SIZE]);

unsigned long long generate_salt(void);

#endif
