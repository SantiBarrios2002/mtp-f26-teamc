# Radio Team decisions — MTP-F26 Team C

Status key: **Proposed** (RT recommendation, not yet signed off) · **Agreed** (RT + PM sign-off,
with date) · **Superseded**.
Santiago accepted the RT recommendations as the working baseline on 4-Oct-26 so that build work
can start. They still need sign-off from Andrian and Marcos (PM), ideally at the Fri 9-Oct
session and no later than Fri 16-Oct.

---

> **9-Oct-26: band switch.** All four teams (us + 3 others) chose 2.4 GHz for every mode, so the
> 868 MHz records below (D-R1, D-R2, D-R5 initial) are **superseded** by D-R1b, D-R2b and D-R5b.
> They are kept for the report (why 868 was the better link and what 2.4 GHz costs). Numbers:
> `python3 radio/linkbudget.py`, scenarios S16–S20 of the workbook.

## D-R1b — Band: 2.4 GHz ISM (2400–2483.5 MHz), all modes, 250 kbps

**Status:** Agreed between the teams (reported by Santiago, 9-Oct-26) · details Proposed · **Owner:** RT

- **Why it is right for the competition:** MRM runs all teams *simultaneously* (rules: "all teams will have
  to operate their systems simultaneously"). At 868 the four teams would share one 250 kHz channel
  (~67/4 kbps each); at 2.4 GHz each team gets its own channel (D-R4). NM needs one PHY shared by all
  teams, and everyone is now on 2.4 GHz. No duty-cycle limit, so unlimited test rounds.
- **What it costs:** limit is **10 mW e.i.r.p.** (UN-85 b). UN-85 a) does not help a non-hopping
  radio (10 mW/MHz, and our signal is < 1 MHz wide); only FHSS gets 100 mW (Atenea Q3, S20: +10 dB).
  Margins at h = 0.75 m (flat earth, worst-case ground bounce): **SRI 70 m +27.0 dB, MRM 260 m
  +4.4 dB, NM 100 m hop (−10 dB obstruction) +10.9 dB**, vs +36.7 / +13.9 / +20.5 at 868. MRM is the
  thin one: +9.3 dB at 1.0 m height, −2.6 dB at 0.5 m, so mount the patches as high as the box allows.
- **One air rate, 250 kbps GFSK, in every mode:** best sensitivity (−94 dBm chip) and best
  selectivity (D-R4). Goodput ~159 kbps with 32 B payloads, vs 67 kbps needed for the whole 1 MB file
  uncompressed, so 1–2 Mbps buys nothing that scores. 1 Mbps stays an SRI option (+18 dB).
- Legal channels: RF_CH 0–83 (f = 2400 + RF_CH MHz); ≤ 82 at 2 Mbps.

**Reverse if:** never for the band (the teams agreed). Revisit the rate if T2 shows 250 kbps PER problems.

## D-R2b — Module: Ebyte E01-ML01DP5 (genuine nRF24L01P + PA/LNA, SMA)

**Status:** Proposed (RT pick 9-Oct-26, Santiago delegated the choice) · **Owner:** RT → handed to ET

- Genuine Nordic nRF24L01P (Ebyte), PA + LNA front end (assumed Skyworks RFX2401C: PA ~25 dB,
  LNA 12 dB / NF 2.5 dB, always in line), SMA-K, 2.54 mm DIP pins: SPI + CE + CSN + IRQ, 3.3 V.
  PA/LNA switching is inside the module. Mature drivers (RF24 library, ports for the Pico).
- **Power setting:** RF_PWR = −18 dBm gives about **+7 dBm at the SMA** (ESTIMATE from the PA gain;
  Ebyte publishes no table). With a 1 dB pigtail and a 4.5 dBi patch that is 10.5 dBm e.i.r.p., so a
  **≥ 0.5 dB pad** (buy 1/2/3 dB SMA pads) brings it to 10.0. With the 2 dBi QM whip no pad is needed
  (+8 dBm e.i.r.p.). **T0 must measure the real output** with a power meter / SA before any field test.
- Sensitivity in the model: chip value (−94 dBm @250 kbps), LNA ignored. Ebyte quotes ~−96 for the
  SMD sister part; T2 decides.
- **Backup:** E01-ML01SP4 (same radio, SMD 14.85 × 18 mm, u.FL) if ET prefers to solder it to the carrier.
- **Do not buy** E01C-* parts or no-name "nRF24L01+PA+LNA" boards: Ebyte's own E01C manual says
  Si24R1 clone. Check the chip on arrival (register behaviour, RPD bit).
