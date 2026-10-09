"""Build MTP-F26_TeamC_Radio_LinkBudget.xlsx from the scenarios in linkbudget.py.

Every result cell is a live Excel formula; inputs are blue. Re-run after
editing SCENARIOS / FRAMING in linkbudget.py, or edit the blue cells directly
in the workbook.

    python3 radio/build_workbook.py
"""

from __future__ import annotations

import sys
from pathlib import Path

from openpyxl import Workbook
from openpyxl.comments import Comment
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter

sys.path.insert(0, str(Path(__file__).parent))
import linkbudget as lb  # noqa: E402

OUT = Path(__file__).parent / "MTP-F26_TeamC_Radio_LinkBudget.xlsx"

FONT = "Arial"
F_IN = Font(name=FONT, color="0000FF")
F_CALC = Font(name=FONT, color="000000")
F_BOLD = Font(name=FONT, bold=True)
F_LINK = Font(name=FONT, color="008000")
F_TITLE = Font(name=FONT, bold=True, size=14)
F_NOTE = Font(name=FONT, italic=True, color="555555", size=9)
FILL_KEY = PatternFill("solid", fgColor="FFFF00")
FILL_HDR = PatternFill("solid", fgColor="D9E1F2")
FILL_SEC = PatternFill("solid", fgColor="F2F2F2")
FILL_RES = PatternFill("solid", fgColor="E2EFDA")
THIN = Side(style="thin", color="BFBFBF")
BOX = Border(left=THIN, right=THIN, top=THIN, bottom=THIN)
WRAP = Alignment(wrap_text=True, vertical="top")

SRC_RFM69 = "HopeRF RFM69HCW datasheet v1.1, Tables 2/4/6 and §3.3.7 - https://cdn-shop.adafruit.com/product-files/3076/RFM69HCW-V1.1.pdf"
SRC_SI4463 = "Silicon Labs Si4463/61/60-C datasheet Rev 1.0, Table 3 (RX 915/868 MHz) - https://datasheet.octopart.com/SI4463-C2A-GM-Silicon-Labs-datasheet-78834891.pdf"
SRC_CC1101 = "TI CC1101 datasheet SWRS061I, Tables 4/7 (868/915 MHz) - https://www.ti.com/lit/ds/symlink/cc1101.pdf"
SRC_NRF24 = "Nordic nRF24L01+ Product Specification v1.0, Tables 7 and 17 - https://www.sparkfun.com/datasheets/Components/SMD/nRF24L01Pluss_Preliminary_Product_Specification_v1_0.pdf"
SRC_CC2500 = "TI CC2500 datasheet, RF receive section - https://www.ti.com/lit/ds/symlink/cc2500.pdf"
SRC_CNAF17 = "CNAF 2017 (Orden ETU/1033/2017), notas UN - https://www.boe.es/buscar/doc.php?id=BOE-A-2017-12318"
SRC_CNAF21 = "CNAF 2021 (Orden ETD/1449/2021), notas UN - https://www.boe.es/diario_boe/txt.php?id=BOE-A-2021-21346"


def style_range(ws, ref, font=None, fill=None, border=None, align=None, fmt=None):
    for row in ws[ref]:
        for c in row:
            if font:
                c.font = font
            if fill:
                c.fill = fill
            if border:
                c.border = border
            if align:
                c.alignment = align
            if fmt:
                c.number_format = fmt


# ---------------------------------------------------------------------------
# Link Budget sheet (AN5142 layout, one column per scenario)
# ---------------------------------------------------------------------------
# Per-scenario extra inputs not held in linkbudget.Link
RX_BW_KHZ = [125, 200, 200, 100, 1000, 2000, 125, 200, 500, 500, 125, 500, 200, 200, 200,
             500, 1000, 500, 500, 500]
NF_DB = [7, 7, 7, 7, 10, 10, 7, 7, 10, 10, 7, 10, 7, 7, 7,
         10, 10, 10, 10, 10]  # E01: chip NF kept (LNA ignored, conservative)
REG = {  # (round, radio, limit type, limit dBm, sensitivity source)
    lb.F868: ("ERP", 27.0),   # 500 mW e.r.p., 869.40-869.65 MHz, UN-39
    lb.F24: ("EIRP", 10.0),   # 10 mW e.i.r.p., UN-85 b)
    869.85: ("ERP", 7.0),     # 5 mW e.r.p., 869.70-870.00 MHz, UN-39 (bench, no DC limit)
    lb.F24_FHSS: ("EIRP", 20.0),  # 100 mW e.i.r.p., UN-85 a) FHSS - only if Atenea Q3 = yes
}


def sens_source(s: lb.Link) -> str:
    if "RFM69" in s.name and s.rate_kbps == 100:
        return "ESTIMATE: datasheet stops at 38.4 kbps (-105 dBm); ~-5 dB for 2x bandwidth. Measure on the bench."
    if "RFM69" in s.name:
        return "RFM69HCW datasheet: -105 dBm @38.4 kbps, FDA 40 kHz"
    if "Si4463" in s.name:
        return "Si4463 datasheet: -104 dBm typ @100 kbps GFSK (BER 0.1 %)"
    if "CC1101" in s.name:
        return "CC1101 datasheet: -104 dBm @38.4 kBaud GFSK, 100 kHz filter (PER 1 %)"
    if "E01-ML01DP5" in s.name:
        return (f"nRF24L01+ chip datasheet: {s.sens_dbm:.0f} dBm @{s.rate_kbps:g} kbps; module LNA ignored "
                "(Ebyte quotes ~-96 @250k for the sister E01-ML01SP4). Measure in T2.")
    if "nRF24" in s.name:
        return f"nRF24L01+ datasheet: {s.sens_dbm:.0f} dBm @{s.rate_kbps:g} kbps (BER 0.1 %)"
    return ""


