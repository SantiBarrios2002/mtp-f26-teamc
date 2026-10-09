/* MTP-F26 Team C - frame formats (see the protocol proposal page).
 * All multi-byte fields are big-endian. Bodies are <= 64 B (RFM69 length byte). */
#ifndef FRAMES_H
#define FRAMES_H

#define FRAME_MAX 64u

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

/* DATA:      [type|flags][session][seq:2][payload <= 60]
 * ACK:       [type][session][next_seq:2][bitmap:2][rssi]
 * HELLO:     [type][session][len:4][crc32:4][codec][name <= 24]
 * HELLO_ACK: [type][session][next_seq:2][crc32:4]
 * END:       [type][session]
 * NM_DATA:   [0x4E][file_id:2][idx][total][data <= 57][crc16:2]   (software CRC) */
#define DATA_HDR 4u
#define DATA_PAYLOAD 60u
#define ACK_LEN 7u
#define HELLO_HDR 11u
#define HELLO_NAME_MAX 24u
#define HELLO_ACK_LEN 8u
#define END_LEN 2u
#define NM_HDR 5u
#define NM_CHUNK 57u

#define CODEC_NONE 0u
#define CODEC_DEFLATE 1u

#endif /* FRAMES_H */
