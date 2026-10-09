/* MTP-F26 Team C - point-to-point file link. See link.h. */
#include "link.h"

#include <string.h>

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t get16(const uint8_t *p) { return ((uint32_t)p[0] << 8) | p[1]; }

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t now(const proto_env_t *e) { return e->now_us(e->ctx); }

#define TURNAROUND_US 1000u /* RX<->TX switch of the radio + MCU [ASSUMPTION, bench-measure] */
#define FRAME_GAP_US 300u   /* between frames: 130 us PLL settling + FIFO load + CE pulse [ASSUMPTION] */

/* Airtime of an nRF24 frame with `body` bytes: 1 B preamble + 5 B address + 9-bit packet
 * control field + body + 2 B CRC (same as nrf24_airtime_us in radio/fw). */
static uint32_t air_us(uint32_t body, uint32_t bitrate_bps)
{
    return (uint32_t)(((uint64_t)((1u + 5u + body + 2u) * 8u + 9u) * 1000000u) / bitrate_bps);
}

void link_cfg_for_bitrate(link_cfg_t *cfg, uint32_t bitrate_bps)
{
    uint32_t frame = air_us(FRAME_MAX, bitrate_bps) + FRAME_GAP_US;
    /* ACK anyway when 2.5 frame slots pass without a frame: never inside a burst
     * unless two frames in a row are lost. */
    cfg->idle_ack_us = frame * 5u / 2u;
    /* The sender waits for that idle ACK plus its airtime and both turnarounds. */
    cfg->ack_timeout_us = cfg->idle_ack_us + air_us(ACK_LEN, bitrate_bps) + 2u * TURNAROUND_US + 5000u;
    cfg->hello_min_us = 3u * frame > 20000u ? 3u * frame : 20000u;
    cfg->hello_max_us = 200000;
    cfg->timeouts_to_hello = 8;
    cfg->end_gap_us = frame;
}

void link_cfg_default(link_cfg_t *cfg) { link_cfg_for_bitrate(cfg, 250000u); }

/* ------------------------------------------------------------------ sender */

int link_tx_init(link_tx_t *t, const proto_env_t *env, const link_cfg_t *cfg, const uint8_t *data,
                 uint32_t len, uint32_t crc32, uint8_t codec, const char *name)
{
    memset(t, 0, sizeof *t);
    t->env = env;
    t->cfg = *cfg;
    t->data = data;
    t->len = len;
    t->crc32 = crc32;
    t->codec = codec;
    strncpy(t->name, name, HELLO_NAME_MAX);
    t->n_frames = (len + DATA_PAYLOAD - 1u) / DATA_PAYLOAD;
    t->state = LTX_IDLE;
    t->done_at = PROTO_NO_TIMER;
    return t->n_frames > 0xFFFFu ? -1 : 0;
}

static void send_hello(link_tx_t *t)
{
    uint8_t f[FRAME_MAX];
    size_t n = strlen(t->name);
    f[0] = FT_HELLO;
    f[1] = t->session;
    put32(f + 2, t->len);
    put32(f + 6, t->crc32);
    f[10] = t->codec;
    memcpy(f + HELLO_HDR, t->name, n);
    t->env->send(t->env->ctx, f, (uint8_t)(HELLO_HDR + n));
    t->st.hellos++;
    t->state = LTX_WAIT_HELLO;
    t->arm_us = t->hello_us;
}

static void send_end(link_tx_t *t)
{
    uint8_t f[END_LEN] = {FT_END, t->session};
    t->env->send(t->env->ctx, f, END_LEN);
    t->end_left--;
    t->arm_us = t->cfg.end_gap_us;
}

static void start_end(link_tx_t *t)
{
    t->state = LTX_END;
    t->end_left = 3;
    send_end(t);
}

