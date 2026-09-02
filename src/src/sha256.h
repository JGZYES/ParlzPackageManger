/* sha256.h */
#ifndef PMM_SHA256_H
#define PMM_SHA256_H
#include <stddef.h>

typedef struct {
    unsigned int state[8];
    unsigned long long bitlen;
    unsigned char data[64];
    unsigned int datalen;
} PmmSha256;

void pmm_sha256_init(PmmSha256 *c);
void pmm_sha256_update(PmmSha256 *c, const unsigned char *data, size_t len);
void pmm_sha256_final(PmmSha256 *c, unsigned char out[32]);
/* Hex digest (65 bytes incl NUL). Returns 0 on success. */
int pmm_sha256_file(const char *path, char *hex);
void pmm_sha256_hex(const unsigned char digest[32], char *hex);

#endif
