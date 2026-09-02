/* sha1.c - FIPS 180-4 SHA-1 */
#include "sha1.h"
#include <stdio.h>
#include <string.h>

#define ROTL(x,n) (((x) << (n)) | ((x) >> (32 - (n))))

static void transform(PmmSha1 *c, const unsigned char block[64]) {
    unsigned int w[80], a, b, d, e, f, k, tmp;
    unsigned int cc = c->state[2]; /* avoid shadowing */
    for (int i = 0, j = 0; i < 16; i++, j += 4)
        w[i] = ((unsigned int)block[j] << 24) | ((unsigned int)block[j+1] << 16) |
               ((unsigned int)block[j+2] << 8) | (unsigned int)block[j+3];
    for (int i = 16; i < 80; i++)
        w[i] = ROTL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    a = c->state[0]; b = c->state[1]; unsigned int c2 = cc; d = c->state[3]; e = c->state[4];
    for (int i = 0; i < 80; i++) {
        if (i < 20)      { f = (b & c2) | ((~b) & d);       k = 0x5A827999; }
        else if (i < 40) { f = b ^ c2 ^ d;                  k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c2) | (b & d) | (c2 & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c2 ^ d;                  k = 0xCA62C1D6; }
        tmp = ROTL(a, 5) + f + e + k + w[i];
        e = d; d = c2; c2 = ROTL(b, 30); b = a; a = tmp;
    }
    c->state[0]+=a; c->state[1]+=b; c->state[2]+=c2; c->state[3]+=d; c->state[4]+=e;
}

void pmm_sha1_init(PmmSha1 *c) {
    c->count = 0;
    c->state[0]=0x67452301; c->state[1]=0xEFCDAB89; c->state[2]=0x98BADCFE;
    c->state[3]=0x10325476; c->state[4]=0xC3D2E1F0;
}

void pmm_sha1_update(PmmSha1 *c, const unsigned char *data, size_t len) {
    size_t i = 0, j = (size_t)(c->count & 63);
    c->count += len;
    if (j + len > 63) {
        size_t fill = 64 - j;
        memcpy(c->buffer + j, data, fill);
        transform(c, c->buffer);
        for (i = fill; i + 63 < len; i += 64)
            transform(c, data + i);
        j = 0;
    }
    memcpy(c->buffer + j, data + i, len - i);
}

void pmm_sha1_final(PmmSha1 *c, unsigned char out[20]) {
    unsigned char finalcount[8];
    unsigned long long bits = c->count * 8; /* bit length before padding */
    for (int i = 0; i < 8; i++)
        finalcount[i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xFF);
    unsigned char pad = 0x80;
    pmm_sha1_update(c, &pad, 1);
    pad = 0x00;
    while ((c->count & 63) != 56)
        pmm_sha1_update(c, &pad, 1);
    pmm_sha1_update(c, finalcount, 8);
    for (int i = 0; i < 5; i++) {
        out[i*4]   = (unsigned char)(c->state[i] >> 24);
        out[i*4+1] = (unsigned char)(c->state[i] >> 16);
        out[i*4+2] = (unsigned char)(c->state[i] >> 8);
        out[i*4+3] = (unsigned char)(c->state[i]);
    }
}

void pmm_sha1_hex(const unsigned char digest[20], char *hex) {
    static const char *d = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        hex[i*2] = d[digest[i] >> 4];
        hex[i*2+1] = d[digest[i] & 15];
    }
    hex[40] = '\0';
}

int pmm_sha1_file(const char *path, char *hex) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    PmmSha1 c;
    unsigned char buf[65536], digest[20];
    size_t n;
    pmm_sha1_init(&c);
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        pmm_sha1_update(&c, buf, n);
    fclose(f);
    pmm_sha1_final(&c, digest);
    pmm_sha1_hex(digest, hex);
    return 0;
}
