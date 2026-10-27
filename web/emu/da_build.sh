#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DAURADE in the browser: firmware/src/da_fw.c and Daurade's modules on Felucca's browser build (web/emu/da_web.c),
# compiled to WebAssembly, plus Felucca's page and worklet with Daurade's name on them (after Daurade's da_build.sh).
#   web/emu/da_build.sh   ->  build/da_emu/{index.html, worklet.js, daurade.wasm, daurade-logo.png, fonts/}
# Needs build/gen (the generated headers: tools/build.py's generate step) and either Emscripten (emcc) or clang
# with wasm-ld and a WASI sysroot: WASI_SYSROOT=dir (include/wasm32-wasip1, lib/wasm32-wasip1 and
# lib/wasm32-unknown-wasip1/libclang_rt.builtins.a; the npm package @yowasp/clang carries one in
# gen/llvm-resources.tar; crt1-reactor.o gives the _initialize the worklet calls). The module has no imports the page must answer but WASI's, which the worklet stubs:
# nothing of Daurade's or Felucca's runtime reaches them.
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "daurade emu: build/gen first (tools/build.py generate)"; exit 1; }
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
DAV=$(sed -n 's/^#define DA_VERSION "\([^"]*\)".*/\1/p' firmware/src/da_version.h)
OUT=build/da_emu
mkdir -p "$OUT/fonts" "$OUT/obj"
MODS="da_groove tuna_libm"
if command -v emcc >/dev/null 2>&1; then
    SRCS=""
    for m in $MODS; do SRCS="$SRCS firmware/src/$m.c"; done
    # shellcheck disable=SC2086
    emcc -O2 -ffp-contract=off -std=gnu99 -w -DFELUCCA_SLICE=0 "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
        --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
        -o "$OUT/daurade.wasm" web/emu/da_web.c $SRCS
else
    R=${WASI_SYSROOT:?"daurade emu: no emcc; set WASI_SYSROOT to a WASI sysroot for clang"}
    SHIM="$OUT/shim"
    mkdir -p "$SHIM/sys"
    # (the host tests' fork / pipe / wait, which hostsim.c names and the browser never calls)
    printf '#pragma once\n#include <sys/types.h>\npid_t waitpid(pid_t, int *, int);\npid_t fork(void);\nint pipe(int[2]);\n#define WEXITSTATUS(s) (((s) >> 8) & 0xff)\n#define WIFEXITED(s) (((s) & 0x7f) == 0)\n' > "$SHIM/sys/wait.h"
    CF="--target=wasm32-wasip1 -nodefaultlibs -isystem $SHIM -isystem $R/include/wasm32-wasip1 -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src"
    # shellcheck disable=SC2086
    clang $CF -std=gnu99 -D_WASI_EMULATED_PROCESS_CLOCKS -D_WASI_EMULATED_SIGNAL -DFELUCCA_SLICE=0 "-DFELUCCA_VERSION=\"$VERSION\"" \
        -c web/emu/da_web.c -o "$OUT/obj/da_web.o"
    OBJS="$OUT/obj/da_web.o"
    for m in $MODS; do
        # shellcheck disable=SC2086
        clang $CF -std=c11 -fno-builtin -c "firmware/src/$m.c" -o "$OUT/obj/$m.o"
        OBJS="$OBJS $OUT/obj/$m.o"
    done
    # shellcheck disable=SC2086
    wasm-ld --no-entry --export-dynamic --gc-sections -z stack-size=1048576 --initial-memory=33554432 \
        -L"$R/lib/wasm32-wasip1" "$R/lib/wasm32-wasip1/crt1-reactor.o" $OBJS -lc -lm "$R/lib/wasm32-unknown-wasip1/libclang_rt.builtins.a" -o "$OUT/daurade.wasm"
fi
# Felucca's worklet, its module given WASI stubs; Felucca's page with Daurade's name, its own saves
sed -e 's|WebAssembly.instantiate(m.wasm, {})|WebAssembly.instantiate(m.wasm, { wasi_snapshot_preview1: new Proxy({}, { get: () => () => 0 }) })|' \
    -e 's|^// Felucca in an AudioWorklet: felucca.wasm|// Daurade (and Felucca) in an AudioWorklet: daurade.wasm|' web/emu/worklet.js > "$OUT/worklet.js"
grep -q "wasi_snapshot_preview1" "$OUT/worklet.js" || { echo "daurade emu: the worklet's instantiate line moved"; exit 1; }
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/ (Daurade $DAV)/g" \
    -e 's|<title>Felucca</title>|<title>Daurade</title>|' \
    -e 's|content="Felucca, the multi-engine synth firmware for the FM-1, running in your browser: the same code the instrument runs."|content="Daurade, grooves in chrome for the FM-1, running in your browser: the same code the instrument runs."|' \
    -e 's|const STORE = "felucca-emu-flash"|const STORE = "daurade-emu-flash"|' -e 's|"felucca-emu-knobs"|"daurade-emu-knobs"|' -e 's|"felucca-emu-knobmode"|"daurade-emu-knobmode"|' \
    -e 's|fetch("felucca.wasm")|fetch("daurade.wasm")|' -e 's|felucca.wasm: |daurade.wasm: |' \
    -e 's|  Felucca by Leo Kuroshita, Hügelton Instruments|  Daurade on Felucca · Felucca by Leo Kuroshita, Hügelton Instruments|' \
    -e 's|  <nav><a href="../installer/">Install</a><a href="../editor/">Editor</a><a href="https://github.com/hugelton/Felucca">Source</a></nav>|  <nav><a href="../install/">Install</a><a href="https://github.com/delirioux/daurade">Daurade source</a><a href="https://github.com/hugelton/Felucca">Felucca source</a></nav>|' \
    -e 's|Felucca is not affiliated with or endorsed by them.|Daurade and Felucca are not affiliated with or endorsed by them.|' \
    -e 's|<link rel="icon" href="[^"]*">|<link rel="icon" href="daurade-logo.png">|' \
    -e 's|^<div class="device" id="device">|<img src="daurade-logo.png" alt="Daurade" width="160" height="160" style="display:block;margin:0 auto 12px;border-radius:8px">\n&|' \
    web/emu/index.html > "$OUT/index.html"
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt "$OUT/fonts/"
cp docs/daurade-logo.png "$OUT/"
grep -q 'src="daurade-logo.png"' "$OUT/index.html" || { echo "daurade emu: the page's device line moved"; exit 1; }
echo "daurade emu: $OUT ($(wc -c < "$OUT/daurade.wasm" | tr -d ' ') B wasm, Daurade $DAV on Felucca $VERSION)"
