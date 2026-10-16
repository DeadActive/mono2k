#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K's browser build: the firmware's C core to web/m2k.wasm (Emscripten, no JS glue: the worklet instantiates it).
#   sh web/build.sh
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
[ -f firmware/gen/m2k_tables.h ] || .venv/bin/python tools/gen_tables.py
[ -f firmware/gen/m2k_fonts.h ] || .venv/bin/python tools/gen_font.py
[ -f firmware/gen/m2k_icons.h ] || .venv/bin/python tools/gen_icons.py
emcc -O3 -std=gnu11 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen web/m2k_web.c \
    -sSTANDALONE_WASM=1 --no-entry -sINITIAL_MEMORY=4194304 -sALLOW_MEMORY_GROWTH=0 -sSTACK_SIZE=131072 \
    -sEXPORTED_FUNCTIONS=_w_audio,_w_render,_w_frame,_w_leds,_w_init,_w_key,_w_button,_w_encoder,_w_midi,_w_store_buf,_w_store_max,_w_store_pack,_w_store_unpack,_w_proj_export,_w_proj_import,_w_project,_w_project_name,_w_projects,_w_dirty,_w_setting,_w_pattern,_w_patterns,_w_info,_w_mark,_w_icon,_w_machines,_w_mi,_w_stat,_w_syn,_w_pat_name \
    -o web/m2k.wasm
ls -l web/m2k.wasm
