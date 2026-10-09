/* MTP-F26 Team C - discrete-event radio channel simulator.
 *
 * Simulated time in microseconds. Each node gets a proto_env_t whose send()
 * queues frames on a half-duplex radio:
 *   - airtime from the real driver (rfm69_airtime_us with the chosen profile);
 *   - a turnaround delay before the first frame and a gap between queued frames;
 *   - a node cannot receive while it transmits or turns around (deaf);
 *   - two overlapping transmissions that a node can both hear collide;
 *   - per-link random loss, optional Gilbert-Elliott bursts, a blackout window
 *     ("attack"), node reboots;
 *   - the real duty-cycle guard (dc_guard) refuses frames over the hourly budget.
 */
#ifndef SIM_H
#define SIM_H

#include <stdint.h>

#include "dc_guard.h"
#include "frames.h"
#include "proto_env.h"
#include "rfm69_config.h"

#define SIM_MAX_NODES 16
#define SIM_QCAP 64
#define SIM_LOG 256

typedef struct sim sim_t;
typedef void (*sim_call_fn)(sim_t *s, void *arg);

typedef struct {
    void *proto;
    void (*on_frame)(void *proto, const uint8_t *f, uint8_t len, uint8_t rssi);
    void (*on_timer)(void *proto);
    void (*on_tx_idle)(void *proto);
} sim_hooks_t;

typedef struct {
    uint64_t tx_us;
    uint32_t frames_tx, duty_refused, q_overflow;
    uint32_t rx_ok, rx_lost, rx_collision, rx_deaf, rx_blackout;
} sim_node_stats_t;

typedef struct {
    sim_t *sim;
    int id;
    proto_env_t env;
    sim_hooks_t hooks;
    uint8_t q[SIM_QCAP][FRAME_MAX];
    uint8_t qlen[SIM_QCAP];
    unsigned qh, qn;
    int busy, begin_pending, down;
    uint64_t begin_at, begin_switch, tx_free_at;
    uint8_t cur[FRAME_MAX];
    uint8_t cur_len;
    uint64_t cur_start;
    uint32_t timer_gen;
    dc_guard_t dc;
    sim_node_stats_t st;
} sim_node_t;

typedef struct {
    uint64_t t, seq;
    int type, node;
    uint32_t gen;
    sim_call_fn fn;
    void *arg;
} sim_ev_t;

struct sim {
    uint64_t now, seq;
    sim_ev_t *heap;
    unsigned heap_n, heap_cap;
    sim_node_t nodes[SIM_MAX_NODES];
    int n;
    double loss[SIM_MAX_NODES][SIM_MAX_NODES];   /* >= 1.0: out of range (no interference either) */
    uint8_t rssi[SIM_MAX_NODES][SIM_MAX_NODES];
    int ge_on;
    double ge_p_gb, ge_p_bg, ge_loss_bad;
    uint8_t ge_bad[SIM_MAX_NODES][SIM_MAX_NODES];
    uint64_t bo_from, bo_to;
    const rfm69_profile_t *profile;
    uint32_t t_turn_us, t_gap_us;
    struct {
        uint64_t s, e, deaf_from, deaf_to;
        int src;
    } log[SIM_LOG];
    unsigned log_head;
    uint64_t rng;
};

void sim_init(sim_t *s, int n_nodes, const rfm69_profile_t *profile, uint64_t seed);
void sim_free(sim_t *s);
void sim_set_link(sim_t *s, int a, int b, double loss, uint8_t rssi);   /* both directions */
void sim_set_hooks(sim_t *s, int node, const sim_hooks_t *h);
const proto_env_t *sim_env(sim_t *s, int node);
void sim_at(sim_t *s, uint64_t t_us, sim_call_fn fn, void *arg);
/* Node goes down now (queue and timer lost); fn(arg) runs when it is back up. */
void sim_reboot(sim_t *s, int node, uint64_t down_us, sim_call_fn fn, void *arg);
void sim_run_until(sim_t *s, uint64_t t_us);
double sim_rand(sim_t *s);

#endif /* SIM_H */
