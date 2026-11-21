"""Builds <out>/compare_XX.jpg: the render (left) next to the approved reference (right)."""
import sys
from pathlib import Path
from PIL import Image

out, refs = Path(sys.argv[1]), Path(sys.argv[2])
pairs = [("main", "01_main")] + [(str(i), r) for i, r in enumerate(
    ["02_saturation", "03_resampler", "04_tape", "05_chorus",
     "06_tremolo", "07_reverse", "08_delay", "09_reverb"])]
H = 900
for render, ref in pairs:
    a, b = out / f"{render}.png", refs / f"{ref}.jpg"
    if not a.exists() or not b.exists():
        continue
    ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    ia = ia.resize((round(ia.width * H / ia.height), H), Image.LANCZOS)
    ib = ib.resize((round(ib.width * H / ib.height), H), Image.LANCZOS)
    sheet = Image.new("RGB", (ia.width + ib.width + 20, H), (20, 20, 20))
    sheet.paste(ia, (0, 0))
    sheet.paste(ib, (ia.width + 20, 0))
    sheet.save(out / f"compare_{ref}.jpg", quality=85)
