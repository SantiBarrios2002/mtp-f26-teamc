"""MTP-F26 Team C - RFM69HCW physical-layer profiles and register generator.

Turns the radio decisions (D-R1 band, D-R2 module, see radio/DECISIONS.md) into
SX1231/RFM69HCW register values, checks them against the CNAF UN-39 sub-band
869.40-869.65 MHz and the SX1231 datasheet limits, budgets the 10 % duty
cycle, and writes radio/fw/rfm69_config.h for the firmware. A bench profile
uses 869.70-870.00 MHz (5 mW e.r.p., no duty-cycle limit) for repeated tests.

Register formulas (SX1231 / RFM69HCW datasheet, FXOSC = 32 MHz):
    Fstep   = FXOSC / 2^19                  (61.035 Hz)
    Frf     = Fstep * RegFrf                (24 bit)
    Fdev    = Fstep * RegFdev               (14 bit)
    BitRate = FXOSC / RegBitrate            (16 bit)
    RxBw    = FXOSC / (Mant * 2^(Exp + 2))  (FSK, single-side), Mant in {16, 20, 24}
    Pout    = -14 + OutputPower dBm         (PA1 + PA2, +2..+17 dBm, no high-power mode)
Constraints checked: 0.5 <= beta = 2 Fdev / BR <= 10;  Fdev + BR/2 <= 500 kHz;
single-side RxBw >= Fdev + BR/2 + worst-case LO offset between two boxes.

Occupied bandwidth uses Carson's rule 2 (Fdev + BR/2), which over-estimates
GFSK; the bench spectrum test T13 replaces it with a measurement.

Run:  python3 radio/phy_config.py           (report + regenerate the header)
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

from linkbudget import F868, FRAMING

FXOSC = 32e6
FSTEP = FXOSC / 2 ** 19
# CNAF UN-39 sub-bands: (lo, hi, max e.r.p. dBm, duty-cycle limit or None)
BAND_G3 = (869.400e6, 869.650e6, 27.0, 0.10)   # 500 mW e.r.p., <= 10 % DC: competition band
BAND_G4 = (869.700e6, 870.000e6, 7.0, None)    # 5 mW e.r.p., no DC limit: bench / repeated tests
XTAL_PPM = 20.0       # per box [ASSUMPTION: worst case until the module's crystal spec is confirmed]
G_ANT_DBI = 0.0       # in-box antenna [ASSUMPTION, as in linkbudget.py]
L_MATCH_DB = -0.5

RXBW_MANT = {16: 0b00, 20: 0b01, 24: 0b10}
SHAPING = {"none": 0b00, "BT1.0": 0b01, "BT0.5": 0b10, "BT0.3": 0b11}

# Register addresses (SX1231 / RFM69HCW)
REG = dict(
    OPMODE=0x01, DATAMODUL=0x02, BITRATEMSB=0x03, BITRATELSB=0x04, FDEVMSB=0x05, FDEVLSB=0x06,
    FRFMSB=0x07, FRFMID=0x08, FRFLSB=0x09, VERSION=0x10, PALEVEL=0x11, OCP=0x13,
    RXBW=0x19, AFCBW=0x1A, AFCFEI=0x1E, DIOMAPPING1=0x25, IRQFLAGS1=0x27, IRQFLAGS2=0x28,
    RSSITHRESH=0x29, PREAMBLEMSB=0x2C, PREAMBLELSB=0x2D, SYNCCONFIG=0x2E, SYNCVALUE1=0x2F,
    PACKETCONFIG1=0x37, PAYLOADLENGTH=0x38, FIFOTHRESH=0x3C, PACKETCONFIG2=0x3D,
    TESTPA1=0x5A, TESTPA2=0x5C, TESTDAGC=0x6F,
)


@dataclass
class Profile:
    name: str
    purpose: str
    bitrate: float          # bps
    fdev: float             # Hz
    shaping: str = "BT0.5"
    f_center: float = F868 * 1e6
    band: tuple = BAND_G3
    preamble_b: int = 4
    sync: bytes = b"\x6c\xa3"
    whitening: bool = True
    hw_crc: bool = True
    max_len: int = 64       # length-byte value: 4 B header + 60 B payload (fits the 66 B FIFO)
    pa_dbm: int = 17        # +17 dBm: RFM69HCW max without the 1 %-DC-limited high-power mode

    # --- register-level values ------------------------------------------
    @property
    def frf_reg(self) -> int:
        return round(self.f_center / FSTEP)

    @property
    def br_reg(self) -> int:
        return round(FXOSC / self.bitrate)

    @property
    def fdev_reg(self) -> int:
        return round(self.fdev / FSTEP)

    @property
    def f_actual(self) -> float:
        return self.frf_reg * FSTEP

    @property
    def br_actual(self) -> float:
        return FXOSC / self.br_reg

    @property
    def fdev_actual(self) -> float:
        return self.fdev_reg * FSTEP

    @property
    def beta(self) -> float:
        return 2 * self.fdev_actual / self.br_actual

    @property
    def obw(self) -> float:
        """Carson occupied bandwidth (Hz)."""
        return 2 * (self.fdev_actual + self.br_actual / 2)

    @property
    def lo_offset(self) -> float:
        """Worst-case TX carrier error of one box (Hz)."""
        return self.f_actual * XTAL_PPM * 1e-6

    @property
    def rxbw_needed(self) -> float:
        """Single-side RxBw to catch a worst-case remote box without AFC."""
        return self.fdev_actual + self.br_actual / 2 + 2 * self.lo_offset

    @property
    def rxbw(self) -> tuple[float, int, int]:
        """Smallest SX1231 FSK channel filter >= rxbw_needed: (Hz, mant, exp)."""
        opts = sorted((FXOSC / (m * 2 ** (e + 2)), m, e) for m in RXBW_MANT for e in range(8))
        for bw, m, e in opts:
            if bw >= self.rxbw_needed:
                return bw, m, e
        raise ValueError(f"{self.name}: no RxBw >= {self.rxbw_needed:.0f} Hz")

    @property
    def erp_dbm(self) -> float:
        return self.pa_dbm + L_MATCH_DB + G_ANT_DBI - 2.15

    # --- checks -----------------------------------------------------------
    def checks(self) -> list[tuple[str, bool, str]]:
        lo = self.f_actual - self.obw / 2 - self.lo_offset
        hi = self.f_actual + self.obw / 2 + self.lo_offset
        b_lo, b_hi, erp_max, _ = self.band
        return [
            ("0.5 <= beta <= 10", 0.5 <= self.beta <= 10, f"beta = {self.beta:.2f}"),
            ("Fdev + BR/2 <= 500 kHz", self.fdev_actual + self.br_actual / 2 <= 500e3,
             f"{(self.fdev_actual + self.br_actual / 2) / 1e3:.1f} kHz"),
            (f"emission inside {b_lo / 1e6:.2f}-{b_hi / 1e6:.2f} MHz incl. drift", lo >= b_lo and hi <= b_hi,
             f"{lo / 1e6:.4f}-{hi / 1e6:.4f} MHz, guard {min(lo - b_lo, b_hi - hi) / 1e3:.1f} kHz"),
            (f"e.r.p. <= {10 ** (erp_max / 10):.0f} mW ({erp_max:.0f} dBm)", self.erp_dbm <= erp_max,
             f"{self.erp_dbm:.1f} dBm = {10 ** (self.erp_dbm / 10):.0f} mW"),
            ("PA in PA1+PA2 range +2..+17 dBm", 2 <= self.pa_dbm <= 17, f"{self.pa_dbm} dBm"),
            ("frame fits 66 B FIFO", 1 + self.max_len <= 66, f"1 + {self.max_len} B"),
        ]

    # --- timing -----------------------------------------------------------
    def airtime_us(self, length_byte: int) -> float:
        """On-air time of one frame: preamble + sync + length byte + body + CRC."""
        n = self.preamble_b + len(self.sync) + 1 + length_byte + (2 if self.hw_crc else 0)
        return n * 8 / self.br_actual * 1e6

    # --- registers ----------------------------------------------------------
    def registers(self) -> list[tuple[str, int, int]]:
        bw, m, e = self.rxbw
        rxbw = (0b010 << 5) | (RXBW_MANT[m] << 3) | e          # DccFreq 4 % (default)
        afcbw = (0b100 << 5) | (RXBW_MANT[m] << 3) | e         # DccFreq default for AFC; AFC off for now
        pcfg1 = 0x80 | (0x40 if self.whitening else 0) | (0x10 if self.hw_crc else 0)
        sync_cfg = 0x80 | ((len(self.sync) - 1) << 3)          # SyncOn, size, tol 0
        regs = [
            ("OPMODE", REG["OPMODE"], 0x04),                   # sequencer on, standby
            ("DATAMODUL", REG["DATAMODUL"], SHAPING[self.shaping]),   # packet, FSK, shaping
            ("BITRATEMSB", REG["BITRATEMSB"], self.br_reg >> 8),
            ("BITRATELSB", REG["BITRATELSB"], self.br_reg & 0xFF),
            ("FDEVMSB", REG["FDEVMSB"], self.fdev_reg >> 8),
            ("FDEVLSB", REG["FDEVLSB"], self.fdev_reg & 0xFF),
            ("FRFMSB", REG["FRFMSB"], self.frf_reg >> 16),
            ("FRFMID", REG["FRFMID"], (self.frf_reg >> 8) & 0xFF),
            ("FRFLSB", REG["FRFLSB"], self.frf_reg & 0xFF),
            ("PALEVEL", REG["PALEVEL"], 0x60 | (self.pa_dbm + 14)),   # PA1 + PA2
            ("OCP", REG["OCP"], 0x1A),                         # default 95 mA trim, on
            ("RXBW", REG["RXBW"], rxbw),
            ("AFCBW", REG["AFCBW"], afcbw),
            ("AFCFEI", REG["AFCFEI"], 0x00),                   # AFC off: RxBw sized for worst offset
            ("RSSITHRESH", REG["RSSITHRESH"], 0xE4),           # -114 dBm (bench-tune)
            ("PREAMBLEMSB", REG["PREAMBLEMSB"], self.preamble_b >> 8),
            ("PREAMBLELSB", REG["PREAMBLELSB"], self.preamble_b & 0xFF),
            ("SYNCCONFIG", REG["SYNCCONFIG"], sync_cfg),
        ]
        regs += [(f"SYNCVALUE{i + 1}", REG["SYNCVALUE1"] + i, b) for i, b in enumerate(self.sync)]
        regs += [
            ("PACKETCONFIG1", REG["PACKETCONFIG1"], pcfg1),    # variable length
            ("PAYLOADLENGTH", REG["PAYLOADLENGTH"], self.max_len),
            ("FIFOTHRESH", REG["FIFOTHRESH"], 0x8F),           # TX starts on FifoNotEmpty
            ("PACKETCONFIG2", REG["PACKETCONFIG2"], 0x02),     # AutoRxRestartOn, AES off
            ("TESTPA1", REG["TESTPA1"], 0x55),                 # normal PA (no +20 dBm mode)
            ("TESTPA2", REG["TESTPA2"], 0x70),
            ("TESTDAGC", REG["TESTDAGC"], 0x30),               # fading margin improvement
        ]
        return regs


PROFILES = [
    Profile("SRI_100K", "SRI/QM/MRM team-internal link: 10 000 lines in 2 min",
            bitrate=100e3, fdev=50e3),
    Profile("ROBUST_38K4", "fallback when 100 kbps fails on campus (+5 dB sensitivity)",
            bitrate=38.4e3, fdev=38.4e3),
    Profile("NM_COMMON", "draft PHY shared by all teams in NM (see nm_phy_proposal.md)",
            bitrate=100e3, fdev=50e3, sync=b"\x2d\xd4", whitening=False, hw_crc=False),
    Profile("BENCH_5MW", "bench and repeated campus tests: 869.85 MHz, 5 mW e.r.p., no duty-cycle limit",
            bitrate=100e3, fdev=50e3, f_center=869.850e6, band=BAND_G4, pa_dbm=9),
]


def duty_cycle_report(p: Profile) -> str:
    """TX seconds per box for one 120 s round at this profile, vs the hourly budget."""
    fr = FRAMING["sub-GHz FIFO radio (RFM69/Si4463/CC1101), 60 B payload"]
    t_bit = 1 / p.br_actual
    t_frame = (fr.payload_b + fr.overhead_b) * 8 * t_bit
    t_ack = fr.ack_b * 8 * t_bit
    t_cycle = fr.frames_per_ack * t_frame + t_ack + 2 * fr.turnaround_ms * 1e-3
    tx_sender = 120 * fr.frames_per_ack * t_frame / t_cycle
    tx_receiver = 120 * t_ack / t_cycle
    dc = p.band[3]
    lines = [
        f"  sender TX per 120 s round  : {tx_sender:6.1f} s",
        f"  receiver TX (ACKs) per round: {tx_receiver:6.1f} s",
    ]
    if dc is None:
        lines.append("  duty cycle                  : no limit in this sub-band")
    else:
        lines += [f"  budget per box per hour     : {dc * 3600:6.1f} s",
                  f"  full-length sender rounds/h : {dc * 3600 / tx_sender:6.2f}"]
    return "\n".join(lines)


def header_text() -> str:
    out = [
        "/* GENERATED by radio/phy_config.py - do not edit by hand. */",
        "#ifndef RFM69_CONFIG_H",
        "#define RFM69_CONFIG_H",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct { uint8_t addr; uint8_t value; } rfm69_reg_t;",
        "",
        "typedef struct {",
        "    const char *name;",
        "    const rfm69_reg_t *regs;",
        "    uint8_t n_regs;",
        "    uint32_t bitrate_bps;    /* actual, after register quantisation */",
        "    uint8_t preamble_bytes;",
        "    uint8_t sync_bytes;",
        "    uint8_t hw_crc;          /* 1: radio appends/checks CRC-16 (2 B on air) */",
        "    uint8_t max_len;         /* largest length-byte value accepted */",
        "} rfm69_profile_t;",
        "",
    ]
    for p in PROFILES:
        out.append(f"/* {p.name}: {p.purpose}")
        out.append(f" * {p.f_actual / 1e6:.6f} MHz, {p.br_actual / 1e3:.3f} kbps, Fdev {p.fdev_actual / 1e3:.3f} kHz, "
                   f"{p.shaping}, beta {p.beta:.2f}, RxBw {p.rxbw[0] / 1e3:.1f} kHz, PA +{p.pa_dbm} dBm */")
        out.append(f"static const rfm69_reg_t RFM69_REGS_{p.name}[] = {{")
        for name, addr, val in p.registers():
            out.append(f"    {{0x{addr:02X}, 0x{val:02X}}},  /* {name} */")
        out.append("};")
        out.append(f"static const rfm69_profile_t RFM69_PROFILE_{p.name} = {{")
        out.append(f'    "{p.name}", RFM69_REGS_{p.name},')
        out.append(f"    (uint8_t)(sizeof RFM69_REGS_{p.name} / sizeof RFM69_REGS_{p.name}[0]),")
        out.append(f"    {round(p.br_actual)}u, {p.preamble_b}u, {len(p.sync)}u, {int(p.hw_crc)}u, {p.max_len}u,")
        out.append("};")
        out.append("")
    out.append("#endif /* RFM69_CONFIG_H */")
    return "\n".join(out) + "\n"


def report() -> bool:
    ok_all = True
    for p in PROFILES:
        bw = p.rxbw[0]
        print(f"\n{p.name} - {p.purpose}")
        print(f"  Frf {p.f_actual / 1e6:.6f} MHz (reg 0x{p.frf_reg:06X}), BR {p.br_actual / 1e3:.3f} kbps "
              f"(0x{p.br_reg:04X}), Fdev {p.fdev_actual / 1e3:.3f} kHz (0x{p.fdev_reg:04X}), {p.shaping}")
        print(f"  Carson OBW {p.obw / 1e3:.1f} kHz, LO offset +-{p.lo_offset / 1e3:.1f} kHz/box, "
              f"RxBw needed {p.rxbw_needed / 1e3:.1f} -> {bw / 1e3:.1f} kHz")
        print(f"  frame airtime (64 B body) {p.airtime_us(64):.0f} us")
        for label, ok, detail in p.checks():
            ok_all &= ok
            print(f"  [{'OK ' if ok else 'FAIL'}] {label:46} {detail}")
        print(duty_cycle_report(p))
    return ok_all


if __name__ == "__main__":
    ok = report()
    hdr = Path(__file__).with_name("fw") / "rfm69_config.h"
    hdr.parent.mkdir(exist_ok=True)
    hdr.write_text(header_text())
    print(f"\nwrote {hdr.relative_to(Path(__file__).parent.parent)}  ({'all checks pass' if ok else 'CHECKS FAILED'})")
    raise SystemExit(0 if ok else 1)
