/* MTP-F26 Team C - protocol simulator: scenarios, scoring and test suite.
 *
 *   sim sri  [--loss P] [--burst] [--blackout A:B] [--rx-reboot T] [--tx-reboot T]
 *            [--codec deflate|none] [--profile 100k|38k4] [--file F] [--seed N] [--duration S]
 *   sim nm   [--nodes N] [--p1 P] [--p2 P] [--file F] [--seed N] [--duration S]
 *   sim test   run the pass/fail suite (exit code 1 on failure)
 *
 * Scoring follows the rules: the received bytes are decompressed and compared with
 * the original; only the first continuous run of identical lines counts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codec.h"
#include "link.h"
#include "nm_gossip.h"
#include "rfm69.h"
#include "sim.h"

#define S_US 1000000ULL
#define DEFAULT_FILE "build/sample.txt"

/* ----------------------------------------------------------------- files */

typedef struct {
    uint8_t *data;
    uint32_t len;
    uint32_t *ends; /* byte offset just past each line terminator */
    uint32_t n_lines;
} txtfile_t;

static int load_file(const char *path, txtfile_t *f)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "cannot open %s (run `make sample`)\n", path);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    f->data = malloc((size_t)n);
    f->len = (uint32_t)fread(f->data, 1, (size_t)n, fp);
    fclose(fp);
    int utf16 = f->len >= 2 && f->data[0] == 0xFF && f->data[1] == 0xFE;
    f->ends = malloc(sizeof(uint32_t) * (f->len / 2u + 1u));
    f->n_lines = 0;
    for (uint32_t i = utf16 ? 2u : 0u; i < f->len; i += utf16 ? 2u : 1u)
        if (f->data[i] == '\n' && (!utf16 || (i + 1u < f->len && f->data[i + 1u] == 0)))
            f->ends[f->n_lines++] = i + (utf16 ? 2u : 1u);
    return 0;
}

/* Lines counted the competition way: identical, from the start, no gaps. */
static uint32_t score_lines(const txtfile_t *f, const uint8_t *rx, uint32_t rx_len, int *exact)
{
    uint32_t m = 0, lim = rx_len < f->len ? rx_len : f->len;
    while (m < lim && rx[m] == f->data[m])
        m++;
    *exact = (m == rx_len);
    uint32_t lines = 0;
    while (lines < f->n_lines && f->ends[lines] <= m)
        lines++;
    return lines;
}

/* ------------------------------------------------------------------ SRI */

typedef struct {
    double loss, bo_from, bo_to, rx_reboot, tx_reboot, duration;
    int burst, codec;
    const rfm69_profile_t *profile;
    const char *file;
    uint64_t seed;
} sri_opts_t;

typedef struct {
    uint32_t lines, total_lines, raw_len, xfer_len;
    int exact;
    double ratio, done_s, recovery_s, tx_s_sender, tx_s_receiver;
    link_tx_stats_t tx;
    link_rx_stats_t rx;
    sim_node_stats_t ns0, ns1;
} sri_result_t;

typedef struct {
    sim_t sim;
    link_tx_t tx;
    link_rx_t rx;
    link_rx_store_t store;
    link_cfg_t cfg;
    const uint8_t *xfer;
    uint32_t xfer_len, crc;
    uint8_t codec;
} sri_ctx_t;

static void h_tx_frame(void *p, const uint8_t *f, uint8_t len, uint8_t rssi)
{
    (void)rssi;
    link_tx_on_frame(p, f, len);
}
static void h_tx_timer(void *p) { link_tx_on_timer(p); }
static void h_tx_idle(void *p) { link_tx_on_tx_idle(p); }
static void h_rx_frame(void *p, const uint8_t *f, uint8_t len, uint8_t rssi) { link_rx_on_frame(p, f, len, rssi); }
static void h_rx_timer(void *p) { link_rx_on_timer(p); }

static void sri_tx_boot(sim_t *s, void *arg)
{
    sri_ctx_t *c = arg;
    link_tx_init(&c->tx, sim_env(s, 0), &c->cfg, c->xfer, c->xfer_len, c->crc, c->codec, "sample.txt");
    link_tx_start(&c->tx);
}
static void sri_rx_boot(sim_t *s, void *arg)
{
    sri_ctx_t *c = arg;
    link_rx_init(&c->rx, sim_env(s, 1), &c->cfg, &c->store);
}
static void sri_rx_reboot(sim_t *s, void *arg) { sim_reboot(s, 1, 300000, sri_rx_boot, arg); }
static void sri_tx_reboot(sim_t *s, void *arg) { sim_reboot(s, 0, 300000, sri_tx_boot, arg); }

