# Team C protocol code + PC simulator

The protocol code that will run on the Pico, plus a laptop simulator that runs that
same code against a modelled radio channel in simulated time. A 2-minute round takes
milliseconds, so protocols & software can develop, measure and break the protocol
before any hardware exists. Proposal it implements:
https://claude.ai/artifact/6g1P41SGs1eGf9Jme8YsAK

```
make test                     # build, generate sample files, run the pass/fail suite (~1 s)
./build/sim sri --loss 0.1 --blackout 20:30 --rx-reboot 40
./build/sim sri --codec none --profile 38k4 --file my_file.txt
./build/sim nm --nodes 12 --p1 0.2 --p2 0.8 --seed 7
```
Needs gcc, make, zlib (`libz-dev`) and Python 3 with `pypdf` (sample generator only).

## Layout

| Path | What |
|---|---|
| `src/proto_env.h` | Platform interface: `send`, `now_us`, `set_timer`, `rand32`. The protocol never touches hardware directly. |
| `src/frames.h` | Frame formats (DATA, ACK, HELLO, HELLO_ACK, END, NM_DATA). |
| `src/link.c` | SRI/MRM link: selective-repeat ARQ, 16-frame bursts, bitmap ACK, HELLO resume, timeouts scaled to the bit rate. |
| `src/nm_gossip.c` | Network Mode: Trickle gossip (RFC 6206), software CRC-16, no routing. |
| `src/codec.c` | deflate/inflate via the zlib API (miniz offers the same API on the Pico). |
| `sim/sim.c` | Discrete-event channel: half-duplex radios, airtime from `radio/fw`, collisions, loss, bursts, blackout, reboots, real duty-cycle guard. |
| `sim/main.c` | Scenarios, competition-style scoring, CLI, test suite. |
| `tools/make_sample.py` | 10 000 × 100 B test file from the course PDFs (UTF-8 or UTF-16). |

Scoring follows the rules: at STOP the receiver's bytes are decompressed and compared with
the original, and only the first continuous run of identical lines counts.

## Results (7-Oct-26, 1 MB sample, deflate 3.38×, 20 seeds unless noted)

| Scenario | Lines | Whole file in (worst) |
|---|---|---|
| Clean channel | 10 000 | 32 s |
| 5 % random loss | 10 000 | 38 s |
| 10 % random loss | 10 000 | 44 s |
| Bursty fades + 10 s jamming attack | 10 000 | 50 s (link back 0.13 s after the attack) |
| 5 % loss + receiver reboot (15 s) + sender reboot (40 s) | 10 000 | 39 s |
| 38.4 kbps fallback profile, 5 % loss | 10 000 | 92 s |
| UTF-16 file (2 MB, 5.16×), 5 % loss | 10 000 | 49 s (1 seed) |
| **No compression**, 5 % loss | **9 643** | not finished (1 seed) |
| NM, 10 nodes, 10 % / 70 % loss (1 / 2 hops) | all nodes | 14 s |
| NM, 16 nodes, 30 % / 95 % loss | all nodes | 54 s |

The simulator caught one real bug: with fixed timeouts, the 38.4 kbps fallback delivered only
883 lines, because one frame (15.2 ms) outlasted the receiver's 15 ms idle-ACK timer, so it
ACKed in the middle of bursts and went deaf. Timeouts now derive from the bit rate.

## Model assumptions (replace with bench measurements)

- Turnaround RX↔TX 1 ms, 0.5 ms gap between frames in a burst (`sim.c`, `link.c`).
- Loss is a fixed probability per link (plus optional Gilbert-Elliott fades). It is not yet
  derived from the link budget or from measured PER (test T2/T4).
- Collisions destroy both frames (no capture effect). Range for hearing = range for interfering.
- Not modelled: CPU time for compression, flash erase stalls on the Pico, USB copy time.

## Not done yet

- MRM scenario with several teams transmitting at once, plus listen-before-talk.
- Automatic profile fallback (100 kbps → 38.4 kbps) negotiated in HELLO.
- Board port: implement `proto_env_t` on the Pico (`rfm69_send`, `time_us_64`, an alarm),
  call `*_on_frame` from the RX poll loop and `*_on_tx_idle` after `rfm69_send` returns.
