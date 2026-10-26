#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# BLE MIDI: 2026 DEADACTIVE
"""Gate 6 (MONO2K's Bluetooth build on two cores): the second core never meets a flash access.

The second core (CS-1's audio worker, its loop in RAM) runs the render's jobs, which execute from XIP: a flash erase,
program or read turns XIP off, and a job running then would fetch garbage. MONO2K only submits a job inside
m2k_render (the main loop: m2k_par_begin) and joins it before the block ends (m2k_par_end), so between two renders
core 1 idles in RAM; every flash access (the store, the Patch Editor's link, the Bluetooth record, the BLE unit's
look for the radio calibration through core_flash_read) is made by the main loop outside the render. What breaks
that is a flash access reachable from where a job may be running, or the audio interrupt waiting for core 1 (SLOOP's
Bluetooth build, which lost its sound, rendered in its audio interrupt and waited there for core 1):
  1. from the render (m2k_render) and the job itself (par_job): no flash function, no Bluetooth service (ble_service,
     the BT tasks run there and may read the flash);
  2. from any interrupt entry (isr_*): no flash function, no render, no wait for core 1 (audio_worker_join).
The graph is gate 3's (check_ble_boot.py: calls, tail gotos and the address loads of long and far calls, FL_FAR).

  check_core1.py build/fm1/mono2k.dis
  check_core1.py --selftest"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_ble_boot import graph  # noqa: E402

FLASH = {"fl_erase4k_ram", "fl_prog_ram", "fl_read_ram", "fl_jedec_ram", "fl_status_ram", "fl_erase4k", "fl_write",
         "st_read"}
BT_SERVICE = {"ble_service", "ble_os_service", "bt_service"}
JOB_ROOTS = ("m2k_render", "par_job")
WAIT = {"audio_worker_join", "par_end"}


def reach(g, roots):
    seen, todo = set(), [r for r in roots if r in g]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo.extend(g.get(f, ()))
    return seen


def errors(text):
    g = graph(text)
    out = []
    missing = [r for r in JOB_ROOTS[:1] if r not in g]
    if missing:
        out.append(f"{', '.join(missing)} not in the listing")
    job = reach(g, JOB_ROOTS)
    for f in sorted(job & (FLASH | BT_SERVICE)):
        out.append(f"{f} reachable from the render or a core-1 job (a flash access while core 1 may run from XIP)")
    isr = reach(g, [n for n in g if n.startswith("isr_")])
    for f in sorted(isr & (FLASH | WAIT | {"m2k_render"} | BT_SERVICE)):
        out.append(f"{f} reachable from an interrupt (it may meet a core-1 job, or wait for core 1)")
    return out


def selftest():
    def fn(name, *body):
        return f"{name}:\n" + "".join(f"  2000000:    00 49    \t{b}\n" for b in body)
    base = fn("app_loop", "call -4 <m2k_render : 2000010 >", "call -4 <store_save : 2000020 >") + \
        fn("m2k_render", "call -4 <track_block : 2000030 >") + fn("track_block", "rts") + \
        fn("store_save", "r0 = 29360128 <fl_erase4k_ram : 1c00000 >", "call r0") + fn("fl_erase4k_ram", "rts") + \
        fn("isr_alnk0", "call -4 <fm1_alnk0_irq : 2000040 >") + fn("fm1_alnk0_irq", "rts")
    assert errors(base) == [], "the main loop's flash write outside the render flagged"
    bad = base.replace(fn("track_block", "rts"), fn("track_block", "call -4 <store_save : 2000020 >"))
    assert any("fl_erase4k_ram" in e for e in errors(bad)), "a flash write from the render not caught"
    bad = base.replace(fn("fm1_alnk0_irq", "rts"), fn("fm1_alnk0_irq", "call -4 <m2k_render : 2000010 >"))
    assert any("m2k_render" in e for e in errors(bad)), "the render in the audio interrupt not caught"
    bad = base.replace(fn("fm1_alnk0_irq", "rts"), fn("fm1_alnk0_irq", "call -4 <audio_worker_join : 2000050 >")) + \
        fn("audio_worker_join", "rts")
    assert any("audio_worker_join" in e for e in errors(bad)), "the audio interrupt waiting for core 1 not caught"
    bad = base.replace(fn("track_block", "rts"), fn("track_block", "call -4 <ble_service : 2000060 >")) + \
        fn("ble_service", "rts")
    assert any("ble_service" in e for e in errors(bad)), "the BT service from the render not caught"
    print("check_core1: selftest ok")


def main(argv):
    if argv[1:] == ["--selftest"]:
        selftest()
        return 0
    e = errors(open(argv[1]).read())
    for x in e:
        print("gate 6:", x)
    if not e:
        print("gate 6: no flash access, no BT service where core 1 may run a job; the audio interrupt never waits")
    return 1 if e else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