def round_of(name: str) -> str:
    return name.split("|")[0].strip()


def radio_of(name: str) -> str:
    parts = [p.strip() for p in name.split("|")]
    return " | ".join(parts[1:])


ROWS = {}  # label key -> row number


def build_link_budget(wb):
    ws = wb.active
    ws.title = "Link Budget"
    ws["A1"] = "MTP-F26 Team C - Radio link budget (course-corrected Maxim AN5142 method)"
    ws["A1"].font = F_TITLE
    ws["A2"] = ("Blue = input, black = formula, yellow = key assumption to confirm by measurement. "
                "Losses are entered as NEGATIVE dB (AN5142 convention). Flat-earth uses the course correction "
                "(no 1/2 factor) and the course sheet's |cos| worst-case envelope.")
    ws["A2"].font = F_NOTE
    ws.merge_cells("A2:P2")

    hdr = ["Block", "Variable", "Units", "Equation / source"]
    for i, h in enumerate(hdr, start=1):
        ws.cell(row=3, column=i, value=h)
    first_col = 5
    scen = lb.SCENARIOS
    for j, s in enumerate(scen):
        ws.cell(row=3, column=first_col + j, value=f"S{j + 1}")
    style_range(ws, f"A3:{get_column_letter(first_col + len(scen) - 1)}3", font=F_BOLD, fill=FILL_HDR, border=BOX)

    r = 4
    spec = []  # (key, label, var, units, eq, kind, value_fn or formula template)

    def sec(title):
        spec.append(("_sec", title, "", "", "", "sec", None))

    def inp(key, label, var, units, eq, fn, key_assumption=False, fmt="0.0#"):
        spec.append((key, label, var, units, eq, "in_key" if key_assumption else "in", (fn, fmt)))

    def calc(key, label, var, units, eq, tmpl, fmt="0.0"):
        spec.append((key, label, var, units, eq, "calc", (tmpl, fmt)))

    sec("Scenario")
    inp("round", "Round", "", "", "Rules PDF: SRI 70 m, MRM ~260 m, NM hop (distance unknown)", lambda s, j: round_of(s.name), fmt="@")
    inp("radio", "Radio / configuration", "", "", "", lambda s, j: radio_of(s.name), fmt="@")
    inp("rate", "Air data rate", "R", "kbps", "Rate the sensitivity refers to", lambda s, j: s.rate_kbps, fmt="0.0")
    inp("regtype", "Regulatory limit type", "", "", "868: e.r.p. (UN-39); 2.4 GHz: e.i.r.p. (UN-85 b)", lambda s, j: REG[s.f_mhz][0], fmt="@")
    inp("reglim", "Regulatory power limit", "Plim", "dBm", "500 mW e.r.p. = 27 dBm; 10 mW e.i.r.p. = 10 dBm", lambda s, j: REG[s.f_mhz][1])
    sec("System variables")
    inp("f0", "Frequency", "f0", "MHz", "869.525 = centre of 869.40-869.65 MHz", lambda s, j: s.f_mhz, fmt="0.000")
    inp("c", "Speed of light", "c", "m/s", "constant", lambda s, j: lb.C, fmt="0")
    calc("lam", "Wavelength", "λ", "m", "λ = c / f0", "={c}/({f0}*1000000)", fmt="0.0000")
    sec("Transmitter")
    inp("ppa", "PA power", "PPA", "dBm", "Module setting (see Candidates sheet for limits)", lambda s, j: s.p_pa_dbm)
    inp("lmt", "TX match loss", "LMatchT", "dB", "ASSUMPTION", lambda s, j: s.l_match_tx)
    inp("lct", "TX connector/cable loss", "LConT", "dB", "0 if antenna soldered on PCB", lambda s, j: s.l_con_tx)
    calc("pt", "TX power at antenna", "PT", "dBm", "PT = PPA + LMatchT + LConT", "={ppa}+{lmt}+{lct}")
    inp("gt", "TX antenna gain (in box)", "GT", "dBi", "ASSUMPTION: incl. efficiency/detuning inside container", lambda s, j: s.g_tx_dbi, key_assumption=True)
    calc("eirp", "EIRP", "EIRP", "dBm", "EIRP = PT + GT", "={pt}+{gt}")
    calc("erp", "ERP", "ERP", "dBm", "ERP = EIRP - 2.15", "={eirp}-2.15")
    calc("regchk", "Regulatory check", "", "", "Compare ERP or EIRP with limit", '=IF(IF({regtype}="ERP",{erp},{eirp})<={reglim},"OK","OVER LIMIT")', fmt="@")
    sec("Path")
    inp("d", "Distance", "d", "m", "", lambda s, j: s.d_m, fmt="0")
    inp("htx", "TX antenna height", "hTX", "m", "70 cm support [RULE 'about 70 cm'] + position in box", lambda s, j: s.h_tx_m, key_assumption=True, fmt="0.00")
    inp("hrx", "RX antenna height", "hRX", "m", "", lambda s, j: s.h_rx_m, key_assumption=True, fmt="0.00")
    inp("a", "Ground bounce amplitude", "a", "-", "1 = worst case (course sheet default)", lambda s, j: s.a, fmt="0.00")
    calc("dd", "Reflection path Δ", "Δd", "m", "Δd = √(d²+(hTX+hRX)²) - √(d²+(hTX-hRX)²)",
         "=SQRT({d}^2+({htx}+{hrx})^2)-SQRT({d}^2+({htx}-{hrx})^2)", fmt="0.00000")
    calc("x", "Ground-bounce phase", "x", "rad", "x = 2π·Δd/λ", "=2*PI()*{dd}/{lam}", fmt="0.000")
    calc("regime", "Propagation regime", "", "", "x < π/2: smooth d^-4 region; else interference zone (|cos| creates artificial nulls)",
         '=IF({x}<PI()/2,"beyond breakpoint","interference zone")', fmt="@")
    calc("lfs", "Free space loss", "LFS", "dB", "LFS = 20·log10(λ / 4πd)", "=20*LOG10({lam}/(4*PI()*{d}))")
    calc("lgb", "Ground bounce loss", "LGB", "dB", "LGB = 10·log10(1 + a² - 2a·|cos x|)  [course-corrected, no 1/2]",
         "=10*LOG10(1+{a}^2-2*{a}*ABS(COS({x})))")
    calc("lfe", "Flat earth loss", "LFE", "dB", "LFE = LFS + LGB", "={lfs}+{lgb}")
    inp("l0", "Medium / container loss", "L0", "dB", "ASSUMPTION: IKEA 365+ PP lid/walls; measure open vs closed (T5)", lambda s, j: s.l_medium, key_assumption=True)
    inp("lmp", "Multipath loss", "LMP", "dB", "ASSUMPTION: 0 on open campus paths; AN5142 suggests ≥20 dB in cluttered areas", lambda s, j: s.l_multipath, key_assumption=True)
    inp("lobs", "Obstruction loss", "LObs", "dB", "NM: building corner ASSUMED -10 dB", lambda s, j: s.l_obstruction, key_assumption=True)
    calc("pchfs", "Power at RX antenna, free space", "PChanFS", "dBm", "= EIRP + LFS + L0", "={eirp}+{lfs}+{l0}")
    calc("pchfe", "Power at RX antenna, flat earth", "PChanFE", "dBm", "= EIRP + LFE + L0 + LMP + LObs", "={eirp}+{lfe}+{l0}+{lmp}+{lobs}")
    sec("Receiver")
    inp("gr", "RX antenna gain (in box)", "GR", "dBi", "ASSUMPTION", lambda s, j: s.g_rx_dbi, key_assumption=True)
    inp("lcr", "RX connector/cable loss", "LConR", "dB", "", lambda s, j: s.l_con_rx)
    calc("prfs", "RX power, free space", "PRFS", "dBm", "= PChanFS + GR + LConR", "={pchfs}+{gr}+{lcr}")
    calc("prfe", "RX power, flat earth", "PRFE", "dBm", "= PChanFE + GR + LConR", "={pchfe}+{gr}+{lcr}")
    inp("sens", "RX sensitivity", "Sens", "dBm", "Datasheet (see source row)", lambda s, j: s.sens_dbm, fmt="0.0")
    inp("senssrc", "Sensitivity source", "", "", "", lambda s, j: sens_source(s), fmt="@")
    sec("Results")
    calc("mfs", "Link margin, free space", "MFS", "dB", "= PRFS - Sens", "={prfs}-{sens}")
    calc("mfe", "LINK MARGIN, FLAT EARTH", "MFE", "dB", "= PRFE - Sens  (design value)", "={prfe}-{sens}")
    calc("mwhat", "Margin with what-if multipath", "", "dB", "= PRFE - LMP + (what-if LMP in D-col input) - Sens",
         "={prfe}-{lmp}+$D${self}-{sens}")
    sec("Receiver noise sanity check (AN5142 sensitivity block)")
    inp("nf", "RX noise figure", "NF", "dB", "ASSUMPTION", lambda s, j: NF_DB[j])
    inp("t0", "Operating temperature", "T0", "K", "", lambda s, j: 290, fmt="0")
    calc("te", "Effective noise temperature", "Te", "K", "Te = T0·(10^(NF/10) - 1)", "={t0}*(10^({nf}/10)-1)", fmt="0")
    inp("bw", "Receive bandwidth", "BWRX", "kHz", "ASSUMPTION: channel filter for the chosen rate", lambda s, j: RX_BW_KHZ[j], fmt="0")
    inp("tant", "Antenna temperature", "TAnt", "K", "", lambda s, j: 300, fmt="0")
    inp("k", "Boltzmann constant (mantissa)", "k", "1e-23 J/K", "k = 1.38e-23 J/K (as course sheet); exponent applied in Pn", lambda s, j: round(lb.K_BOLTZ / 1e-23, 4), fmt="0.00")
    calc("pn", "Noise power at RX", "Pn", "dBm", "Pn = 10·log10(k·(TAnt+Te)·BW) + 30", "=10*LOG10({k}*({tant}+{te})*{bw}*1000)-230+30")
    calc("snrsens", "SNR at sensitivity", "", "dB", "= Sens - Pn (should be ~8-15 dB for FSK)", "={sens}-{pn}")
    calc("snr", "SNR at PRFE", "SNR", "dB", "= PRFE - Pn", "={prfe}-{pn}")

    # first pass: assign rows
    for key, *_ in spec:
        r += 0
    row_of = {}
    rr = 4
    for item in spec:
        key = item[0]
        if key != "_sec":
            row_of[key] = rr
        rr += 1
    ROWS.update(row_of)

    rr = 4
    for key, label, var, units, eq, kind, payload in spec:
        if kind == "sec":
            ws.cell(row=rr, column=1, value=label).font = F_BOLD
            style_range(ws, f"A{rr}:{get_column_letter(first_col + len(scen) - 1)}{rr}", fill=FILL_SEC)
            rr += 1
            continue
        ws.cell(row=rr, column=1, value=label).font = F_BOLD if key in ("mfe",) else F_CALC
        ws.cell(row=rr, column=2, value=var).font = F_CALC
        ws.cell(row=rr, column=3, value=units).font = F_CALC
        if eq.startswith("="):  # a leading "=" would be stored as a (broken) formula
            eq = f"{var or label} {eq}"
        ws.cell(row=rr, column=4, value=eq).font = F_NOTE
        for j, s in enumerate(scen):
            col = first_col + j
            cl = get_column_letter(col)
            cell = ws.cell(row=rr, column=col)
            if kind.startswith("in"):
                fn, fmt = payload
                cell.value = fn(s, j)
                cell.font = F_IN
                if kind == "in_key":
                    cell.fill = FILL_KEY
            else:
                tmpl, fmt = payload
                refs = {k: f"{cl}{v}" for k, v in row_of.items()}
                refs["self"] = rr
                cell.value = tmpl.format(**refs)
                cell.font = F_BOLD if key == "mfe" else F_CALC
                if key in ("mfe", "mfs", "mwhat"):
                    cell.fill = FILL_RES
            cell.number_format = fmt
            cell.border = BOX
        rr += 1

    # what-if multipath input lives in column D of its row
    wr = row_of["mwhat"]
    ws.cell(row=wr, column=4, value=-20).font = F_IN
    ws.cell(row=wr, column=4).fill = FILL_KEY
    ws.cell(row=wr, column=4).comment = Comment(
        "What-if multipath loss (dB) applied instead of the LMP row. -20 dB is the AN5142 default "
        "for non-open paths. Change it to see the margin under other assumptions.", "Radio team")

    ws.column_dimensions["A"].width = 34
    ws.column_dimensions["B"].width = 10
    ws.column_dimensions["C"].width = 7
    ws.column_dimensions["D"].width = 48
    for j in range(len(scen)):
        ws.column_dimensions[get_column_letter(first_col + j)].width = 17
    for row in ws.iter_rows(min_row=row_of["round"], max_row=row_of["radio"]):
        for c in row[first_col - 1:]:
            c.alignment = WRAP
    ws.row_dimensions[row_of["radio"]].height = 42
    ws.row_dimensions[row_of["senssrc"]].height = 60
    for c in ws[row_of["senssrc"]][first_col - 1:]:
        c.alignment = WRAP
    ws.freeze_panes = ws.cell(row=4, column=first_col)
    return ws, row_of, first_col


