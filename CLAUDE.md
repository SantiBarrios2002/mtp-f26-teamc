# MTP-F26 — Team C (UPC)

Santiago's workspace for the MTP F26 course project (Network Upgrade III+ Sustainability
competition). Santiago is on the **Radio Team**. This file is the shared memory between
sessions: read it first, and update the **Team**, **Status** and **Session log** sections
at the end of every session.

## Team C

| Sub-team | Members | Scope |
|---|---|---|
| RT — Radio | Santiago, Andrian, Marcos | Spectrum analysis + band choice, spectrum-sharing talks with other teams, antennas (QM → NM omni → MRM), MRM link budget, MRM multiple-access strategy |
| ET — Electronics | Carlos, Michele, Bikash, Guillem | Hardware; picks the micro and adapts to the radio module RT chooses |
| PT — Protocols/SW | Ibrahim, Guillem, Sofia | Protocol + software |
| PM / leader | Marcos | |

- ~9–10 people. Members move between RT and PT.
- Decision split: **RT picks the transceiver module and antennas; ET adapts and picks the micro.**
- Source of truth: `Project_Definition_Team_C.docx` (Ed. 00, 3-Oct-26) in the team Drive
  folder "MTP" (id `1fDriz2utaKJeJH9nbBDpE4Sf5K1nGDj2`). Writing there is visible to the
  whole team — ask Santiago before creating/changing files in it.

## Calendar

Classes and tutor meetings are on **Fridays**. Week 2 = Fri 2-Oct-26.

| Date | Milestone |
|---|---|
| Fri 16-Oct-26 | Proposed (not yet agreed) RT band + module decision; order radios + antennas (10–12 day delivery) |
| Fri 4-Dec-26 | Pretest |
| Fri 11-Dec-26 | Competition / project deadline (week 12) |
| Fri 18-Dec-26 | Presentations |

## Layout

- `docs/` — course material: competition rules PDF, Session #1 and PM slides, AN5142
  (annotated PDF + course-corrected `.xls`), CNAF poster, Project Definition example, `.mpp`
  example plan.
- `radio/linkbudget.py` — course-corrected AN5142 model (no 1/2 factor in flat-earth,
  |cos| worst-case ground bounce). `SCENARIOS` and `FRAMING` live here. Run
  `python3 radio/linkbudget.py`.
- `radio/build_workbook.py` — generates `radio/MTP-F26_TeamC_Radio_LinkBudget.xlsx` with
  live formulas (the hand-in artefact). Keep it in sync with `linkbudget.py`.
- `radio/build_course_sheet.py` → `radio/AN5142_course_layout_869MHz.xlsx`: the course AN5142 sheet
  rebuilt row for row (Ground Multipath inlined), course example as a check + SRI/MRM/NM baseline sheets.
- `radio/phy_config.py` — RFM69HCW profiles (`SRI_100K`, `ROBUST_38K4`, `NM_COMMON`,
  `BENCH_5MW`), CNAF/datasheet checks, duty-cycle budget; regenerates `radio/fw/rfm69_config.h`.
- `radio/fw/` — portable C99 RFM69 driver behind a HAL, duty-cycle guard, CRC-16, mock SX1231
  and host tests: `make -C radio/fw test` (regenerates the config first).
- `radio/DECISIONS.md` (D-R1..D-R5 records), `HANDOFF_ET.md` (interface spec for electronics),
  `NM_PHY_PROPOSAL.md` (shared PHY draft for other teams), `TEST_PLAN.md` (T0/T2/T4/T5/T13 with
  expected values), `ATENEA_QUESTIONS.md` (for Marcos to post).
- Radio brief (artifact, private — Santiago shares it with the team):
  https://claude.ai/artifact/MqP1rz4NzHmGTgJCYMYGqw — source `radio/brief.html`; edit it and
  republish to the same URL (v0.3 published 9-Oct: Adafruit breakout, antenna pick, shopping list).
- Hardware proposal for electronics (artifact, private): https://claude.ai/artifact/81s3ZPuHznAvgXU9zWjaWb
  — source `radio/hw_proposal.html` (v0.2 9-Oct: Pico 2 + Adafruit RFM69HCW carrier PCB, BoM, power, build stages).
- `hw/` — carrier PCB work. `hw/KICAD_SESSION_PROMPT.md` is the brief for the KiCad MCP design session
  (constraints, parts, pin map, power, mechanics, checkpoints, deliverables).
- `proto/` — PT protocol code (`src/`: link ARQ+resume, NM Trickle gossip, codec) and the PC
  simulator (`sim/`): `make -C proto test` (11 scenarios, ~1 s). See `proto/README.md`.
- Protocol/firmware proposal for PT (artifact, private): https://claude.ai/artifact/6g1P41SGs1eGf9Jme8YsAK
  — source `radio/pt_proposal.html` (deflate stream, SR-ARQ + resume, NM Trickle gossip, firmware modules).

## Working rules

