#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K's machine marks. Each machine has a brand mark, a slanted, chamfered letterform (after the Y2K techno
# faces) drawn on a module grid. It is used twice: flat (64 px, the machine card, as Model:Cycles shows its
# engines) and extruded in 3D, ray-marched, its face solid and its sides dithered to 1 bit with screen-fixed
# patterns (after Kloroform's dithered letters), swaying in 12 frames. Writes firmware/gen/m2k_icons.h.
#   .venv/bin/python tools/gen_icons.py [--sheet build/icons.png]
import os, sys, math
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "firmware", "gen", "m2k_icons.h")
N = 64          # pixels of a frame
FRAMES = 12
G = 128         # the design grid


# The marks are isotypes, not letters (after Fors' plugin marks): each a small symbol for what its machine does,
# and all of one family:
#   a 12-unit grid, the symbol inside 1..11; solid ink only; solid masses set against thin lines (0.8 units);
#   circles, their arcs, 45-degree and fanned lines; negative space; and one signature, THE SLIT: every piece is
#   set apart from the next by half a unit.
SS = 4                      # supersampling while drawing
U = G * SS / 12.0           # one unit, in drawing pixels
SL = 0.5                    # the slit
LN = 0.8                    # a thin line


def mark(name):
    im = Image.new("L", (G * SS, G * SS), 0)
    d = ImageDraw.Draw(im)
    W = 255

    def R(x0, y0, x1, y1, c=W):
        d.rectangle([x0 * U, y0 * U, x1 * U - 1, y1 * U - 1], fill=c)

    def P(pts, c=W):
        d.polygon([(x * U, y * U) for x, y in pts], fill=c)

    def DISC(cx, cy, r, c=W):
        d.ellipse([(cx - r) * U, (cy - r) * U, (cx + r) * U - 1, (cy + r) * U - 1], fill=c)

    def STROKE(pts, w, c=W):                          # a line of width w along pts, its ends cut square
        L, Rr = [], []
        for i, (x, y) in enumerate(pts):
            x0, y0 = pts[max(i - 1, 0)]
            x1, y1 = pts[min(i + 1, len(pts) - 1)]
            tx, ty = x1 - x0, y1 - y0
            n = math.hypot(tx, ty) or 1
            nx, ny = -ty / n * w / 2, tx / n * w / 2
            L.append((x + nx, y + ny))
            Rr.append((x - nx, y - ny))
        P(L + Rr[::-1], c)

    def LINE(x0, y0, x1, y1, w=LN, c=W):
        STROKE([(x0, y0), (x1, y1)], w, c)

    def ELLIPSE(cx, cy, a, b, rot, t0, t1, n=90):     # points of a turned ellipse, from angle t0 to t1
        cr, sr = math.cos(rot), math.sin(rot)
        out = []
        for k in range(n + 1):
            t = t0 + (t1 - t0) * k / n
            x, y = a * math.cos(t), b * math.sin(t)
            out.append((cx + x * cr - y * sr, cy + x * sr + y * cr))
        return out

    if name == "GSIN":          # the sine: a disc, cut in two by one cycle of it
        DISC(6, 6, 5)
        pts = [(0.5 + 11 * k / 80, 6 - 2.2 * math.sin(2 * math.pi * k / 80)) for k in range(81)]
        STROKE(pts, SL * 1.4, 0)
    elif name == "GNOI":        # noise: a solid block, dissolving into tiles
        R(1, 1.5, 5, 10.5)
        keep = [[1, 1, 0, 1, 1, 1], [1, 0, 1, 0, 1, 0], [0, 1, 0, 0, 1, 0], [0, 0, 1, 0, 0, 0]]
        for i in range(4):
            for j in range(6):
                if keep[i][j]:
                    x0, y0 = 5.5 + i * 1.5, 1.5 + j * 1.5
                    R(x0, y0, x0 + 1.0, y0 + 1.0)
    elif name == "SID":         # the chip: a body, its notch, thin pins
        R(3.5, 1.5, 8.5, 10.5)
        DISC(6, 1.5, 1.0, 0)
        for k in range(4):
            y = 2.9 + k * 2.1
            R(1.0, y, 3.0, y + LN)
            R(9.0, y, 11.0, y + LN)
    elif name == "FMS":       # static FM: a carrier, its phase shifted by the modulator: the disc cut, its halves offset
        for top, dx in ((True, -0.8), (False, 0.8)):
            t = Image.new("L", im.size, 0)
            td = ImageDraw.Draw(t)
            cx, cy, r = 6 + dx, 6, 4.6
            td.ellipse([(cx - r) * U, (cy - r) * U, (cx + r) * U - 1, (cy + r) * U - 1], fill=255)
            if top: td.rectangle([0, (6 - SL / 2) * U, im.size[0], im.size[1]], fill=0)
            else: td.rectangle([0, 0, im.size[0], (6 + SL / 2) * U], fill=0)
            im.paste(255, mask=t)
    elif name == "FMD":         # dynamic FM: the carrier, its sidebands fanning out
        DISC(3.6, 6, 2.6)
        for k in range(5):
            a = (k - 2) * 0.24
            r0 = 2.6 + SL + 0.2
            LINE(3.6 + r0 * math.cos(a), 6 + r0 * math.sin(a), 3.6 + 8.2 * math.cos(a), 6 + 8.2 * math.sin(a))
    elif name == "SAW":         # the supersaw: a tooth, two detuned slivers of it behind
        P([(5, 10.5), (11, 1.5), (11, 10.5)])
        for off, w in [(1.4, 0.8), (2.6, 0.55)]:
            P([(5 - off, 10.5), (11 - off, 1.5), (11 - off + w, 1.5), (5 - off + w, 10.5)])
    elif name == "ENS":         # the ensemble: three voices of a chord, each cut out of the last
        DISC(4.4, 7.4, 2.7)
        DISC(7.6, 7.4, 2.7 + SL, 0)
        DISC(7.6, 7.4, 2.7)
        DISC(6.0, 4.6, 2.7 + SL, 0)
        DISC(6.0, 4.6, 2.7)
    elif name == "WAV":         # the wavetable: its frames stacked, their ends a cycle
        for k in range(8):
            y = 1.6 + k * 1.25
            L = 6.0 + 4.6 * math.sin(2 * math.pi * (k + 0.5) / 8)
            R(1, y, 1 + max(1.2, L), y + LN)
    elif name == "VOX":       # the voice: a speech bubble, its formants cut out of it
        d.rounded_rectangle([1 * U, 1.5 * U, 11 * U - 1, 9 * U - 1], radius=int(2.6 * U), fill=W)
        P([(2.6, 8.4), (2.6, 11.2), (5.6, 8.4)])
        for k, h in enumerate([1.6, 3.6, 5.0, 3.0, 1.4]):
            x = 3.05 + k * 1.3
            R(x, 5.25 - h / 2, x + SL * 1.3, 5.25 + h / 2, 0)
    return im                   # G * SS square, 8 bit