# ---------------------------------------------------------------------------
# Height sweep
# ---------------------------------------------------------------------------
def build_height(wb, row_of, first_col):
    ws = wb.create_sheet("Height Sweep")
    ws["A1"] = "Flat-earth margin (dB) vs common antenna height (both ends) - beyond the breakpoint, margin grows 20·log10(h1·h2)"
    ws["A1"].font = F_BOLD
    ws["A2"] = ("Each cell recomputes the Link Budget column with hTX = hRX = height in row 4. "
                "Values in the 'interference zone' (see Link Budget regime row) can show artificial |cos| nulls.")
    ws["A2"].font = F_NOTE
    heights = [0.5, 0.75, 1.0, 1.25, 1.5]
    ws["A4"] = "Scenario  \\  height (m)"
    ws["A4"].font = F_BOLD
    for i, h in enumerate(heights):
        c = ws.cell(row=4, column=2 + i, value=h)
        c.font = F_IN
        c.number_format = "0.00"
        c.fill = FILL_HDR
    L = "'Link Budget'!"
    for j, s in enumerate(lb.SCENARIOS):
        r = 5 + j
        cl = get_column_letter(first_col + j)
        ws.cell(row=r, column=1, value=f"=\"S{j + 1}  \"&{L}{cl}{row_of['round']}&\" | \"&{L}{cl}{row_of['radio']}").font = F_LINK

        def ref(k):
            return f"{L}{cl}{row_of[k]}"
        for i in range(len(heights)):
            hc = f"{get_column_letter(2 + i)}$4"
            dd = f"(SQRT({ref('d')}^2+(2*{hc})^2)-{ref('d')})"
            f = (f"={ref('eirp')}+20*LOG10({ref('lam')}/(4*PI()*{ref('d')}))"
                 f"+10*LOG10(1+{ref('a')}^2-2*{ref('a')}*ABS(COS(2*PI()*{dd}/{ref('lam')})))"
                 f"+{ref('l0')}+{ref('lmp')}+{ref('lobs')}+{ref('gr')}+{ref('lcr')}-{ref('sens')}")
            c = ws.cell(row=r, column=2 + i, value=f)
            c.number_format = "0.0"
            c.border = BOX
    ws.column_dimensions["A"].width = 62
    for i in range(len(heights)):
        ws.column_dimensions[get_column_letter(2 + i)].width = 10


