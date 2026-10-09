/* MTP-F26 Team C - duty-cycle guard for 869.40-869.65 MHz (CNAF UN-39: <= 10 %,
 * i.e. 360 s of TX in any one hour). Tracks TX time in one-minute buckets over
 * a 61-minute window, so it never under-counts the trailing hour. Assumes
 * millis() does not wrap during a session (49 days on a 32-bit counter). */
#ifndef DC_GUARD_H
#define DC_GUARD_H

#include <stdint.h>

#define DC_GUARD_BUCKETS 61u
#define DC_GUARD_LIMIT_MS 360000u          /* 10 % of 3600 s */
#define DC_GUARD_DEFAULT_MS 342000u        /* 95 % of the limit: margin for clock error */

typedef struct {
    uint32_t budget_ms;
    uint32_t used_ms[DC_GUARD_BUCKETS];
    uint32_t minute[DC_GUARD_BUCKETS];    /* minute index each bucket currently holds */
} dc_guard_t;

void dc_guard_init(dc_guard_t *g, uint32_t budget_ms);
uint32_t dc_guard_used_ms(const dc_guard_t *g, uint32_t now_ms);
int dc_guard_allows(const dc_guard_t *g, uint32_t now_ms, uint32_t tx_ms);
void dc_guard_add(dc_guard_t *g, uint32_t now_ms, uint32_t tx_ms);

#endif /* DC_GUARD_H */
