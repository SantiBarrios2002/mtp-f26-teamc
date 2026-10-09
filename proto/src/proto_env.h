/* MTP-F26 Team C - platform interface for the protocol modules.
 *
 * The protocol code (link.c, nm_gossip.c) is pure logic: it never touches a radio,
 * a clock or an RTOS directly. Each platform fills one proto_env_t:
 *   - the PC simulator (proto/sim) with a channel model and simulated time;
 *   - the Pico firmware with rfm69_send(), time_us_64() and a hardware alarm.
 * The platform then calls the module's on_frame / on_timer / on_tx_idle handlers.
 */
#ifndef PROTO_ENV_H
#define PROTO_ENV_H

#include <stdint.h>

#define PROTO_NO_TIMER UINT64_MAX

typedef struct proto_env {
    void *ctx;
    /* Queue one frame (<= 64 B body) for transmission; frames go out in order. */
    void (*send)(void *ctx, const uint8_t *frame, uint8_t len);
    uint64_t (*now_us)(void *ctx);
    /* One timer per protocol instance; a new call replaces the previous one.
     * PROTO_NO_TIMER cancels it. */
    void (*set_timer)(void *ctx, uint64_t at_us);
    uint32_t (*rand32)(void *ctx);
} proto_env_t;

#endif /* PROTO_ENV_H */
