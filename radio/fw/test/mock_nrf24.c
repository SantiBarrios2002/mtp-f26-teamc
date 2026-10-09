/* Host mock of an nRF24L01+. See mock_nrf24.h. */
#include "mock_nrf24.h"

#include <string.h>

#define RATE_MASK 0x28u

void mock_init(mock_nrf24_t *m)
{
    memset(m, 0, sizeof *m);
    m->regs[0x00] = 0x08; /* CONFIG reset value: EN_CRC */
    m->regs[0x03] = 0x03;
    m->regs[0x05] = 0x02;
    m->regs[0x06] = 0x0E;
    memset(m->rx_addr_p0, 0xE7, 5);
    memset(m->tx_addr, 0xE7, 5);
}

void mock_link(mock_nrf24_t *a, mock_nrf24_t *b)
{
    a->peers[a->n_peers++] = b;
    b->peers[b->n_peers++] = a;
}

static int powered(const mock_nrf24_t *m) { return m->regs[0x00] & 0x02; }
static int prim_rx(const mock_nrf24_t *m) { return m->regs[0x00] & 0x01; }

static uint8_t status(const mock_nrf24_t *m)
{
    uint8_t s = m->regs[0x07] & 0x70u;
    s |= m->rx_n ? 0x00u : 0x0Eu;            /* RX_P_NO: pipe 0 or empty (111) */
    s |= m->tx_n == 3 ? 0x01u : 0x00u;
    return s;
}

static uint8_t fifo_status(const mock_nrf24_t *m)
{
    return (uint8_t)((m->rx_n == 0 ? 0x01u : 0) | (m->rx_n == 3 ? 0x02u : 0) |
                     (m->tx_n == 0 ? 0x10u : 0) | (m->tx_n == 3 ? 0x20u : 0));
}

int mock_deliver(mock_nrf24_t *m, uint8_t rf_ch, uint8_t rate_bits, const uint8_t *addr,
                 const uint8_t *data, uint8_t len)
{
    if (!powered(m) || !prim_rx(m) || !m->ce)
        return 0;
    if (m->regs[0x05] != rf_ch || (m->regs[0x06] & RATE_MASK) != rate_bits)
        return 0;
    if (!(m->regs[0x02] & 1u) || memcmp(m->rx_addr_p0, addr, 5) != 0)
        return 0;
    int dpl = (m->regs[0x1D] & 0x04u) && (m->regs[0x1C] & 0x01u) && (m->regs[0x01] & 0x01u);
    if (!dpl && len != m->regs[0x11])
        return 0;
    if (m->rx_n == 3) {
        m->rx_dropped++;
        return 0;
    }
    memcpy(m->rx_fifo[m->rx_n], data, len);
    m->rx_len[m->rx_n++] = len;
    m->regs[0x07] |= 0x40u; /* RX_DR */
    return 1;
}

static void transmit_head(mock_nrf24_t *m)
{
    if (!m->tx_n || m->tx_stuck)
        return;
    for (unsigned p = 0; p < m->n_peers; p++)
        mock_deliver(m->peers[p], m->regs[0x05], m->regs[0x06] & RATE_MASK, m->tx_addr, m->tx_fifo[0],
                     m->tx_len[0]);
    m->frames_tx++;
    memmove(m->tx_fifo[0], m->tx_fifo[1], 2 * 32);
    memmove(m->tx_len, m->tx_len + 1, 2);
    m->tx_n--;
    m->regs[0x07] |= 0x20u; /* TX_DS */
}

static void m_select(void *ctx, int active)
{
    mock_nrf24_t *m = ctx;
    if (active) {
        m->csn_low = 1;
        m->idx = 0;
        return;
    }
    m->csn_low = 0;
    if (m->dead_spi || m->idx == 0)
        return;
    unsigned n = m->idx - 1;
    uint8_t c = m->cmd;
    if ((c & 0xE0u) == 0x20u && n >= 1) {          /* W_REGISTER */
        uint8_t reg = c & 0x1Fu;
        if (reg == 0x0A && n >= 5)
            memcpy(m->rx_addr_p0, m->buf, 5);
        else if (reg == 0x10 && n >= 5)
            memcpy(m->tx_addr, m->buf, 5);
        else if (reg == 0x07)
            m->regs[0x07] &= (uint8_t)~(m->buf[0] & 0x70u);
        else if (reg == 0x03)
            m->regs[0x03] = m->buf[0] & 0x03u;
        else if (reg == 0x06)
            m->regs[0x06] = m->not_plus ? (uint8_t)(m->buf[0] & ~0x20u) : m->buf[0];
        else
            m->regs[reg] = m->buf[0];
    } else if ((c == 0xA0u || c == 0xB0u) && n >= 1 && m->tx_n < 3) {
        memcpy(m->tx_fifo[m->tx_n], m->buf, n);
        m->tx_len[m->tx_n++] = (uint8_t)n;
        if (c == 0xB0u)
            m->noack_writes++;
        else
            m->ack_writes++;
    } else if (c == 0x61u && m->rx_n) {            /* R_RX_PAYLOAD pops the head */
        memmove(m->rx_fifo[0], m->rx_fifo[1], 2 * 32);
        memmove(m->rx_len, m->rx_len + 1, 2);
        m->rx_n--;
    } else if (c == 0xE1u) {
        m->tx_n = 0;
    } else if (c == 0xE2u) {
        m->rx_n = 0;
    }
}

static uint8_t m_transfer(void *ctx, uint8_t out)
{
    mock_nrf24_t *m = ctx;
    if (m->dead_spi || !m->csn_low)
        return 0x00u;
    if (m->idx == 0) {
        m->cmd = out;
        m->idx = 1;
        return status(m);
    }
    unsigned k = m->idx - 1;
    m->idx++;
    if (k < sizeof m->buf)
        m->buf[k] = out;
    uint8_t c = m->cmd;
    if ((c & 0xE0u) == 0x00u) {                    /* R_REGISTER */
        uint8_t reg = c & 0x1Fu;
        if (reg == 0x0A)
            return k < 5 ? m->rx_addr_p0[k] : 0;
        if (reg == 0x10)
            return k < 5 ? m->tx_addr[k] : 0;
        if (reg == 0x07)
            return status(m);
        if (reg == 0x09)
            return (uint8_t)(m->rpd ? 1 : 0);
        if (reg == 0x17)
            return fifo_status(m);
        return m->regs[reg];
    }
    if (c == 0x60u)
        return m->bad_width ? 40u : (m->rx_n ? m->rx_len[0] : 0u);
    if (c == 0x61u)
        return m->rx_n && k < m->rx_len[0] ? m->rx_fifo[0][k] : 0u;
    return 0x00u;
}

static void m_ce(void *ctx, int high)
{
    mock_nrf24_t *m = ctx;
    if (high && !m->ce) {
        m->ce_rise_us = m->now_us;
    } else if (!high && m->ce && powered(m) && !prim_rx(m)) {
        if (m->now_us - m->ce_rise_us >= 10u)
            transmit_head(m);
        else
            m->short_ce_pulses++;
    }
    m->ce = high;
}

static void m_delay_us(void *ctx, uint32_t us) { ((mock_nrf24_t *)ctx)->now_us += us; }
static uint32_t m_micros(void *ctx) { return ((mock_nrf24_t *)ctx)->now_us; }

void mock_hal(mock_nrf24_t *m, nrf24_hal_t *hal)
{
    hal->ctx = m;
    hal->select = m_select;
    hal->transfer = m_transfer;
    hal->ce = m_ce;
    hal->delay_us = m_delay_us;
    hal->micros = m_micros;
}
