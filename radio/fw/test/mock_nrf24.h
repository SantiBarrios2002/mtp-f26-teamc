/* Host mock of an nRF24L01+ at the SPI-command level, for the driver tests.
 * Linked mocks form an "ether": a frame sent by one reaches every peer that is
 * powered up, in RX with CE high, on the same RF_CH, data rate and address. */
#ifndef MOCK_NRF24_H
#define MOCK_NRF24_H

#include <stdint.h>

#include "../nrf24.h"

#define MOCK_MAX_PEERS 4

typedef struct mock_nrf24 {
    uint8_t regs[32];
    uint8_t rx_addr_p0[5], tx_addr[5];
    uint8_t tx_fifo[3][32], tx_len[3], tx_n;
    uint8_t rx_fifo[3][32], rx_len[3], rx_n;
    int csn_low, ce;
    uint8_t cmd, buf[33];
    unsigned idx;
    uint32_t now_us, ce_rise_us;
    struct mock_nrf24 *peers[MOCK_MAX_PEERS];
    unsigned n_peers;
    /* knobs */
    int dead_spi, not_plus, tx_stuck, rpd, bad_width;
    /* counters */
    unsigned frames_tx, noack_writes, ack_writes, rx_dropped, short_ce_pulses;
} mock_nrf24_t;

void mock_init(mock_nrf24_t *m);
void mock_hal(mock_nrf24_t *m, nrf24_hal_t *hal);
void mock_link(mock_nrf24_t *a, mock_nrf24_t *b);
/* Deliver a frame as if sent on (rf_ch, rf_setup rate bits, address); returns 1 if accepted. */
int mock_deliver(mock_nrf24_t *m, uint8_t rf_ch, uint8_t rate_bits, const uint8_t *addr,
                 const uint8_t *data, uint8_t len);

#endif /* MOCK_NRF24_H */
