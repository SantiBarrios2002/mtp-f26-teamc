# Network Mode — common physical layer: draft proposal (D-R3)

From: Team C Radio Team · Version 0.1, 4-Oct-26 · **Draft for discussion with all teams.**

In NM every team's box must hear every other team's box, so we all need one shared PHY. This is
our starting proposal. It works on the three popular sub-GHz chips (SX1231/RFM69, CC1101,
Si4463), and we want to agree it early so everyone can buy hardware that supports it.

## Proposal

| Parameter | Value | Why |
|---|---|---|
| Carrier | **869.525 MHz** | Centre of 869.40–869.65 MHz: the only 868 sub-band allowing 500 mW e.r.p. and 10 % duty cycle (CNAF UN-39) |
| Modulation | 2-GFSK, BT 0.5 | Supported by all three chips |
| Bit rate | **100 kbps** | Highest rate that fits the 250 kHz sub-band with crystal drift |
| Deviation | **±50 kHz** (β = 1) | Carson bandwidth 200 kHz, 7.6 kHz guard at ±20 ppm |
| Max TX power | ≤ +17 dBm conducted | Keeps every box under 500 mW e.r.p. with any small antenna |
| Preamble | 4 bytes `0xAA` | |
| Sync word | 2 bytes **`0x2D 0xD4`** | Default on CC1101 and in the RadioHead RF69 driver, so most teams already have it |
| Length | 1 byte, variable length, counts the bytes after itself | Same convention on the three chips |
| Max length byte | 64 | Fits a 64 B FIFO without refill |
| Whitening / Manchester | **off** | The chips' whitening implementations are not guaranteed compatible |
| CRC | **hardware CRC off**; **CRC-16/CCITT-FALSE in software**, last 2 bytes of the body, big-endian | Hardware CRCs differ (SX1231: CCITT polynomial; CC1101: x¹⁶+x¹⁵+x²+1). Check value `0x29B1` for "123456789" |
| Bit order | MSB first | |

Above the PHY (addressing, header, relaying, channel access) is a protocols-team discussion.
We suggest a small fixed header (destination, source, sequence, type) inside the body.

## Interop test

Before week 6, each team brings one box. Each box sends 1 000 frames to each other box at 10 m
(bench power, 869.85 MHz to avoid the duty-cycle budget: same settings, carrier moved). Pass:
≥ 99 % received with a valid software CRC.

## Fallback

If a team's chip can't do 100 kbps: **38.4 kbps, ±38.4 kHz deviation** (β = 2), same framing.

## Duty cycle note

The 10 % limit is **per hour per box**. A box that relays continuously for 2 minutes uses about
a third of its hourly budget, so NM rounds and rehearsals have to be scheduled with that in mind.
We are asking on Atenea how the competition schedule handles it.
