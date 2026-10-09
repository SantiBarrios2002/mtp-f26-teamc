/* MTP-F26 Team C - file compression. See codec.h. */
#include "codec.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int codec_deflate(const uint8_t *in, uint32_t n, int level, uint8_t **out, uint32_t *out_len)
{
    z_stream z;
    memset(&z, 0, sizeof z);
    if (deflateInit(&z, level) != Z_OK)
        return -1;
    uLong cap = deflateBound(&z, n);
    *out = malloc(cap);
    if (!*out) {
        deflateEnd(&z);
        return -1;
    }
    z.next_in = (Bytef *)(uintptr_t)in;
    z.avail_in = n;
    z.next_out = *out;
    z.avail_out = (uInt)cap;
    int rc = deflate(&z, Z_FINISH);
    *out_len = (uint32_t)z.total_out;
    deflateEnd(&z);
    return rc == Z_STREAM_END ? 0 : -1;
}

uint32_t codec_inflate_prefix(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t cap)
{
    z_stream z;
    memset(&z, 0, sizeof z);
    if (inflateInit(&z) != Z_OK)
        return 0;
    z.next_in = (Bytef *)(uintptr_t)in;
    z.avail_in = n;
    z.next_out = out;
    z.avail_out = cap;
    inflate(&z, Z_SYNC_FLUSH); /* stops cleanly where the received prefix ends */
    uint32_t got = (uint32_t)z.total_out;
    inflateEnd(&z);
    return got;
}

uint32_t codec_crc32(const uint8_t *data, uint32_t n)
{
    return (uint32_t)crc32(0L, data, n);
}
