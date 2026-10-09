# Radio test plan — RFM69HCW 868 MHz

Version 0.1, 4-Oct-26 · Owner: RT · Expected values come from `linkbudget.py` (flat-earth,
worst-case ground bounce, antennas at 0.75 m, 0 dBi, −1 dB container).

**Duty-cycle rule for all tests:** use profile `BENCH_5MW` (869.85 MHz, +9 dBm, no limit)
unless the test needs the competition band. In 869.40–869.65 MHz each box may transmit
**≤ 360 s per hour**. Log every TX second, and leave the firmware duty-cycle guard enabled.

Record for every run: date, place, profile, distance, antenna, heights, box open/closed,
frames sent/received, mean and min RSSI (driver: `last_rssi_x2 / 2` dBm).

## T0 — Bring-up (bench, as soon as modules arrive)

1. Wire one module to the micro; run `rfm69_init`. Pass: returns `RFM69_OK`.
   `RFM69_ERR_SPI` means a wiring/NSS problem; `RFM69_ERR_VERSION` means the wrong chip or
   the module isn't powered.
2. Two boxes, 1 m apart, `BENCH_5MW`: 1 000 frames of 64 B. Pass: ≥ 999 received, RSSI ≈ −20
   to −40 dBm.

## T2 — Sensitivity / PER vs bit rate (bench) — **gate for D-R2 before Quick Mode**

Without an attenuator we lower TX power and move the boxes apart (corridor):
1. For `SRI_100K` and `ROBUST_38K4` settings (moved to 869.85 MHz), step the PA from +9 down to
   +2 dBm and the distance up until PER rises.
2. Send 1 000 frames per point; log PER and RSSI.
3. Find the RSSI where PER = 1 %. That is our measured sensitivity.

Pass: 100 kbps reaches PER ≤ 1 % at RSSI ≤ **−95 dBm** (model uses −100; datasheet gives none
above 38.4 kbps). If it is worse than −95, reopen D-R2 (Si4463 fallback) and update
`linkbudget.py` with the measured value.

## T4 — Range, open box (campus, line of sight)

Profile `BENCH_5MW`, boxes on ~70 cm supports. Distances 10 / 70 / 150 / 260 m.

| Distance | Expected RSSI | Expected margin |
|---|---|---|
| 10 m | −43 dBm | +57 dB |
| 70 m | −71 dBm | +29 dB |
| 150 m | −85 dBm | +16 dB |
| 260 m | −94 dBm | +6 dB |

Then **one** 2-minute file transfer at 70 m with `SRI_100K` (competition band, ~115 s of TX)
to measure goodput. Pass: ≥ 10 000 lines of ~100 B, or within 10 % of the model's 67 kbps
goodput.

Fit the measured RSSI against the model: the difference is the real multipath/medium loss,
which replaces the −1 dB / 0 dB assumptions in the workbook.

## T5 — Closed box and antenna A/B

Same as T4 at 70 m and 260 m, box closed. Compare the inverted-L wire against the bought
FPC/helical antenna, and the antenna at the top of the box against the bottom.
Pass: closed-box margin at 260 m ≥ +10 dB with `SRI_100K` power (+17 dBm). The model expects
+14 dB minus the extra container loss we measure.

## T13 — Spectrum and e.r.p. (lab, with a spectrum analyser)

1. Carrier offset of each module (CW or long preamble): replaces the ±20 ppm assumption
   (`XTAL_PPM` in `phy_config.py`).
2. 99 % occupied bandwidth at 100 kbps. Pass: whole emission inside 869.40–869.65 MHz
   including the measured offset.
3. Conducted power at +17 dBm. Estimate e.r.p. with the antenna gain. Pass: ≤ 500 mW
   (expected ~27 mW). Put the plot in the ANNEX.

## Order

T0 → T2 (before QM) → T4 → T13 (book the lab) → T5 → re-run T4/T5 with the final antenna.