- Conventions in the model: gains in dBi, losses as **negative** dB, powers in dBm.
- LibreOffice is not installed. `openpyxl` is not installed system-wide, so build the workbook from a
  scratch venv (`python3 -m venv <scratch>/venv && <scratch>/venv/bin/pip install openpyxl
  formulas`), then recalculate it with `formulas` and compare against `linkbudget.py`. Scratch
  venvs under /tmp disappear, so recreate it when needed.
- After changing scenarios: also extend `RX_BW_KHZ` / `NF_DB` in `build_workbook.py` (one
  entry per scenario) and add any new carrier frequency to its `REG` table.
- Before ending a session: `make -C radio/fw test` and `make -C proto test` must pass, and
  `python3 radio/phy_config.py` must report all checks OK.
- The Sept-18 `mtp-f26.zip` "advisory kit" in Downloads is **not** team work (assumes
  "Team X", wrong dates, pre-picked RFM69/nRF24/Black Pill). Use its analysis as input only;
  never present its choices as team decisions.
- Treat anything not in the Project Definition or confirmed by Santiago as a proposal.
- Git repo: https://github.com/SantiBarrios2002/mtp-f26-teamc (public, created 9-Oct-26). Early commits
  may go straight to main (Santiago, 9-Oct); never force-push. `docs/` is
  git-ignored (copyrighted course material). Put PDF exports of the pages in `briefs/` too.

## Status (keep current)

**Radio — key results (3-Oct):**
- Only two bands allow a 2-min round: **869.40–869.65 MHz** (500 mW e.r.p., 10 % DC,
  UN-39) and **2.4 GHz** (10 mW e.i.r.p., UN-85 b).
- Flat-earth margins: 868 MHz +36..+44 dB @ 70 m, +19..+21 dB @ 260 m; nRF24 +6..+9 dB
  @ 70 m, **−4.6 dB @ 260 m**.
- 868 MHz needs ≥ 100 kbps to move 10k lines in 2 min.
- RFM69HCW datasheet limits +20 dBm to 1 % duty cycle.

**Working baseline (Santiago, 4-Oct; needs Andrian + Marcos sign-off by 16-Oct):**
- D-R1 band 869.40–869.65 MHz @ 869.525 MHz · D-R2 RFM69HCW 868 (Adafruit PID 3070), +17 dBm, 100 kbps
  GFSK (Fdev 50 kHz) · D-R5 antenna: λ/4 wire for QM, Molex 211140-0100 flex (u.FL) for the final, T5 decides.
- Duty cycle binds: ~115 s TX per 2-min sending round vs 360 s/h → ≈3 rounds/h/box, tests
  included. Bench/repeat tests go to 869.70–870.00 MHz (5 mW, no limit).
- Baseline margins (workbook S13–S15): MRM 260 m +13.9 dB · NM 100 m hop with −10 dB
  obstruction +20.5 dB · bench 70 m at +9 dBm +28.7 dB. The 100 kbps sensitivity (−100 dBm)
  is still an estimate until T2.

**Open decisions:**
- [ ] Sign-off of D-R1/D-R2/D-R5 (RT + PM)
- [ ] D-R3 NM common PHY with other teams (draft ready)
- [ ] D-R4 MRM access plan (week 8–9)
- [ ] D-R5 final antenna: Molex vs wire in test T5, consultant review (week 8)

**Next up:**
- [ ] Share the radio brief + new radio/ docs with Andrian and Marcos; get sign-off
- [ ] Marcos posts `ATENEA_QUESTIONS.md`; send `HANDOFF_ET.md` to electronics
- [ ] Send `NM_PHY_PROPOSAL.md` to other teams' leaders
- [ ] Order by 16-Oct: 3 × Adafruit RFM69HCW (PID 3070), 4 × Molex 211140-0100, 3 × u.FL; then T0 + T2 before QM
- [ ] Start the KiCad session with `hw/KICAD_SESSION_PROMPT.md` (carrier PCB draft for ET)
- [ ] Write the HAL for whichever micro ET picks
- [ ] Share the hardware proposal (v0.2) with ET; get their micro choice + OK on the Adafruit breakout by 16-Oct
- [ ] Measure the IKEA 365+ box inside dimensions (the KiCad session needs them for the board outline)
- [ ] Andrian: research map or Noise & Interference sheet? (asked 9-Oct); campus interference survey is the open item
- [ ] Atenea Q6–Q9 (USB through the foil; combo boards; role switch; encoding + output name) go with the others
- [ ] Share the protocol proposal with PT (Ibrahim, Guillem, Sofia); get agreement by 16-Oct; PT owner for NM gossip talks

## Session log

Newest first. One entry per session: what was done, what was decided (and by whom),
what's pending.

### 2026-10-09
- Andrian is starting band research: data/time, bandwidth, noise and interference per band,
  EIRP, spectral efficiency, modulation. Most of it is already in the workbook. Still open:
  measuring interference on campus (SDR/analyser survey) and 4-GFSK as an option (Si4463 only).
  Offered him a research map or a Noise & Interference sheet; no answer yet.
- Created the public GitHub repo `mtp-f26-teamc` (Santiago: OK if other teams see it). Excluded `docs/`
  and added `briefs/` (PDF exports), a README, and a sample-generator fallback to repo text when
  the course PDFs are missing (2.67×; all 11 sim tests still pass).
