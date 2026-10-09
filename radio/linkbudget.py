"""MTP-F26 Team C - Radio Team link-budget and throughput model.

Reference implementation of the course-corrected Maxim AN5142 method
(course spreadsheet AN5142-link-budget_corr_211022.xls). It is the same
maths as the workbook MTP-F26_TeamC_Radio_LinkBudget.xlsx; that workbook is
the hand-in artefact, this script is for quick what-ifs and for checking the
workbook's formulas.

Conventions (as in AN5142): gains in dBi; losses entered as NEGATIVE dB and
added; powers in dBm.

Course correction (annotations on AN5142 p.2): the flat-earth formula has NO
1/2 factor:
    P_R = P_T G_T G_R (lambda / 4 pi d)^2 * (1 + a^2 - 2a cos(2 pi dd / lambda))
The course spreadsheet uses |cos| so the ground-bounce term is a worst-case
envelope (never constructive). We do the same.

Run:  python3 radio/linkbudget.py
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field, replace

C = 299_792_458.0  # m/s
K_BOLTZ = 1.38e-23  # J/K, as in the course sheet


@dataclass
class Link:
    name: str
    f_mhz: float            # carrier frequency
    p_pa_dbm: float         # PA output power
    sens_dbm: float         # receiver sensitivity at the chosen data rate
    rate_kbps: float        # air data rate the sensitivity refers to
    d_m: float              # link distance
    g_tx_dbi: float = 0.0   # incl. efficiency / detuning inside the box
    g_rx_dbi: float = 0.0
    l_match_tx: float = -0.5
    l_con_tx: float = 0.0
    l_con_rx: float = 0.0
    h_tx_m: float = 0.75    # 70 cm support [RULE "about 70 cm"] + antenna height in box [ASSUMPTION]
    h_rx_m: float = 0.75
    a: float = 1.0          # normalised ground-bounce amplitude (1 = worst case)
    l_medium: float = -1.0  # container wall loss [ASSUMPTION]
    l_multipath: float = 0.0
    l_obstruction: float = 0.0
    notes: str = ""

    # --- derived ---------------------------------------------------------
    @property
    def lam(self) -> float:
        return C / (self.f_mhz * 1e6)

    @property
    def p_t(self) -> float:
        return self.p_pa_dbm + self.l_match_tx + self.l_con_tx

    @property
    def eirp(self) -> float:
        return self.p_t + self.g_tx_dbi

    @property
    def erp(self) -> float:
        return self.eirp - 2.15

    def l_fs(self, d: float | None = None) -> float:
        d = self.d_m if d is None else d
        return 20 * math.log10(self.lam / (4 * math.pi * d))

    def delta_d(self, d: float | None = None) -> float:
        d = self.d_m if d is None else d
        return math.hypot(d, self.h_tx_m + self.h_rx_m) - math.hypot(d, self.h_tx_m - self.h_rx_m)

    def l_gb(self, d: float | None = None) -> float:
        x = 2 * math.pi * self.delta_d(d) / self.lam
        return 10 * math.log10(1 + self.a ** 2 - 2 * self.a * abs(math.cos(x)))

    def l_fe(self, d: float | None = None) -> float:
        return self.l_fs(d) + self.l_gb(d)

    def p_rx_fs(self, d: float | None = None) -> float:
        return self.eirp + self.l_fs(d) + self.l_medium + self.g_rx_dbi + self.l_con_rx

    def p_rx_fe(self, d: float | None = None) -> float:
        return (self.eirp + self.l_fe(d) + self.l_medium + self.l_multipath
                + self.l_obstruction + self.g_rx_dbi + self.l_con_rx)

    def margin_fs(self, d: float | None = None) -> float:
        return self.p_rx_fs(d) - self.sens_dbm

    def margin_fe(self, d: float | None = None) -> float:
        return self.p_rx_fe(d) - self.sens_dbm

    def max_range_fe(self, lo: float = 5.0, hi: float = 20_000.0) -> float:
        """Distance where the flat-earth margin reaches 0 dB (bisection on the
        envelope; beyond the two-ray breakpoint it decreases monotonically)."""
        if self.margin_fe(hi) > 0:
            return hi
        lo = max(lo, 4 * self.h_tx_m * self.h_rx_m / self.lam)  # start past the breakpoint
        for _ in range(100):
            mid = (lo + hi) / 2
            if self.margin_fe(mid) > 0:
                lo = mid
            else:
                hi = mid
        return lo


# ---------------------------------------------------------------------------
# Throughput: how many lines arrive in a round
# ---------------------------------------------------------------------------
@dataclass
class Framing:
    payload_b: int
    overhead_b: float       # preamble + sync + header + CRC (+ length byte)
    frames_per_ack: int     # selective-repeat window acknowledged by one ACK
    ack_b: float            # ACK frame size on air incl. its own overhead
    turnaround_ms: float    # one TX->RX or RX->TX switch (radio + MCU)
    sys_eff: float = 0.85   # SPI / FIFO servicing / USB-write losses [ASSUMPTION]

    def goodput_kbps(self, rate_kbps: float) -> float:
        t_bit = 1.0 / (rate_kbps * 1e3)
        t_frame = (self.payload_b + self.overhead_b) * 8 * t_bit
        t_ack = self.ack_b * 8 * t_bit
        t_cycle = self.frames_per_ack * t_frame + t_ack + 2 * self.turnaround_ms * 1e-3
        payload_bits = self.frames_per_ack * self.payload_b * 8
        return payload_bits / t_cycle * self.sys_eff / 1e3


def lines_delivered(goodput_kbps: float, seconds: float = 120.0, compression: float = 1.0,
                    bytes_per_line: float = 100.0, total_lines: int = 10_000) -> int:
    raw_bytes = goodput_kbps * 1e3 * seconds / 8
    return int(min(total_lines, raw_bytes * compression / bytes_per_line))


# ---------------------------------------------------------------------------
# Scenarios (datasheet values; see workbook "Candidates" sheet for sources)
# ---------------------------------------------------------------------------
F868 = 869.525   # centre of 869.40-869.65 MHz (500 mW e.r.p., 10 % DC) - CNAF UN-39
F24 = 2440.0     # mid ISM 2.4 GHz - CNAF UN-85 b) 10 mW e.i.r.p.

RFM69_38k = dict(f_mhz=F868, p_pa_dbm=17, sens_dbm=-105, rate_kbps=38.4,
                 notes="RFM69HCW +17 dBm (+20 dBm only at <=1 % duty cycle per datasheet)")
RFM69_100k = dict(f_mhz=F868, p_pa_dbm=17, sens_dbm=-100, rate_kbps=100,
                  notes="sensitivity at 100 kbps is an ESTIMATE (datasheet gives 38.4 kbps max)")
SI4463_100k = dict(f_mhz=F868, p_pa_dbm=20, sens_dbm=-104, rate_kbps=100,
                   notes="Si4463 typ -104 dBm @100 kbps GFSK (BER 0.1 %)")
CC1101_38k = dict(f_mhz=F868, p_pa_dbm=12, sens_dbm=-104, rate_kbps=38.4,
                  notes="CC1101 +12 dBm max at 868")
NRF24_1M = dict(f_mhz=F24, p_pa_dbm=0, sens_dbm=-85, rate_kbps=1000, g_tx_dbi=2, g_rx_dbi=2,
                l_match_tx=0.0, notes="nRF24L01+ 0 dBm, 2 dBi whips")
NRF24_2M = dict(NRF24_1M, sens_dbm=-82, rate_kbps=2000)
NRF24_250k = dict(NRF24_1M, sens_dbm=-94, rate_kbps=250)

# 2.4 GHz baseline (9-Oct-26, all teams on 2.4 GHz): Ebyte E01-ML01DP5 (genuine nRF24L01P + PA/LNA,
# SMA) with RF_PWR = -18 dBm -> ~+7 dBm at the SMA [ESTIMATE from RFX2401C gain, measure T0].
# Path = SMA pigtail -1 dB + 0.5 dB pad, FR4 patch 4.5 dBi [ASSUMPTION, measure T5] -> EIRP 10.0 dBm.
# The pad sits in the shared TX/RX path, so RX pays it too. Sensitivity: chip figure (LNA ignored).
F24_FHSS = 2441.0  # same band, UN-85 a) FHSS class: 100 mW e.i.r.p. - only if Atenea Q3 says yes
E01_250k = dict(f_mhz=F24, p_pa_dbm=7, l_match_tx=0.0, l_con_tx=-1.5, l_con_rx=-1.5,
                g_tx_dbi=4.5, g_rx_dbi=4.5, sens_dbm=-94, rate_kbps=250,
                notes="E01-ML01DP5 @RF_PWR -18 (~+7 dBm, est.), 1 dB pigtail + 0.5 dB pad, 4.5 dBi FR4 patch")
E01_1M = dict(E01_250k, sens_dbm=-85, rate_kbps=1000)

SCENARIOS = [
    Link("SRI 70 m | RFM69HCW 868 | 38.4 kbps", d_m=70, **RFM69_38k),
    Link("SRI 70 m | RFM69HCW 868 | 100 kbps", d_m=70, **RFM69_100k),
    Link("SRI 70 m | Si4463 868 | 100 kbps", d_m=70, **SI4463_100k),
    Link("SRI 70 m | CC1101 868 | 38.4 kbps", d_m=70, **CC1101_38k),
    Link("SRI 70 m | nRF24 2.4G | 1 Mbps", d_m=70, **NRF24_1M),
    Link("SRI 70 m | nRF24 2.4G | 2 Mbps", d_m=70, **NRF24_2M),
    Link("MRM 260 m | RFM69HCW 868 | 38.4 kbps", d_m=260, **RFM69_38k),
    Link("MRM 260 m | Si4463 868 | 100 kbps", d_m=260, **SI4463_100k),
    Link("MRM 260 m | nRF24 2.4G | 250 kbps", d_m=260, **NRF24_250k),
    Link("MRM 260 m | nRF24 2.4G | 250 kbps, 8 dBi patches",
         d_m=260, **dict(NRF24_250k, g_tx_dbi=8, g_rx_dbi=8)),
    Link("NM hop 100 m | RFM69HCW 868 | 38.4 kbps, -10 dB obstr.",
         d_m=100, l_obstruction=-10, **RFM69_38k),
    Link("NM hop 100 m | nRF24 2.4G | 250 kbps, -10 dB obstr.",
         d_m=100, l_obstruction=-10, **NRF24_250k),
    # Working baseline (4-Oct-26): RFM69HCW at 100 kbps everywhere, see radio/DECISIONS.md
    Link("MRM 260 m | RFM69HCW 868 | 100 kbps (baseline)", d_m=260, **RFM69_100k),
    Link("NM hop 100 m | RFM69HCW 868 | 100 kbps, -10 dB obstr. (baseline)",
         d_m=100, l_obstruction=-10, **RFM69_100k),
    Link("Bench 70 m | RFM69HCW 869.85 | 100 kbps, +9 dBm (BENCH_5MW)", d_m=70,
         **dict(RFM69_100k, f_mhz=869.85, p_pa_dbm=9,
                notes="869.70-870.00 MHz: 5 mW e.r.p., no duty-cycle limit; for repeated tests")),
    # 2.4 GHz baseline (9-Oct-26): E01-ML01DP5, 250 kbps everywhere, FR4 patches, see DECISIONS.md D-R1b
    Link("SRI 70 m | E01-ML01DP5 2.4G | 250 kbps, patches (2.4 baseline)", d_m=70, **E01_250k),
    Link("SRI 70 m | E01-ML01DP5 2.4G | 1 Mbps, patches", d_m=70, **E01_1M),
    Link("MRM 260 m | E01-ML01DP5 2.4G | 250 kbps, patches (2.4 baseline)", d_m=260, **E01_250k),
    Link("NM hop 100 m | E01-ML01DP5 2.4G | 250 kbps, -10 dB obstr. (2.4 baseline)",
         d_m=100, l_obstruction=-10, **E01_250k),
    Link("MRM 260 m | E01-ML01DP5 2.4G | 250 kbps, 100 mW FHSS (if Atenea Q3 = yes)", d_m=260,
         **dict(E01_250k, f_mhz=F24_FHSS, p_pa_dbm=17,
                notes="UN-85 a) FHSS: 100 mW e.i.r.p.; needs >=15 hop channels + EN 300 328 adaptivity")),
]

# nRF24L01+ RX selectivity, C/I in dB at 250 kbps (Product Spec v1.0 table 8; negative = interferer
# may be that much STRONGER than the wanted signal at sensitivity + 3 dB). Chip property: LNA doesn't help.
NRF24_CI_250K = {0: 12, 1: -12, 2: -33, 3: -38, 6: -50, 25: -60}
# MRM channel plan proposal for 4 teams (RF_CH, f = 2400 + RF_CH MHz): three in the gaps between WiFi
# channels 1/6/11 at 25 MHz spacing, the fourth at the band edge, 8 MHz from its neighbour.
MRM_CHANNELS = [24, 49, 74, 82]


def ci_needed(offset_mhz: float) -> float:
    """Selectivity (dB) the nRF24 offers at this channel offset (conservative step lookup)."""
    best = NRF24_CI_250K[0]
    for off, ci in sorted(NRF24_CI_250K.items()):
        if offset_mhz >= off:
            best = ci
    return best


def near_far(wanted: "Link", r_m: float, isolation_db: float = 0.0) -> float:
    """Interferer-over-wanted (dB) at our receiver for another team's box r_m away (free space,
    same EIRP and antenna, inside the ground-bounce breakpoint so free space); isolation_db = pattern
    rejection of both antennas (side-on patches)."""
    return wanted.p_rx_fs(r_m) - isolation_db - wanted.p_rx_fe()

FRAMING = {
    # 4 B preamble + 2 B sync + 1 B len + 4 B header + 2 B CRC; ACK every 16 frames
    "sub-GHz FIFO radio (RFM69/Si4463/CC1101), 60 B payload": Framing(60, 13, 16, 20, 1.0),
    # nRF24: 1 B preamble + 5 B address + 9-bit PCF + 2 B CRC; 32 B payload max; 130 us settling
    "nRF24L01+, 32 B payload": Framing(32, 9.125, 16, 12, 0.25),
}


def report() -> None:
    print("LINK BUDGET (course-corrected AN5142, |cos| envelope, h = 0.75 m, container -1 dB)")
    hdr = f"{'scenario':58} {'EIRP':>5} {'ERP':>5} {'L_FE':>7} {'P_RFE':>7} {'sens':>5} {'M_FE':>6} {'M@-20':>6} {'Rmax':>6}"
    print(hdr)
    for s in SCENARIOS:
        print(f"{s.name:58} {s.eirp:5.1f} {s.erp:5.1f} {s.l_fe():7.1f} {s.p_rx_fe():7.1f} "
              f"{s.sens_dbm:5.0f} {s.margin_fe():6.1f} {s.margin_fe() - 20:6.1f} {s.max_range_fe():6.0f}")

    print("\nHEIGHT SENSITIVITY: flat-earth margin (dB) vs common antenna height")
    hs = [0.5, 0.75, 1.0, 1.5]
    print(f"{'scenario':58} " + " ".join(f"{h:>6}m" for h in hs))
    for s in SCENARIOS[:1] + SCENARIOS[4:5] + SCENARIOS[6:10] + [x for x in SCENARIOS if "2.4 baseline" in x.name]:
        row = [replace(s, h_tx_m=h, h_rx_m=h).margin_fe() for h in hs]
        print(f"{s.name:58} " + " ".join(f"{m:7.1f}" for m in row))

    print("\nTHROUGHPUT: lines in 120 s (cap 10 000; ~100 B/line)")
    need = 1_000_000 * 8 / 120 / 1e3
    print(f"goodput needed for the whole 1 MB file in 120 s: {need:.1f} kbps uncompressed, "
          f"{need / 2:.1f} kbps at 2x compression")
    for label, fr in FRAMING.items():
        rates = [38.4, 100, 150] if "sub-GHz" in label else [250, 1000, 2000]
        for r in rates:
            g = fr.goodput_kbps(r)
            print(f"  {label:52} @{r:6.1f} kbps -> goodput {g:6.1f} kbps | lines x1: "
                  f"{lines_delivered(g):5d}  x2: {lines_delivered(g, compression=2):5d}")

    mrm = next(s for s in SCENARIOS if s.name.startswith("MRM") and "2.4 baseline" in s.name)
    print("\nMRM NEAR-FAR (4 teams simultaneous): other team's box r m from our receiver vs our 260 m link")
    print(f"  nRF24 C/I at 250 kbps by offset (MHz): {NRF24_CI_250K}")
    for r in (2, 5, 10, 20):
        aligned, side = near_far(mrm, r), near_far(mrm, r, isolation_db=20)
        print(f"  r = {r:3d} m: interferer {aligned:5.1f} dB above wanted (patches aligned), "
              f"{side:5.1f} dB (side-on, ~10 dB rejection per patch)")
    chans = MRM_CHANNELS
    print(f"  channel plan RF_CH {chans} = {[2400 + c for c in chans]} MHz")
    for a, b in zip(chans, chans[1:]):
        off = b - a
        print(f"    {2400 + a}/{2400 + b} MHz: {off} MHz apart -> selectivity {ci_needed(off)} dB "
              f"-> co-located boxes must keep I/W below {-ci_needed(off)} dB")


if __name__ == "__main__":
    report()
