/* sha1.h */
#ifndef PMM_SHA1_H
#define PMM_SHA1_H
#include <stddef.h>

typedef struct {
    unsigned int state[5];
    unsigned long long count;
    unsigned char buffer[64];
} PmmSha1;

void pmm_sha1_init(PmmSha1 *c);
void pmm_sha1_update(PmmSha1 *c, const unsigned char *data, size_t len);
void pmm_sha1_final(PmmSha1 *c, unsigned char out[20]);
int pmm_sha1_file(const char *path, char *hex); /* 41 bytes incl NUL */
void pmm_sha1_hex(const unsigned char digest[20], char *hex);

#endif
