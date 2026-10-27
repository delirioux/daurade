#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Daurade's screen on the host: builds tests/da_ui_test.c on Felucca's gfx.c, runs it (the controls, the layout lint
# over every page and palette) and turns its frames into PNGs and a GIF of PLAY.
#   tests/da_ui_test.sh [OUTDIR]     (default build/da_ui; Python with Pillow for the PNGs)
set -eu
OUT=${1:-build/da_ui}
mkdir -p build/host "$OUT/ppm"
[ -f build/gen/ui_fonts.h ] || python3 -c "import sys; sys.argv=['x']; sys.path.insert(0,'tools'); import build; build.generate()"
cc -O2 -w -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/da_ui_test tests/da_ui_test.c \
   firmware/src/da_groove.c -lm
build/host/da_ui_test "$OUT"
python3 - "$OUT" <<'PY' || echo "(no Pillow: the PPM frames are in $1/ppm)"
import glob, os, sys
from PIL import Image
out = sys.argv[1]
os.makedirs(out + "/png", exist_ok=True)
for p in sorted(glob.glob(out + "/ppm/*.ppm")):
    n = os.path.basename(p)[:-4]
    if not n.startswith("anim_"):
        Image.open(p).save(out + "/png/" + n + ".png")
fr = [Image.open(p).convert("RGB") for p in sorted(glob.glob(out + "/ppm/anim_*.ppm"))]
if fr:
    fr[0].save(out + "/daurade_play.gif", save_all=True, append_images=fr[1:], duration=48, loop=0)
PY
