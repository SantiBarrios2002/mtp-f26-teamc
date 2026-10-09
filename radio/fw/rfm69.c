/* MTP-F26 Team C - portable RFM69HCW (SX1231) packet driver. See rfm69.h. */
#include "rfm69.h"

#define REG_FIFO 0x00u
#define REG_OPMODE 0x01u
#define REG_SYNCVALUE1 0x2Fu
#define REG_VERSION 0x10u
#define REG_RSSIVALUE 0x24u
#define REG_DIOMAPPING1 0x25u
#define REG_IRQFLAGS1 0x27u
#define REG_IRQFLAGS2 0x28u

#define IRQ1_MODEREADY 0x80u
#define IRQ2_PACKETSENT 0x08u
#define IRQ2_PAYLOADREADY 0x04u

#define DIO0_PACKETSENT 0x00u   /* DIO0 mapping 00 in TX */
#define DIO0_PAYLOADREADY 0x40u /* DIO0 mapping 01 in RX */

#define MODE_READY_TIMEOUT_MS 10u

uint8_t rfm69_read_reg(rfm69_t *dev, uint8_t addr)
{
    const rfm69_hal_t *h = dev->hal;
    h->select(h->ctx, 1);
    h->transfer(h->ctx, addr & 0x7Fu);
    uint8_t v = h->transfer(h->ctx, 0x00u);
    h->select(h->ctx, 0);
    return v;
}

void rfm69_write_reg(rfm69_t *dev, uint8_t addr, uint8_t value)
{
    const rfm69_hal_t *h = dev->hal;
    h->select(h->ctx, 1);
    h->transfer(h->ctx, addr | 0x80u);
    h->transfer(h->ctx, value);
    h->select(h->ctx, 0);
}

static int wait_flag(rfm69_t *dev, uint8_t reg, uint8_t mask, uint32_t timeout_ms)
{
    const rfm69_hal_t *h = dev->hal;
    uint32_t t0 = h->millis(h->ctx);
    for (;;) {
        if (rfm69_read_reg(dev, reg) & mask)
            return 1;
        if (h->millis(h->ctx) - t0 > timeout_ms)
            return 0;
        h->delay_ms(h->ctx, 1);
    }
}

uint32_t rfm69_airtime_us(const rfm69_profile_t *p, uint8_t len)
{
    uint32_t bytes = (uint32_t)p->preamble_bytes + p->sync_bytes + 1u + len + (p->hw_crc ? 2u : 0u);
    return (uint32_t)(((uint64_t)bytes * 8u * 1000000u + p->bitrate_bps / 2u) / p->bitrate_bps);
}

rfm69_status_t rfm69_set_mode(rfm69_t *dev, rfm69_mode_t mode)
{
    if (mode == RFM69_MODE_TX)
        rfm69_write_reg(dev, REG_DIOMAPPING1, DIO0_PACKETSENT);
    else if (mode == RFM69_MODE_RX)
        rfm69_write_reg(dev, REG_DIOMAPPING1, DIO0_PAYLOADREADY);

    uint8_t op = rfm69_read_reg(dev, REG_OPMODE);
    rfm69_write_reg(dev, REG_OPMODE, (uint8_t)((op & 0xE3u) | ((uint8_t)mode << 2)));
    dev->mode = mode;
    if (mode == RFM69_MODE_SLEEP)
        return RFM69_OK;
    return wait_flag(dev, REG_IRQFLAGS1, IRQ1_MODEREADY, MODE_READY_TIMEOUT_MS) ? RFM69_OK
                                                                                 : RFM69_ERR_TIMEOUT;
}

