#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Daurade's logo as SVG (assets/logo.svg): render it at 1024 x 1024 to assets/logo.png (any browser or
rsvg-convert), then run tools/da_logo.py for the power-on screen.
Usage: draw_logo.py [ACCENT_COLOUR] [OUT.svg]"""
# The look (family style):
# black ground, a ring, audio bars, a line-art fish in one accent colour + cream,
# music notes, a little knob box (four knobs, 2 x 2, like the FM-1), a wide italic name
# (Daurade's: a steep chrome italic with long aggressive blades, speed streaks and glints).
# Daurade's own touches: silver/chrome accent, the brow band across the bream's forehead,
# and a few glints round the notes.
import math, sys

ACC = sys.argv[1] if len(sys.argv) > 1 else "#b4bcc7"   # the accent colour (silver)
OUT = sys.argv[2] if len(sys.argv) > 2 else str(__import__("pathlib").Path(__file__).resolve().parent.parent / "assets" / "logo.svg")
CREAM, INK, BG = "#eef1f5", "#050608", "#000000"   # cool white instead of cream, to match the chrome
W = H = 1024
CX, CY, R = 512, 440, 362

# ---- letters on a 6 x 7 grid, chamfered like the rest of the family
L = {
    "D": [[(0,0),(4.4,0),(6,1.6),(6,5.4),(4.4,7),(0,7)],
          [(1.7,1.7),(3.8,1.7),(4.3,2.2),(4.3,4.8),(3.8,5.3),(1.7,5.3)]],
    "A": [[(0,7),(0,1.6),(1.6,0),(4.4,0),(6,1.6),(6,7),(4.3,7),(4.3,4.7),(1.7,4.7),(1.7,7)],
          [(1.7,1.7),(4.3,1.7),(4.3,3.1),(1.7,3.1)]],
    "U": [[(0,0),(1.7,0),(1.7,5.3),(4.3,5.3),(4.3,0),(6,0),(6,5.4),(4.4,7),(1.6,7),(0,5.4)]],
    "R": [[(0,0),(4.4,0),(6,1.6),(6,3.0),(4.9,4.0),(6.1,7),(4.3,7),(3.3,4.5),(1.7,4.5),(1.7,7),(0,7)],
          [(1.7,1.7),(4.3,1.7),(4.3,2.9),(1.7,2.9)]],
    "E": [[(0,0.9),(0.9,0),(6,0),(6,1.6),(1.7,1.6),(1.7,2.7),(5,2.7),(5,4.3),(1.7,4.3),(1.7,5.4),(6,5.4),(6,7),(0.9,7),(0,6.1)]],
}
WID = {c: 6 for c in L}
GAP, WORD, SK = 0.7, "DAURADE", 0.40
total = sum(WID[c] for c in WORD) + GAP * (len(WORD) - 1)
U = 14.2
TY0 = 836


SPIKES = [  # one list per letter of DAURADE, in the same 6 x 7 grid
    [[(0,0),(1.7,0),(-0.7,-1.4)], [(6,2.2),(6,4.4),(7.6,2.9)]],                      # D
    [[(4.4,0),(6,1.6),(7.2,-1.1)], [(0,6.2),(1.7,7),(-0.8,8.5)]],                    # A
    [[(0,0),(1.7,0),(0.3,-1.7)], [(4.3,0),(6,0),(6.8,-1.3)]],                        # U
    [[(0,0),(1.7,0),(-1.0,-1.1)], [(4.3,7),(6.1,7),(7.9,8.3)]],                      # R
    [[(1.6,0),(3.4,0),(2.0,-1.6)], [(4.3,6.2),(6,7),(7.1,8.6)]],                     # A
    [[(0,5.6),(1.7,7),(-1.1,8.1)], [(4.4,0),(6,1.6),(7.4,-0.8)]],                    # D
    [[(4.4,0),(6,0),(8.0,-0.9),(6,1.6),(4.4,1.6)], [(4.2,2.7),(5,2.7),(6.9,3.4),(5,4.3),(4.2,4.3)],
     [(4.4,5.4),(6,5.4),(8.2,7.9),(4.4,7)]],                                         # E
]


REACH = [2.0, 1.9, 1.3, 1.35, 1.3, 1.9, 2.1]   # how far the blade tips shoot out, per letter (1 = as drawn above);
# the middle letters stay shorter so their blades do not cross the ring or each other


def stretch(poly, reach):
    """Push every tip (a point outside the letter's 6 x 7 box) further out from the blade's base."""
    base = [(x, y) for x, y in poly if -0.01 <= x <= 6.01 and -0.01 <= y <= 7.01]
    bx = sum(x for x, _ in base) / len(base)
    by = sum(y for _, y in base) / len(base)
    out = []
    for x, y in poly:
        if (x, y) in base:
            out.append((x, y))
        else:
            out.append((bx + (x - bx) * reach, by + (y - by) * reach))
    return out


def spike_path():
    x0 = (W - total * U - 7 * U * SK) / 2
    d, x = [], 0.0
    for i, c in enumerate(WORD):
        for poly in (stretch(p, REACH[i]) for p in SPIKES[i]):
            pts = [f"{x0 + (x + px) * U + (7 - py) * U * SK:.1f},{TY0 + py * U:.1f}" for px, py in poly]
            d.append("M" + " L".join(pts) + " Z")
        x += WID[c] + GAP
    return " ".join(d)


def word_path():
    x0 = (W - total * U - 7 * U * SK) / 2
    d, x = [], 0.0
    for c in WORD:
        for poly in L[c]:
            pts = [f"{x0 + (x + px) * U + (7 - py) * U * SK:.1f},{TY0 + py * U:.1f}" for px, py in poly]
            d.append("M" + " L".join(pts) + " Z")
        x += WID[c] + GAP
    return " ".join(d)


# ---- audio bars, a waveform round the centre line, clipped to the ring
bars = []
n = 24
for i in range(n):
    x = 176 + i * 29
    t = i / (n - 1)
    h = 60 + 230 * abs(math.sin(t * 7.3 + 0.6)) * (0.55 + 0.45 * math.sin(t * math.pi))
    bars.append(f'<rect x="{x}" y="{CY - 10 - h * 0.62:.0f}" width="13" height="{h:.0f}" rx="2"/>')

# ---- the daurade (gilt-head bream), facing right: deep oval body, steep forehead, forked tail
body = ("M 872 392 "
        "C 864 340, 836 290, 790 262 "                  # snout up the steep forehead
        "C 730 226, 620 222, 520 236 "                  # top of the back
        "C 420 252, 320 296, 262 340 "                  # back down to the tail stem
        "Q 210 270, 136 222 Q 178 300, 206 372 "        # upper tail lobe
        "Q 176 444, 134 520 Q 212 466, 262 402 "        # lower tail lobe
        "C 330 444, 430 484, 540 494 "                  # belly
        "C 650 502, 760 476, 830 432 "                  # throat
        "C 852 418, 866 406, 872 392 Z")
belly = ("M 830 432 C 760 476, 650 502, 540 494 C 430 484, 330 444, 262 402 "
         "C 350 426, 460 448, 560 452 C 660 456, 760 440, 836 414 Z")
# the spiny dorsal fin along the back: a membrane with spine tips, scalloped between them
dors_base = [(704, 234), (650, 227), (596, 228), (542, 233), (488, 242), (434, 256), (380, 276), (334, 300)]
dors_h = [30, 52, 58, 54, 48, 42, 34, 18]
tips = [(x - 18, y - h) for (x, y), h in zip(dors_base, dors_h)]
dorsal = f"M {dors_base[0][0]} {dors_base[0][1]} L {tips[0][0]} {tips[0][1]}"
for (ax, ay), (bx, by) in zip(tips, tips[1:]):
    mx, my = (ax + bx) / 2, (ay + by) / 2 + 14          # the dip between two spines
    dorsal += f" Q {mx + 6:.0f} {my:.0f} {bx} {by}"
dorsal += " " + " ".join(f"L {x} {y}" for x, y in reversed(dors_base)) + " Z"
rays = "".join(f'<path d="M {x - 3} {y} L {tx} {ty}"/>' for (x, y), (tx, ty) in zip(dors_base, tips))
anal = "M 420 474 Q 384 508 350 522 Q 338 486 330 448 Z"
pect = "M 680 404 Q 620 430, 560 418 Q 616 386, 676 384 Z"
pelvic = "M 610 486 L 582 538 L 640 492 Z"
gill = "M 726 288 C 700 330, 698 400, 724 456"
gill_spot = "M 732 296 C 718 312, 712 326, 712 340 C 726 334, 738 318, 742 300 Z"
band = "M 772 290 Q 806 280 840 300"   # the gold brow
mouth = "M 872 392 C 856 398, 842 400, 828 398"
scales = "".join(
    f'<path d="M {x} {y} q 14 12 0 24"/>'
    for x in range(380, 700, 46) for y in range(300, 420, 34)
    if (x - 540) ** 2 / 190 ** 2 + (y - 360) ** 2 / 80 ** 2 < 1)


def note(x, y, s, double=False):
    """An eighth (or a pair of eighths) note."""
    g = [f'<ellipse cx="{x}" cy="{y}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x} {y})"/>',
         f'<rect x="{x + 9*s:.1f}" y="{y - 58*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>']
    if double:
        x2 = x + 44 * s
        g += [f'<ellipse cx="{x2}" cy="{y - 6*s}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x2} {y - 6*s})"/>',
              f'<rect x="{x2 + 9*s:.1f}" y="{y - 64*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>',
              f'<polygon points="{x + 9*s:.1f},{y - 58*s:.1f} {x2 + 13.5*s:.1f},{y - 64*s:.1f} {x2 + 13.5*s:.1f},{y - 52*s:.1f} {x + 9*s:.1f},{y - 46*s:.1f}"/>']
    else:
        g.append(f'<path d="M {x + 13.5*s:.1f} {y - 58*s:.1f} q {22*s:.1f} {14*s:.1f} {16*s:.1f} {38*s:.1f} '
                 f'q {-2*s:.1f} {-16*s:.1f} {-16*s:.1f} {-22*s:.1f} z"/>')
    return "".join(g)


def glint(x, y, r):
    """A four-point sparkle."""
    k = r * 0.22
    return (f'<path d="M {x} {y - r} Q {x + k} {y - k} {x + r} {y} Q {x + k} {y + k} {x} {y + r} '
            f'Q {x - k} {y + k} {x - r} {y} Q {x - k} {y - k} {x} {y - r} Z"/>')


def knob(x, y):
    ticks = "".join(
        f'<line x1="{x + 30*math.cos(a):.1f}" y1="{y + 30*math.sin(a):.1f}" x2="{x + 37*math.cos(a):.1f}" y2="{y + 37*math.sin(a):.1f}"/>'
        for a in [math.radians(d) for d in range(135, 406, 45)])
    return (f'<g stroke="{INK}" stroke-width="3" stroke-linecap="round">{ticks}</g>'
            f'<circle cx="{x}" cy="{y}" r="22" fill="{ACC}" stroke="{INK}" stroke-width="5"/>'
            f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - 16}" stroke="{INK}" stroke-width="5" stroke-linecap="round"/>')


word = word_path()
spikes = spike_path()
BX, BY, BW, BH = 512 - 95, 596, 190, 176
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
<defs>
<linearGradient id="chrome" gradientUnits="userSpaceOnUse" x1="0" y1="{TY0}" x2="0" y2="{TY0 + 7*U}"><stop offset="0" stop-color="#ffffff"/><stop offset="0.18" stop-color="#dfe5ee"/><stop offset="0.3" stop-color="#7b8593"/><stop offset="0.34" stop-color="#ffffff"/><stop offset="0.5" stop-color="#2c323b"/><stop offset="0.53" stop-color="#ffffff"/><stop offset="0.72" stop-color="#aeb6c2"/><stop offset="0.88" stop-color="#f4f7fb"/><stop offset="1" stop-color="#6f7885"/></linearGradient>
<clipPath id="ring"><circle cx="{CX}" cy="{CY}" r="{R - 14}"/></clipPath></defs>
<rect width="{W}" height="{H}" fill="{BG}"/>
<circle cx="{CX}" cy="{CY}" r="{R}" fill="#07090d" stroke="{ACC}" stroke-width="11"/>
<g clip-path="url(#ring)" fill="{ACC}">{"".join(bars)}</g>

<!-- the box with four knobs (2 x 2), wired to the ring -->
<path d="M {BX} {BY + BH/2} C {BX - 80} {BY + BH/2}, {BX - 110} {BY - 20}, {BX - 120} {BY - 70}" fill="none" stroke="{ACC}" stroke-width="7"/>
<path d="M {BX + BW} {BY + BH/2} C {BX + BW + 80} {BY + BH/2}, {BX + BW + 110} {BY - 20}, {BX + BW + 120} {BY - 70}" fill="none" stroke="{ACC}" stroke-width="7"/>
<rect x="{BX}" y="{BY}" width="{BW}" height="{BH}" rx="16" fill="{CREAM}" stroke="{ACC}" stroke-width="9"/>
{knob(BX + 55, BY + 52)}{knob(BX + 135, BY + 52)}{knob(BX + 55, BY + 128)}{knob(BX + 135, BY + 128)}

<!-- the daurade -->
<g transform="rotate(-5 520 370) translate(-10 -6)">
  <path d="{body}" fill="none" stroke="{BG}" stroke-width="26" stroke-linejoin="round"/>
  <path d="{dorsal}" fill="none" stroke="{BG}" stroke-width="20" stroke-linejoin="round"/>
  <path d="{dorsal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <g fill="none" stroke="{CREAM}" stroke-width="3" stroke-linecap="round">{rays}</g>
  <path d="{anal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{body}" fill="{INK}" stroke="{ACC}" stroke-width="10" stroke-linejoin="round"/>
  <path d="{belly}" fill="{CREAM}"/>
  <g fill="none" stroke="{ACC}" stroke-width="3" opacity="0.6">{scales}</g>
  <path d="M 238 330 Q 196 286, 158 254" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="M 238 412 Q 196 456, 158 492" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="{gill}" fill="none" stroke="{ACC}" stroke-width="6" stroke-linecap="round"/>
  <path d="{gill_spot}" fill="{ACC}"/>
  <path d="{pect}" fill="{CREAM}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{pelvic}" fill="{CREAM}" stroke="{ACC}" stroke-width="5" stroke-linejoin="round"/>
  <path d="{band}" fill="none" stroke="{ACC}" stroke-width="14" stroke-linecap="round"/>
  <path d="{mouth}" fill="none" stroke="{ACC}" stroke-width="6" stroke-linecap="round"/>
  <circle cx="800" cy="320" r="18" fill="{CREAM}" stroke="{ACC}" stroke-width="5"/>
  <circle cx="805" cy="319" r="8.5" fill="{INK}"/>
</g>

<g fill="{CREAM}">{note(756, 186, 0.75)}{note(806, 152, 0.9, True)}{note(880, 226, 0.7)}</g>
<g fill="{ACC}">{glint(724, 120, 16)}{glint(912, 160, 12)}{glint(860, 92, 9)}</g>

<g stroke="{ACC}" stroke-linecap="round"><line x1="70" y1="{TY0 + 22}" x2="190" y2="{TY0 + 22}" stroke-width="7"/><line x1="40" y1="{TY0 + 52}" x2="176" y2="{TY0 + 52}" stroke-width="5" opacity="0.7"/><line x1="88" y1="{TY0 + 80}" x2="164" y2="{TY0 + 80}" stroke-width="4" opacity="0.45"/><line x1="860" y1="{TY0 + 30}" x2="972" y2="{TY0 + 30}" stroke-width="5" opacity="0.7"/><line x1="846" y1="{TY0 + 62}" x2="950" y2="{TY0 + 62}" stroke-width="4" opacity="0.45"/></g>
<path d="{spikes}" fill="none" stroke="{BG}" stroke-width="16" stroke-linejoin="miter"/>
<path d="{word}" fill="none" stroke="{BG}" stroke-width="22" stroke-linejoin="round"/>
<path d="{spikes}" fill="url(#chrome)" stroke="#2a2f37" stroke-width="3" stroke-linejoin="miter"/>
<path d="{word}" fill="url(#chrome)" fill-rule="evenodd" stroke="#2a2f37" stroke-width="3" stroke-linejoin="round"/>
<g fill="#ffffff">{glint(W/2 + 268, TY0 + 4, 30)}{glint(W/2 - 296, TY0 + 96, 16)}{glint(W/2 + 40, TY0 + 2, 12)}</g>
</svg>'''
open(OUT, "w").write(svg)
print("ok", OUT)
