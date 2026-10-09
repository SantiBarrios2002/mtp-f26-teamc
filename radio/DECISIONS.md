# Radio Team decisions — MTP-F26 Team C

Status key: **Proposed** (RT recommendation, not yet signed off) · **Agreed** (RT + PM sign-off,
with date) · **Superseded**.
Santiago accepted the RT recommendations as the working baseline on 4-Oct-26 so that build work
can start. They still need sign-off from Andrian and Marcos (PM), ideally at the Fri 9-Oct
session and no later than Fri 16-Oct.

---

## D-R1 — Primary band: 869.40–869.65 MHz (centre 869.525 MHz)

**Status:** Proposed (working baseline 4-Oct-26) · **Owner:** RT

- Only this sub-band and 2.4 GHz allow a continuous 2-minute round (CNAF UN-39: 500 mW e.r.p.,
  ≤10 % duty cycle; UN-85 b: 10 mW e.i.r.p.).
- Range: +37 dB margin at 70 m, +14 dB at 260 m at 100 kbps (flat-earth, worst-case ground
  bounce). 2.4 GHz with nRF24 has −4.6 dB at 260 m with whips.
- Throughput: 100 kbps fits the 250 kHz channel (Carson 200 kHz + ±17 kHz drift, 7.6 kHz guard)
  and gives ~67 kbps goodput. That just carries 10 000 lines raw, so compression is the margin.
- **Duty cycle is the binding constraint:** one 2-min sending round is ~115 s of TX; the budget
  is 360 s per hour per box, so **≈3 full sending rounds per hour per box**, campus tests
  included. Mitigations: the firmware duty-cycle guard (`fw/dc_guard.c`), and run bench and
  repeated tests in 869.70–870.00 MHz (5 mW, no duty-cycle limit, profile `BENCH_5MW`).
- 2.4 GHz stays open as an **MRM add-on** (D-R4), not the primary band.

**Reverse if:** the other teams converge on 2.4 GHz for NM, or Atenea rules the competition
schedule out of the 10 % budget.

## D-R2 — Module: HopeRF RFM69HCW, 868 MHz variant (Semtech SX1231H)

**Status:** Proposed (working baseline 4-Oct-26) · **Owner:** RT → handed to ET

Why RFM69HCW over Si4463:
- Range is not the constraint at 868; the Si4463's extra 3 dB of power and sensitivity buys
  margin we don't need.
- Bring-up risk: RFM69 has mature open drivers (RadioHead, LowPowerLab) and simple registers;
  Si4463 needs Silicon Labs' WDS-generated config blobs.
- NM interoperability: the RFM69 is the module other teams are most likely to pick, which makes
  a common NM PHY easier.
- Lower protocol-ban risk: Si4463 advertises IEEE 802.15.4g PHY support.

Operating point: **+17 dBm (PA1+PA2), not +20 dBm**. The datasheet limits the +20 dBm
high-power mode to 1 % duty cycle. At +17 dBm the e.r.p. is ~27 mW, far under 500 mW.

Configuration: `phy_config.py` profiles `SRI_100K` (100 kbps GFSK BT 0.5, Fdev 50 kHz),
`ROBUST_38K4` (fallback, +5 dB), `NM_COMMON` (shared PHY draft) and `BENCH_5MW`. Driver in `fw/`.

Purchase (recommended 9-Oct-26): the **Adafruit RFM69HCW breakout, PID 3070** ("868 or 915 MHz"; per
LowPowerLab the 868 and 915 MHz HopeRF parts are the same hardware). Buy 3 (2 boxes + 1 spare) for the bench and
Quick Mode, and **keep it in the final boxes** (soldered to the carrier PCB by its 0.1" headers), so the module shown
at QM is literally the one that competes. It adds ~€6–8 per device over the bare module. Its 3.3 V regulator takes the
~95 mA TX bursts off the micro's supply, and it has u.FL/SMA pads. Switch to the bare RFM69HCW-868 only if the team
decides so **before** QM.

**Frozen at Quick Mode** (rules): changing the module afterwards loses the bonuses, so bench
test T2 must confirm 100 kbps PER before QM.

**Reverse if:** T2 shows 100 kbps sensitivity worse than −95 dBm, or Atenea bans the chip.
Fallback: Si4463 (same band, same link budget, more effort).

## D-R5 (initial) — Antenna for QM: λ/4 inverted-L wire, top of the box

**Status:** Proposed · **Owner:** RT

- A λ/4 at 869.5 MHz is 86 mm; the box is 70 mm tall. Start with a solid-core wire soldered to
  the ANT pad, ~60 mm vertical up the box wall plus ~25 mm horizontal under the lid. Cut long,
  trim for best RSSI (later with a VNA at the antenna lab).
- Mount the radio and antenna as **high in the box as possible**: antenna height is the biggest
  lever (+12 dB going from 0.5 to 1.0 m at both ends).
- In parallel, buy one 868 MHz u.FL FPC or helical antenna for an A/B comparison in T5.
- Ask Concepción Santos / Carles Puente to review before the final choice (week 8).
- Shortlist for the final antenna (9-Oct-26, datasheet figures; buy with the radios for test T5).
  The RFM69HCW has no antenna, only a 50 Ω ANT pad, so add a u.FL next to it:
  | Antenna | Size | 868 MHz figures | Notes |
  |---|---|---|---|
  | Molex 211140-0100 (flex, u.FL, 100 mm cable) | 38 × 10 mm | 0.3 dBi peak, > 55 % eff. | Smallest; sticks vertically on the box wall near the top |
  | Taoglas FXP895.07.0200C (flex, u.FL) | 69.3 × 20 mm | 1.9 dBi peak, 52 % eff. | Better gain; 69 mm barely fits the 70 mm box height |
  | Linx ANT-868-HETH (helical, through-hole) | 25.4 × 15.3 × 8.9 mm | 5.6 dBi peak (on Linx's eval board) | Solders to the carrier PCB; needs its ground plane; real gain in the box will be lower |
  | Ignion NN chip booster | ~ a few mm | depends on matching network | Needs a matching network + VNA tuning on the PCB; Carles Puente co-founded Ignion |

## Open

| ID | Decision | Target |
|---|---|---|
| D-R3 | NM PHY shared by all teams (draft: `NM_PHY_PROPOSAL.md`) | draft 16-Oct, frozen by week 6 |
| D-R4 | MRM access plan (own 868 slot, shared 250 kHz split, or 2.4 GHz add-on) | week 8–9 |
| D-R5 | Final antenna | week 8 |
