/* MTP-F26 Team C - portable nRF24L01+ packet driver. See nrf24.h. */
#include "nrf24.h"

#include <stddef.h>

#define CMD_R_REGISTER 0x00u
#define CMD_W_REGISTER 0x20u
#define CMD_R_RX_PAYLOAD 0x61u
#define CMD_W_TX_PAYLOAD_NOACK 0xB0u
#define CMD_FLUSH_TX 0xE1u
#define CMD_FLUSH_RX 0xE2u
#define CMD_R_RX_PL_WID 0x60u

#define REG_CONFIG 0x00u
#define REG_SETUP_AW 0x03u
#define REG_RF_CH 0x05u
#define REG_RF_SETUP 0x06u
#define REG_STATUS 0x07u
#define REG_RPD 0x09u
#define REG_RX_ADDR_P0 0x0Au
#define REG_TX_ADDR 0x10u
#define REG_FIFO_STATUS 0x17u

#define CONFIG_PRIM_RX 0x01u
#define RF_DR_LOW 0x20u
#define STATUS_RX_DR 0x40u
#define STATUS_TX_DS 0x20u
#define FIFO_RX_EMPTY 0x01u

#define POWER_ON_RESET_US 100000u /* PS v1.0: 100 ms after VDD >= 1.9 V */
#define POWER_UP_US 2000u         /* Tpd2stby 1.5 ms with an external clock margin */
#define CE_PULSE_US 15u           /* >= 10 us starts one TX */

static uint8_t command(nrf24_t *dev, uint8_t cmd, const uint8_t *out, uint8_t *in, uint8_t n)
{
    const nrf24_hal_t *h = dev->hal;
    h->select(h->ctx, 1);
    uint8_t status = h->transfer(h->ctx, cmd);
    for (uint8_t i = 0; i < n; i++) {
        uint8_t v = h->transfer(h->ctx, out ? out[i] : 0xFFu);
        if (in)
            in[i] = v;
    }
    h->select(h->ctx, 0);
    return status;
}

uint8_t nrf24_read_reg(nrf24_t *dev, uint8_t addr)
{
    uint8_t v = 0;
    command(dev, (uint8_t)(CMD_R_REGISTER | (addr & 0x1Fu)), NULL, &v, 1);
    return v;
}

void nrf24_write_reg(nrf24_t *dev, uint8_t addr, uint8_t value)
{
    command(dev, (uint8_t)(CMD_W_REGISTER | (addr & 0x1Fu)), &value, NULL, 1);
}

static void write_addr(nrf24_t *dev, uint8_t reg, const uint8_t *addr)
{
    command(dev, (uint8_t)(CMD_W_REGISTER | reg), addr, NULL, 5);
}

uint32_t nrf24_airtime_us(const nrf24_profile_t *p, uint8_t len)
{
    /* 1 B preamble + 5 B address + 9-bit packet control field + payload + CRC */
    uint32_t bits = (1u + 5u + len + p->crc_bytes) * 8u + 9u;
    return (uint32_t)(((uint64_t)bits * 1000000u + p->bitrate_bps / 2u) / p->bitrate_bps);
}

nrf24_status_t nrf24_init(nrf24_t *dev, const nrf24_hal_t *hal, const nrf24_profile_t *profile)
{
    dev->hal = hal;
    dev->profile = profile;
    dev->rf_ch = profile->rf_ch;
    dev->rx_on = 0;
    dev->last_rpd = 0;

    hal->ce(hal->ctx, 0);
    hal->delay_us(hal->ctx, POWER_ON_RESET_US);

    /* SPI sanity: SETUP_AW holds 2 bits, write/read back both patterns. */
    static const uint8_t pattern[2] = {0x01u, 0x02u};
    for (unsigned i = 0; i < 2; i++) {
        nrf24_write_reg(dev, REG_SETUP_AW, pattern[i]);
        if (nrf24_read_reg(dev, REG_SETUP_AW) != pattern[i])
            return NRF24_ERR_SPI;
    }
    /* nRF24L01+ check: the plain nRF24L01 ignores RF_DR_LOW (no 250 kbps). */
    nrf24_write_reg(dev, REG_RF_SETUP, RF_DR_LOW);
    if (!(nrf24_read_reg(dev, REG_RF_SETUP) & RF_DR_LOW))
        return NRF24_ERR_NOT_PLUS;

    write_addr(dev, REG_TX_ADDR, profile->address);
    write_addr(dev, REG_RX_ADDR_P0, profile->address);
    command(dev, CMD_FLUSH_TX, NULL, NULL, 0);
    command(dev, CMD_FLUSH_RX, NULL, NULL, 0);
    for (unsigned i = 0; i < profile->n_regs; i++)   /* CONFIG last: power up */
        nrf24_write_reg(dev, profile->regs[i].addr, profile->regs[i].value);
    hal->delay_us(hal->ctx, POWER_UP_US);
    return NRF24_OK;
}

