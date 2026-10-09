/* MTP-F26 Team C - point-to-point file link for SRI and MRM.
 *
 * Selective-repeat ARQ: the sender transmits bursts of up to LINK_WINDOW frames
 * (missing ones first, then new ones); the receiver answers with one ACK holding
 * next_seq (everything before it has arrived) and a bitmap of the frames after it.
 * Delivery is strictly in order, because only the first unbroken run of correct
 * lines scores. A HELLO handshake names the file (length + CRC-32), so either side
 * can reboot or lose the link and resume where the receiver stopped.
 */
#ifndef LINK_H
#define LINK_H

#include <stdint.h>

#include "frames.h"
#include "proto_env.h"

#define LINK_WINDOW 16u

typedef struct {
    uint32_t ack_timeout_us;     /* after the burst ends, wait this long for an ACK */
    uint32_t hello_min_us;       /* HELLO retry interval, doubling up to hello_max_us */
    uint32_t hello_max_us;
    uint16_t timeouts_to_hello;  /* consecutive ACK timeouts before re-sending HELLO */
    uint32_t idle_ack_us;        /* receiver: ACK anyway after this long without frames */
    uint32_t end_gap_us;
} link_cfg_t;

/* Timeouts derived from the air bit rate, so a profile change (e.g. 1 Mbps for SRI)
 * keeps the receiver from ACKing in the middle of a burst. */
void link_cfg_for_bitrate(link_cfg_t *cfg, uint32_t bitrate_bps);
void link_cfg_default(link_cfg_t *cfg); /* = link_cfg_for_bitrate(cfg, 250000) */

typedef struct {
    uint32_t frames, retx, bursts, acks, timeouts, hellos;
} link_tx_stats_t;

typedef enum { LTX_IDLE, LTX_WAIT_HELLO, LTX_WAIT_ACK, LTX_END, LTX_DONE } link_tx_state_t;

typedef struct {
    const proto_env_t *env;
    link_cfg_t cfg;
    const uint8_t *data;
    uint32_t len, crc32;
    uint8_t codec;
    char name[HELLO_NAME_MAX + 1];
    uint8_t session;
    link_tx_state_t state;
    uint32_t n_frames;
    uint32_t base;          /* receiver's next_seq: every frame < base is acknowledged */
    uint16_t acked;         /* bit i: frame base+1+i acknowledged */
    uint32_t next_new;      /* first frame never sent */
    uint16_t timeouts;
    uint32_t hello_us;
    uint32_t arm_us;        /* timer to arm when the radio queue drains */
    uint8_t end_left;
    uint64_t done_at;
    link_tx_stats_t st;
} link_tx_t;

/* Returns 0, or -1 if the file needs more than 65 535 frames. */
int link_tx_init(link_tx_t *t, const proto_env_t *env, const link_cfg_t *cfg, const uint8_t *data,
                 uint32_t len, uint32_t crc32, uint8_t codec, const char *name);
void link_tx_start(link_tx_t *t);
void link_tx_on_frame(link_tx_t *t, const uint8_t *f, uint8_t len);
void link_tx_on_timer(link_tx_t *t);
void link_tx_on_tx_idle(link_tx_t *t);

/* Receiver state that must survive a reboot (kept in flash on the Pico). */
typedef struct {
    uint8_t valid, done, codec;
    uint32_t len, crc32;
    char name[HELLO_NAME_MAX + 1];
    uint32_t committed;     /* bytes received in order; frames are DATA_PAYLOAD each */
    uint8_t *buf;
    uint32_t cap;
} link_rx_store_t;

typedef struct {
    uint32_t frames, dups, acks, hellos;
} link_rx_stats_t;

typedef struct {
    const proto_env_t *env;
    link_cfg_t cfg;
    link_rx_store_t *store;
    uint8_t have_session, session;
    uint32_t next;
    uint16_t have;          /* bit (seq % LINK_WINDOW): frame buffered */
    uint8_t win[LINK_WINDOW][DATA_PAYLOAD];
    uint8_t win_len[LINK_WINDOW];
    uint8_t last_rssi;
    link_rx_stats_t st;
} link_rx_t;

/* Initialise (also after a reboot: the store is kept, everything else is lost). */
void link_rx_init(link_rx_t *r, const proto_env_t *env, const link_cfg_t *cfg, link_rx_store_t *store);
void link_rx_on_frame(link_rx_t *r, const uint8_t *f, uint8_t len, uint8_t rssi);
void link_rx_on_timer(link_rx_t *r);

#endif /* LINK_H */
