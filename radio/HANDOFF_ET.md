# Radio → Electronics handoff — RFM69HCW 868 MHz

From: Radio Team (Santiago, Andrian, Marcos) · To: Electronics Team (Carlos, Michele, Bikash,
Guillem) · Version 0.1, 4-Oct-26 · Status: **working baseline, pending D-R2 sign-off**
(see `DECISIONS.md`).

Per the team's division of decisions, the RT picks the transceiver and ET adapts to it and picks
the micro. This sheet lists what the micro and board must provide.

## Part

- **HopeRF RFM69HCW, 868 MHz variant** (SX1231H inside). Order the **868** version; the 433 and
  915 versions have different matching networks.
- The bare module is a 16 × 16 mm SMD with 2 mm pad pitch (check against the datasheet before
  layout), so it is not breadboard-friendly. For QM and the bench, a breakout with the module
  already soldered is easier: e.g. Adafruit "RFM69HCW 868/915 MHz" (PID 3070, 3.3 V regulator and
  5 V level shifting on board). Check distributor stock and price.
- Quantity: one per box plus spares. Suggest 4 (2 boxes + 2 spares) so a damaged module doesn't
  stop testing. Buy from a distributor, not a marketplace: clones exist.

## Electrical interface

| Signal | Direction (micro) | Notes |
|---|---|---|
| 3.3V | supply | **3.3 V only, not 5 V tolerant.** Budget **≥ 120 mA peak** (≈95 mA TX at +17 dBm). Put 10 µF + 100 nF close to the pin. |
| GND | — | Solid ground under the module |
| SCK, MOSI, MISO | SPI | **Mode 0**, MSB first, ≤ 10 MHz (4–8 MHz is plenty: 100 kbps is 12.5 kB/s) |
| NSS | out | Chip select, active low; the driver toggles it per register access |
| RESET | out | **Active HIGH.** Keep low in normal operation; the driver pulses it high for 1 ms at start-up |
| DIO0 | in (IRQ-capable) | PacketSent in TX / PayloadReady in RX. Driver polls today, can switch to IRQ |
| DIO1 | in (optional) | FIFO level, only needed if we ever stream frames > 64 B |
| DIO2–DIO5 | n.c. | |
| ANT | — | 50 Ω. Route to a short wire antenna or a u.FL connector. Keep the trace short and away from the micro and USB |

## What the micro needs (radio side only)

- One SPI master plus 3 GPIOs (NSS, RESET, DIO0); 4 with DIO1.
- A millisecond tick and a busy-wait delay: these are all the driver's HAL needs (`fw/rfm69.h`).
- Throughput: the radio peaks at 12.5 kB/s, so any modern micro keeps up. Micro choice is ET's.
- RT suggestion (7-Oct, not a decision): Raspberry Pi Pico 2 (non-W) on a custom carrier PCB,
  see https://claude.ai/artifact/81s3ZPuHznAvgXU9zWjaWb (source `radio/hw_proposal.html`).
- **No WiFi/BT chip on the board** (protocol ban), and no micro with an integrated radio.

## Firmware

`radio/fw/` is a portable C99 driver (no Arduino or vendor dependencies) with host tests
(`make -C radio/fw test`). The board only has to implement `rfm69_hal_t`: select, transfer,
reset, delay_ms, millis. Once the micro is chosen, the RT will write the HAL for it, or pair with
whoever writes the board support.

## Placement

- Put the module and antenna **as high in the box as possible**. Antenna height is our biggest
  link-budget lever.
- Keep the antenna away from the battery, metal and the USB cable.

## Needed back from ET

1. Micro choice and pin map (SPI instance, NSS / RESET / DIO0 pins).
2. Power budget check: battery + regulator can deliver the 120 mA TX peak without brown-out.
3. Order lead time for the modules, so T2 can run before Quick Mode.
