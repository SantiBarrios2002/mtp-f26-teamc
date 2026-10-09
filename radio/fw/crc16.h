/* MTP-F26 Team C - CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection,
 * no final XOR; check value 0x29B1 for "123456789").
 * Used end to end in NM frames (across relays), on top of the nRF24's hardware CRC,
 * which only protects one hop. */
#ifndef CRC16_H
#define CRC16_H

#include <stddef.h>
#include <stdint.h>

uint16_t crc16_ccitt(const uint8_t *data, size_t len);

#endif /* CRC16_H */