static void send_burst(link_tx_t *t)
{
    uint32_t list[LINK_WINDOW], n = 0;
    uint32_t end = t->base + LINK_WINDOW;
    if (end > t->n_frames)
        end = t->n_frames;
    for (uint32_t s = t->base; s < end; s++) {
        uint32_t i = s - t->base;
        if (i > 0 && ((t->acked >> (i - 1u)) & 1u))
            continue;
        list[n++] = s;
    }
    for (uint32_t k = 0; k < n; k++) {
        uint8_t f[FRAME_MAX];
        uint32_t s = list[k], off = s * DATA_PAYLOAD;
        uint32_t plen = t->len - off < DATA_PAYLOAD ? t->len - off : DATA_PAYLOAD;
        int retx = s < t->next_new;
        f[0] = (uint8_t)(FT_DATA | (k == n - 1u ? FF_LAST : 0u) | (retx ? FF_RETX : 0u));
        f[1] = t->session;
        put16(f + 2, s);
        memcpy(f + DATA_HDR, t->data + off, plen);
        t->env->send(t->env->ctx, f, (uint8_t)(DATA_HDR + plen));
        t->st.frames++;
        if (retx)
            t->st.retx++;
        else
            t->next_new = s + 1u;
    }
    t->st.bursts++;
    t->state = LTX_WAIT_ACK;
    t->arm_us = t->cfg.ack_timeout_us;
}

static void advance(link_tx_t *t)
{
    if (t->base >= t->n_frames)
        start_end(t);
    else
        send_burst(t);
}

void link_tx_start(link_tx_t *t)
{
    t->session = (uint8_t)t->env->rand32(t->env->ctx);
    t->hello_us = t->cfg.hello_min_us;
    send_hello(t);
}

void link_tx_on_frame(link_tx_t *t, const uint8_t *f, uint8_t len)
{
    if (len < 2 || f[0] == FT_NM_DATA || f[1] != t->session)
        return;
    uint8_t type = f[0] & FT_TYPE_MASK;
    if (type == FT_HELLO_ACK && len >= HELLO_ACK_LEN && t->state == LTX_WAIT_HELLO) {
        if (get32(f + 4) != t->crc32)
            return;
        t->env->set_timer(t->env->ctx, PROTO_NO_TIMER);
        t->base = get16(f + 2);
        t->acked = 0;
        t->timeouts = 0;
        if (t->next_new < t->base)
            t->next_new = t->base;
        advance(t);
    } else if (type == FT_ACK && len >= ACK_LEN && t->state == LTX_WAIT_ACK) {
        uint32_t nb = get16(f + 2);
        if (nb < t->base)
            return; /* stale */
        t->env->set_timer(t->env->ctx, PROTO_NO_TIMER);
        t->base = nb;
        t->acked = (uint16_t)get16(f + 4);
        t->timeouts = 0;
        t->st.acks++;
        advance(t);
    }
}

void link_tx_on_timer(link_tx_t *t)
{
    switch (t->state) {
    case LTX_WAIT_HELLO:
        t->hello_us = t->hello_us * 2u > t->cfg.hello_max_us ? t->cfg.hello_max_us : t->hello_us * 2u;
        send_hello(t);
        break;
    case LTX_WAIT_ACK:
        t->st.timeouts++;
        if (++t->timeouts >= t->cfg.timeouts_to_hello) {
            t->timeouts = 0;
            t->hello_us = t->cfg.hello_min_us;
            send_hello(t);
        } else {
            send_burst(t);
        }
        break;
    case LTX_END:
        if (t->end_left > 0) {
            send_end(t);
        } else {
            t->state = LTX_DONE;
            t->done_at = now(t->env);
        }
        break;
    default:
        break;
    }
}

void link_tx_on_tx_idle(link_tx_t *t)
{
    if (t->arm_us) {
        t->env->set_timer(t->env->ctx, now(t->env) + t->arm_us);
        t->arm_us = 0;
    }
}

/* ---------------------------------------------------------------- receiver */

void link_rx_init(link_rx_t *r, const proto_env_t *env, const link_cfg_t *cfg, link_rx_store_t *store)
{
    memset(r, 0, sizeof *r);
    r->env = env;
    r->cfg = *cfg;
    r->store = store;
}