# ---------------------------------------------------------------------------
# Throughput
# ---------------------------------------------------------------------------
def build_throughput(wb):
    ws = wb.create_sheet("Throughput")
    ws["A1"] = "How many lines arrive in one round? (selective-repeat ARQ, block ACK)"
    ws["A1"].font = F_TITLE
    g = [
        ("File size", 1_000_000, "B", "RULE: about 1 MB"),
        ("Lines in file", 10_000, "lines", "RULE: about 10 000 equal-length lines"),
        ("Round duration", 120, "s", "RULE: 2 minutes (SRI, MRM)"),
        ("Compression ratio (planning)", 2.0, "x", "ASSUMPTION: measure on sample text; 1.0 = none"),
        ("Usable bandwidth at 869.40-869.65 MHz", 250, "kHz", "RULE (CNAF UN-39): whole 250 kHz may be one high-speed channel"),
    ]
    for i, (lab, v, u, n) in enumerate(g):
        r = 3 + i
        ws.cell(row=r, column=1, value=lab)
        c = ws.cell(row=r, column=2, value=v)
        c.font = F_IN
        if "Compression" in lab:
            c.fill = FILL_KEY
        ws.cell(row=r, column=3, value=u)
        ws.cell(row=r, column=4, value=n).font = F_NOTE
    ws["A9"] = "Bytes per line"
    ws["B9"] = "=B3/B4"
    ws["A10"] = "Goodput needed for the whole file, no compression"
    ws["B10"] = "=B3*8/B5/1000"
    ws["C10"] = "kbps"
    ws["A11"] = "Goodput needed for the whole file, with planning compression"
    ws["B11"] = "=B10/B6"
    ws["C11"] = "kbps"
    for r in (9, 10, 11):
        ws.cell(row=r, column=2).number_format = "0.0"
        ws.cell(row=r, column=2).fill = FILL_RES

    hdr = ["Configuration", "Air rate (kbps)", "Payload (B)", "Frame overhead (B)", "Frames per ACK",
           "ACK frame (B)", "Turnaround (ms)", "System efficiency", "Mod. index h",
           "Frame time (ms)", "Cycle time (ms)", "Goodput (kbps)", "Lines, no compression",
           "Lines, planning compression", "Occupied BW est. (kHz)", "Fits 250 kHz sub-band?"]
    for i, h in enumerate(hdr):
        c = ws.cell(row=13, column=1 + i, value=h)
        c.font = F_BOLD
        c.fill = FILL_HDR
        c.alignment = WRAP
        c.border = BOX
    sub = lb.FRAMING["sub-GHz FIFO radio (RFM69/Si4463/CC1101), 60 B payload"]
    nrf = lb.FRAMING["nRF24L01+, 32 B payload"]
    rows = [
        ("868 FSK radio @38.4 kbps", 38.4, sub, 1.0),
        ("868 FSK radio @100 kbps", 100, sub, 1.0),
        ("868 GFSK radio @150 kbps, h=0.5", 150, sub, 0.5),
        ("868 MRM shared: 5 ch x 50 kHz @19.2 kbps", 19.2, sub, 1.0),
        ("nRF24 2.4 GHz @250 kbps", 250, nrf, None),
        ("nRF24 2.4 GHz @1 Mbps", 1000, nrf, None),
        ("nRF24 2.4 GHz @2 Mbps", 2000, nrf, None),
    ]
    for i, (lab, rate, fr, h) in enumerate(rows):
        r = 14 + i
        vals = [lab, rate, fr.payload_b, fr.overhead_b, fr.frames_per_ack, fr.ack_b, fr.turnaround_ms, fr.sys_eff, h]
        for k, v in enumerate(vals):
            c = ws.cell(row=r, column=1 + k, value=v)
            c.border = BOX
            if k >= 1 and v is not None:
                c.font = F_IN
        ws.cell(row=r, column=10, value=f"=(C{r}+D{r})*8/B{r}")
        ws.cell(row=r, column=11, value=f"=E{r}*J{r}+F{r}*8/B{r}+2*G{r}")
        ws.cell(row=r, column=12, value=f"=E{r}*C{r}*8/K{r}*H{r}")
        ws.cell(row=r, column=13, value=f"=MIN($B$4,INT(L{r}*1000*$B$5/8/$B$9))")
        ws.cell(row=r, column=14, value=f"=MIN($B$4,INT(L{r}*1000*$B$5/8*$B$6/$B$9))")
        if h is not None:
            ws.cell(row=r, column=15, value=f"=B{r}*(1+I{r})")  # Carson: 2·Δf + Rb, Δf = h·Rb/2
            ws.cell(row=r, column=16, value=f'=IF(O{r}<=$B$7,"yes","NO")')
        else:
            ws.cell(row=r, column=15, value="~1000-2000 (2.4 GHz, n/a)")
            ws.cell(row=r, column=16, value="n/a")
        for col in range(10, 17):
            c = ws.cell(row=r, column=col)
            c.border = BOX
            c.number_format = "0.0" if col in (10, 11, 12, 15) else "0"
        for col in (12, 13, 14):
            ws.cell(row=r, column=col).fill = FILL_RES
    ws["A22"] = ("Notes: overhead = preamble + sync + length + 4 B header + CRC-16 (sub-GHz) or nRF24 Enhanced ShockBurst "
                 "fields (1 B preamble, 5 B address, 9-bit PCF, 2 B CRC). Turnaround = radio TX/RX switch + MCU handling "
                 "[ASSUMPTION]. System efficiency covers SPI/FIFO service and USB writes [ASSUMPTION]. Occupied bandwidth uses "
                 "Carson's rule 2Δf + Rb with Δf = h·Rb/2 (an estimate; verify on the spectrum analyser, test T13). "
                 "The MRM-shared row shows what 868 MHz gives if all teams split 869.4-869.65 MHz.")
    ws["A22"].font = F_NOTE
    ws["A22"].alignment = WRAP
    ws.merge_cells("A22:P24")
    ws.column_dimensions["A"].width = 40
    for col in range(2, 17):
        ws.column_dimensions[get_column_letter(col)].width = 12
    ws.row_dimensions[13].height = 45