static int run_sri(const sri_opts_t *o, sri_result_t *r)
{
    txtfile_t f;
    if (load_file(o->file, &f))
        return -1;
    sri_ctx_t *c = calloc(1, sizeof *c);
    uint8_t *comp = NULL;
    memset(r, 0, sizeof *r);
    r->raw_len = f.len;
    r->total_lines = f.n_lines;
    if (o->codec == CODEC_DEFLATE) {
        if (codec_deflate(f.data, f.len, 9, &comp, &c->xfer_len))
            return -1;
        c->xfer = comp;
    } else {
        c->xfer = f.data;
        c->xfer_len = f.len;
    }
    c->codec = (uint8_t)o->codec;
    c->crc = codec_crc32(c->xfer, c->xfer_len);
    r->xfer_len = c->xfer_len;
    r->ratio = (double)f.len / c->xfer_len;

    link_cfg_for_bitrate(&c->cfg, o->profile->bitrate_bps);
    c->store.cap = 4u << 20;
    c->store.buf = malloc(c->store.cap);

    sim_t *s = &c->sim;
    sim_init(s, 2, o->profile, o->seed);
    sim_set_link(s, 0, 1, o->loss, 140);
    if (o->burst) {
        s->ge_on = 1;
        s->ge_p_gb = 0.02; /* ~7 % of frames in a fade, 80 % lost there */
        s->ge_p_bg = 0.25;
        s->ge_loss_bad = 0.8;
    }
    if (o->bo_to > o->bo_from) {
        s->bo_from = (uint64_t)(o->bo_from * S_US);
        s->bo_to = (uint64_t)(o->bo_to * S_US);
    }
    sim_hooks_t ht = {&c->tx, h_tx_frame, h_tx_timer, h_tx_idle};
    sim_hooks_t hr = {&c->rx, h_rx_frame, h_rx_timer, NULL};
    sim_set_hooks(s, 0, &ht);
    sim_set_hooks(s, 1, &hr);
    sri_rx_boot(s, c);
    sim_at(s, 0, sri_tx_boot, c); /* GO */
    if (o->rx_reboot > 0)
        sim_at(s, (uint64_t)(o->rx_reboot * S_US), sri_rx_reboot, c);
    if (o->tx_reboot > 0)
        sim_at(s, (uint64_t)(o->tx_reboot * S_US), sri_tx_reboot, c);

    /* Step in 10 ms to time completion and recovery after the blackout. */
    uint64_t end = (uint64_t)(o->duration * S_US);
    uint32_t at_bo_end = 0;
    r->done_s = r->recovery_s = -1;
    for (uint64_t t = 0; t <= end; t += 10000) {
        sim_run_until(s, t);
        if (r->done_s < 0 && c->store.committed == c->xfer_len)
            r->done_s = (double)t / S_US;
        if (s->bo_to && t == (s->bo_to / 10000) * 10000)
            at_bo_end = c->store.committed;
        if (s->bo_to && t > s->bo_to && r->recovery_s < 0 &&
            (c->store.committed > at_bo_end || c->store.committed == c->xfer_len))
            r->recovery_s = (double)(t - s->bo_to) / S_US;
    }

    /* STOP: decompress what arrived in order and score it. */
    uint8_t *out = c->store.buf;
    uint32_t out_len = c->store.committed;
    uint8_t *plain = NULL;
    if (c->codec == CODEC_DEFLATE) {
        plain = malloc(f.len + 1024u);
        out_len = codec_inflate_prefix(c->store.buf, c->store.committed, plain, f.len + 1024u);
        out = plain;
    }
    r->lines = score_lines(&f, out, out_len, &r->exact);
    r->tx = c->tx.st;
    r->rx = c->rx.st;
    r->ns0 = s->nodes[0].st;
    r->ns1 = s->nodes[1].st;
    r->tx_s_sender = (double)s->nodes[0].st.tx_us / S_US;
    r->tx_s_receiver = (double)s->nodes[1].st.tx_us / S_US;

    free(plain);
    free(comp);
    free(c->store.buf);
    sim_free(s);
    free(c);
    free(f.data);
    free(f.ends);
    return 0;
}

