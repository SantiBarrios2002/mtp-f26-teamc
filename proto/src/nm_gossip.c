/* MTP-F26 Team C - Network Mode gossip. See nm_gossip.h. */
#include "nm_gossip.h"

#include <string.h>

#include "crc16.h"

enum { PH_FIRE = 0, PH_END = 1 };

static uint64_t now(const nm_t *n) { return n->env->now_us(n->env->ctx); }

void nm_cfg_default(nm_cfg_t *cfg)
{
    cfg->imin_us = 1000000;
    cfg->imax_us = 8000000;
    cfg->k = 3;
}

void nm_init(nm_t *n, const proto_env_t *env, const nm_cfg_t *cfg)
{
    memset(n, 0, sizeof *n);
    n->env = env;
    n->cfg = *cfg;
    n->complete_at = PROTO_NO_TIMER;
}

static void begin_interval(nm_t *n)
{
    uint32_t half = n->interval_us / 2u;
    uint32_t t = half + (half ? n->env->rand32(n->env->ctx) % half : 0u);
    uint64_t t0 = now(n);
    n->dup_count = 0;
    n->phase = PH_FIRE;
    n->end_at = t0 + n->interval_us;
    n->env->set_timer(n->env->ctx, t0 + t);
}

static void reset_trickle(nm_t *n)
{
    if (!n->active || n->interval_us > n->cfg.imin_us) {
        n->active = 1;
        n->interval_us = n->cfg.imin_us;
        begin_interval(n);
    }
}

int nm_complete(const nm_t *n)
{
    return n->total > 0 && n->have == (n->total >= 32u ? 0xFFFFFFFFu : (1u << n->total) - 1u);
}

int nm_set_file(nm_t *n, const uint8_t *file, uint32_t len)
{
    uint32_t total = (len + NM_CHUNK - 1u) / NM_CHUNK;
    if (total == 0 || total > NM_MAX_CHUNKS)
        return -1;
    n->file_id = crc16_ccitt(file, len);
    n->total = (uint8_t)total;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t off = i * NM_CHUNK, l = len - off < NM_CHUNK ? len - off : NM_CHUNK;
        memcpy(n->data[i], file + off, l);
        n->clen[i] = (uint8_t)l;
        n->have |= 1u << i;
    }
    n->complete_at = now(n);
    reset_trickle(n);
    return 0;
}

uint32_t nm_get_file(const nm_t *n, uint8_t *out, uint32_t cap)
{
    uint32_t len = 0;
    for (uint32_t i = 0; i < n->total; i++) {
        if (!((n->have >> i) & 1u) || len + n->clen[i] > cap)
            break;
        memcpy(out + len, n->data[i], n->clen[i]);
        len += n->clen[i];
    }
    return len;
}

static void broadcast(nm_t *n)
{
    for (uint32_t i = 0; i < n->total; i++) {
        if (!((n->have >> i) & 1u))
            continue;
        uint8_t f[FRAME_MAX];
        uint32_t len = NM_HDR + n->clen[i];
        f[0] = FT_NM_DATA;
        f[1] = (uint8_t)(n->file_id >> 8);
        f[2] = (uint8_t)n->file_id;
        f[3] = (uint8_t)i;
        f[4] = n->total;
        memcpy(f + NM_HDR, n->data[i], n->clen[i]);
        uint16_t c = crc16_ccitt(f, len);
        f[len] = (uint8_t)(c >> 8);
        f[len + 1u] = (uint8_t)c;
        n->env->send(n->env->ctx, f, (uint8_t)(len + 2u));
        n->st.tx_frames++;
    }
}

void nm_on_timer(nm_t *n)
{
    if (!n->active)
        return;
    if (n->phase == PH_FIRE) {
        if (n->dup_count < (uint32_t)n->cfg.k * n->total)
            broadcast(n);
        else
            n->st.suppressed++;
        n->phase = PH_END;
        n->env->set_timer(n->env->ctx, n->end_at);
    } else {
        n->interval_us = n->interval_us * 2u > n->cfg.imax_us ? n->cfg.imax_us : n->interval_us * 2u;
        begin_interval(n);
    }
}

void nm_on_frame(nm_t *n, const uint8_t *f, uint8_t len)
{
    if (len < NM_HDR + 1u + 2u || f[0] != FT_NM_DATA)
        return;
    uint16_t c = (uint16_t)((f[len - 2u] << 8) | f[len - 1u]);
    if (crc16_ccitt(f, (size_t)len - 2u) != c) {
        n->st.rx_bad_crc++;
        return;
    }
    uint16_t fid = (uint16_t)((f[1] << 8) | f[2]);
    uint8_t idx = f[3], total = f[4];
    if (total == 0 || total > NM_MAX_CHUNKS || idx >= total)
        return;
    if (n->total == 0) {
        n->file_id = fid;
        n->total = total;
    } else if (fid != n->file_id || total != n->total) {
        n->st.rx_other_file++;
        return;
    }
    if ((n->have >> idx) & 1u) {
        n->st.rx_dup++;
        n->dup_count++;
        return;
    }
    uint32_t dlen = (uint32_t)len - NM_HDR - 2u;
    memcpy(n->data[idx], f + NM_HDR, dlen);
    n->clen[idx] = (uint8_t)dlen;
    n->have |= 1u << idx;
    n->st.rx_new++;
    if (nm_complete(n) && n->complete_at == PROTO_NO_TIMER)
        n->complete_at = now(n);
    reset_trickle(n);
}
