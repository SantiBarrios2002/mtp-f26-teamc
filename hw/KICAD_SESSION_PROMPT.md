# Prompt for the KiCad board-design session

Paste everything below the line into a new Claude Code session started in `~/dev/mtp-f26`.

---

We're starting the **carrier PCB** for Team C's MTP F26 transceiver, using the **KiCad MCP**
tools. Read `CLAUDE.md` first: team, calendar, the decisions so far and the git rules. Then read
`radio/HANDOFF_ET.md`, `radio/DECISIONS.md` (D-R2, D-R5), `radio/hw_proposal.html` (hardware
suggestion v0.2) and the "Mechanical Features" and "Telecom and Electronics Hardware" sections of
the competition rules (`docs/`, if present locally).

**Ownership:** the electronics team (Carlos, Michele, Bikash, Guillem) owns the board and the micro
choice. This is the radio team's **draft for them to review**. Label the schematic and the PCB
"DRAFT – for ET review", and never present a choice as agreed unless `CLAUDE.md` says so.

## Hard constraints (competition rules)

- **One and only one module with a microprocessor** (the Pico 2). No other part may contain a CPU:
  no USB-host controller chips with their own MCU (e.g. CH376), no micro + radio combo boards.
- **No chip implementing WiFi, Bluetooth, ZigBee, 2G–5G**, even if unused. So no Pico W, Pico 2 W,
  ESP32 or similar.
- Everything fits **inside an IKEA 365+ box, 15 × 15 × 7 cm** (outside). Only connectors, switches
  and buttons may stick out. Operates **autonomously** on battery; no cables plugged in during a round.
- **At least one Tx/Rx switch or button** and a **pilot light that blinks in Tx/Rx**. Extra
  switches are allowed (e.g. a network-mode switch), but **no switch may indicate the position
  in the network**.
- **Drop test** from table height to concrete before MRM: no sockets, nothing loose.
- ESD-safe (IEC/EN rules cited in the PDF). Cost **≤ €150 per device**. A board made at the
  ETSETB lab counts **€15** in the BoM, and every part (screws, wire, foam) must be in the BoM.
- **Modules shown at Quick Mode are final:** the Pico 2 and the Adafruit RFM69HCW breakout must be
  the same parts on the bench, at Quick Mode and in the final board.

## Decided parts (baseline; ET may challenge)

| Function | Part | Notes |
|---|---|---|
| Micro module | **Raspberry Pi Pico 2** (RP2350, **non-W**) | Solder flat via castellations (KiCad `MCU_RaspberryPi_and_Boards` has a Pico footprint; check it matches the Pico 2) |
| Radio | **Adafruit RFM69HCW breakout 868/915 MHz, PID 3070** | Mounted by 0.1" headers. **Verify its pin order, pitch, outline and mounting holes** from Adafruit's guide and its PCB files on GitHub before drawing the footprint. It has its own 3.3 V regulator (VIN 3.3–6 V) and level shifting |
| Antenna | u.FL SMT connector on the breakout; λ/4 wire (QM) or **Molex 211140-0100** flex (final) | The antenna is off-board, on the box wall, so the PCB only needs to keep the breakout's antenna end clear and near an edge |
| USB stick port | Through-hole **USB-A receptacle** at the board edge | Host via **Pico-PIO-USB** on two GPIOs. Check the library README for the D+/D− pin rule (D− = D+ + 1) and any series resistors |
| USB ESD | **USBLC6-2SC6** on D+/D−; TVS on VBUS | |
| 5 V for the stick | **TPS61023** boost (or equivalent) + current-limited switch **TPS2051B / AP2552** (≈500 mA), enabled by a GPIO; FAULT to a GPIO | Only on while loading or saving the file. 100 µF + 100 nF on VBUS |
| Battery | 1 × **protected 18650** Li-ion in a holder with strap/zip-tie slots | Drop test |
| Charger | **MCP73831-2** (4.2 V), RPROG for ~500 mA, from a **charge-only USB-C** receptacle (CC1/CC2 5.1 kΩ to GND, no data) | Charge LED; TVS on its VBUS |
| Power path | Battery → **slide switch** (panel) → Schottky or ideal diode → Pico **VSYS** | Follow the Pico datasheet's "external supply on VSYS" guidance: the Pico has its own VBUS→VSYS diode for USB programming |
| Radio supply | Adafruit **VIN from the switched battery rail (VSYS)**, 47–100 µF bulk at VIN | Keeps ~95 mA TX bursts off the Pico's 300 mA 3V3 rail. Add a **solder jumper** to feed VIN from Pico 3V3 instead, and check the breakout's LDO dropout at 3.3 V battery |
| UI | Tx/Rx pushbutton, TX/RX role switch, SRI/MRM↔NM mode switch, activity LED (blinks), status LED | Switches/button at the board edge; RC + ESD on button lines |
| Debug | UART0 header (GP0/GP1 + GND), Pico SWD accessible, test points | Test points: VBAT, VSYS, 3V3, 5V_USB, GND |

## Proposed Pico 2 pin map (verify against the Pico 2 datasheet before using it)