static void print_sri(const char *label, const sri_result_t *r)
{
    printf("%s\n", label);
    printf("  lines delivered    %u / %u%s\n", r->lines, r->total_lines, r->exact ? "" : "   (CORRUPT BYTES!)");
    printf("  file               %u B -> %u B on air (%.2fx)\n", r->raw_len, r->xfer_len, r->ratio);
    if (r->done_s >= 0)
        printf("  whole file in      %.2f s\n", r->done_s);
    else
        printf("  whole file in      not finished\n");
    if (r->recovery_s >= 0)
        printf("  recovery after blackout %.2f s\n", r->recovery_s);
    printf("  TX time            sender %.1f s, receiver %.1f s (budget 342 s/h)\n", r->tx_s_sender, r->tx_s_receiver);
    printf("  sender             %u frames, %u retx, %u bursts, %u acks, %u timeouts, %u hellos\n",
           r->tx.frames, r->tx.retx, r->tx.bursts, r->tx.acks, r->tx.timeouts, r->tx.hellos);
    printf("  channel            rx lost %u, deaf %u, collisions %u, blackout %u, duty-refused %u\n",
           r->ns1.rx_lost + r->ns0.rx_lost, r->ns1.rx_deaf + r->ns0.rx_deaf,
           r->ns1.rx_collision + r->ns0.rx_collision, r->ns1.rx_blackout + r->ns0.rx_blackout,
           r->ns0.duty_refused + r->ns1.duty_refused);
}

/* ------------------------------------------------------------------- NM */

typedef struct {
    int nodes;
    double p1, p2, duration;
    const char *file;
    uint64_t seed;
} nm_opts_t;

typedef struct {
    int nodes, complete, exact;
    double far_s, last_s, max_tx_s;
    uint32_t file_len, collisions, frames;
} nm_result_t;

static void h_nm_frame(void *p, const uint8_t *f, uint8_t len, uint8_t rssi)
{
    (void)rssi;
    nm_on_frame(p, f, len);
}
static void h_nm_timer(void *p) { nm_on_timer(p); }

static int run_nm(const nm_opts_t *o, nm_result_t *r)
{
    txtfile_t f;
    if (load_file(o->file, &f))
        return -1;
    uint32_t nm_len = f.n_lines >= 5 ? f.ends[4] : f.len; /* the NM file: 5 lines */
    if (nm_len > NM_MAX_CHUNKS * NM_CHUNK)
        nm_len = NM_MAX_CHUNKS * NM_CHUNK;
    sim_t *s = calloc(1, sizeof *s);
    nm_t *nm = calloc((size_t)o->nodes, sizeof *nm);
    nm_cfg_t cfg;
    nm_cfg_default(&cfg);
    sim_init(s, o->nodes, &RFM69_PROFILE_NM_COMMON, o->seed);
    for (int i = 0; i < o->nodes; i++)
        for (int j = i + 1; j < o->nodes; j++)
            sim_set_link(s, i, j, j - i == 1 ? o->p1 : j - i == 2 ? o->p2 : 1.0, 150);
    for (int i = 0; i < o->nodes; i++) {
        nm_init(&nm[i], sim_env(s, i), &cfg);
        sim_hooks_t h = {&nm[i], h_nm_frame, h_nm_timer, NULL};
        sim_set_hooks(s, i, &h);
    }
    nm_set_file(&nm[0], f.data, nm_len); /* GO: the sender at the SW corner */
    sim_run_until(s, (uint64_t)(o->duration * S_US));

    memset(r, 0, sizeof *r);
    r->nodes = o->nodes;
    r->file_len = nm_len;
    r->exact = 1;
    uint8_t buf[NM_MAX_CHUNKS * NM_CHUNK];
    for (int i = 0; i < o->nodes; i++) {
        if (nm_complete(&nm[i])) {
            r->complete++;
            double t = (double)nm[i].complete_at / S_US;
            if (t > r->last_s)
                r->last_s = t;
            if (nm_get_file(&nm[i], buf, sizeof buf) != nm_len || memcmp(buf, f.data, nm_len))
                r->exact = 0;
        }
        double tx = (double)s->nodes[i].st.tx_us / S_US;
        if (tx > r->max_tx_s)
            r->max_tx_s = tx;
        r->collisions += s->nodes[i].st.rx_collision;
        r->frames += s->nodes[i].st.frames_tx;
    }
    r->far_s = nm_complete(&nm[o->nodes - 1]) ? (double)nm[o->nodes - 1].complete_at / S_US : -1;
    sim_free(s);
    free(s);
    free(nm);
    free(f.data);
    free(f.ends);
    return 0;
}

