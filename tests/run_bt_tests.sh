#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# BLE MIDI: 2026 DEADACTIVE
# The Bluetooth build's host tests (tools/check.sh runs them): the BLE unit's own (tests/ble/, Felucca's, unchanged),
# FM-Link's engine, and the FM-1's Bluetooth glue with MONO2K's core as the Bluetooth build has it (tests/bt_*.c on
# tests/bt_test.h), each of MONO2K's under ASan + UBSan; the gates' self-tests; the memory placement gate's test.
#   sh tests/run_bt_tests.sh
set -e
cd "$(dirname "$0")/.."
B=build/host/bt
mkdir -p "$B"
PY=${PYTHON:-python3}
"$PY" tools/fm1/gen_icons_rle.py "$B/m2k_icons_rle.h" > /dev/null
CC="cc -O1 -g -std=gnu11 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -fsanitize=address,undefined -fno-sanitize-recover=all"
BCC="cc -O1 -Wall -Wno-unused-function"         # (the BLE unit's own tests as the reference runs them: ble_os_test
                                                 # switches stacks, which ASan does not follow)
fail=0
run() { if "$@"; then :; else echo "FAIL: $*"; fail=1; fi; }
for t in ble_midi ble_scan ble_central ble_vm ble_gatt; do
    $BCC -o "$B/${t}_test" "tests/ble/${t}_test.c"
    run "$B/${t}_test" > "$B/${t}_test.out" && tail -1 "$B/${t}_test.out"
done
$BCC -Wno-deprecated-declarations -Ifirmware/src/ble -o "$B/ble_os_test" tests/ble/ble_os_test.c
run "$B/ble_os_test" > "$B/ble_os_test.out" && tail -1 "$B/ble_os_test.out"
run "$PY" tests/ble/check_ble_fatal.py
$CC -o "$B/fmlink_test" tests/fmlink_test.c
run "$B/fmlink_test" > "$B/fmlink_test.out" && tail -1 "$B/fmlink_test.out"
for t in bt_devs_test bt_midi_test bt_fml_test bt_page_test smc_test m2k_bt_test; do
    $CC -DM2K_BT=1 -DBT_HOST -Ifirmware/src -Ifirmware/gen -Ifirmware/fm1 -I"$B" -o "$B/$t" "tests/$t.c"
    run "$B/$t" > "$B/$t.out" && tail -1 "$B/$t.out"
    grep -h "^FAIL" "$B/$t.out" || true
done
run "$PY" tools/check_update_path.py --selftest
run "$PY" tools/check_ble_boot.py --selftest
run "$PY" tools/check_core1.py --selftest
run "$PY" tests/ble_layout_test.py
[ $fail = 0 ] && echo "bluetooth tests: all passed" || { echo "bluetooth tests: FAILED"; exit 1; }
