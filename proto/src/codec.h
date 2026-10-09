/* MTP-F26 Team C - file compression, zlib API.
 * The PC simulator links the system zlib; on the Pico, miniz provides the same
 * deflate/inflate API (zlib-compatible names), so this file is meant to port as is.
 * The receiver stores the compressed stream and inflates it after STOP. */
#ifndef CODEC_H
#define CODEC_H

#include <stdint.h>

/* Compress in -> *out (malloc'd). Returns 0 on success. */
int codec_deflate(const uint8_t *in, uint32_t n, int level, uint8_t **out, uint32_t *out_len);
/* Inflate as much of a (possibly truncated) stream as possible. Returns bytes written. */
uint32_t codec_inflate_prefix(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t cap);
uint32_t codec_crc32(const uint8_t *data, uint32_t n);

#endif /* CODEC_H */
