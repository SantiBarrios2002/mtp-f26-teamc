# Team C protocol code + PC simulator

The protocol code that will run on the Pico, plus a laptop simulator that runs that
same code against a modelled radio channel in simulated time. A 2-minute round takes
milliseconds, so protocols & software can develop, measure and break the protocol
before any hardware exists. Proposal it implements:
https://claude.ai/artifact/6g1P41SGs1eGf9Jme8YsAK

```
make test                     # build, generate sample files, run the pass/fail suite (~1 s)
./build/sim sri --loss 0.1 --blackout 20:30 --rx-reboot 40
./build/sim sri --codec none --profile 1m --file my_file.txt
./build/sim nm --nodes 12 --p1 0.2 --p2 0.8 --seed 7
```
Needs gcc, make, zlib (`libz-dev`) and Python 3 with `pypdf` (sample generator only).

## Layout

| Path | What |
|---|---|
| `src/proto_env.h` | Platform interface: `send`, `now_us`, `set_timer`, `rand32`. The protocol never touches hardware directly. |
| `src/frames.h` | Frame formats (DATA, ACK, HELLO, HELLO_ACK, END, NM_DATA), all ≤ 32 B (nRF24 payload). |
| `src/link.c` | SRI/MRM link: selective-repeat ARQ, 16-frame bursts, bitmap ACK, HELLO resume, timeouts scaled to the bit rate. |
| `src/nm_gossip.c` | Network Mode: Trickle gossip (RFC 6206), software CRC-16, no routing. |
| `src/codec.c` | deflate/inflate via the zlib API (miniz offers the same API on the Pico). |
| `sim/sim.c` | Discrete-event channel: half-duplex radios, airtime from `radio/fw` (nRF24), collisions, loss, bursts, blackout, reboots. |
| `sim/main.c` | Scenarios, competition-style scoring, CLI, test suite. |
| `tools/make_sample.py` | 10 000 × 100 B test file from the course PDFs (UTF-8 or UTF-16). |

Scoring follows the rules: at STOP the receiver's bytes are decompressed and compared with
the original, and only the first continuous run of identical lines counts.

## Results (9-Oct-26, nRF24 at 250 kbps, 32 B frames, seed 1)

Band switch to 2.4 GHz (radio/DECISIONS.md D-R1b): frames shrank from 64 B to 32 B (DATA payload
60 → 28 B, HELLO name ≤ 21, NM chunk 25 B) and the air rate rose from 100 to 250 kbps. Without
docs/ the sample generator falls back to repo text (2.62× deflate instead of 3.38×).

| Scenario | Lines | Whole file in |
|---|---|---|
| Clean channel | 10 000 | 23.9 s |
| 5 % random loss | 10 000 | 28.0 s |
| **No compression**, 5 % loss | **10 000** | 73.5 s (was 9 643 lines at 868 MHz) |
| Bursty fades (~6 % loss) | 10 000 | 29.7 s |
| 10 s jamming attack + 5 % loss | 10 000 | 38.1 s (link back 0.09 s after the attack) |
| Receiver / sender reboot at 15 s | 10 000 | 28.3 s |
| 1 Mbps profile (TEAM_1M), 5 % loss | 10 000 | 12.6 s |
| UTF-16 file (2 MB, 4.23×), 5 % loss | 10 000 | 34.9 s |
| NM, 10 nodes, 10 % / 70 % loss (1 / 2 hops) | all nodes | 12.0 s |
| NM, 10 nodes, 30 % / 95 % loss | all nodes | 42.9 s |

The 7-Oct RFM69 results (100 kbps, 64 B frames) are in git history. The simulator once caught a
real bug there: with fixed timeouts the 38.4 kbps fallback delivered 883 lines, because one frame
outlasted the receiver's idle-ACK timer. Timeouts still derive from the bit rate (T8 checks it).

## Model assumptions (replace with bench measurements)

- Turnaround RX↔TX 1 ms, 0.3 ms gap between frames in a burst (130 µs nRF24 PLL settling + FIFO load)
  (`sim.c`, `link.c`).
- Loss is a fixed probability per link (plus optional Gilbert-Elliott fades). It is not yet
  derived from the link budget or from measured PER (test T2/T4).
- Collisions destroy both frames (no capture effect). Range for hearing = range for interfering.
- Not modelled: CPU time for compression, flash erase stalls on the Pico, USB copy time.

## Not done yet

- MRM scenario with 4 teams on their own channels (near-far: adjacent-channel leakage), plus
  listen-before-talk with `nrf24_carrier()`.
- Two-radio boxes (D-R5b): receive on both E01s, send on the one facing the peer.
- Board port: implement `proto_env_t` on the Pico (`nrf24_send`, `time_us_64`, an alarm),
  call `*_on_frame` from the RX poll loop and `*_on_tx_idle` after `nrf24_send` returns.
