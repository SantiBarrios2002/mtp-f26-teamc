/* MTP-F26 Team C - duty-cycle guard. See dc_guard.h. */
#include "dc_guard.h"

#define NO_MINUTE 0xFFFFFFFFu

void dc_guard_init(dc_guard_t *g, uint32_t budget_ms)
{
    g->budget_ms = budget_ms;
    for (uint32_t i = 0; i < DC_GUARD_BUCKETS; i++) {
        g->used_ms[i] = 0;
        g->minute[i] = NO_MINUTE;
    }
}

uint32_t dc_guard_used_ms(const dc_guard_t *g, uint32_t now_ms)
{
    uint32_t now_min = now_ms / 60000u, sum = 0;
    for (uint32_t i = 0; i < DC_GUARD_BUCKETS; i++)
        if (g->minute[i] != NO_MINUTE && now_min - g->minute[i] < DC_GUARD_BUCKETS)
            sum += g->used_ms[i];
    return sum;
}

int dc_guard_allows(const dc_guard_t *g, uint32_t now_ms, uint32_t tx_ms)
{
    return dc_guard_used_ms(g, now_ms) + tx_ms <= g->budget_ms;
}

void dc_guard_add(dc_guard_t *g, uint32_t now_ms, uint32_t tx_ms)
{
    uint32_t now_min = now_ms / 60000u, i = now_min % DC_GUARD_BUCKETS;
    if (g->minute[i] != now_min) {
        g->minute[i] = now_min;
        g->used_ms[i] = 0;
    }
    g->used_ms[i] += tx_ms;
}
