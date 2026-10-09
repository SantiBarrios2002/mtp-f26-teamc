/* MTP-F26 Team C - discrete-event radio channel simulator. See sim.h. */
#include "sim.h"

#include <stdlib.h>
#include <string.h>

#include "rfm69.h"

enum { EV_TX_BEGIN, EV_TX_END, EV_TIMER, EV_CALL, EV_UP };

static uint64_t rng_next(sim_t *s)
{
    s->rng ^= s->rng >> 12;
    s->rng ^= s->rng << 25;
    s->rng ^= s->rng >> 27;
    return s->rng * 2685821657736338717ULL;
}

double sim_rand(sim_t *s) { return (double)(rng_next(s) >> 11) * (1.0 / 9007199254740992.0); }

/* ------------------------------------------------------------ event heap */

static int ev_less(const sim_ev_t *a, const sim_ev_t *b)
{
    return a->t < b->t || (a->t == b->t && a->seq < b->seq);
}

static void push(sim_t *s, sim_ev_t ev)
{
    if (s->heap_n == s->heap_cap) {
        s->heap_cap = s->heap_cap ? s->heap_cap * 2u : 256u;
        s->heap = realloc(s->heap, s->heap_cap * sizeof *s->heap);
    }
    ev.seq = s->seq++;
    unsigned i = s->heap_n++;
    while (i > 0) {
        unsigned p = (i - 1u) / 2u;
        if (!ev_less(&ev, &s->heap[p]))
            break;
        s->heap[i] = s->heap[p];
        i = p;
    }
    s->heap[i] = ev;
}

static sim_ev_t pop(sim_t *s)
{
    sim_ev_t top = s->heap[0], last = s->heap[--s->heap_n];
    unsigned i = 0;
    for (;;) {
        unsigned c = 2u * i + 1u;
        if (c >= s->heap_n)
            break;
        if (c + 1u < s->heap_n && ev_less(&s->heap[c + 1u], &s->heap[c]))
            c++;
        if (!ev_less(&s->heap[c], &last))
            break;
        s->heap[i] = s->heap[c];
        i = c;
    }
    if (s->heap_n)
        s->heap[i] = last;
    return top;
}

static void push_node(sim_t *s, uint64_t t, int type, int node, uint32_t gen)
{
    sim_ev_t ev = {t, 0, type, node, gen, NULL, NULL};
    push(s, ev);
}

/* --------------------------------------------------------- env callbacks */

static void schedule_begin(sim_node_t *nd, uint64_t sw)
{
    sim_t *s = nd->sim;
    nd->begin_pending = 1;
    nd->begin_switch = sw;
    nd->begin_at = s->now + sw;
    push_node(s, nd->begin_at, EV_TX_BEGIN, nd->id, 0);
}

static void env_send(void *ctx, const uint8_t *frame, uint8_t len)
{
    sim_node_t *nd = ctx;
    if (nd->down || len > FRAME_MAX)
        return;
    if (nd->qn == SIM_QCAP) {
        nd->st.q_overflow++;
        return;
    }
    unsigned slot = (nd->qh + nd->qn++) % SIM_QCAP;
    memcpy(nd->q[slot], frame, len);
    nd->qlen[slot] = len;
    if (!nd->busy && !nd->begin_pending)
        schedule_begin(nd, nd->sim->t_turn_us);
}

static uint64_t env_now(void *ctx) { return ((sim_node_t *)ctx)->sim->now; }

static void env_set_timer(void *ctx, uint64_t at)
{
    sim_node_t *nd = ctx;
    nd->timer_gen++;
    if (at != PROTO_NO_TIMER)
        push_node(nd->sim, at < nd->sim->now ? nd->sim->now : at, EV_TIMER, nd->id, nd->timer_gen);
}

static uint32_t env_rand32(void *ctx) { return (uint32_t)(rng_next(((sim_node_t *)ctx)->sim) >> 32); }

/* ------------------------------------------------------------- set-up */

void sim_init(sim_t *s, int n_nodes, const rfm69_profile_t *profile, uint64_t seed)
{
    memset(s, 0, sizeof *s);
    s->n = n_nodes;
    s->profile = profile;
    s->t_turn_us = 1000; /* RX<->TX switch incl. PLL lock and FIFO load [ASSUMPTION, as in linkbudget.py] */
    s->t_gap_us = 500;   /* between queued frames: FIFO refill + TX restart [ASSUMPTION] */
    s->rng = seed * 0x9E3779B97F4A7C15ULL + 1u;
    for (int i = 0; i < SIM_MAX_NODES; i++)
        for (int j = 0; j < SIM_MAX_NODES; j++)
            s->loss[i][j] = 1.0;
    for (int i = 0; i < n_nodes; i++) {
        sim_node_t *nd = &s->nodes[i];
        nd->sim = s;
        nd->id = i;
        nd->env.ctx = nd;
        nd->env.send = env_send;
        nd->env.now_us = env_now;
        nd->env.set_timer = env_set_timer;
        nd->env.rand32 = env_rand32;
        dc_guard_init(&nd->dc, DC_GUARD_DEFAULT_MS);
    }
}

void sim_free(sim_t *s) { free(s->heap); }

void sim_set_link(sim_t *s, int a, int b, double loss, uint8_t rssi)
{
    s->loss[a][b] = s->loss[b][a] = loss;
    s->rssi[a][b] = s->rssi[b][a] = rssi;
}

void sim_set_hooks(sim_t *s, int node, const sim_hooks_t *h) { s->nodes[node].hooks = *h; }