nrf24_status_t nrf24_set_channel(nrf24_t *dev, uint8_t rf_ch)
{
    if (rf_ch > NRF24_MAX_LEGAL_CH)
        return NRF24_ERR_CH;
    nrf24_write_reg(dev, REG_RF_CH, rf_ch);
    dev->rf_ch = rf_ch;
    return NRF24_OK;
}

static void set_prim_rx(nrf24_t *dev, int rx)
{
    uint8_t c = nrf24_read_reg(dev, REG_CONFIG);
    c = rx ? (uint8_t)(c | CONFIG_PRIM_RX) : (uint8_t)(c & ~CONFIG_PRIM_RX);
    nrf24_write_reg(dev, REG_CONFIG, c);
}

nrf24_status_t nrf24_send(nrf24_t *dev, const uint8_t *body, uint8_t len)
{
    const nrf24_hal_t *h = dev->hal;
    if (len == 0 || len > dev->profile->max_len || len > NRF24_MAX_PAYLOAD)
        return NRF24_ERR_LEN;

    h->ce(h->ctx, 0);
    dev->rx_on = 0;
    set_prim_rx(dev, 0);
    command(dev, CMD_FLUSH_TX, NULL, NULL, 0);
    command(dev, CMD_W_TX_PAYLOAD_NOACK, body, NULL, len);
    h->ce(h->ctx, 1);
    h->delay_us(h->ctx, CE_PULSE_US);
    h->ce(h->ctx, 0);

    uint32_t limit = NRF24_TX_SETTLE_US + nrf24_airtime_us(dev->profile, len) * 2u + 1000u;
    uint32_t t0 = h->micros(h->ctx);
    nrf24_status_t st = NRF24_ERR_TIMEOUT;
    for (;;) {
        if (command(dev, 0xFFu, NULL, NULL, 0) & STATUS_TX_DS) {   /* NOP returns STATUS */
            st = NRF24_OK;
            break;
        }
        if (h->micros(h->ctx) - t0 > limit)
            break;
        h->delay_us(h->ctx, 50);
    }
    nrf24_write_reg(dev, REG_STATUS, STATUS_TX_DS);
    if (st != NRF24_OK)
        command(dev, CMD_FLUSH_TX, NULL, NULL, 0);
    return st;
}

nrf24_status_t nrf24_start_rx(nrf24_t *dev)
{
    const nrf24_hal_t *h = dev->hal;
    set_prim_rx(dev, 1);
    nrf24_write_reg(dev, REG_STATUS, STATUS_RX_DR);
    h->ce(h->ctx, 1);
    h->delay_us(h->ctx, NRF24_TX_SETTLE_US);
    dev->rx_on = 1;
    return NRF24_OK;
}

nrf24_status_t nrf24_poll_rx(nrf24_t *dev, uint8_t *body, uint8_t *len)
{
    if (nrf24_read_reg(dev, REG_FIFO_STATUS) & FIFO_RX_EMPTY)
        return NRF24_ERR_NODATA;

    uint8_t n = 0;
    command(dev, CMD_R_RX_PL_WID, NULL, &n, 1);
    if (n == 0 || n > NRF24_MAX_PAYLOAD) {
        /* PS v1.0: a width > 32 means a corrupt frame; flush the RX FIFO. */
        command(dev, CMD_FLUSH_RX, NULL, NULL, 0);
        nrf24_write_reg(dev, REG_STATUS, STATUS_RX_DR);
        *len = 0;
        return NRF24_ERR_LEN;
    }
    command(dev, CMD_R_RX_PAYLOAD, NULL, body, n);
    *len = n;
    dev->last_rpd = nrf24_read_reg(dev, REG_RPD) & 1u;
    if (nrf24_read_reg(dev, REG_FIFO_STATUS) & FIFO_RX_EMPTY)
        nrf24_write_reg(dev, REG_STATUS, STATUS_RX_DR);
    return NRF24_OK;
}

int nrf24_carrier(nrf24_t *dev)
{
    return nrf24_read_reg(dev, REG_RPD) & 1u;
}