- The Adafruit RFM69HCW order (old D-R2) is **cancelled**.
- **Shopping (order by Fri 16-Oct; prices and EU lead time still to verify):** 10 × E01-ML01DP5
  (2 boxes × 2 antennas, 4 bench, 2 spare), 2 × E01-ML01SP4, a set of SMA pads (1, 2, 3 dB, ×2),
  6 × SMA-to-u.FL / SMA pigtails (~10 cm) for the patches, 2 × 2.4 GHz SMA whips (QM, bench).

**Reverse if:** T0 shows the output at RF_PWR −18 cannot be padded to ≤ 10 dBm e.i.r.p. without losing
> 3 dB, or T2 shows a clone/poor receiver. Fallback: E01-ML01SP4, then a no-PA module.

## D-R4 (draft) — MRM access: one channel per team (FDMA)

**Status:** Draft for the inter-team meeting · **Owner:** RT

- Four teams, each on its own channel, all at 250 kbps. Proposal: **RF_CH 24 / 49 / 74 / 82**
  (2424 / 2449 / 2474 / 2482 MHz): three sit in the gaps between WiFi channels 1/6/11 at 25 MHz spacing,
  the fourth at the band edge, 8 MHz from its neighbour.
- Near-far is the real limit (ARQ means both ends transmit). Another team's box at 2 m with patches
  aligned arrives **55 dB above** our 260 m signal; side-on (~10 dB rejection per patch) 35 dB; at 10 m,
  41 / 21 dB. nRF24 selectivity at 250 kbps: −50 dB beyond 6 MHz, −60 dB beyond 25 MHz.
  So: the 25 MHz pairs are safe even co-located; **the 74/82 pair needs ≥ 10 m between those two
  teams' boxes** (or side-on placement). Ask the organisers for ≥ 10 m spacing at each end anyway.
- Final channel numbers after the campus 2.4 GHz survey (Andrian): if eduroam is not on 1/6/11 there,
  shift the plan.

## D-R5b — Antennas: two FR4 patches per box, vertical, front and back

**Status:** Proposed (9-Oct-26; two antennas per box are allowed, per Santiago) · **Owner:** RT

- **The patches must stand vertically.** A patch radiates broadside; flat on the carrier PCB in a box
  lying flat it points at the sky and is ~5–10 dB down at the horizon. So each patch is its own small
  board (≈ 60 × 60 mm, FR4 1.6 mm, same KiCad project/lab order as the carrier) fixed to a box wall,
  fed by an SMA/u.FL pigtail. **Vertical linear polarisation** (feed on the bottom edge), agreed with
  the other teams for NM.
- One patch on the front wall, one on the back: SRI/MRM aim the front one at the peer; in NM the
  chain neighbours sit on both sides. **One E01 module per patch** (two per box): no RF switch or RF
  layout on our PCB, both radios can listen at once (selection diversity), TX on the one facing the peer.
- Patch for 2.44 GHz on FR4 (εr 4.4, h 1.6 mm, transmission-line model): **W 37.4 × L 28.9 mm**,
  inset-fed 50 Ω. Expected 4–5 dBi on FR4 [ASSUMPTION, model uses 4.5]. Bandwidth only ~27 MHz
  (VSWR < 2) while FR4 εr ±0.2 moves f0 by **±55 MHz**, and the box wall pulls it lower. So: fab a
  **tuning coupon** with L = 28.0 / 28.5 / 29.0 / 29.5 / 30.0 mm, measure S11 inside the closed box on a
  VNA (Prof. Santos, RF lab), keep the length centred on our channel. It only has to cover our 1 MHz
  channel, not the whole ISM band.
- Height: the box is 7 cm tall outside; a 60 mm board needs ~62 mm inside (measure the IKEA box). If it
  does not fit, use a 55 mm ground (some gain loss) or ask whether the box may stand on edge (Atenea Q4).
- Quick Mode antenna: 2 dBi SMA whip straight on the E01 (+8 dBm e.i.r.p., no pad). Rules: antennas
  need not be final at QM.
- Review with Prof. Puente / Prof. Santos before the board order. T5 compares patch vs whip in the box.

**Alternative asked 9-Oct: one omni for RX and TX, one E01 per box.** The E01 can't reach the cap
by itself (+7 dBm out, no lower step than −18 dBm in the chip), so with a 2 dBi omni and a 1 dB pigtail
it radiates 8 dBm, 2 dB under the cap, and the receive side also loses the patch's 2.5 dB:
**−4 dB on every link** vs D-R5b.

