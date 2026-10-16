#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The host's PBM screenshots as one sheet: tools/montage.py build/host out.png [scale]
import sys, glob, os
from PIL import Image, ImageDraw
src, out = sys.argv[1], sys.argv[2]
scale = int(sys.argv[3]) if len(sys.argv) > 3 else 2
files = sorted(glob.glob(os.path.join(src, "*.pbm")))
cols = 4
W, H = 240 * scale, 240 * scale
rows = (len(files) + cols - 1) // cols
sheet = Image.new("L", (cols * (W + 12) + 12, rows * (H + 30) + 12), 200)
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(f).convert("L").resize((W, H), Image.NEAREST)
    x, y = 12 + (i % cols) * (W + 12), 12 + (i // cols) * (H + 30)
    sheet.paste(im, (x, y))
    d.text((x, y + H + 4), os.path.basename(f)[:-4], fill=0)
sheet.save(out)
print(out, len(files))
