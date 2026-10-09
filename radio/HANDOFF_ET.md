# Radio → Electronics handoff — Ebyte E01-ML01DP5 (nRF24L01+ PA/LNA), 2.4 GHz

From: Radio Team (Santiago, Andrian, Marcos) · To: Electronics Team (Carlos, Michele, Bikash,
Guillem) · Version 0.2, 9-Oct-26 (v0.1 was the RFM69 at 868 MHz, superseded: all teams moved to
2.4 GHz) · Status: **RT proposal, pending D-R2b sign-off** (see `DECISIONS.md`).

Per the team's division of decisions, the RT picks the transceiver and ET adapts to it and picks
the micro. This sheet lists what the micro and board must provide.

## Part

- **Ebyte E01-ML01DP5**: genuine Nordic nRF24L01P + PA/LNA, **SMA-K** antenna connector, 2 × 4 pin
  header at 2.54 mm. **Not** the E01C-* parts (Si24R1 clone, per Ebyte's own manual) and not
  no-name "nRF24L01+PA+LNA" boards.
- **Two per box** (D-R5b: one per patch antenna, front and back wall). If the team picks the
  single-omni option instead, one per box and leave the second footprint empty.
- SMD alternative with u.FL: E01-ML01SP4 (14.85 × 18 mm, 1.27 mm pitch), same driver.
- Quantity on the order (D-R2b): 10 × E01-ML01DP5, 2 × E01-ML01SP4.

## Electrical interface (verify the pin order against the Ebyte datasheet before layout)

| Pin | Signal | Direction (micro) | Notes |
|---|---|---|---|
| 1 | GND | — | Solid ground under the module |
| 2 | VCC | supply | **2.0–3.6 V only.** Not from the battery rail (Li-ion reaches 4.2 V). Budget **~130 mA peak per module in TX**, ~26 mA in RX |
| 3 | CE | out | High = RX on / pulse ≥ 10 µs = send one frame |
| 4 | CSN | out | SPI chip select, active low |
| 5 | SCK | SPI | **Mode 0**, MSB first, ≤ 10 MHz (8 MHz is plenty: 250 kbps = 31 kB/s) |
| 6 | MOSI | SPI | |
| 7 | MISO | SPI | Tri-stated when CSN is high, so two modules can share the bus |
| 8 | IRQ | in (IRQ-capable) | Active low: TX done / RX ready. The driver polls today; IRQ is optional |

The module drives its own PA/LNA switch from the nRF24, so there is **no TXEN/RXEN** to wire.

## Power (the PA/LNA modules are known to be supply-noise sensitive)

- A dedicated **3.3 V LDO for the radios** (e.g. AP2112K-3.3 or TLV75533, ≥ 500 mA) from the
  switched battery rail. Below ~3.5 V battery it tracks the battery minus dropout, still inside
  the 2.0–3.6 V range.
- At each module: **100 µF bulk + 10 µF + 100 nF** right at VCC/GND.
- Only one module transmits at a time (firmware), so the radio peak is ~130 + 26 mA.

## What the micro needs (radio side only)

- One SPI master shared by both modules, plus per module CSN, CE and IRQ: **3 + 6 GPIOs**.
- A microsecond clock and a busy-wait delay: everything the driver's HAL needs (`fw/nrf24.h`).
- Proposed Pico 2 pins: SPI0 GP16 MISO / GP18 SCK / GP19 MOSI; radio A CSN GP17, CE GP20, IRQ GP21;
  radio B CSN GP22, CE GP14, IRQ GP15 (see `hw/KICAD_SESSION_PROMPT.md`).
- **No WiFi/BT chip on the board** (protocol ban), and no micro with an integrated radio.

## Firmware

`radio/fw/` is a portable C99 driver (no Arduino or vendor dependencies) with host tests
(`make -C radio/fw test`, 138 checks against a mock nRF24). The board only has to implement
`nrf24_hal_t`: select, transfer, ce, delay_us, micros. Register profiles come from
`radio/phy_config.py`, which also checks the 10 dBm e.i.r.p. limit.

## Placement and antennas

- Patch antennas (D-R5b) are separate ~60 × 60 mm FR4 boards standing on the front and back walls,
  vertical polarisation, fed by ~10 cm SMA pigtails. Put each E01 near its wall so the pigtail is
  short, and **as high in the box as possible** (antenna height is our biggest link-budget lever).
- Keep the modules away from the battery, the USB cable and the 5 V boost.
- Quick Mode: a 2 dBi SMA whip straight on the module is fine (rules: antennas need not be final).

## Needed back from ET

1. OK on the E01-ML01DP5 and the radio LDO, and the pin map.
2. Power budget check: battery + regulators deliver the TX peak without brown-out.
3. The box inside height: a 60 mm patch board needs ~62 mm.