# ---------------------------------------------------------------------------
# Candidates
# ---------------------------------------------------------------------------
def build_candidates(wb):
    ws = wb.create_sheet("Candidates")
    ws["A1"] = "Radio module shortlist - datasheet values (verified 2026-10-03) and compliance notes"
    ws["A1"].font = F_TITLE
    hdr = ["Module / chip", "Band(s)", "Max TX power", "TX current @max", "RX current", "Sensitivity (datasheet)",
           "Max data rate", "Limits / gotchas", "Rule compliance (standard-protocol ban)", "Fits our use",
           "Unit price", "Source"]
    for i, h in enumerate(hdr):
        c = ws.cell(row=3, column=1 + i, value=h)
        c.font = F_BOLD
        c.fill = FILL_HDR
        c.alignment = WRAP
        c.border = BOX
    rows = [
        ["HopeRF RFM69HCW (Semtech SX1231H), 868 MHz variant", "433 / 868 / 915 MHz variants",
         "+20 dBm (PA_BOOST); +17 dBm without high-power mode", "130 mA @+20; 95 mA @+17", "16 mA",
         "-118 dBm @1.2 kbps; -114 @4.8 kbps; -105 @38.4 kbps (FDA 40 kHz). No figure above 38.4 kbps.",
         "300 kbps FSK", "+20 dBm limited to 1 % duty cycle (datasheet §3.3.7) -> use +17 dBm for 2-min rounds. 66 B FIFO. No SAW filter.",
         "Proprietary FSK packet radio - LOW risk; confirm on Atenea", "SRI/NM/MRM at 868; huge libraries (RadioHead, LowPowerLab)",
         "TO VERIFY (ET to quote Mouser/DigiKey/LCSC)", SRC_RFM69],
        ["Si4463 modules (e.g. HopeRF RFM26W, NiceRF/Ebyte boards)", "142-1050 MHz",
         "+20 dBm", "85 mA @+20 dBm, 868 MHz", "~10-14 mA (datasheet)",
         "868/915: -109 dBm @40 kbps; -104 @100 kbps; -97 @500 kbps (GFSK, BER 0.1 %); -88 @1 Mbps 4GFSK",
         "500 kbps (G)FSK; 1 Mbps 4GFSK", "Best sensitivity + power of the group; no duty-cycle note found. Config via WDS-generated register sets (steeper learning curve).",
         "Proprietary radio, but the datasheet advertises IEEE 802.15.4g PHY support -> LOW-MEDIUM risk; ask Atenea", "Strongest 868 option for MRM margin",
         "TO VERIFY", SRC_SI4463],
        ["TI CC1101 modules (e.g. Ebyte E07-M1101D)", "300-348 / 387-464 / 779-928 MHz",
         "+12 dBm @868", "34 mA @+12 dBm", "~15-17 mA",
         "868: -112 dBm @1.2 kBaud; -104 @38.4; -95 @250 (540 kHz filter); -90 @500 kBaud MSK",
         "500 kBaud (600 kbps 4-FSK)", "Lowest power of the 868 options. 250 kBaud setting needs 540 kHz filter -> does NOT fit the 250 kHz sub-band; practical ceiling ~100 kbps there.",
         "Proprietary - LOW risk", "Fine for SRI; thinner MRM margin", "TO VERIFY", SRC_CC1101],
        ["Nordic nRF24L01+ (and PA/LNA variants)", "2.400-2.525 GHz",
         "0 dBm (PA/LNA modules: more, but must be backed off to <=10 dBm e.i.r.p.)", "11.3 mA @0 dBm", "~12-14 mA",
         "-82 dBm @2 Mbps; -85 @1 Mbps; -94 @250 kbps (BER 0.1 %)", "2 Mbps",
         "32 B max payload; auto-ACK/retransmit in hardware (Enhanced ShockBurst). Many clones on the market - buy from a distributor.",
         "Enhanced ShockBurst is Nordic-proprietary - LOW-MEDIUM risk; confirm on Atenea", "Speed for SRI; weak margin at 260 m",
         "TO VERIFY", SRC_NRF24],
        ["PICK 9-Oct-26: Ebyte E01-ML01DP5 (genuine nRF24L01P + PA/LNA, SMA-K, DIP 2.54 mm)", "2.400-2.525 GHz (use <=2483.5)",
         "+20 dBm; ~+7 dBm at RF_PWR -18 (ESTIMATE, PA ~25 dB gain, no bypass)", "~120 mA (sister SP4)", "~26 mA (sister SP4)",
         "chip -94 dBm @250k / -85 @1M; Ebyte ~-96 @250k for the SP4 (LNA 12 dB, NF 2.5 dB)", "2 Mbps",
         "Lowest setting ~+7 dBm: with a 4.5 dBi patch needs ~0.5-3 dB pad for 10 dBm e.i.r.p.; measure (T0). "
         "PA/LNA switched inside the module: MCU needs SPI + CE + CSN + IRQ only. 32 B payload.",
         "Proprietary (Enhanced ShockBurst) - LOW-MEDIUM risk; confirm on Atenea", "2.4 GHz baseline, all modes",
         "~US$3.9 (Tindie, EBYTE store) - VERIFY lead time to Spain",
         "https://www.cdebyte.com/products/E01-ML01DP5 ; https://voltiq.ru/datasheets/ebyte/E01-ML01SP4.pdf"],
        ["BACKUP: Ebyte E01-ML01SP4 (same radio, SMD 14.85 x 18 mm, IPEX/u.FL)", "2.4 GHz", "+20 dBm", "120 mA", "26 mA",
         "-96 dBm typ @250k (datasheet)", "2 Mbps", "1.27 mm pitch SMD: solder to the carrier PCB; u.FL to the patch.",
         "As above", "Carrier-PCB option", "VERIFY", "https://voltiq.ru/datasheets/ebyte/E01-ML01SP4.pdf"],
        ["EXCLUDE: Ebyte E01C-* and no-name 'nRF24L01+PA+LNA' boards", "2.4 GHz", "", "", "", "", "",
         "Ebyte's own E01C manual states the chip is a Si24R1 clone (worse receiver, selectivity unknown)", "",
         "Do not buy (bench only)", "",
         "https://atta.szlcsc.com/upload/public/pdf/source/20220121/F2AB78B6B3F0E91D6E5268DDE53AA4EA.pdf"],
        ["TI CC2500", "2.400-2.4835 GHz", "+1 dBm", "21.5 mA @+1 dBm", "~13-17 mA",
         "-104 dBm @2.4 kBaud; -99 @10 kBaud; -89 @250 kBaud; -83 @500 kBaud", "500 kBaud",
         "Similar class to nRF24 with more modulation control.", "Proprietary - LOW risk", "Alternative 2.4 GHz option",
         "TO VERIFY", SRC_CC2500],
        ["EXCLUDE: LoRa chips (SX127x/RFM95, SX126x, SX1280, LLCC68)", "433/868/2.4 GHz", "", "", "", "", "",
         "", "LoRa is named as banned on the Session 1 spec slide; even FSK-only use of a LoRa chip is a compliance risk",
         "Do not buy", "", "Session #1 slide 'Transceiver Specs (I)'"],
        ["EXCLUDE: radio SoCs (CC1310/CC1312, nRF52, ESP32, STM32WL...)", "", "", "", "", "", "",
         "", "Contain a microprocessor (second micro module) and/or BLE/ZigBee/LoRa stacks", "Do not buy", "",
         "Rules PDF: 'one but only one module including a microprocessor'; standard-protocol ban"],
        ["EXCLUDE: 915 MHz (US) module variants", "902-928 MHz", "", "", "", "", "", "",
         "915-921 MHz is mobile/GSM-R spectrum in Spain (CNAF UN-40), not SRD", "Buy 868 variants only", "", SRC_CNAF17],
    ]
    for i, row in enumerate(rows):
        r = 4 + i
        for k, v in enumerate(row):
            c = ws.cell(row=r, column=1 + k, value=v)
            c.alignment = WRAP
            c.border = BOX
            c.font = F_CALC
        ws.row_dimensions[r].height = 95
    widths = [30, 16, 20, 16, 12, 34, 14, 40, 34, 24, 16, 40]
    for i, w in enumerate(widths):
        ws.column_dimensions[get_column_letter(1 + i)].width = w


