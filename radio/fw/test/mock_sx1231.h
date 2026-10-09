/* Host-side SX1231 model for driver tests: register file, burst SPI with
 * auto-increment, a FIFO that does not auto-increment, ModeReady, PacketSent
 * and PayloadReady. A frame sent in TX is captured in `air`. */
#ifndef MOCK_SX1231_H
#define MOCK_SX1231_H

#include <stdint.h>

#include "../rfm69.h"

typedef struct {
    uint8_t regs[128];
    uint8_t fifo[66];
    unsigned fifo_len, fifo_rd;
    int selected, have_addr, writing;
    uint8_t addr;
    uint8_t air[66];        /* last frame transmitted: [len][body] */
    unsigned air_len, n_tx;
    int reset_high, reset_pulses;
    uint32_t now_ms;
    int stuck_tx;           /* if set, PacketSent never rises */
} mock_sx1231_t;

void mock_init(mock_sx1231_t *m);
void mock_hal(mock_sx1231_t *m, rfm69_hal_t *hal);
/* Put a received frame [len][body] in the FIFO and raise PayloadReady. */
void mock_deliver(mock_sx1231_t *m, const uint8_t *frame, unsigned n, uint8_t rssi_reg);

#endif