| GPIO | Signal | Notes |
|---|---|---|
| GP16 | MISO ← breakout MISO | SPI0 RX |
| GP17 | CS → breakout CS | SPI0 CSn |
| GP18 | SCK → breakout SCK | SPI0 SCK |
| GP19 | MOSI → breakout MOSI | SPI0 TX |
| GP20 | RST → breakout RST | SX1231 reset is active high; **check the breakout's pull** |
| GP21 | G0 (DIO0) ← breakout | IRQ: packet sent / payload ready |
| GP22 | G1 (DIO1) ← breakout | optional |
| GP12 / GP13 | USB-A D+ / D− (PIO-USB) | consecutive pins, D− = D+ + 1 |
| GP11 | USB 5 V enable (boost EN + switch EN) | |
| GP10 | USB switch FAULT (open drain, pull-up) | |
| GP2 | Tx/Rx button (to GND, internal pull-up) | |
| GP3 | Role switch TX/RX | Atenea question Q8 pending: keep it, it can stay unpopulated |
| GP4 | Mode switch SRI/MRM ↔ NM | |
| GP6 | Activity LED (blinks in Tx/Rx, rule) | |
| GP7 | Status LED | |
| GP27 | MCP73831 STAT | with pull-up |
| GP29 / ADC3 | VSYS/3 (internal on the Pico) | battery voltage, no extra divider |
| GP0 / GP1 | UART0 TX/RX debug header | |

The radio driver (`radio/fw/rfm69.h`) only needs SPI + CS + RST + G0 + a millisecond clock, so
moving pins is fine. Record any change in `hw/DESIGN_NOTES.md`.

## Mechanical

- Board **≤ 100 × 80 mm**, 2 layers, 1.6 mm, 4 × **M3** mounting holes (3.2 mm, ≥ 3.5 mm from edges).
  Before freezing the outline, ask me for the box's inside dimensions (I'll measure).
- **USB-A, USB-C, slide switch and Tx/Rx button on one edge**, flush with the board edge, so they
  reach holes in the box wall.
- The breakout sits near the board edge that will face the top of the box. Keep copper and
  components clear of its antenna end; keep the battery and USB away from it.
- Silkscreen: "MTP F26 · Team C · carrier rev A · DRAFT", pin labels, test-point names.
- **Design rules:** the ETSETB lab's capabilities are unknown (plated vias? minimum track?).
  Start conservative: 0.4 mm signal tracks, 0.8–1.0 mm power, 0.3 mm clearance, few vias of
  0.8/0.4 mm, a ground pour on both sides. List the questions for Prof. Chávez / Prof. Jiménez in
  the design notes. Note the alternative: an external 2-layer fab (cheaper per board, but 1–2
  weeks).

## How to work

1. **Check the tools first:** KiCad version (`kicad-cli version`), and which KiCad MCP tools exist
   and what they can do (schematic editing, PCB setup, routing?). Tell me plainly what the MCP
   cannot do (e.g. autorouting) before planning around it.
2. **Research before drawing:** Pico 2 datasheet (pinout, VSYS/VBUS guidance, ADC3 = VSYS/3),
   the Adafruit 3070 guide and PCB files (pin order, dimensions, RST polarity, VIN range,
   regulator part), the Pico-PIO-USB README (pins, resistors), and the datasheets for MCP73831,
   TPS61023, TPS2051B/AP2552 and USBLC6-2SC6. Cite sources in `hw/DESIGN_NOTES.md`.
3. **Checkpoint 1:** show me the block diagram, the final pin map and the power budget (radio
   ~95 mA TX, Pico 30–50 mA, stick 50–100 mA at 5 V), and wait for my OK.
4. **Schematic** in `hw/teamc-carrier/`, hierarchical sheets: `power`, `mcu`, `radio`,
   `usb_host`, `ui`. Real part numbers in a field (MPN) on every symbol. **ERC with 0 errors**;
   explain any warning you leave.
5. **Checkpoint 2:** export the schematic (PDF/SVG) and summarise ERC; wait for my OK.
6. **PCB:** outline, holes, placement following the mechanical rules, then routing as far as
   the tools allow (power first, short USB D+/D− pair, ground pour). **DRC with 0 errors.**
   Render top/bottom and a 3D view; show me before exporting fabrication files.
7. **Outputs:** gerbers + drill (zip) only if routed and DRC-clean; `hw/BOM.csv` (ref, qty, value,
   footprint, MPN, supplier, unit price excl. VAT, total), including the €15 lab-PCB line, with
   totals with and without non-purchased parts (BoM rule); `hw/DESIGN_NOTES.md` (decisions,
   pin map, power budget, open questions for ET and the lab).
8. **Git:** edit in a worktree, small commits. Pushing early commits straight to `main` is OK
   (never force-push). Afterwards update `CLAUDE.md` (layout, status, session log), run
   `make -C radio/fw test` and `make -C proto test`, and give me the pull command for my checkout.

## Open questions to keep visible

- Atenea Q6: may the foil be opened at the USB port during setup? Board design is unaffected,
  but the USB-A position on the box wall depends on it.
- Atenea Q8: role switch allowed? Keep it on the board; it can stay unfitted.
- ET: agree the Pico 2 + Adafruit breakout, the PCB route (lab or external fab), the battery
  choice and the holder.
- Lab: design rules, plated vias, solder mask, turnaround time.