# ---------------------------------------------------------------------------
# Regulatory
# ---------------------------------------------------------------------------
def build_regulatory(wb):
    ws = wb.create_sheet("Regulatory")
    ws["A1"] = "Usable unlicensed bands for a 2-minute continuous round (CNAF 2017 notas UN, as the rules require)"
    ws["A1"].font = F_TITLE
    ws["A2"] = ("Text checked on 2026-10-03 against the BOE publication of CNAF 2017 and the current CNAF 2021; "
                "the limits below are identical in both. Duty cycle is assessed over a 1-hour window (ERC/REC 70-03).")
    ws["A2"].font = F_NOTE
    hdr = ["Band", "CNAF note", "Max power", "Access condition", "2-min continuous TX OK?", "Width", "Comment"]
    for i, h in enumerate(hdr):
        c = ws.cell(row=4, column=1 + i, value=h)
        c.font = F_BOLD
        c.fill = FILL_HDR
        c.border = BOX
        c.alignment = WRAP
    rows = [
        ["433.050-434.790 MHz", "UN-30", "1 mW e.r.p. (-13 dBm/10 kHz)", "none", "Yes, but 1 mW", "1.74 MHz", "Too weak."],
        ["433.050-434.040 MHz", "UN-30", "10 mW e.r.p.", "duty cycle <= 10 %", "Yes (120 s < 360 s/h)", "0.99 MHz",
         "λ/4 = 17 cm: does not fit the 15x15x7 cm box without heavy shortening."],
        ["868.000-868.600 MHz", "UN-39", "25 mW e.r.p.", "LBT+AFA (EN 300 220) or duty cycle <= 1 %", "No (1 % = 36 s/h)", "600 kHz", ""],
        ["868.700-869.200 MHz", "UN-39", "25 mW e.r.p.", "LBT+AFA or duty cycle <= 0.1 %", "No", "500 kHz", ""],
        ["869.400-869.650 MHz", "UN-39", "500 mW e.r.p.", "LBT+AFA or duty cycle <= 10 %", "YES (120 s < 360 s/h)", "250 kHz",
         "THE 868 option. 25 kHz channelisation, but the whole band may be one high-speed data channel. One 120 s SRI + one 120 s MRM = 240 s/h for the TX device: compliant."],
        ["869.700-870.000 MHz", "UN-39", "5 mW e.r.p. (no DC) / 25 mW (DC <= 1 %)", "", "5 mW only", "300 kHz", "Low power."],
        ["2400-2483.5 MHz", "UN-85 b)", "10 mW e.i.r.p.", "none (generic SRD, EN 300 440)", "YES", "83.5 MHz",
         "Room for every team on its own channel (MRM). Campus WiFi/BT share the band."],
        ["2400-2483.5 MHz", "UN-85 a)", "100 mW e.i.r.p.; 100 mW/100 kHz FHSS, 10 mW/MHz other", "mitigation techniques (EN 300 328) mandatory", "Yes, if mitigation implemented", "83.5 MHz",
         "Wideband data/RLAN class. Whether a student FHSS/LBT design qualifies is a question for Atenea."],
        ["5725-5875 MHz", "UN-130", "25 mW e.i.r.p.", "none (generic SRD, EN 300 440)", "Yes", "150 MHz",
         "Few cheap non-WiFi modules; higher path loss and container loss. Not shortlisted."],
    ]
    for i, row in enumerate(rows):
        r = 5 + i
        for k, v in enumerate(row):
            c = ws.cell(row=r, column=1 + k, value=v)
            c.alignment = WRAP
            c.border = BOX
            if "869.400" in row[0] or "UN-85 b" in row[1]:
                c.fill = FILL_RES
        ws.row_dimensions[r].height = 48
    r = 5 + len(rows) + 1
    ws.cell(row=r, column=1, value="Sources").font = F_BOLD
    ws.cell(row=r + 1, column=1, value=SRC_CNAF17)
    ws.cell(row=r + 2, column=1, value=SRC_CNAF21)
    ws.cell(row=r + 3, column=1, value="Rules PDF: 'The system will need to be compliant with limitations according to CNAF 2017 rules (Notas UN CNAF 2017)'")
    for w, col in zip([22, 10, 26, 30, 22, 10, 60], "ABCDEFG"):
        ws.column_dimensions[col].width = w


