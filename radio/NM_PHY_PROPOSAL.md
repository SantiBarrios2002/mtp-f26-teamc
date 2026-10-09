# Network Mode — common physical layer: draft proposal (D-R3)

From: Team C Radio Team · Version 0.2, 9-Oct-26 (v0.1 was the 868 MHz draft, superseded) ·
**Draft for discussion with all teams.**

All four teams now use 2.4 GHz. In NM every team's box must hear every other team's box, so we
need one shared PHY. This proposal uses plain nRF24L01+ settings: any nRF24L01+ board (with or
without PA/LNA) can join with a few register writes, and no team has to share code.

## Proposal

| Parameter | Value | Why |
|---|---|---|
| Channel | **RF_CH 80 = 2480 MHz** | Top of the band, clear of WiFi channels 1/6/11 (eduroam); not one of the four MRM channels (24/49/74/82), so NM rehearsals don't disturb MRM tests |
| Data rate | **250 kbps** (`RF_SETUP` RF_DR_LOW = 1, RF_DR_HIGH = 0) | Best sensitivity (−94 dBm chip) and best rejection of neighbours; NM moves only ~0.5 kB |
| Address | **5 bytes `C2 5A 9D 3E 71`** (`SETUP_AW` = 3; `TX_ADDR` = `RX_ADDR_P0`, written LSByte first as listed) | No 0x00/0xFF/0x55/0xAA runs; different from every team's own address |
| CRC | **hardware CRC-16** (`CONFIG` EN_CRC = 1, CRCO = 1) | Same on every nRF24L01+ |
| Payload | **dynamic length 1..32 B** (`FEATURE` = 0x05 EN_DPL + EN_DYN_ACK, `DYNPD` = 0x01, `EN_AA` = 0x01) | DPL needs ENAA on the pipe |
| ACKs | **none on air**: send every frame with `W_TX_PAYLOAD_NOACK` (0xB0), `SETUP_RETR` = 0 | Broadcast medium: Enhanced-ShockBurst ACKs from several receivers would collide |
| TX power | **≤ 10 dBm e.i.r.p.** (CNAF UN-85 b) | Bare nRF24: 0 dBm is fine with any antenna ≤ 10 dBi. PA modules (E01-ML01DP5 etc.): lowest setting (−18 dBm, ~+7 dBm out) + antenna ≤ 3 dBi, or a pad |
| Polarisation | **vertical** | Mismatched polarisation costs 10–20 dB between teams |
| Channel access | listen-before-talk with the RPD bit (> −64 dBm at the chip), random back-off | The nRF24 has no CCA in hardware; RPD needs ≥ 170 µs in RX |

Register summary (write CONFIG last, it powers up the radio): `EN_AA 01, EN_RXADDR 01, SETUP_AW 03,
SETUP_RETR 00, RF_CH 50, RF_SETUP 20 (| power bits), RX_PW_P0 20, FEATURE 05, DYNPD 01, CONFIG 0E`
(PTX) / `0F` (PRX). Our generated header: `radio/fw/nrf24_config.h`, profile `NM_COMMON`.

Above the PHY (frame header, relaying, timing) is a protocols-team discussion. Our suggestion:
the first body byte is a frame type (`0x4E` 'N' for NM data), then file id, chunk index, chunk
count, ≤ 25 B of data and a software CRC-16/CCITT-FALSE over the body (end to end across relays).

## Chips that are not nRF24L01+

- **Si24R1 clones** speak the same air format, but their receivers are weaker; fine for NM.
- **CC2500, SX1280 (GFSK) and others** can only join if they reproduce the Enhanced-ShockBurst
  frame bit for bit (preamble, 5 B address, 9-bit packet control field with the NO_ACK bit, CRC
  over address + PCF + payload). That is hard; please tell us early if your team uses one.

## Interop test

Before week 6, each team brings one box. Each box sends 1 000 frames to every other box at 10 m
on RF_CH 80. Pass: ≥ 99 % received with a valid software CRC. No duty-cycle limit at 2.4 GHz, so
the test can be repeated freely.