rfm69_status_t rfm69_init(rfm69_t *dev, const rfm69_hal_t *hal,
                          const rfm69_profile_t *profile, dc_guard_t *guard)
{
    dev->hal = hal;
    dev->profile = profile;
    dev->guard = guard;
    dev->last_rssi_x2 = 0;

    /* Datasheet POR/manual reset: RESET high >= 100 us, then wait >= 5 ms. */
    hal->reset(hal->ctx, 1);
    hal->delay_ms(hal->ctx, 1);
    hal->reset(hal->ctx, 0);
    hal->delay_ms(hal->ctx, 10);

    /* SPI sanity: write/read back two patterns through a scratch register. */
    static const uint8_t pattern[2] = {0xAAu, 0x55u};
    for (unsigned i = 0; i < 2; i++) {
        rfm69_write_reg(dev, REG_SYNCVALUE1, pattern[i]);
        if (rfm69_read_reg(dev, REG_SYNCVALUE1) != pattern[i])
            return RFM69_ERR_SPI;
    }
    if (rfm69_read_reg(dev, REG_VERSION) != RFM69_VERSION)
        return RFM69_ERR_VERSION;

    for (unsigned i = 0; i < profile->n_regs; i++)
        rfm69_write_reg(dev, profile->regs[i].addr, profile->regs[i].value);
    return rfm69_set_mode(dev, RFM69_MODE_STDBY);
}

rfm69_status_t rfm69_send(rfm69_t *dev, const uint8_t *body, uint8_t len)
{
    const rfm69_hal_t *h = dev->hal;
    if (len == 0 || len > dev->profile->max_len || len + 1u > RFM69_FIFO_SIZE)
        return RFM69_ERR_LEN;

    uint32_t air_us = rfm69_airtime_us(dev->profile, len);
    uint32_t air_ms = (air_us + 999u) / 1000u;
    if (dev->guard && !dc_guard_allows(dev->guard, h->millis(h->ctx), air_ms))
        return RFM69_ERR_DUTY;

    rfm69_status_t st = rfm69_set_mode(dev, RFM69_MODE_STDBY);
    if (st != RFM69_OK)
        return st;

    h->select(h->ctx, 1);
    h->transfer(h->ctx, REG_FIFO | 0x80u);
    h->transfer(h->ctx, len);
    for (uint8_t i = 0; i < len; i++)
        h->transfer(h->ctx, body[i]);
    h->select(h->ctx, 0);

    st = rfm69_set_mode(dev, RFM69_MODE_TX);
    if (st == RFM69_OK && !wait_flag(dev, REG_IRQFLAGS2, IRQ2_PACKETSENT, air_ms * 2u + 10u))
        st = RFM69_ERR_TIMEOUT;
    if (dev->guard)
        dc_guard_add(dev->guard, h->millis(h->ctx), air_ms);
    rfm69_status_t st2 = rfm69_set_mode(dev, RFM69_MODE_STDBY);
    return st != RFM69_OK ? st : st2;
}

rfm69_status_t rfm69_start_rx(rfm69_t *dev)
{
    return rfm69_set_mode(dev, RFM69_MODE_RX);
}

rfm69_status_t rfm69_poll_rx(rfm69_t *dev, uint8_t *body, uint8_t *len)
{
    const rfm69_hal_t *h = dev->hal;
    if (!(rfm69_read_reg(dev, REG_IRQFLAGS2) & IRQ2_PAYLOADREADY))
        return RFM69_ERR_NODATA;

    dev->last_rssi_x2 = (int16_t)-(int16_t)rfm69_read_reg(dev, REG_RSSIVALUE);
    rfm69_status_t st = RFM69_OK;

    /* Standby before reading the FIFO so the next frame cannot overwrite it. */
    rfm69_set_mode(dev, RFM69_MODE_STDBY);
    h->select(h->ctx, 1);
    h->transfer(h->ctx, REG_FIFO);
    uint8_t n = h->transfer(h->ctx, 0x00u);
    if (n == 0 || n > dev->profile->max_len) {
        st = RFM69_ERR_LEN;
        n = 0;
    }
    for (uint8_t i = 0; i < n; i++)
        body[i] = h->transfer(h->ctx, 0x00u);
    h->select(h->ctx, 0);
    *len = n;

    /* Re-arm RX for the next frame. */
    rfm69_status_t st2 = rfm69_start_rx(dev);
    return st != RFM69_OK ? st : st2;
}
