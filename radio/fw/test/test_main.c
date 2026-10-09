/* Host tests for the Team C radio firmware. Run: make -C radio/fw test */
#include <stdio.h>
#include <string.h>

#include "../crc16.h"
#include "../dc_guard.h"
#include "../rfm69.h"
#include "mock_sx1231.h"

static int failures, checks;
#define CHECK(cond)                                                              \
    do {                                                                         \
        checks++;                                                                \
        if (!(cond)) {                                                           \
            failures++;                                                          \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                        \
    } while (0)

static void test_crc(void)
{
    const uint8_t s[] = "123456789";
    CHECK(crc16_ccitt(s, 9) == 0x29B1u);
    CHECK(crc16_ccitt(s, 0) == 0xFFFFu);
}

static void test_airtime(void)
{
    /* Must match radio/phy_config.py: SRI 64 B body = 5840 us, NM (no hw CRC) = 5680 us. */
    CHECK(rfm69_airtime_us(&RFM69_PROFILE_SRI_100K, 64) == 5840u);
    CHECK(rfm69_airtime_us(&RFM69_PROFILE_NM_COMMON, 64) == 5680u);
    CHECK(rfm69_airtime_us(&RFM69_PROFILE_ROBUST_38K4, 64) == 15202u);
}

static void test_init_writes_profile(void)
{
    const rfm69_profile_t *profiles[] = {&RFM69_PROFILE_SRI_100K, &RFM69_PROFILE_ROBUST_38K4,
                                         &RFM69_PROFILE_NM_COMMON, &RFM69_PROFILE_BENCH_5MW};
    for (unsigned k = 0; k < 4; k++) {
        mock_sx1231_t m;
        rfm69_hal_t hal;
        rfm69_t dev;
        mock_init(&m);
        mock_hal(&m, &hal);
        CHECK(rfm69_init(&dev, &hal, profiles[k], NULL) == RFM69_OK);
        CHECK(m.reset_pulses == 1);
        for (unsigned i = 0; i < profiles[k]->n_regs; i++)
            if (profiles[k]->regs[i].addr != 0x01)
                CHECK(m.regs[profiles[k]->regs[i].addr] == profiles[k]->regs[i].value);
        CHECK(((m.regs[0x01] >> 2) & 7u) == RFM69_MODE_STDBY);
    }
    /* 869.525 MHz -> Frf 0xD9619A; bench 869.85 MHz -> 0xD97666 */
    CHECK(RFM69_REGS_SRI_100K[6].value == 0xD9 && RFM69_REGS_SRI_100K[7].value == 0x61 &&
          RFM69_REGS_SRI_100K[8].value == 0x9A);
}

static void test_init_errors(void)
{
    mock_sx1231_t m;
    rfm69_hal_t hal;
    rfm69_t dev;
    mock_init(&m);
    m.regs[0x10] = 0x22;
    mock_hal(&m, &hal);
    CHECK(rfm69_init(&dev, &hal, &RFM69_PROFILE_SRI_100K, NULL) == RFM69_ERR_VERSION);
}

/* Broken MISO / unpowered module: every read returns 0xFF. */

static uint8_t dead_transfer(void *ctx, uint8_t out) { (void)ctx; (void)out; return 0xFF; }
static void dead_select(void *ctx, int a) { (void)ctx; (void)a; }

static void test_spi_failure(void)
{
    mock_sx1231_t m;
    rfm69_hal_t hal;
    rfm69_t dev;
    mock_init(&m);
    mock_hal(&m, &hal);
    hal.transfer = dead_transfer;
    hal.select = dead_select;
    CHECK(rfm69_init(&dev, &hal, &RFM69_PROFILE_SRI_100K, NULL) == RFM69_ERR_SPI);
}

static void test_send_and_receive(void)
{
    mock_sx1231_t a, b;
    rfm69_hal_t ha, hb;
    rfm69_t ra, rb;
    mock_init(&a);
    mock_init(&b);
    mock_hal(&a, &ha);
    mock_hal(&b, &hb);
    CHECK(rfm69_init(&ra, &ha, &RFM69_PROFILE_SRI_100K, NULL) == RFM69_OK);
    CHECK(rfm69_init(&rb, &hb, &RFM69_PROFILE_SRI_100K, NULL) == RFM69_OK);

    uint8_t msg[64];
    for (unsigned i = 0; i < sizeof msg; i++)
        msg[i] = (uint8_t)(i * 7u + 1u);
    CHECK(rfm69_send(&ra, msg, 64) == RFM69_OK);
    CHECK(a.n_tx == 1 && a.air_len == 65 && a.air[0] == 64 && memcmp(a.air + 1, msg, 64) == 0);
    CHECK(((a.regs[0x01] >> 2) & 7u) == RFM69_MODE_STDBY);
    CHECK(a.regs[0x25] == 0x00); /* DIO0 = PacketSent was set for TX */

    CHECK(rfm69_start_rx(&rb) == RFM69_OK);
    CHECK(b.regs[0x25] == 0x40);
    uint8_t got[64], n = 0;
    CHECK(rfm69_poll_rx(&rb, got, &n) == RFM69_ERR_NODATA);
    mock_deliver(&b, a.air, a.air_len, 140); /* RSSI reg 140 -> -70 dBm */
    CHECK(rfm69_poll_rx(&rb, got, &n) == RFM69_OK);
    CHECK(n == 64 && memcmp(got, msg, 64) == 0);
    CHECK(rb.last_rssi_x2 == -140);
    CHECK(((b.regs[0x01] >> 2) & 7u) == RFM69_MODE_RX); /* re-armed */
    CHECK(rfm69_poll_rx(&rb, got, &n) == RFM69_ERR_NODATA);
}

static void test_length_limits(void)
{
    mock_sx1231_t m;
    rfm69_hal_t hal;
    rfm69_t dev;
    mock_init(&m);
    mock_hal(&m, &hal);
    rfm69_init(&dev, &hal, &RFM69_PROFILE_SRI_100K, NULL);
    uint8_t buf[66] = {0};
    CHECK(rfm69_send(&dev, buf, 0) == RFM69_ERR_LEN);
    CHECK(rfm69_send(&dev, buf, 65) == RFM69_ERR_LEN);
    CHECK(m.n_tx == 0);

    rfm69_start_rx(&dev);
    uint8_t bad[2] = {200, 0};
    mock_deliver(&m, bad, 2, 100);
    uint8_t n = 99;
    CHECK(rfm69_poll_rx(&dev, buf, &n) == RFM69_ERR_LEN);
    CHECK(n == 0);
}

static void test_tx_timeout(void)
{
    mock_sx1231_t m;
    rfm69_hal_t hal;
    rfm69_t dev;
    mock_init(&m);
    mock_hal(&m, &hal);
    rfm69_init(&dev, &hal, &RFM69_PROFILE_SRI_100K, NULL);
    m.stuck_tx = 1;
    uint8_t buf[10] = {0};
    CHECK(rfm69_send(&dev, buf, 10) == RFM69_ERR_TIMEOUT);
    CHECK(((m.regs[0x01] >> 2) & 7u) == RFM69_MODE_STDBY);
}

static void test_dc_guard(void)
{
    dc_guard_t g;
    dc_guard_init(&g, DC_GUARD_LIMIT_MS);
    /* Transmit 6 s every minute: 60 minutes = 360 s exactly hits the limit. */
    for (uint32_t min = 0; min < 60; min++)
        dc_guard_add(&g, min * 60000u + 1000u, 6000u);
    CHECK(dc_guard_used_ms(&g, 59u * 60000u + 2000u) == 360000u);
    CHECK(!dc_guard_allows(&g, 59u * 60000u + 2000u, 1u));
    /* Minute 0 is still inside the trailing 61-minute window at minute 60. */
    CHECK(dc_guard_used_ms(&g, 60u * 60000u) == 360000u);
    /* At minute 61 the minute-0 bucket ages out. */
    CHECK(dc_guard_used_ms(&g, 61u * 60000u) == 354000u);
    CHECK(dc_guard_allows(&g, 61u * 60000u, 6000u));
    /* Bucket reuse: minute 61 overwrites minute 0's slot. */
    dc_guard_add(&g, 61u * 60000u, 500u);
    CHECK(dc_guard_used_ms(&g, 61u * 60000u) == 354500u);
}

static void test_send_respects_guard(void)
{
    mock_sx1231_t m;
    rfm69_hal_t hal;
    rfm69_t dev;
    dc_guard_t g;
    mock_init(&m);
    mock_hal(&m, &hal);
    dc_guard_init(&g, 20u); /* tiny budget: 20 ms */
    rfm69_init(&dev, &hal, &RFM69_PROFILE_SRI_100K, &g);
    uint8_t buf[64] = {0};
    int sent = 0;
    while (rfm69_send(&dev, buf, 64) == RFM69_OK)
        sent++;
    /* 64 B frame = 5.84 ms -> 6 ms charged; 3 frames = 18 ms, a 4th would be 24 ms. */
    CHECK(sent == 3);
    CHECK(m.n_tx == 3);
    CHECK(rfm69_send(&dev, buf, 64) == RFM69_ERR_DUTY);
}

int main(void)
{
    test_crc();
    test_airtime();
    test_init_writes_profile();
    test_init_errors();
    test_spi_failure();
    test_send_and_receive();
    test_length_limits();
    test_tx_timeout();
    test_dc_guard();
    test_send_respects_guard();
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