static void send_ack(link_rx_t *r)
{
    uint8_t f[ACK_LEN];
    uint32_t bm = 0;
    for (uint32_t i = 0; i + 1u < LINK_WINDOW; i++)
        if ((r->have >> ((r->next + 1u + i) % LINK_WINDOW)) & 1u)
            bm |= 1u << i;
    f[0] = FT_ACK;
    f[1] = r->session;
    put16(f + 2, r->next);
    put16(f + 4, bm);
    f[6] = r->last_rssi;
    r->env->set_timer(r->env->ctx, PROTO_NO_TIMER);
    r->env->send(r->env->ctx, f, ACK_LEN);
    r->st.acks++;
}

static void on_hello(link_rx_t *r, const uint8_t *f, uint8_t len)
{
    link_rx_store_t *s = r->store;
    uint32_t flen = get32(f + 2), crc = get32(f + 6);
    if (!s->valid || s->crc32 != crc || s->len != flen) {
        if (flen > s->cap)
            return;
        s->valid = 1;
        s->done = 0;
        s->len = flen;
        s->crc32 = crc;
        s->codec = f[10];
        uint32_t n = len - HELLO_HDR > HELLO_NAME_MAX ? HELLO_NAME_MAX : (uint32_t)len - HELLO_HDR;
        memcpy(s->name, f + HELLO_HDR, n);
        s->name[n] = '\0';
        s->committed = 0;
    }
    r->session = f[1];
    r->have_session = 1;
    r->next = s->committed / DATA_PAYLOAD;
    r->have = 0;
    r->st.hellos++;

    uint8_t a[HELLO_ACK_LEN];
    a[0] = FT_HELLO_ACK;
    a[1] = r->session;
    put16(a + 2, r->next);
    put32(a + 4, s->crc32);
    r->env->set_timer(r->env->ctx, PROTO_NO_TIMER);
    r->env->send(r->env->ctx, a, HELLO_ACK_LEN);
}

static void on_data(link_rx_t *r, const uint8_t *f, uint8_t len)
{
    link_rx_store_t *s = r->store;
    uint32_t seq = get16(f + 2), plen = (uint32_t)len - DATA_HDR;
    r->st.frames++;
    if (seq < r->next || seq >= r->next + LINK_WINDOW || plen > DATA_PAYLOAD) {
        r->st.dups++;
    } else {
        uint32_t slot = seq % LINK_WINDOW;
        if ((r->have >> slot) & 1u) {
            r->st.dups++;
        } else {
            memcpy(r->win[slot], f + DATA_HDR, plen);
            r->win_len[slot] = (uint8_t)plen;
            r->have |= (uint16_t)(1u << slot);
        }
        /* Commit the in-order run to the store. */
        for (;;) {
            uint32_t sl = r->next % LINK_WINDOW;
            if (!((r->have >> sl) & 1u) || s->committed + r->win_len[sl] > s->cap)
                break;
            memcpy(s->buf + s->committed, r->win[sl], r->win_len[sl]);
            s->committed += r->win_len[sl];
            r->have &= (uint16_t)~(1u << sl);
            r->next++;
        }
    }
    if (f[0] & FF_LAST)
        send_ack(r);
    else
        r->env->set_timer(r->env->ctx, now(r->env) + r->cfg.idle_ack_us);
}

void link_rx_on_frame(link_rx_t *r, const uint8_t *f, uint8_t len, uint8_t rssi)
{
    if (len < 2 || f[0] == FT_NM_DATA)
        return;
    uint8_t type = f[0] & FT_TYPE_MASK;
    r->last_rssi = rssi;
    if (type == FT_HELLO && len >= HELLO_HDR) {
        on_hello(r, f, len);
        return;
    }
    if (!r->have_session || f[1] != r->session)
        return;
    if (type == FT_DATA && len > DATA_HDR)
        on_data(r, f, len);
    else if (type == FT_END && r->store->committed == r->store->len)
        r->store->done = 1;
}

void link_rx_on_timer(link_rx_t *r)
{
    if (r->have_session)
        send_ack(r);
}
