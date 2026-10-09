# MTP F26 · Team C workspace

Santiago's working repository for the **MTP F26** course project at UPC (Network Upgrade III +
Sustainability competition), shared with Team C. It holds the radio team's analysis and
firmware, plus the starting protocol code and simulator handed to protocols & software.

> **Status:** on 9-Oct-26 all four teams moved to **2.4 GHz**. The radio choices here (Ebyte
> E01-ML01DP5 / nRF24L01+, 250 kbps, two patch antennas) are a **working baseline**, not yet
> signed off by the team. See [`radio/DECISIONS.md`](radio/DECISIONS.md). Hardware and protocol
> pages are **suggestions** to the electronics and protocols teams, who own those decisions.

## Read first

| Document | For |
|---|---|
| [`briefs/TeamC Radio Brief v0.4.pdf`](briefs/) | 2.4 GHz band, module (E01-ML01DP5), antennas, MRM channel plan, what to buy, link budget, test plan |
| [`briefs/TeamC Hardware Proposal v0.3.pdf`](briefs/) | Suggested board for electronics: Pico 2 + 2 × E01-ML01DP5, BoM, power |
| [`briefs/TeamC Protocol Proposal v0.3.pdf`](briefs/) | Suggested protocol for protocols & software, with simulator results |
| [`CLAUDE.md`](CLAUDE.md) | Team, calendar, status and session log (kept up to date each session) |

## Layout

| Path | What |
|---|---|
| `radio/linkbudget.py` | Course-corrected AN5142 link budget and throughput model (`python3 radio/linkbudget.py`) |
| `radio/MTP-F26_TeamC_Radio_LinkBudget.xlsx` | The same model as a workbook with live formulas (built by `build_workbook.py`) |
| `radio/AN5142_course_layout_869MHz.xlsx` | The **course** AN5142 sheet rebuilt in its own layout (course 2.4 GHz example + our old 869 MHz baseline; built by `build_course_sheet.py`) |
| `radio/phy_config.py` | nRF24 radio profiles, CNAF UN-85 and datasheet checks, e.i.r.p. per antenna; generates `radio/fw/nrf24_config.h` |
| `radio/fw/` | Portable C99 nRF24L01+ driver, CRC-16, mock radio and host tests |
| `radio/*.md` | Decisions, electronics handoff, shared NM radio settings, test plan, Atenea questions |
| `radio/*.html` | Sources of the three published pages (PDF exports in `briefs/`) |
| `proto/` | Protocol code (link with resume, NM gossip, compression) and the PC simulator. See [`proto/README.md`](proto/README.md) |
| `hw/` | Carrier PCB (KiCad), starting with [`hw/KICAD_SESSION_PROMPT.md`](hw/KICAD_SESSION_PROMPT.md) |
| `docs/` | Course material, **not included**. See [`docs/README.md`](docs/README.md) |

## Quick start

Needs `gcc`, `make`, `zlib` (`sudo apt install build-essential zlib1g-dev`) and Python 3.

```sh
make -C radio/fw test        # radio driver: 138 checks
make -C proto test           # protocol simulator: 11 scenarios in ~1 s
python3 radio/phy_config.py  # radio profiles: channel, band edge, e.i.r.p. checks
python3 radio/linkbudget.py  # link budget table
./proto/build/sim sri --loss 0.1 --blackout 20:30   # try your own scenario
```

Rebuilding the workbook needs `openpyxl` (`pip install openpyxl`).