static void print_nm(const char *label, const nm_result_t *r)
{
    printf("%s\n", label);
    printf("  nodes with the file  %d / %d%s\n", r->complete, r->nodes, r->exact ? "" : "   (CORRUPT FILE!)");
    if (r->far_s >= 0)
        printf("  far end complete     %.2f s (last node %.2f s)\n", r->far_s, r->last_s);
    else
        printf("  far end complete     never\n");
    printf("  NM file              %u B; %u frames sent in total, %u collisions\n", r->file_len, r->frames,
           r->collisions);
    printf("  busiest node TX      %.2f s\n", r->max_tx_s);
}

/* ---------------------------------------------------------------- tests */

static int failures;

static void expect(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

static sri_opts_t sri_defaults(void)
{
    sri_opts_t o = {0};
    o.duration = 120;
    o.codec = CODEC_DEFLATE;
    o.profile = &RFM69_PROFILE_SRI_100K;
    o.file = DEFAULT_FILE;
    o.seed = 1;
    return o;
}

static nm_opts_t nm_defaults(void)
{
    nm_opts_t o = {10, 0.1, 0.7, 300, DEFAULT_FILE, 1};
    return o;
}

static int run_tests(void)
{
    sri_result_t r;
    sri_opts_t o;
    char what[160];

    o = sri_defaults();
    if (run_sri(&o, &r))
        return 2;
    print_sri("\nT1  SRI, clean channel, deflate", &r);
    expect(r.lines == r.total_lines && r.exact, "all lines, byte-exact");
    expect(r.done_s >= 0 && r.done_s < 60, "whole file in under 60 s");

    o = sri_defaults();
    o.loss = 0.05;
    run_sri(&o, &r);
    print_sri("\nT2  SRI, 5 % random loss, deflate", &r);
    expect(r.lines == r.total_lines && r.exact, "all lines at 5 % loss (proposal target)");

    o = sri_defaults();
    o.loss = 0.05;
    o.codec = CODEC_NONE;
    run_sri(&o, &r);
    print_sri("\nT3  SRI, 5 % random loss, NO compression (worst case)", &r);
    expect(r.exact, "byte-exact prefix");
    snprintf(what, sizeof what, "information only: %u lines without compression", r.lines);
    expect(1, what);

    o = sri_defaults();
    o.burst = 1;
    run_sri(&o, &r);
    print_sri("\nT4  SRI, bursty fades (Gilbert-Elliott, ~6 % loss), deflate", &r);
    expect(r.lines == r.total_lines && r.exact, "all lines with burst losses");

    o = sri_defaults();
    o.loss = 0.05;
    o.bo_from = 20;
    o.bo_to = 30;
    run_sri(&o, &r);
    print_sri("\nT5  SRI, 10 s jamming attack (20-30 s) + 5 % loss", &r);
    expect(r.lines == r.total_lines && r.exact, "all lines despite the attack");
    expect(r.recovery_s >= 0 && r.recovery_s < 1.0, "link back within 1 s of the attack ending");

    o = sri_defaults();
    o.loss = 0.05;
    o.rx_reboot = 15;
    run_sri(&o, &r);
    print_sri("\nT6  SRI, receiver reboots at 15 s", &r);
    expect(r.lines == r.total_lines && r.exact, "receiver resumes, all lines");

    o = sri_defaults();
    o.loss = 0.05;
    o.tx_reboot = 15;
    run_sri(&o, &r);
    print_sri("\nT7  SRI, sender reboots at 15 s", &r);
    expect(r.lines == r.total_lines && r.exact, "sender resumes, all lines");

    o = sri_defaults();
    o.loss = 0.05;
    o.profile = &RFM69_PROFILE_ROBUST_38K4;
    run_sri(&o, &r);
    print_sri("\nT8  SRI, fallback profile 38.4 kbps, 5 % loss, deflate", &r);
    expect(r.exact, "byte-exact prefix");
    expect(r.lines >= 9000, "fallback still delivers >= 9 000 lines (timeouts scale with bit rate)");

    o = sri_defaults();
    o.loss = 0.05;
    o.file = "build/sample_utf16.txt";
    run_sri(&o, &r);
    print_sri("\nT9  SRI, UTF-16 file (2 MB), 5 % loss, deflate", &r);
    expect(r.lines == r.total_lines && r.exact, "all lines of a UTF-16 file");

    nm_result_t n;
    nm_opts_t no = nm_defaults();
    if (run_nm(&no, &n))
        return 2;
    print_nm("\nT10 NM, 10 nodes in a line, 10 % loss to neighbours, 70 % two hops away", &n);
    expect(n.complete == n.nodes && n.exact, "every node gets an exact copy");
    expect(n.far_s >= 0 && n.far_s < 60, "far end within 60 s");

    no = nm_defaults();
    no.p1 = 0.3;
    no.p2 = 0.95;
    run_nm(&no, &n);
    print_nm("\nT11 NM, harsh: 30 % loss to neighbours, 95 % two hops away", &n);
    expect(n.complete == n.nodes && n.exact, "every node still gets an exact copy in 5 min");

    printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}

/* ------------------------------------------------------------------ CLI */

static double arg_d(int *i, int argc, char **argv)
{
    if (*i + 1 >= argc) {
        fprintf(stderr, "missing value for %s\n", argv[*i]);
        exit(2);
    }
    return atof(argv[++*i]);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "test"))
        return run_tests();
    if (!strcmp(argv[1], "sri")) {
        sri_opts_t o = sri_defaults();
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--loss"))
                o.loss = arg_d(&i, argc, argv);
            else if (!strcmp(argv[i], "--burst"))
                o.burst = 1;
            else if (!strcmp(argv[i], "--blackout") && i + 1 < argc)
                sscanf(argv[++i], "%lf:%lf", &o.bo_from, &o.bo_to);
            else if (!strcmp(argv[i], "--rx-reboot"))
                o.rx_reboot = arg_d(&i, argc, argv);
            else if (!strcmp(argv[i], "--tx-reboot"))
                o.tx_reboot = arg_d(&i, argc, argv);
            else if (!strcmp(argv[i], "--codec") && i + 1 < argc)
                o.codec = !strcmp(argv[++i], "none") ? CODEC_NONE : CODEC_DEFLATE;
            else if (!strcmp(argv[i], "--profile") && i + 1 < argc)
                o.profile = !strcmp(argv[++i], "38k4") ? &RFM69_PROFILE_ROBUST_38K4 : &RFM69_PROFILE_SRI_100K;
            else if (!strcmp(argv[i], "--file") && i + 1 < argc)
                o.file = argv[++i];
            else if (!strcmp(argv[i], "--seed"))
                o.seed = (uint64_t)arg_d(&i, argc, argv);
            else if (!strcmp(argv[i], "--duration"))
                o.duration = arg_d(&i, argc, argv);
            else {
                fprintf(stderr, "unknown option %s\n", argv[i]);
                return 2;
            }
        }
        sri_result_t r;
        if (run_sri(&o, &r))
            return 2;
        print_sri("SRI run", &r);
        return 0;
    }
    if (!strcmp(argv[1], "nm")) {
        nm_opts_t o = nm_defaults();
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--nodes"))
                o.nodes = (int)arg_d(&i, argc, argv);
            else if (!strcmp(argv[i], "--p1"))
                o.p1 = arg_d(&i, argc, argv);
            else if (!strcmp(argv[i], "--p2"))
                o.p2 = arg_d(&i, argc, argv);
            else if (!strcmp(argv[i], "--file") && i + 1 < argc)
                o.file = argv[++i];
            else if (!strcmp(argv[i], "--seed"))
                o.seed = (uint64_t)arg_d(&i, argc, argv);
            else if (!strcmp(argv[i], "--duration"))
                o.duration = arg_d(&i, argc, argv);
            else {
                fprintf(stderr, "unknown option %s\n", argv[i]);
                return 2;
            }
        }
        if (o.nodes < 2 || o.nodes > SIM_MAX_NODES) {
            fprintf(stderr, "--nodes must be 2..%d\n", SIM_MAX_NODES);
            return 2;
        }
        nm_result_t r;
        if (run_nm(&o, &r))
            return 2;
        print_nm("NM run", &r);
        return 0;
    }
    fprintf(stderr, "usage: sim test | sim sri [options] | sim nm [options]\n");
    return 2;
}
