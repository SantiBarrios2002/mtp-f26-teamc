/* MTP-F26 Team C - CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection,
 * no final XOR; check value 0x29B1 for "123456789").
 * Used in the NM common PHY, where hardware CRC is off because vendors differ
 * (SX1231 uses the CCITT polynomial, CC1101 uses x^16+x^15+x^2+1). */
#ifndef CRC16_H
#define CRC16_H

#include <stddef.h>
#include <stdint.h>

uint16_t crc16_ccitt(const uint8_t *data, size_t len);

#endif /* CRC16_H */
