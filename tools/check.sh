#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The build checks, from the repo's root: sh tools/check.sh
#  1. warnings: the host and the reference renderer with clang -Wall -Wextra, and with GCC -Wall -Wextra when a GCC
#     is installed (its -Wstringop-overflow and -Warray-bounds see more than clang's); none allowed
#  2. the device rules: the core built for a 32-bit soft-float target (armv7m) calls no float helper and no 64-bit
#     division (__aeabi_ldivmod / __aeabi_uldivmod, __divdi3 ...); its section sizes are printed
#  3. sanitizers: the self-test, every screen, every bank A track, every reference spec and the power test's loop
#     under ASan + UBSan; any report fails the run
#  4. the wave banks (tools/wave_check.py); the bus's controls, every value as recorded and every old value as it
#     sounded once converted (tools/bus_curves.py)
set -u
B=build/check
mkdir -p "$B"
FAIL=0
W="-Wall -Wextra -Wno-unused-function -Wno-unused-parameter"
I="-Ifirmware/src -Ifirmware/gen"

echo "== warnings"
for cc in clang $(ls /opt/homebrew/bin/gcc-[0-9]* /usr/local/bin/gcc-[0-9]* 2>/dev/null) ${GCC:-}; do
    case "$($cc --version 2>/dev/null | head -1)" in *clang*) [ "$cc" = clang ] || continue ;; esac
    for src in host/m2k_host.c host/m2k_ref.c; do
        out=$($cc -O2 $W $I -o "$B/$(basename "$src" .c)" "$src" -lm 2>&1)
        n=$(printf "%s" "$out" | grep -c "warning:")
        echo "  $cc $src: $n warnings"
        [ "$n" -eq 0 ] || { printf "%s\n" "$out"; FAIL=1; }
    done
done
ls /opt/homebrew/bin/gcc-[0-9]* /usr/local/bin/gcc-[0-9]* >/dev/null 2>&1 || [ -n "${GCC:-}" ] ||
    echo "  GCC: SKIPPED (none installed; GCC=path to use one)"

echo "== device rules (armv7m, soft float)"
if clang --target=armv7m-none-eabi -mfloat-abi=soft -ffreestanding -O2 $W $I -c firmware/src/m2k_core.c -o "$B/core_arm.o"; then
    bad=$(nm -u "$B/core_arm.o" | grep -E "__aeabi_(l|ul)divmod|__(u)?(div|mod)di3|__aeabi_[fd](add|sub|mul|div|cmp|2|cmpeq|cmplt)|__aeabi_[iul]2[fd]|__(add|sub|mul|div)[sd]f3|__float|__fix")
    if [ -n "$bad" ]; then echo "  calls a float helper or 64-bit division:"; echo "$bad"; FAIL=1; else echo "  no float helper, no 64-bit division"; fi
    flash=0; ram=0
    for row in $(objdump -h "$B/core_arm.o" | awk '$1 ~ /^[0-9]+$/ && NF >= 3 {print $2 ":" $3}'); do
        name=${row%%:*}; sz=$((0x${row##*:}))
        case "$name" in .text|.rodata*|.data|.ARM.exidx) flash=$((flash + sz)) ;; esac
        case "$name" in .data|.bss) ram=$((ram + sz)) ;; esac
    done
    echo "  flash (code, tables, waves): $((flash / 1024)) KB; RAM (static, POOL included): $((ram / 1024)) KB"
else
    FAIL=1
fi

echo "== sanitizers"
SAN="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
clang $SAN -w $I -o "$B/m2k_host_san" host/m2k_host.c -lm && clang $SAN -w $I -o "$B/m2k_ref_san" host/m2k_ref.c -lm || FAIL=1
mkdir -p "$B/screens" "$B/pitch" "$B/ref"
{
    "$B/m2k_host_san" --selftest
    "$B/m2k_host_san" "$B/screens"
    "$B/m2k_host_san" "$B/pitch" --pitch
    for f in tools/ref/*.spec; do "$B/m2k_ref_san" "$B/ref" < "$f" > /dev/null; done
    "$B/m2k_ref_san" "$B/ref" --loop 88 < tools/ref/power.loop > /dev/null
} > "$B/san.log" 2>&1
rep=$(grep -E "runtime error|ERROR: AddressSanitizer|SELFTEST FAILED" "$B/san.log" | sort | uniq -c)
if [ -n "$rep" ]; then echo "$rep"; FAIL=1; else echo "  self-test, screens, bank A, $(ls tools/ref/*.spec | wc -l | tr -d ' ') specs, the power loop: clean"; fi

echo "== the wave banks"
.venv/bin/python tools/wave_check.py > "$B/waves.log" 2>&1 && tail -1 "$B/waves.log" | sed 's/^/  /' || { cat "$B/waves.log"; FAIL=1; }

echo "== the bus's controls"
.venv/bin/python tools/bus_curves.py > "$B/bus.log" 2>&1 && tail -1 "$B/bus.log" | sed 's/^/  /' || { cat "$B/bus.log"; FAIL=1; }

[ "$FAIL" -eq 0 ] && echo "check: passed" || echo "check: FAILED"
exit "$FAIL"
