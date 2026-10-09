"""Generate a competition-like test file: N lines of equal length, English-like text.

The real file is unknown ("UNICODE .txt", ~1 MB, ~10 000 equal-length lines,
"return"-separated, may have redundancy). We build word-level Markov text from the
course PDFs so the compression ratio is realistic rather than flattering.

  python3 tools/make_sample.py OUT [--lines 10000] [--width 100] [--crlf] [--utf16] [--seed 1]
--width is bytes per line in UTF-8 including the terminator; --utf16 writes UTF-16LE + BOM.
"""
import argparse
import random
import re
from pathlib import Path

DOCS = Path(__file__).resolve().parents[2] / "docs"
PDFS = ["MTP F26 - Program Competition Rules - Network Upgrade III+ Sustainability.pdf",
        "Session #1-Introduction to MTP-F26.pdf"]


def _repo_text() -> str:
    """Fallback corpus when the course PDFs are not in docs/ (they are not in the repo):
    the English text of this repository's own documents and pages."""
    root = Path(__file__).resolve().parents[2]
    parts = []
    for f in sorted(root.glob("**/*.md")) + sorted(root.glob("radio/*.html")):
        if "build" in f.parts:
            continue
        t = f.read_text(encoding="utf-8", errors="ignore")
        if f.suffix == ".html":
            t = re.sub(r"<(script|style)[^>]*>.*?</\1>", " ", t, flags=re.S)
            t = re.sub(r"<[^>]+>", " ", t)
        parts.append(re.sub(r"[#|*`>_\[\]()-]+", " ", t))
    return " ".join(parts)


def corpus_words() -> list[str]:
    text = []
    try:
        import logging
        import pypdf
        logging.getLogger("pypdf").setLevel(logging.ERROR)
        for name in PDFS:
            for page in pypdf.PdfReader(DOCS / name).pages:
                text.append(page.extract_text() or "")
    except (ImportError, FileNotFoundError, OSError):
        print("course PDFs not found in docs/: using this repository's own documents as the corpus")
        text = [_repo_text()]
    words = re.sub(r"\s+", " ", " ".join(text)).split(" ")
    return [w for w in words if w and "©" not in w and not w.isdigit()]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--lines", type=int, default=10_000)
    ap.add_argument("--width", type=int, default=100)
    ap.add_argument("--crlf", action="store_true")
    ap.add_argument("--utf16", action="store_true")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--source", choices=["corpus", "markov"], default="corpus",
                    help="corpus: the course text verbatim, repeated (~3x deflate, conservative); "
                         "markov: generated text (compresses better, ~4x)")
    a = ap.parse_args()

    words = corpus_words()
    nxt: dict[str, list[str]] = {}
    for w1, w2 in zip(words, words[1:]):
        nxt.setdefault(w1, []).append(w2)
    rng = random.Random(a.seed)
    eol = "\r\n" if a.crlf else "\n"
    width = a.width - len(eol)

    def stream():
        if a.source == "corpus":
            while True:  # repeats every ~55 KB: beyond deflate's 32 KB window
                yield from words
        w = rng.choice(words)
        while True:
            yield w
            w = rng.choice(nxt.get(w) or words)

    # Fill every line completely (words may wrap mid-word): no padding, so the
    # compression ratio is not inflated by runs of spaces.
    gen = stream()
    buf = ""
    need = a.lines * width
    parts = []
    while len(buf) < need:
        parts.append(next(gen).encode("ascii", "ignore").decode() or "x")
        if len(parts) >= 4096:
            buf += " ".join(parts) + " "
            parts = []
    lines = [buf[i * width:(i + 1) * width] for i in range(a.lines)]
    text = eol.join(lines) + eol
    data = b"\xff\xfe" + text.encode("utf-16-le") if a.utf16 else text.encode("ascii")
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    Path(a.out).write_bytes(data)
    print(f"wrote {a.out}: {len(lines)} lines, {len(data)} B")


if __name__ == "__main__":
    main()
