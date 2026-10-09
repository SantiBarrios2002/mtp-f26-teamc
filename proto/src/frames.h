/* MTP-F26 Team C - frame formats (see the protocol proposal page).
 * All multi-byte fields are big-endian. Bodies are <= 32 B (nRF24L01+ dynamic payload). */
#ifndef FRAMES_H
#define FRAMES_H

#define FRAME_MAX 32u

/* Byte 0 of SRI/MRM frames: type in bits 0-5, flags in bits 6-7 (DATA only). */
#define FT_DATA 0x01u
#define FT_ACK 0x02u
#define FT_HELLO 0x03u
#define FT_HELLO_ACK 0x04u
#define FT_END 0x05u
#define FT_TYPE_MASK 0x3Fu
#define FF_RETX 0x40u
#define FF_LAST 0x80u

/* NM frames are recognised by the exact byte 0x4E ('N'), before masking. */
#define FT_NM_DATA 0x4Eu

/* DATA:      [type|flags][session][seq:2][payload <= 28]   (16-bit seq: files up to 1.83 MB)
 * ACK:       [type][session][next_seq:2][bitmap:2][rssi]
 * HELLO:     [type][session][len:4][crc32:4][codec][name <= 21]
 * HELLO_ACK: [type][session][next_seq:2][crc32:4]
 * END:       [type][session]
 * NM_DATA:   [0x4E][file_id:2][idx][total][data <= 25][crc16:2]   (end-to-end software CRC) */
#define DATA_HDR 4u
#define DATA_PAYLOAD 28u
#define ACK_LEN 7u
#define HELLO_HDR 11u
#define HELLO_NAME_MAX 21u
#define HELLO_ACK_LEN 8u
#define END_LEN 2u
#define NM_HDR 5u
#define NM_CHUNK 25u

#define CODEC_NONE 0u
#define CODEC_DEFLATE 1u

#endif /* FRAMES_H */