NAMES = ["GSIN", "GNOI", "SID", "FMS", "FMD", "SAW", "ENS", "WAV", "VOX"]


GS = 256        # the distance field's grid: finer than the mark, for clean normals


def sdf2d(m):
    """signed distance to the mark's edge, in grid pixels (negative inside)"""
    G = m.shape[0]
    edge = m & ~(np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1))
    edge[0, :] |= m[0, :]; edge[-1, :] |= m[-1, :]; edge[:, 0] |= m[:, 0]; edge[:, -1] |= m[:, -1]
    ey, ex = np.nonzero(edge)
    ys, xs = np.mgrid[0:G, 0:G]
    pts = np.stack([xs.ravel(), ys.ravel()], 1).astype(np.float32)
    e = np.stack([ex, ey], 1).astype(np.float32)
    best = np.full(len(pts), 1e9, np.float32)
    for i in range(0, len(e), 256):
        d = ((pts[:, None, :] - e[None, i:i + 256, :]) ** 2).sum(-1)
        best = np.minimum(best, d.min(1))
    dist = np.sqrt(best).reshape(G, G)
    return np.where(m, -dist, dist)


def make_sdf(m):
    G = m.shape[0]
    d2 = sdf2d(m) / (G / 2)          # in object units: the mark spans -1..1
    def f(p):
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        gx = (x + 1) * (G / 2) - 0.5
        gy = (1 - y) * (G / 2) - 0.5
        cx, cy = np.clip(gx, 0, G - 1.001), np.clip(gy, 0, G - 1.001)
        x0, y0 = cx.astype(int), cy.astype(int)
        fx, fy = cx - x0, cy - y0
        v = (d2[y0, x0] * (1 - fx) * (1 - fy) + d2[y0, x0 + 1] * fx * (1 - fy) +
             d2[y0 + 1, x0] * (1 - fx) * fy + d2[y0 + 1, x0 + 1] * fx * fy)
        out = np.sqrt(np.maximum(np.abs(gx - cx), np.abs(gy - cy)) ** 2) / (G / 2)
        v = v + out
        dz = np.abs(z) - 0.15
        q0, q1 = np.maximum(v + 0.05, 0), np.maximum(dz, 0)
        return np.sqrt(q0 ** 2 + q1 ** 2) + np.minimum(np.maximum(v + 0.05, dz), 0) - 0.05
    return f


def length(v):
    return np.sqrt((v * v).sum(-1))


def rot(p, ax, ay):
    cy, sy = math.cos(ay), math.sin(ay)
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    x, z = cy * x + sy * z, -sy * x + cy * z
    cx, sx = math.cos(ax), math.sin(ax)
    y, z = cx * y - sx * z, sx * y + cx * z
    return np.stack([x, y, z], -1)