# ---------------------------------------------------------------------------
# README
# ---------------------------------------------------------------------------
def build_readme(wb):
    ws = wb.create_sheet("README", 0)
    lines = [
        ("MTP-F26 Team C - Radio Team workbook", F_TITLE),
        ("Owner: Radio Team (Santiago, Andrian, Marcos). Version 0.1, 2026-10-03.", F_CALC),
        ("", None),
        ("Sheets", F_BOLD),
        ("Link Budget - AN5142 rows (course-corrected), one column per scenario S1-S12. This is the ANNEX 'Link Budget Analysis' artefact.", F_CALC),
        ("Height Sweep - same budgets recomputed for antenna heights 0.5-1.5 m.", F_CALC),
        ("Throughput - lines delivered in 120 s per radio/data rate; occupied bandwidth check for the 250 kHz 868 sub-band.", F_CALC),
        ("Candidates - radio module shortlist with datasheet figures, gotchas and compliance notes.", F_CALC),
        ("Regulatory - CNAF 2017 bands usable for a 2-minute continuous round.", F_CALC),
        ("", None),
        ("Legend", F_BOLD),
        ("Blue text = input you may change. Black = formula. Green = link to another sheet.", F_CALC),
        ("Yellow fill = key assumption that must be replaced by a measurement (antenna gain in box, container loss, multipath, heights).", F_CALC),
        ("Green fill = results.", F_CALC),
        ("", None),
        ("Model", F_BOLD),
        ("P_R = EIRP + L_FS + L_GB + L0 + LMP + LObs + G_R + LConR. L_GB = 10log10(1 + a² - 2a|cos(2πΔd/λ)|): the course correction removes AN5142's 1/2 factor.", F_CALC),
        ("Beyond the breakpoint (x < π/2) loss grows 40 dB/decade and is almost frequency-independent: antenna HEIGHT, TX power and RX sensitivity decide the link.", F_CALC),
        ("In the interference zone the |cos| envelope produces artificial nulls; real links fade between -∞ and +6 dB there. Measure.", F_CALC),
        ("", None),
        ("How to use", F_BOLD),
        ("1. Change blue cells in 'Link Budget' (e.g. antenna gain after measuring), read the margin rows.", F_CALC),
        ("2. The same model is in radio/linkbudget.py for quick what-ifs; radio/build_workbook.py regenerates this file.", F_CALC),
        ("3. Replace every yellow assumption with a measured value (tests T2 bench PER, T4/T5 range, T13 spectrum).", F_CALC),
    ]
    for i, (t, f) in enumerate(lines):
        c = ws.cell(row=1 + i, column=1, value=t)
        if f:
            c.font = f
    ws.column_dimensions["A"].width = 140


def main():
    wb = Workbook()
    ws, row_of, first_col = build_link_budget(wb)
    build_height(wb, row_of, first_col)
    build_throughput(wb)
    build_candidates(wb)
    build_regulatory(wb)
    build_readme(wb)
    # scenario legend under the Link Budget table
    r = ws.max_row + 2
    ws.cell(row=r, column=1, value="Scenario notes").font = F_BOLD
    for j, s in enumerate(lb.SCENARIOS):
        ws.cell(row=r + 1 + j, column=1, value=f"S{j + 1}: {s.name}").font = F_CALC
        ws.cell(row=r + 1 + j, column=4, value=s.notes).font = F_NOTE
    wb.save(OUT)
    print(f"wrote {OUT}  (Link Budget rows: {row_of})")


if __name__ == "__main__":
    main()