| Build (h 0.75 m) | e.i.r.p. | SRI 70 m | MRM 260 m | MRM at 1.0 m | NM 100 m (−10 dB) |
|---|---|---|---|---|---|
| Two patches (D-R5b) | 10.0 | +27.0 | **+4.4** | +9.3 | +10.9 |
| One omni 2 dBi, pigtail | 8.0 | +23.0 | **+0.4** | +5.3 | +6.9 |
| One omni on the SMA (no pigtail) | 8.7 | +24.4 | +1.8 | +6.7 | +8.3 |
| One omni, 0 dBi in the box | 6.0 | +19.0 | −3.6 | +1.3 | +2.9 |
| One omni + 100 mW FHSS (Atenea Q3 yes) | 18.0 | +33.0 | +10.4 | +15.3 | +16.9 |

- For it: simplest hardware (no patch boards, no VNA tuning, one radio), off-the-shelf antenna, no
  aiming, covers every direction in NM.
- Against: MRM at 260 m has no margin at 0.75 m, and an omni can't reject a neighbouring team side-on
  (the 74/82 pair would need ~15–20 m between boxes instead of 10 m).
- Verdict: only if the antennas can sit at ≥ 1.0 m or Atenea allows 100 mW FHSS. A middle option
  keeps the omni's strengths: **one patch (front, for SRI/MRM) + one omni (for NM)**, one E01 each:
  +4.4 dB on MRM and all-round coverage in NM. The carrier PCB keeps footprint B for any of the three.

## D-R1 — Primary band: 869.40–869.65 MHz (centre 869.525 MHz) — *superseded 9-Oct-26 by D-R1b*

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

## D-R2 — Module: HopeRF RFM69HCW, 868 MHz variant (Semtech SX1231H) — *superseded 9-Oct-26 by D-R2b*

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
at QM is literally the one that competes. Price checked 9-Oct at Opencircuit: €13.65 excl. VAT (€16.50 incl.), 10–12 days
delivery, so it costs ~€10 per device more than a bare module (~€3–4). **Order by Fri 16 Oct** to have it for T0/T2
before a Quick Mode attempt in early November. The u.FL connector is not included (buy separately). Its 3.3 V regulator takes the
~95 mA TX bursts off the micro's supply, and it has u.FL/SMA pads. Switch to the bare RFM69HCW-868 only if the team
decides so **before** QM.

**Frozen at Quick Mode** (rules): changing the module afterwards loses the bonuses, so bench
test T2 must confirm 100 kbps PER before QM.

**Reverse if:** T2 shows 100 kbps sensitivity worse than −95 dBm, or Atenea bans the chip.
Fallback: Si4463 (same band, same link budget, more effort).

## D-R5 (initial) — Antenna for QM: λ/4 inverted-L wire, top of the box — *superseded 9-Oct-26 by D-R5b*

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

**Antenna pick (recommended 9-Oct-26, Santiago asked for one choice):**
- Quick Mode: the λ/4 wire (86 mm, inverted-L). The rules allow antennas to change after QM, and it costs nothing.
- Final: **Molex 211140-0100** flex antenna (868-870 MHz, 38 × 10 mm, u.FL, 100 mm cable, 0.3 dBi peak, > 55 %
  efficiency, omni, linear; ~$2.50 at DigiKey). It fits upright on the box wall under the lid and survives the drop
  test glued flat. Its -5 dB return loss costs ~1.7 dB of mismatch, which the +13.9 dB MRM margin absorbs.
- Buy 4 antennas + 3 u.FL SMT connectors for the Adafruit boards (the connector is not included).
- Mount: vertical, as high as possible, ≥ 2 cm from the battery and any metal, same orientation in both boxes.
- Test T5 decides it: keep the Molex if it is within 3 dB of the wire in the closed box; otherwise fall back to the
  wire or try the Taoglas FXP895. Unknown: whether it needs a ground plane. Check the datasheet, then measure.

## Open

| ID | Decision | Target |
|---|---|---|
| D-R3 | NM PHY shared by all teams: redo `NM_PHY_PROPOSAL.md` for nRF24 (250 kbps, common address, CRC-16, no auto-ACK) | draft 16-Oct, frozen by week 6 |
| D-R4 | MRM channel plan (draft above: RF_CH 24/49/74/82); agree with the 3 other teams after the WiFi survey | inter-team meeting, then week 8–9 |
| D-R5b | Patch tuning coupon + VNA, patch vs whip in T5 | week 8 |