# screen-fixed 1-bit patterns, dark (6) to light (2); hit pixels never go lighter than dots
def pattern(level, x, y):
    if level >= 6:
        return np.ones_like(x, bool)
    if level == 5:
        return ~((x % 2 == 0) & (y % 2 == 0))
    if level == 4:
        return (x + y) % 2 == 0
    if level == 3:
        return y % 2 == 0
    return (x % 2 == 0) & (y % 2 == 0)


def render(sdf, ay, ax):
    ys, xs = np.mgrid[0:N, 0:N]
    u = (xs + 0.5) / N * 2.1 - 1.05
    v = -((ys + 0.5) / N * 2.1 - 1.05)
    ro = np.stack([u, v, np.full_like(u, -3.0)], -1)
    rd = np.array([0, 0, 1.0])
    t = np.zeros(u.shape)
    hit = np.zeros(u.shape, bool)
    f = lambda q: sdf(rot(q, ax, ay))
    for _ in range(140):
        p = ro + rd * t[..., None]
        d = f(p)
        hit |= (d < 0.003) & (t < 6)
        t = np.where(hit, t, t + d * 0.6)
    p = ro + rd * t[..., None]
    e = 0.01
    n = np.stack([f(p + [e, 0, 0]) - f(p - [e, 0, 0]), f(p + [0, e, 0]) - f(p - [0, e, 0]),
                  f(p + [0, 0, e]) - f(p - [0, 0, e])], -1)
    n /= np.maximum(length(n)[..., None], 1e-9)
    L = np.array([-0.5, 0.6, -0.62])
    L /= np.linalg.norm(L)
    dif = np.clip((n * L).sum(-1), 0, 1)
    # few textures: the face solid (the flat mark), the sides dithered by their light
    po = rot(p, ax, ay)                               # the hit, in the object's own space
    no = np.stack([sdf(po + [e, 0, 0]) - sdf(po - [e, 0, 0]), sdf(po + [0, e, 0]) - sdf(po - [0, e, 0]),
                   sdf(po + [0, 0, e]) - sdf(po - [0, 0, e])], -1)
    no /= np.maximum(length(no)[..., None], 1e-9)
    front = np.abs(no[..., 2]) > 0.9                  # the letter's face, whatever the angle
    level = np.where(dif > 0.5, 3, 4)                 # the sides, dithered: lines in the light, checks in shadow
    level = np.where(front, 6, level).astype(int)       # the face: solid, the flat mark itself
    ink = np.zeros(u.shape, bool)
    for l in range(2, 7):
        ink |= (level == l) & pattern(l, xs, ys)
    return ink & hit


def pack(ink):
    h, w = ink.shape
    out = []
    for y in range(h):
        for bx in range(w // 8):
            b = 0
            for i in range(8):
                if ink[y, bx * 8 + i]:
                    b |= 0x80 >> i
            out.append(b)
    return out


def main():
    sheet = sys.argv[sys.argv.index("--sheet") + 1] if "--sheet" in sys.argv else None
    lines = ["/* generated by tools/gen_icons.py: do not edit */\n",
             "#define ICON_N %d\n#define ICON_FRAMES %d\n#define ICON_COUNT %d\n" % (N, FRAMES, len(NAMES))]
    frames3d, flats = [], []
    for name in NAMES:
        big = mark(name)
        flat = np.array(big.resize((N, N), Image.BOX)) > 127
        flats.append(flat)
        sdf = make_sdf(np.array(big.resize((GS, GS), Image.BOX)) > 127)
        fr = [render(sdf, 0.72 * math.sin(2 * math.pi * k / FRAMES), 0.2 + 0.1 * math.cos(2 * math.pi * k / FRAMES))
              for k in range(FRAMES)]
        frames3d.append(fr)
        print(name)
    lines.append("static const uint8_t ICON_BITS[%d][%d][%d] = {\n" % (len(NAMES), FRAMES, N * N // 8))
    for fr in frames3d:
        lines.append("  {\n")
        for ink in fr:
            lines.append("    {" + ",".join("0x%02x" % x for x in pack(ink)) + "},\n")
        lines.append("  },\n")
    lines.append("};\n")
    lines.append("static const uint8_t MARK_BITS[%d][%d] = {\n" % (len(NAMES), N * N // 8))
    for fl in flats:
        lines.append("    {" + ",".join("0x%02x" % x for x in pack(fl)) + "},\n")
    lines.append("};\n")
    with open(OUT, "w") as f:
        f.write("".join(lines))
    print("wrote", os.path.normpath(OUT))
    if sheet:
        cols = FRAMES + 1
        img = np.ones((len(NAMES) * (N + 6), cols * (N + 6)), np.uint8) * 255
        for i in range(len(NAMES)):
            y = i * (N + 6)
            img[y:y + N, 0:N] = np.where(flats[i], 0, 255)
            for k, ink in enumerate(frames3d[i]):
                x = (k + 1) * (N + 6)
                img[y:y + N, x:x + N] = np.where(ink, 0, 255)
        Image.fromarray(img).resize((img.shape[1] * 2, img.shape[0] * 2), Image.NEAREST).save(sheet)
        print("sheet", sheet)


main()
