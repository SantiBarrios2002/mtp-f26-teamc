#include "mock_sx1231.h"

#include <string.h>

static void set_mode(mock_sx1231_t *m, uint8_t op)
{
    uint8_t prev = (m->regs[0x01] >> 2) & 7u, mode = (op >> 2) & 7u;
    m->regs[0x01] = op;
    m->regs[0x27] |= 0x80u; /* ModeReady */
    if (prev == RFM69_MODE_TX && mode != RFM69_MODE_TX)
        m->regs[0x28] &= (uint8_t)~0x08u; /* PacketSent clears on leaving TX */
    if (mode == RFM69_MODE_TX && prev != RFM69_MODE_TX) {
        memcpy(m->air, m->fifo, m->fifo_len);
        m->air_len = m->fifo_len;
        m->fifo_len = m->fifo_rd = 0;
        m->n_tx++;
        if (!m->stuck_tx)
            m->regs[0x28] |= 0x08u;
    }
}

static void m_select(void *ctx, int active)
{
    mock_sx1231_t *m = ctx;
    m->selected = active;
    m->have_addr = 0;
}

static uint8_t m_transfer(void *ctx, uint8_t out)
{
    mock_sx1231_t *m = ctx;
    if (!m->selected)
        return 0xFF;
    if (!m->have_addr) {
        m->have_addr = 1;
        m->writing = (out & 0x80u) != 0;
        m->addr = out & 0x7Fu;
        return 0;
    }
    uint8_t ret = 0;
    if (m->addr == 0x00) { /* FIFO: no auto-increment */
        if (m->writing) {
            if (m->fifo_len < sizeof m->fifo)
                m->fifo[m->fifo_len++] = out;
        } else if (m->fifo_rd < m->fifo_len) {
            ret = m->fifo[m->fifo_rd++];
            if (m->fifo_rd == m->fifo_len) {
                m->fifo_len = m->fifo_rd = 0;
                m->regs[0x28] &= (uint8_t)~0x04u; /* PayloadReady clears when FIFO empty */
            }
        }
        return ret;
    }
    if (m->writing) {
        if (m->addr == 0x01)
            set_mode(m, out);
        else if (m->addr != 0x10 && m->addr != 0x27 && m->addr != 0x28)
            m->regs[m->addr] = out;
    } else {
        ret = m->regs[m->addr];
    }
    m->addr = (uint8_t)((m->addr + 1u) & 0x7Fu);
    return ret;
}

static void m_reset(void *ctx, int high)
{
    mock_sx1231_t *m = ctx;
    if (m->reset_high && !high) {
        m->reset_pulses++;
        uint32_t now = m->now_ms;
        int stuck = m->stuck_tx;
        uint8_t ver = m->regs[0x10];
        mock_init(m);
        m->now_ms = now;
        m->stuck_tx = stuck;
        m->regs[0x10] = ver;
        m->reset_pulses = 1;
    }
    m->reset_high = high;
}

static void m_delay(void *ctx, uint32_t ms) { ((mock_sx1231_t *)ctx)->now_ms += ms; }
static uint32_t m_millis(void *ctx) { return ((mock_sx1231_t *)ctx)->now_ms; }

void mock_init(mock_sx1231_t *m)
{
    memset(m, 0, sizeof *m);
    m->regs[0x01] = 0x04;
    m->regs[0x10] = 0x24;
    m->regs[0x27] = 0x80;
}

void mock_hal(mock_sx1231_t *m, rfm69_hal_t *hal)
{
    hal->ctx = m;
    hal->select = m_select;
    hal->transfer = m_transfer;
    hal->reset = m_reset;
    hal->delay_ms = m_delay;
    hal->millis = m_millis;
}

void mock_deliver(mock_sx1231_t *m, const uint8_t *frame, unsigned n, uint8_t rssi_reg)
{
    memcpy(m->fifo, frame, n);
    m->fifo_len = n;
    m->fifo_rd = 0;
    m->regs[0x24] = rssi_reg;
    m->regs[0x28] |= 0x04u;
}
