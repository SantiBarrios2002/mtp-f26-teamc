/* Host tests for the Team C radio firmware. Run: make -C radio/fw test */
#include <stdio.h>
#include <string.h>

#include "../crc16.h"
#include "../nrf24.h"
#include "mock_nrf24.h"

static int failures, checks;
#define CHECK(cond)                                                              \
    do {                                                                         \
        checks++;                                                                \
        if (!(cond)) {                                                           \
            failures++;                                                          \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                        \
    } while (0)

static const nrf24_profile_t *const PROFILES[] = {&NRF24_PROFILE_TEAM_250K, &NRF24_PROFILE_TEAM_1M,
                                                  &NRF24_PROFILE_NM_COMMON, &NRF24_PROFILE_BENCH_WHIP};
#define N_PROFILES (sizeof PROFILES / sizeof PROFILES[0])

typedef struct {
    mock_nrf24_t m;
    nrf24_hal_t hal;
    nrf24_t dev;
} node_t;

static nrf24_status_t node_up(node_t *n, const nrf24_profile_t *p)
{
    mock_init(&n->m);
    mock_hal(&n->m, &n->hal);
    return nrf24_init(&n->dev, &n->hal, p);
}

static void test_crc(void)
{
    const uint8_t s[] = "123456789";
    CHECK(crc16_ccitt(s, 9) == 0x29B1u);
    CHECK(crc16_ccitt(s, 0) == 0xFFFFu);
}

static void test_airtime(void)
{
    /* Must match radio/phy_config.py: (1 + 5 + 32 + 2) * 8 + 9 = 329 bits. */
    CHECK(nrf24_airtime_us(&NRF24_PROFILE_TEAM_250K, 32) == 1316u);
    CHECK(nrf24_airtime_us(&NRF24_PROFILE_TEAM_1M, 32) == 329u);
    CHECK(nrf24_airtime_us(&NRF24_PROFILE_TEAM_250K, 7) == 516u);   /* ACK frame */
}

static void test_profiles(void)
{
    for (unsigned k = 0; k < N_PROFILES; k++) {
        const nrf24_profile_t *p = PROFILES[k];
        CHECK(p->rf_ch <= NRF24_MAX_LEGAL_CH);
        CHECK(p->max_len == NRF24_MAX_PAYLOAD);
        CHECK(p->crc_bytes == 2);
        CHECK(p->regs[p->n_regs - 1].addr == 0x00);                 /* CONFIG last */
        CHECK((p->regs[p->n_regs - 1].value & 0x0Eu) == 0x0Eu);     /* EN_CRC, CRCO, PWR_UP */
    }
    CHECK(memcmp(NRF24_PROFILE_NM_COMMON.address, NRF24_PROFILE_TEAM_250K.address, 5) != 0);
    CHECK(NRF24_PROFILE_NM_COMMON.rf_ch != NRF24_PROFILE_TEAM_250K.rf_ch);
}

static void test_init_writes_profile(void)
{
    for (unsigned k = 0; k < N_PROFILES; k++) {
        const nrf24_profile_t *p = PROFILES[k];
        node_t n;
        CHECK(node_up(&n, p) == NRF24_OK);
        for (unsigned i = 0; i < p->n_regs; i++)
            if (p->regs[i].addr != 0x07)   /* STATUS is write-1-to-clear */
                CHECK(n.m.regs[p->regs[i].addr] == p->regs[i].value);
        CHECK(memcmp(n.m.tx_addr, p->address, 5) == 0);
        CHECK(memcmp(n.m.rx_addr_p0, p->address, 5) == 0);
        CHECK(n.m.regs[0x01] == 0x01 && n.m.regs[0x04] == 0x00);    /* ENAA_P0 for DPL, no retries */
        CHECK(n.m.regs[0x1C] == 0x01 && n.m.regs[0x1D] == 0x05);    /* DPL + DYN_ACK */
        CHECK(n.m.now_us >= 100000u);                               /* power-on reset wait */
    }
    /* 250 kbps at RF_PWR -18 dBm: RF_DR_LOW only */
    node_t n;
    node_up(&n, &NRF24_PROFILE_TEAM_250K);
    CHECK(n.m.regs[0x06] == 0x20);
    node_up(&n, &NRF24_PROFILE_TEAM_1M);
    CHECK(n.m.regs[0x06] == 0x00);
}

static void test_init_errors(void)
{
    node_t n;
    mock_init(&n.m);
    mock_hal(&n.m, &n.hal);
    n.m.dead_spi = 1;
    CHECK(nrf24_init(&n.dev, &n.hal, &NRF24_PROFILE_TEAM_250K) == NRF24_ERR_SPI);

    mock_init(&n.m);
    mock_hal(&n.m, &n.hal);
    n.m.not_plus = 1;
    CHECK(nrf24_init(&n.dev, &n.hal, &NRF24_PROFILE_TEAM_250K) == NRF24_ERR_NOT_PLUS);
}

static void test_send_and_receive(void)
{
    node_t a, b;
    CHECK(node_up(&a, &NRF24_PROFILE_TEAM_250K) == NRF24_OK);
    CHECK(node_up(&b, &NRF24_PROFILE_TEAM_250K) == NRF24_OK);
    mock_link(&a.m, &b.m);
    CHECK(nrf24_start_rx(&b.dev) == NRF24_OK);

    uint8_t rx[32], len = 0;
    CHECK(nrf24_poll_rx(&b.dev, rx, &len) == NRF24_ERR_NODATA);

    uint8_t tx[32];
    for (unsigned i = 0; i < 32; i++)
        tx[i] = (uint8_t)(i * 7u + 1u);
    CHECK(nrf24_send(&a.dev, tx, 32) == NRF24_OK);
    CHECK(a.m.noack_writes == 1 && a.m.ack_writes == 0);   /* never ESB-ACKed frames */
    CHECK(a.m.short_ce_pulses == 0);
    CHECK((a.m.regs[0x07] & 0x20u) == 0);                   /* TX_DS cleared */
    CHECK(nrf24_poll_rx(&b.dev, rx, &len) == NRF24_OK);
    CHECK(len == 32 && memcmp(rx, tx, 32) == 0);

    /* Short frames keep their length (dynamic payload). */
    CHECK(nrf24_send(&a.dev, tx, 7) == NRF24_OK);
    CHECK(nrf24_poll_rx(&b.dev, rx, &len) == NRF24_OK && len == 7 && memcmp(rx, tx, 7) == 0);
    CHECK(nrf24_poll_rx(&b.dev, rx, &len) == NRF24_ERR_NODATA);
    CHECK((b.m.regs[0x07] & 0x40u) == 0);                   /* RX_DR cleared once empty */

    /* Turn-around: B answers, A listens. */
    CHECK(nrf24_start_rx(&a.dev) == NRF24_OK);
    CHECK(nrf24_send(&b.dev, tx, 5) == NRF24_OK);
    CHECK(b.dev.rx_on == 0);
    CHECK(nrf24_poll_rx(&a.dev, rx, &len) == NRF24_OK && len == 5);
}

static void test_isolation(void)
{
    node_t a, b;
    uint8_t tx[4] = {1, 2, 3, 4}, rx[32], len;

    /* Different channel: nothing arrives. */
    node_up(&a, &NRF24_PROFILE_TEAM_250K);
    node_up(&b, &NRF24_PROFILE_TEAM_250K);
    mock_link(&a.m, &b.m);
    CHECK(nrf24_set_channel(&b.dev, 24) == NRF24_OK);
    nrf24_start_rx(&b.dev);
    nrf24_send(&a.dev, tx, 4);
    CHECK(nrf24_poll_rx(&b.dev, rx, &len) == NRF24_ERR_NODATA);
    CHECK(nrf24_set_channel(&b.dev, NRF24_PROFILE_TEAM_250K.rf_ch) == NRF24_OK);
    nrf24_send(&a.dev, tx, 4);
    CHECK(nrf24_poll_rx(&b.dev, rx, &len) == NRF24_OK);

    /* Different address (NM vs team) on the same channel: nothing arrives. */
    node_up(&a, &NRF24_PROFILE_TEAM_250K);
    node_up(&b, &NRF24_PROFILE_NM_COMMON);
    mock_link(&a.m, &b.m);
    nrf24_set_channel(&b.dev, NRF24_PROFILE_TEAM_250K.rf_ch);
    nrf24_start_rx(&b.dev);
    nrf24_send(&a.dev, tx, 4);
    CHECK(nrf24_poll_rx(&b.dev, rx, &len) == NRF24_ERR_NODATA);

    /* Different rate: nothing arrives. */
    node_up(&a, &NRF24_PROFILE_TEAM_1M);
    node_up(&b, &NRF24_PROFILE_TEAM_250K);
    mock_link(&a.m, &b.m);
    nrf24_start_rx(&b.dev);
    nrf24_send(&a.dev, tx, 4);
    CHECK(nrf24_poll_rx(&b.dev, rx, &len) == NRF24_ERR_NODATA);

    /* RX off (standby): nothing arrives. */
    node_up(&a, &NRF24_PROFILE_TEAM_250K);
    node_up(&b, &NRF24_PROFILE_TEAM_250K);
    mock_link(&a.m, &b.m);
    nrf24_send(&a.dev, tx, 4);
    CHECK(b.m.rx_n == 0);
}

static void test_limits(void)
{
    node_t a;
    uint8_t buf[40] = {0}, len;
    node_up(&a, &NRF24_PROFILE_TEAM_250K);
    CHECK(nrf24_send(&a.dev, buf, 0) == NRF24_ERR_LEN);
    CHECK(nrf24_send(&a.dev, buf, 33) == NRF24_ERR_LEN);
    CHECK(a.m.frames_tx == 0);
    CHECK(nrf24_set_channel(&a.dev, 84) == NRF24_ERR_CH);   /* 2484 MHz: outside UN-85 */
    CHECK(nrf24_set_channel(&a.dev, 83) == NRF24_OK);
    CHECK(a.m.regs[0x05] == 83);

    /* RX FIFO is 3 deep: the 4th frame is dropped, the first 3 come out in order. */
    node_t b;
    node_up(&a, &NRF24_PROFILE_TEAM_250K);
    node_up(&b, &NRF24_PROFILE_TEAM_250K);
    mock_link(&a.m, &b.m);
    nrf24_start_rx(&b.dev);
    for (uint8_t i = 1; i <= 4; i++) {
        buf[0] = i;
        nrf24_send(&a.dev, buf, 1);
    }
    CHECK(b.m.rx_dropped == 1);
    for (uint8_t i = 1; i <= 3; i++)
        CHECK(nrf24_poll_rx(&b.dev, buf, &len) == NRF24_OK && buf[0] == i);
    CHECK(nrf24_poll_rx(&b.dev, buf, &len) == NRF24_ERR_NODATA);

    /* Corrupt width (> 32): flushed and reported. */
    nrf24_send(&a.dev, buf, 3);
    b.m.bad_width = 1;
    CHECK(nrf24_poll_rx(&b.dev, buf, &len) == NRF24_ERR_LEN && len == 0);
    CHECK(b.m.rx_n == 0);
}

static void test_tx_timeout(void)
{
    node_t a;
    uint8_t buf[8] = {0};
    node_up(&a, &NRF24_PROFILE_TEAM_250K);
    a.m.tx_stuck = 1;
    uint32_t t0 = a.m.now_us;
    CHECK(nrf24_send(&a.dev, buf, 8) == NRF24_ERR_TIMEOUT);
    CHECK(a.m.now_us - t0 < 10000u);   /* gives up within a few ms */
    CHECK(a.m.tx_n == 0);              /* TX FIFO flushed */
}

static void test_carrier(void)
{
    node_t a;
    node_up(&a, &NRF24_PROFILE_TEAM_250K);
    nrf24_start_rx(&a.dev);
    CHECK(nrf24_carrier(&a.dev) == 0);
    a.m.rpd = 1;
    CHECK(nrf24_carrier(&a.dev) == 1);
}

int main(void)
{
    test_crc();
    test_airtime();
    test_profiles();
    test_init_writes_profile();
    test_init_errors();
    test_send_and_receive();
    test_isolation();
    test_limits();
    test_tx_timeout();
    test_carrier();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