- Antennas: the RFM69HCW has none (only a 50 Ω ANT pad). Shortlist added to D-R5: Molex 211140 flex,
  Taoglas FXP895 flex, Linx ANT-868-HETH helical, Ignion NN chip. Wire λ/4 (86 mm) stays the QM antenna.
- Santiago opened the course AN5142 .xls (f0 = 2467 MHz) thinking it was ours. Built
  `AN5142_course_layout_869MHz.xlsx`, which reproduces the course example to 2e-13 dB. The 869 MHz baseline
  gives margins of SRI +36.7, MRM +13.9 and NM +20.5 dB, matching linkbudget.py. No Excel or LibreOffice here,
  so the original .xls can't be edited with its formulas kept.
- Purchase recommendation: Adafruit RFM69HCW breakout (PID 3070) for bench + QM, kept for the final
  so the QM module stays the same. €13.65 excl. VAT at Opencircuit, 10–12 days delivery: order by 16-Oct. Recorded in D-R2.
- Antenna pick: wire for QM, Molex 211140-0100 for the final (buy 4 + 3 u.FL connectors). Recorded in D-R5.
- Radio brief v0.3 and hardware page v0.2 republished (Adafruit breakout, antenna, shopping list,
  cost ≈€55–65 with lab PCB). PDFs re-exported to `briefs/` and the Windows folder; old versions removed.
- Wrote `hw/KICAD_SESSION_PROMPT.md` for a new session to design the carrier PCB with the KiCad MCP.
- Now a git repo: background sessions edit in a worktree (`.claude/worktrees/`, ignored). Santiago
  allowed pushing early commits straight to main.
- End of day: radio driver 154 checks, sim 11/11 scenarios, phy_config all checks OK.
- **Start next session with:** the KiCad prompt, or the outcome of Fri 16-Oct (sign-off, order placed?).

### 2026-10-07
- Read the full competition rules for hardware constraints. Key ones: one micro module only; no
  WiFi/BT chip, even unused (so no Pico W/ESP32); USB file I/O during a foil-wrapped setup,
  so ≥1 MB on-board storage is needed; drop test; Tx/Rx button + blinking LED; no node-position switch.
- Published the RT hardware suggestion for ET: Pico 2 (RP2350, non-W) + RFM69HW/HCW-868 on a
  custom 2-layer carrier, PIO-USB host on USB-A, 18650 + MCP73831, 5 V boost only for the stick,
  ≈€30–55/device. It is a suggestion, since ET owns the micro choice.
- Added Atenea Q6 (USB through the foil) and Q7 (micro+radio combo boards).
- Published the RT protocol/firmware suggestion for PT. Measured compression on English text in 100 B lines:
  deflate stream ≈3.0× (UTF-16 ≈4.7×), 4 KB blocks 2.2×. At ~3× the file takes ≈40 s instead of 119 s,
  so ≈9 rounds/h fit the duty limit instead of 3. NM proposal: Trickle gossip (RFC 6206), no routing.
  Added Atenea Q8 (role switch) and Q9 (encoding, output file name).
- Built the PC protocol simulator in `proto/`, which runs the same C protocol code the Pico
  will run. All 11 tests pass, and the main scenarios were checked over 20 seeds:
  10k lines in ≤38 s at 5 % loss, ≤50 s with fades plus a 10 s jamming attack, reboots resume,
  NM far end in ≤14 s (10 nodes). Without compression, only 9 643 lines.
  The sim found a bug: fixed timeouts broke the 38.4 kbps fallback (883 lines). Timeouts now
  scale with bit rate, and the fallback delivers 10k lines in 92 s.
- Exported the 3 pages (HTML + PDF, light theme for print) to
  `C:\Users\santi\Documents\Máster\3er Cuatri\MTP\Team C briefs\`. Re-export after republishing.

### 2026-10-04
- Created this CLAUDE.md (team, calendar, layout, status, log).
- Santiago adopted the RT recommendations as the working baseline (868 MHz + RFM69HCW).
- Built `radio/phy_config.py` (register profiles, CNAF fit, duty budget), `radio/fw/` driver +
  154 passing host tests, and the decision/handoff/NM/test-plan/Atenea docs.
- Finding: duty cycle, not range or rate, is the binding constraint (≈3 rounds/h/box).
- Published radio brief v0.2 (baseline, duty budget, ET handoff, NM PHY, test plan, questions).
- Added baseline scenarios S13–S15 to `linkbudget.py` and rebuilt the workbook (15 scenarios;
  formulas recalculated with `formulas`, zero difference vs Python).
- **Start next session with:** what came out of Fri 9-Oct (sign-off? other teams' bands?
  Atenea answers? ET micro choice?), then log it and update Status.

### 2026-10-03
- Read Team C's Project Definition from Drive; mapped sub-teams and RT scope.
- Found the Sept-18 advisory zip diverges from the real PD (dates, team, decisions).
- Built `radio/` workpack: `linkbudget.py`, `build_workbook.py`, link-budget xlsx
  (formulas verified against Python), and published the radio brief artifact.
- Proposed (not decided) 16-Oct band/module decision date.
