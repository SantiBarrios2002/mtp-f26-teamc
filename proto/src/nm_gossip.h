/* MTP-F26 Team C - Network Mode gossip (draft inter-team standard).
 *
 * No routing, addresses or positions: every node rebroadcasts all chunks of the
 * NM file it holds, on a Trickle timer (RFC 6206): interval I from imin doubling
 * to imax, one transmission point at a random time in [I/2, I), suppressed when
 * enough duplicate frames were heard in the interval. Hearing a new chunk resets
 * I to imin, so the file spreads fast and the channel goes quiet once everyone
 * has it. Frames: [0x4E][file_id:2][idx][total][data <= 25][crc16:2].
 */
#ifndef NM_GOSSIP_H
#define NM_GOSSIP_H

#include <stdint.h>

#include "frames.h"
#include "proto_env.h"

#define NM_MAX_CHUNKS 32u   /* 32 x 25 B = 800 B: room for the ~0.5 KB NM file */

typedef struct {
    uint32_t imin_us, imax_us;
    uint8_t k;              /* suppress when >= k x total duplicate frames heard */
} nm_cfg_t;

void nm_cfg_default(nm_cfg_t *cfg);

typedef struct {
    uint32_t tx_frames, rx_new, rx_dup, rx_bad_crc, rx_other_file, suppressed;
} nm_stats_t;

typedef struct {
    const proto_env_t *env;
    nm_cfg_t cfg;
    uint16_t file_id;
    uint8_t total;          /* 0 = nothing heard yet */
    uint32_t have;          /* bit idx: chunk held */
    uint8_t data[NM_MAX_CHUNKS][NM_CHUNK];
    uint8_t clen[NM_MAX_CHUNKS];
    uint8_t active, phase;
    uint32_t interval_us, dup_count;
    uint64_t end_at;
    uint64_t complete_at;   /* PROTO_NO_TIMER until every chunk is held */
    nm_stats_t st;
} nm_t;

void nm_init(nm_t *n, const proto_env_t *env, const nm_cfg_t *cfg);
/* Origin node: load the file and start gossiping. Returns -1 if it is too large. */
int nm_set_file(nm_t *n, const uint8_t *file, uint32_t len);
int nm_complete(const nm_t *n);
uint32_t nm_get_file(const nm_t *n, uint8_t *out, uint32_t cap);
void nm_on_frame(nm_t *n, const uint8_t *f, uint8_t len);
void nm_on_timer(nm_t *n);

#endif /* NM_GOSSIP_H */