const proto_env_t *sim_env(sim_t *s, int node) { return &s->nodes[node].env; }

void sim_at(sim_t *s, uint64_t t_us, sim_call_fn fn, void *arg)
{
    sim_ev_t ev = {t_us, 0, EV_CALL, -1, 0, fn, arg};
    push(s, ev);
}

void sim_reboot(sim_t *s, int node, uint64_t down_us, sim_call_fn fn, void *arg)
{
    sim_node_t *nd = &s->nodes[node];
    nd->down = 1;
    nd->qn = 0;
    nd->timer_gen++;
    sim_ev_t ev = {s->now + down_us, 0, EV_UP, node, 0, fn, arg};
    push(s, ev);
}

/* --------------------------------------------------------- radio model */

static int overlap(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1) { return a0 < b1 && b0 < a1; }

static void tx_idle(sim_node_t *nd)
{
    nd->busy = 0;
    nd->tx_free_at = nd->sim->now;
    if (nd->qn)
        schedule_begin(nd, nd->sim->t_gap_us);
    else if (!nd->down && nd->hooks.on_tx_idle)
        nd->hooks.on_tx_idle(nd->hooks.proto);
}

static void tx_begin(sim_t *s, sim_node_t *nd)
{
    nd->begin_pending = 0;
    if (nd->down)
        return;
    while (nd->qn) {
        unsigned slot = nd->qh;
        nd->qh = (nd->qh + 1u) % SIM_QCAP;
        nd->qn--;
        uint8_t len = nd->qlen[slot];
        uint32_t air = rfm69_airtime_us(s->profile, len);
        uint32_t now_ms = (uint32_t)(s->now / 1000u), air_ms = (air + 999u) / 1000u;
        if (!dc_guard_allows(&nd->dc, now_ms, air_ms)) {
            nd->st.duty_refused++;
            continue;
        }
        dc_guard_add(&nd->dc, now_ms, air_ms);
        memcpy(nd->cur, nd->q[slot], len);
        nd->cur_len = len;
        nd->cur_start = s->now;
        nd->busy = 1;
        unsigned li = s->log_head++ % SIM_LOG;
        s->log[li].s = s->now;
        s->log[li].e = s->now + air;
        s->log[li].deaf_from = s->now - nd->begin_switch;
        s->log[li].deaf_to = s->now + air + s->t_turn_us;
        s->log[li].src = nd->id;
        nd->st.tx_us += air;
        nd->st.frames_tx++;
        push_node(s, s->now + air, EV_TX_END, nd->id, 0);
        return;
    }
    tx_idle(nd);
}

static void tx_end(sim_t *s, sim_node_t *nd)
{
    uint64_t t0 = nd->cur_start, t1 = s->now;
    int i = nd->id;
    for (int j = 0; j < s->n; j++) {
        sim_node_t *rx = &s->nodes[j];
        if (j == i || s->loss[i][j] >= 1.0 || rx->down)
            continue;
        int deaf = rx->begin_pending && rx->begin_at - rx->begin_switch < t1;
        int coll = 0;
        unsigned n = s->log_head < SIM_LOG ? s->log_head : SIM_LOG;
        for (unsigned k = 0; k < n; k++) {
            if (s->log[k].src == j && overlap(s->log[k].deaf_from, s->log[k].deaf_to, t0, t1))
                deaf = 1;
            else if (s->log[k].src != i && s->log[k].src != j && s->loss[s->log[k].src][j] < 1.0 &&
                     overlap(s->log[k].s, s->log[k].e, t0, t1))
                coll = 1;
        }
        if (deaf) {
            rx->st.rx_deaf++;
            continue;
        }
        if (coll) {
            rx->st.rx_collision++;
            continue;
        }
        if (s->bo_to > s->bo_from && overlap(s->bo_from, s->bo_to, t0, t1)) {
            rx->st.rx_blackout++;
            continue;
        }
        double p = s->loss[i][j];
        if (s->ge_on) {
            uint8_t *bad = &s->ge_bad[i][j];
            if (*bad ? sim_rand(s) < s->ge_p_bg : sim_rand(s) < s->ge_p_gb)
                *bad = (uint8_t)!*bad;
            if (*bad)
                p = s->ge_loss_bad;
        }
        if (sim_rand(s) < p) {
            rx->st.rx_lost++;
            continue;
        }
        rx->st.rx_ok++;
        if (rx->hooks.on_frame)
            rx->hooks.on_frame(rx->hooks.proto, nd->cur, nd->cur_len, s->rssi[i][j]);
    }
    tx_idle(nd);
}

void sim_run_until(sim_t *s, uint64_t t_us)
{
    while (s->heap_n && s->heap[0].t <= t_us) {
        sim_ev_t ev = pop(s);
        s->now = ev.t;
        sim_node_t *nd = ev.node >= 0 ? &s->nodes[ev.node] : NULL;
        switch (ev.type) {
        case EV_TX_BEGIN:
            tx_begin(s, nd);
            break;
        case EV_TX_END:
            tx_end(s, nd);
            break;
        case EV_TIMER:
            if (!nd->down && ev.gen == nd->timer_gen && nd->hooks.on_timer)
                nd->hooks.on_timer(nd->hooks.proto);
            break;
        case EV_UP:
            nd->down = 0;
            nd->busy = 0;
            if (ev.fn)
                ev.fn(s, ev.arg);
            break;
        case EV_CALL:
            ev.fn(s, ev.arg);
            break;
        }
    }
    if (s->now < t_us)
        s->now = t_us;
}
