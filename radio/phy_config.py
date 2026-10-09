"""MTP-F26 Team C - nRF24L01+ (Ebyte E01-ML01DP5) physical-layer profiles and register generator.

Turns the radio decisions (D-R1b band, D-R2b module, D-R4 channel plan, D-R5b antennas; see
radio/DECISIONS.md) into nRF24L01+ register values, checks them against CNAF UN-85 b)
(2400-2483.5 MHz, 10 mW e.i.r.p.) and the nRF24L01+ Product Specification v1.0, and writes
radio/fw/nrf24_config.h for the firmware.

Register facts (nRF24L01+ PS v1.0):
    f       = 2400 + RF_CH MHz                     RF_CH 0..125; only 0..83 are legal in Spain
    RF_SETUP  RF_DR_LOW (bit 5) / RF_DR_HIGH (bit 3): 250 kbps = 10, 1 Mbps = 00, 2 Mbps = 01
              RF_PWR (bits 2:1): -18 / -12 / -6 / 0 dBm at the chip
    Frame   = 1 B preamble + 3..5 B address + 9-bit packet control field + 0..32 B payload + 1..2 B CRC
    Dynamic payload length (DYNPD/FEATURE.EN_DPL) needs ENAA on the pipe, so ENAA_P0 is on,
    retries are 0 and every frame goes out with W_TX_PAYLOAD_NOACK (FEATURE.EN_DYN_ACK):
    no Enhanced-ShockBurst ACKs on air, our own ARQ (proto/src/link.c) does the job.

Module (Ebyte E01-ML01DP5): PA ~25 dB gain after the chip, always in line [ESTIMATE from the
RFX2401C datasheet; Ebyte publishes no table], so RF_PWR -18 dBm gives ~+7 dBm at the SMA.
Test T0 replaces E01_PA_GAIN_DB with a measurement.

Occupied bandwidth uses Carson's rule 2 (fdev + BR/2) with the datasheet deviation
(160 kHz at 250 kbps/1 Mbps, 320 kHz at 2 Mbps); the bench spectrum test T13 replaces it.

Run:  python3 radio/phy_config.py           (report + regenerate the header)
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from linkbudget import MRM_CHANNELS, NRF24_CI_250K

BAND = (2400.0e6, 2483.5e6)   # CNAF UN-85 b)
EIRP_MAX_DBM = 10.0           # 10 mW e.i.r.p.
XTAL_PPM = 60.0               # nRF24L01+ crystal requirement: +-60 ppm per box
E01_PA_GAIN_DB = 25.0         # module PA after the chip [ESTIMATE, measure in T0]
E01_PSAT_DBM = 20.0           # module saturates around +20 dBm
MAX_PAYLOAD = 32

# Antenna paths (D-R5b): gain dBi, cable/connector dB, pad dB. Same path for TX and RX.
ANTENNAS = {
    "patch": (4.5, -1.0, -0.5),   # FR4 patch on a wall board, 10 cm pigtail, 0.5 dB SMA pad
    "omni": (2.0, -1.0, 0.0),     # 2 dBi omni (QM whip, or single-omni option), no pad
}

RATES = {250_000: 0b100000, 1_000_000: 0b000000, 2_000_000: 0b001000}
FDEV = {250_000: 160e3, 1_000_000: 160e3, 2_000_000: 320e3}
RF_PWR = {-18: 0b00, -12: 0b01, -6: 0b10, 0: 0b11}

REG = dict(CONFIG=0x00, EN_AA=0x01, EN_RXADDR=0x02, SETUP_AW=0x03, SETUP_RETR=0x04, RF_CH=0x05,
           RF_SETUP=0x06, STATUS=0x07, RX_PW_P0=0x11, DYNPD=0x1C, FEATURE=0x1D)

TEAM_CH = 74          # our MRM/SRI channel: proposal, final after the inter-team meeting (D-R4)
NM_CH = 80            # NM common channel: proposal for the other teams (NM_PHY_PROPOSAL.md)
TEAM_ADDR = bytes([0xE7, 0xC3, 0x1A, 0x5D, 0x92])   # 5 B pipe address: no 0x00/0xFF/0x55/0xAA runs
NM_ADDR = bytes([0xC2, 0x5A, 0x9D, 0x3E, 0x71])     # shared by all teams in NM


@dataclass
class Profile:
    name: str
    purpose: str
    rf_ch: int
    bitrate: int = 250_000
    chip_dbm: int = -18
    address: bytes = TEAM_ADDR
    antenna: str = "patch"
    crc_bytes: int = 2
    plan: list[int] = field(default_factory=lambda: list(MRM_CHANNELS))

    @property
    def f(self) -> float:
        return 2400e6 + self.rf_ch * 1e6

    @property
    def obw(self) -> float:
        return 2 * (FDEV[self.bitrate] + self.bitrate / 2)

    @property
    def drift(self) -> float:
        return self.f * XTAL_PPM * 1e-6

    @property
    def conducted_dbm(self) -> float:
        return min(self.chip_dbm + E01_PA_GAIN_DB, E01_PSAT_DBM)

    @property
    def eirp_dbm(self) -> float:
        g, cable, pad = ANTENNAS[self.antenna]
        return self.conducted_dbm + cable + pad + g

    def airtime_us(self, payload: int) -> float:
        bits = (1 + len(self.address) + payload + self.crc_bytes) * 8 + 9
        return bits / self.bitrate * 1e6

    # --- checks -----------------------------------------------------------
    def checks(self) -> list[tuple[str, bool, str]]:
        lo = self.f - self.obw / 2 - self.drift
        hi = self.f + self.obw / 2 + self.drift
        spacing = min(b - a for a, b in zip(sorted(self.plan), sorted(self.plan)[1:]))
        return [
            ("RF_CH 0..83 (2400-2483 MHz)", 0 <= self.rf_ch <= 83, f"RF_CH {self.rf_ch} = {self.f / 1e6:.0f} MHz"),
            (f"emission inside {BAND[0] / 1e6:.1f}-{BAND[1] / 1e6:.1f} MHz incl. drift", lo >= BAND[0] and hi <= BAND[1],
             f"{lo / 1e6:.3f}-{hi / 1e6:.3f} MHz, guard {min(lo - BAND[0], BAND[1] - hi) / 1e3:.0f} kHz"),
            (f"e.i.r.p. <= {EIRP_MAX_DBM:.0f} dBm ({self.antenna})", self.eirp_dbm <= EIRP_MAX_DBM,
             f"{self.conducted_dbm:+.1f} dBm at SMA -> {self.eirp_dbm:.1f} dBm e.i.r.p."),
            ("RF_PWR is a chip setting", self.chip_dbm in RF_PWR, f"{self.chip_dbm} dBm"),
            ("5 B address, no 0x00/0xFF/0x55/0xAA first byte", len(self.address) == 5
             and self.address[0] not in (0x00, 0xFF, 0x55, 0xAA), self.address.hex(" ").upper()),
            ("CRC-16 in hardware", self.crc_bytes == 2, f"{self.crc_bytes} B"),
            ("MRM plan: neighbours >= 6 MHz apart (C/I <= -50 dB)", spacing >= 6,
             f"RF_CH {self.plan}, min {spacing} MHz -> {NRF24_CI_250K[6 if spacing < 25 else 25]} dB"),
        ]

    # --- registers ----------------------------------------------------------
    def registers(self) -> list[tuple[str, int, int]]:
        config = 0x08 | (0x04 if self.crc_bytes == 2 else 0) | 0x02   # EN_CRC, CRCO, PWR_UP, PTX
        return [
            ("EN_AA", REG["EN_AA"], 0x01),          # needed for DPL; frames go out NOACK
            ("EN_RXADDR", REG["EN_RXADDR"], 0x01),  # pipe 0 only
            ("SETUP_AW", REG["SETUP_AW"], 0x03),    # 5 B addresses
            ("SETUP_RETR", REG["SETUP_RETR"], 0x00),
            ("RF_CH", REG["RF_CH"], self.rf_ch),
            ("RF_SETUP", REG["RF_SETUP"], RATES[self.bitrate] | (RF_PWR[self.chip_dbm] << 1)),
            ("RX_PW_P0", REG["RX_PW_P0"], MAX_PAYLOAD),
            ("FEATURE", REG["FEATURE"], 0x05),      # EN_DPL | EN_DYN_ACK
            ("DYNPD", REG["DYNPD"], 0x01),          # dynamic payload on pipe 0
            ("STATUS", REG["STATUS"], 0x70),        # clear RX_DR | TX_DS | MAX_RT
            ("CONFIG", REG["CONFIG"], config),      # last: powers the radio up
        ]


PROFILES = [
    Profile("TEAM_250K", "SRI/QM/MRM team link: our channel (D-R4), 250 kbps", rf_ch=TEAM_CH),
    Profile("TEAM_1M", "SRI option at 1 Mbps (teams sequential, +18 dB at 70 m)", rf_ch=TEAM_CH, bitrate=1_000_000),
    Profile("NM_COMMON", "PHY shared by all teams in NM (see NM_PHY_PROPOSAL.md)", rf_ch=NM_CH, address=NM_ADDR,
            antenna="omni"),
    Profile("BENCH_WHIP", "bench / QM with the 2 dBi whip straight on the SMA (no pad)", rf_ch=TEAM_CH,
            antenna="omni"),
]


def header_text() -> str:
    out = [
        "/* GENERATED by radio/phy_config.py - do not edit by hand. */",
        "#ifndef NRF24_CONFIG_H",
        "#define NRF24_CONFIG_H",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct { uint8_t addr; uint8_t value; } nrf24_reg_t;",
        "",
        "typedef struct {",
        "    const char *name;",
        "    const nrf24_reg_t *regs;  /* CONFIG last: it powers the radio up */",
        "    uint8_t n_regs;",
        "    uint8_t rf_ch;            /* f = 2400 + rf_ch MHz */",
        "    uint32_t bitrate_bps;",
        "    uint8_t address[5];       /* TX_ADDR = RX_ADDR_P0, LSByte first on SPI */",
        "    uint8_t crc_bytes;",
        "    uint8_t max_len;          /* largest payload (dynamic payload length) */",
        "} nrf24_profile_t;",
        "",
    ]
    for p in PROFILES:
        out.append(f"/* {p.name}: {p.purpose}")
        out.append(f" * RF_CH {p.rf_ch} = {p.f / 1e6:.0f} MHz, {p.bitrate // 1000} kbps, RF_PWR {p.chip_dbm} dBm "
                   f"(~{p.conducted_dbm:+.0f} dBm at the E01 SMA), {p.antenna}: {p.eirp_dbm:.1f} dBm e.i.r.p. */")
        out.append(f"static const nrf24_reg_t NRF24_REGS_{p.name}[] = {{")
        for name, addr, val in p.registers():
            out.append(f"    {{0x{addr:02X}, 0x{val:02X}}},  /* {name} */")
        out.append("};")
        addr = ", ".join(f"0x{b:02X}" for b in p.address)
        out.append(f"static const nrf24_profile_t NRF24_PROFILE_{p.name} = {{")
        out.append(f'    "{p.name}", NRF24_REGS_{p.name},')
        out.append(f"    (uint8_t)(sizeof NRF24_REGS_{p.name} / sizeof NRF24_REGS_{p.name}[0]),")
        out.append(f"    {p.rf_ch}u, {p.bitrate}u, {{{addr}}}, {p.crc_bytes}u, {MAX_PAYLOAD}u,")
        out.append("};")
        out.append("")
    out.append("#endif /* NRF24_CONFIG_H */")
    return "\n".join(out) + "\n"


def report() -> bool:
    ok_all = True
    for p in PROFILES:
        t32 = p.airtime_us(MAX_PAYLOAD)
        print(f"\n{p.name} - {p.purpose}")
        print(f"  RF_CH {p.rf_ch} = {p.f / 1e6:.0f} MHz, {p.bitrate / 1e3:.0f} kbps, Carson OBW {p.obw / 1e3:.0f} kHz, "
              f"drift +-{p.drift / 1e3:.0f} kHz/box")
        print(f"  frame airtime (32 B payload) {t32:.0f} us (+130 us TX settling per frame)")
        for label, ok, detail in p.checks():
            ok_all &= ok
            print(f"  [{'OK ' if ok else 'FAIL'}] {label:52} {detail}")
    print("\nNo duty-cycle limit in 2400-2483.5 MHz (UN-85 b): no TX budget to track.")
    return ok_all


if __name__ == "__main__":
    ok = report()
    hdr = Path(__file__).with_name("fw") / "nrf24_config.h"
    hdr.parent.mkdir(exist_ok=True)
    hdr.write_text(header_text())
    print(f"\nwrote {hdr.relative_to(Path(__file__).parent.parent)}  ({'all checks pass' if ok else 'CHECKS FAILED'})")
    raise SystemExit(0 if ok else 1)
