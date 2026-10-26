#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MONO2K for the FM-1: the app, the update loader and an installable .fwsc package.

  tools/fm1/build.py [--release X.Y]

After CS-1's tools/build.py (on Felucca's, Leo Kuroshita, Hügelton Instruments, GPL-3.0-only): the same steps, the
same checks. Outputs in build/fm1/: mono2k.bin (the app), loader/ota.bin (the update loader), mono2k.fwsc (the
package; mono2k-X.Y.fwsc for a release). The package's identity is FM-1_980 for every build and release (the hub's
FM-1_981 and FM-1_983 are other firmwares'; FM-1_9NN keeps the hub's resume of an interrupted update), as the
installer page reads it; a release sets the version the FM-1 reports (APP_VER) and the file's name.

JieLi's toolchain (clang 4.0.1 for pi32v2) runs on Linux x86-64 only: on a Mac each tool runs in a linux/amd64
container (colima with Rosetta, or Docker). JIELI_TOOLCHAIN (default ~/.jieli/toolchain), AC79_SDK (default
~/fw-AC79_AIoT_SDK: three of its files go into the package) and DOCKER_CONFIG (default ~/.jieli/docker when it
exists) override the places.

FELUCCA_BLE=1 (BLE MIDI: 2026 DEADACTIVE, from Felucca's ble-kit, as OberVave's): the Bluetooth build, one core.
firmware/src/ble/ (its own unit, ble.o) and the JieLi BT libraries (tools/ble_libs.py, AC79_BT_SDK) are linked in
through LTO with firmware/fm1/app_ble.ld, and the build stops on any of its gates (docs/ble/). The package is
mono2k-<version>-ble.fwsc. BLE_MEASURE=1: the regions widened so an image that does not fit can be measured; never
packaged. Unset: the build is as before, byte for byte.
"""
import argparse
import hashlib
import os
import platform
import re
import shutil
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FW = ROOT / "firmware" / "fm1"
OUT = ROOT / "build" / "fm1"
LDR = OUT / "loader"
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(1, str(ROOT / "tools"))     # (tools/ble_libs.py, the Bluetooth build's gates)
import fm1pkg_make  # noqa: E402
import lz4blk  # noqa: E402

APP_XIP = 0x02000120                # the app's offset 0 in the XIP map: the SPL jumps here
APP_SLOT = fm1pkg_make.APP_SLOT
LOADER_LOAD = 0x01C0A800
LOADER_NAME = b"usb_hid_ota.bin"    # the file name the SPL looks for
DOCKER_IMAGE = os.environ.get("JIELI_DOCKER_IMAGE", "debian:bookworm-slim")
SDK_MOUNT = None                    # Bluetooth builds: the JieLi BT SDK (tools/ble_libs.py), mounted read-only at /sdk
BLE_RAM_FLOOR = 4096                # Bluetooth builds keep this much of .data + .bss spare (PORTING.md rule 2)
BLE_POOL_FLOOR = 8192               # .. and this much of the pool
CFLAGS = ["-Os", "-ffunction-sections", "-fno-builtin", "-Wall", "-Wno-unused-function"]
LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t(.*)$")
SDK_SHA256 = {                      # AC79NN_SDK_V1.2.1_2023-12-13, the version the open FM-1 firmwares test
    "uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
}
PRODUCT = "FM-1_980"                # MONO2K's, every release (Felucca 9XY, CoralFlower 930, CS-1 95N, zp12 97N, Dinghy 99N)
VERSION = "0.0"
# helpers the core must not need: the chip has no FPU and the build no libgcc (soft float, 64-bit division)
BANNED = re.compile(r"\b__(?:(?:u?(?:div|mod)di3)|(?:add|sub|mul|div)[sd]f3|(?:fix|float)\w*|(?:eq|ne|lt|le|gt|ge|un)[sd]f2|extendsfdf2|truncdfsf2)\b")


def toolchain():
    tc = Path(os.environ.get("JIELI_TOOLCHAIN", Path.home() / ".jieli" / "toolchain"))
    if not (tc / "pi32v2" / "bin" / "clang").exists():
        raise SystemExit(f"no JieLi toolchain in {tc} (JIELI_TOOLCHAIN)")
    return tc.resolve()


def use_docker():
    native = platform.system() == "Linux" and platform.machine() in ("x86_64", "AMD64")
    return os.environ.get("JIELI_DOCKER", "0" if native else "1") == "1"


def env():
    e = dict(os.environ)
    dc = Path.home() / ".jieli" / "docker"
    if "DOCKER_CONFIG" not in e and (dc / "config.json").exists():
        e["DOCKER_CONFIG"] = str(dc)
    return e


def tc(tool, *args):
    """a toolchain binary (pi32v2/bin/..., common/bin/...) run with cwd ROOT; Path arguments relative to it"""
    rel = [str(Path(a).resolve().relative_to(ROOT)) if isinstance(a, Path) else a for a in args]
    if tool == "cc":                # the toolchain's cc wrapper needs python3: clang directly
        tool, rel = "pi32v2/bin/clang", ["-target", "pi32v2", *rel]
    if use_docker():
        sdk = ["-v", f"{SDK_MOUNT}:/sdk:ro"] if SDK_MOUNT else []
        cmd = ["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{ROOT}:/work", *sdk,
               "-v", f"{toolchain()}:/opt/jieli:ro", "-w", "/work", DOCKER_IMAGE, f"/opt/jieli/{tool}", *rel]
    else:
        cmd = [str(toolchain() / tool), *rel]
    for _ in range(5):              # Docker's amd64 emulation now and then refuses to start a tool: again
        r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, env=env())
        if not (r.returncode and use_docker() and "exec format error" in r.stdout + r.stderr):
            break
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f"build: {tool} failed")
    if r.stderr.strip():
        sys.stderr.write(r.stderr)
    return r.stdout


def tc_all(*cmds):
    with ThreadPoolExecutor(min(4, len(cmds))) as ex:
        return list(ex.map(lambda c: tc(*c), cmds))


# ---- the update loader (Felucca's, as CS-1 builds it)

def crc16(d, c=0):
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
        c &= 0xFFFF
    return c


def ldr_head(dcrc, a, b, attr, name):
    body = struct.pack("<HIIBBH16s", dcrc, a, b, attr, 0, 0, name)
    return struct.pack("<H", crc16(body)) + body


def ldr_wrap(image):
    """ota.bin: an outer JLFS head (32 B), an inner one (32 B, the load address), then blocks [clen u32][dlen u32]
    [an LZ4 block], dlen 4096 but the last"""
    blocks = bytearray()
    for i in range(0, len(image), 4096):
        raw = image[i:i + 4096]
        c = lz4blk.compress(raw)
        if len(c) > 4096 or lz4blk.decompress(c) != raw:
            raise SystemExit(f"loader: block {i // 4096} does not compress below 4096 B")
        blocks += struct.pack("<II", len(c), len(raw)) + c
    inner = ldr_head(crc16(image), len(image), LOADER_LOAD, 0, LOADER_NAME)
    body = inner + bytes(blocks)
    return ldr_head(crc16(body), 0x20, len(body), 0x41, LOADER_NAME) + body


def build_loader():
    LDR.mkdir(parents=True, exist_ok=True)
    src = FW / "loader"
    flags = [*CFLAGS, "-Ifirmware/fm1/hal", "-Ifirmware/fm1/src"]
    tc_all(("cc", "-c", src / "crt0_ldr.S", "-o", LDR / "crt0_ldr.o"),
           ("cc", *flags, "-c", src / "loader.c", "-o", LDR / "loader.o"))
    elf = LDR / "loader.elf"
    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", src / "loader.ld",
       LDR / "crt0_ldr.o", LDR / "loader.o", "-o", elf)
    _, dis, hdr, syms = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, LDR / "loader.bin"),
                               ("common/bin/objdump", "-d", elf),
                               ("common/bin/objdump", "-h", elf),
                               ("common/bin/objdump", "-t", elf))
    (LDR / "loader.dis").write_text(dis)
    for ln in hdr.splitlines():     # the loader rewrites the flash: nothing of it may run from XIP
        p = ln.split()
        if len(p) > 4 and p[1].startswith(".") and int(p[3], 16) >= 0x02000000 and int(p[2], 16):
            raise SystemExit(f"loader: section {p[1]} at {p[3]} is outside RAM")
    image = (LDR / "loader.bin").read_bytes()
    if not image or len(image) > 0x14000:
        raise SystemExit("loader: image empty or too big")
    ota = ldr_wrap(image)
    (LDR / "ota.bin").write_bytes(ota)
    print(f"loader: image {len(image)} B at {LOADER_LOAD:#x}, ota.bin {len(ota)} B")
    if is_ble():                    # gate 1: a Bluetooth build ships the base release's update loader, no other
        pin, got = ble_base("LOADER_SHA256"), hashlib.sha256(ota).hexdigest()
        if got != pin:
            raise SystemExit(f"loader: ota.bin {got} is not the pinned loader (tools/ble_base.txt)")
    return ota


# ---- the Bluetooth build (FELUCCA_BLE=1; BLE MIDI: 2026 DEADACTIVE, from Felucca's ble-kit build.py, as OberVave's)

def is_ble():
    return os.environ.get("FELUCCA_BLE") == "1"


def ble_base(key):
    """a value of tools/ble_base.txt (BLE_BASE_FILE: another copy, for the gates' own tests)"""
    f = Path(os.environ.get("BLE_BASE_FILE") or ROOT / "tools" / "ble_base.txt")
    for ln in f.read_text().splitlines():
        if ln.startswith(key + " "):
            return ln.split()[1]
    raise SystemExit(f"build: {f} has no {key}")


REGION = r"^\s*(\w+)\s*\([rwx]+\)\s*:\s*ORIGIN\s*=\s*(\w+),\s*LENGTH\s*=\s*(\w+)"


def ld_regions(script=None):
    """a linker script's MEMORY regions (default firmware/fm1/app.ld): name -> (origin, length)"""
    regs = {}
    for m in re.finditer(REGION, (script or FW / "app.ld").read_text(), re.M):
        n = m.group(3)
        regs[m.group(1)] = (int(m.group(2), 0), int(n[:-1], 0) * 1024 if n.endswith("K") else int(n, 0))
    return regs


def gate2c_listings(flags):
    """gate 2c: the glue's unit (the platform, the update path) compiled twice with the same flags, once as the plain
    build's (no FELUCCA_BLE) and once as the Bluetooth build's, both without global merging (which small statics
    share a block depends on the whole program), each linked with app.ld and the core (the Bluetooth one with empty
    BLE entry points) -> their listings. What differs between them in an update-path function is what FELUCCA_BLE
    changes in that function's code"""
    g = OUT / "gate2c"
    g.mkdir(exist_ok=True)
    nomerge = ["-mllvm", "-enable-global-merge=false"]
    out = {}
    for tag, fl, extra in (("plain", [f for f in flags if f != "-DFELUCCA_BLE=1"], []),
                           ("ble", flags, [OUT / "ble_stub.o"])):
        tc("cc", *fl, *nomerge, "-c", FW / "m2k_fm1.c", "-o", g / f"{tag}.o")
        tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", FW / "app.ld", OUT / "crt0.o", OUT / "fm1_vec.o",
           OUT / "fm1_isr.o", g / f"{tag}.o", OUT / "core.o", *extra, "-o", g / f"{tag}.elf")
        out[tag] = tc("common/bin/objdump", "-d", g / f"{tag}.elf")
        (g / f"{tag}.dis").write_text(out[tag])
    return out["plain"], out["ble"]


# ---- the app: the platform and its glue (m2k_fm1.c), and MONO2K's core (firmware/src/m2k_core.c), two units

# the second core renders some tracks of each block (the core's m2k_par_begin; DUAL=0 in the environment: one core)
DUAL = 0 if os.environ.get("DUAL") == "0" else 1
# the FM-1's build: one project of 64 patterns (bank A the demos, B-D INIT), two of them unpacked at a time, the rest
# packed in a 46 KB working image (README: the FM-1 build)
CORE = ["-DM2K_FW=1", "-DM2K_PATTERNS=64", "-DM2K_PAT_SLOTS=2", "-DM2K_PROJECTS=1", "-DM2K_IMG_DATA=47104"]
# the Bluetooth build (FELUCCA_BLE=1): one core (DUAL 0: the BLE unit assumes one, and SLOOP's two-core Bluetooth build
# lost its sound), the core's side of the Bluetooth page (M2K_BT), and the fit tools of PORTING.md section 7: no
# console words (MONO2K has no serial console: BLE_CONSOLE=0), the BLE unit at -Oz
BLE_FLAGS = ["-DBLE_CONSOLE=0"]
BLE_CORE = ["-DM2K_BT=1"]
BLE_OPT = "-Oz"


def build_app():
    ble = is_ble()
    flags = [*CORE, *CFLAGS, "-Ifirmware/fm1/hal", "-Ifirmware/fm1/src", "-Ifirmware/fm1/gen", "-Ifirmware/src",
             "-Ifirmware/gen", f'-DFELUCCA_ID="{PRODUCT}"', f'-DAPP_VER="{VERSION}"',
             f"-DDINGHY_DUAL_CORE={DUAL}"]
    core = [*CORE]
    if ble:                         # (the glue's unit, the BLE unit and the core agree on BLE_FLAGS)
        flags += ["-DFELUCCA_BLE=1", *BLE_FLAGS]
        core += BLE_CORE
    cmds = [("cc", f"-DDINGHY_DUAL_CORE={DUAL}", "-c", FW / "crt0.S", "-o", OUT / "crt0.o"),
            ("cc", "-c", FW / "hal" / "fm1_vec.S", "-o", OUT / "fm1_vec.o"),
            ("cc", "-c", FW / "hal" / "fm1_isr.S", "-o", OUT / "fm1_isr.o"),
            ("cc", *flags, "-c", FW / "m2k_fm1.c", "-o", OUT / "mono2k.o"),
            ("cc", *[f for f in CFLAGS if f != "-Os"], "-O2", *core, "-Ifirmware/src", "-Ifirmware/gen", "-c",   # the core
             ROOT / "firmware" / "src" / "m2k_core.c",                                                              # for speed
             "-o", OUT / "core.o")]
    cfg_objs = []
    if ble:                         # (from Felucca's ble-kit build.py; Hortator, fm1-lsdj 548ce73; OberVave's)
        global SDK_MOUNT
        import ble_libs
        if not use_docker():
            raise SystemExit("build: FELUCCA_BLE builds run the toolchain in Docker (the SDK is mounted at /sdk)")
        ble_libs.libs()             # first: a missing SDK or a library that is not the pinned one stops here, named
        SDK_MOUNT = str(ble_libs.SDK)
        cmds.append(("cc", "-c", FW / "hal" / "fm1_ctx.S", "-o", OUT / "fm1_ctx.o"))
        (OUT / "blecfg").mkdir(exist_ok=True)
        for src in ble_libs.SDK_CONFIG_SOURCES:     # the SDK's BT config as bitcode: LTO folds it into the libraries
            obj = OUT / "blecfg" / (Path(src).stem + ".o")
            cmds.append(("cc", *ble_libs.SDK_CFLAGS, "-c", f"/sdk/{src}", "-o", obj))
            cfg_objs.append(obj)
        cmds.append(("cc", "-mcpu=r3", "-Os", "-flto", "-Wall", "-c", ROOT / ble_libs.LTO_STUBS, "-o",
                     OUT / "blecfg" / "ble_lto_stubs.o"))
        cfg_objs.append(OUT / "blecfg" / "ble_lto_stubs.o")
    tc_all(*cmds)
    undef = tc("common/bin/objdump", "-t", OUT / "mono2k.o") + tc("common/bin/objdump", "-t", OUT / "core.o")
    bad = sorted({m.group(0) for m in BANNED.finditer(undef)})
    if bad:
        raise SystemExit(f"the app needs helpers the FM-1 has not: {', '.join(bad)} (a float or a 64-bit division)")
    elf = OUT / "mono2k.elf"
    extra, link, script = [], ("pi32v2/bin/ld",), FW / "app.ld"
    if ble:                         # (only then: the plain link is as before)
        libdir = OUT / "blelibs"
        libdir.mkdir(exist_ok=True)
        libs = ble_libs.libs()
        for p in libs:              # inside ROOT, so the Docker mount sees them
            shutil.copy(p, libdir / Path(p).name)
        objs = ble_libs.members(libdir)     # single SDK members (lbuf, circular_buf, wlc, encryption)
        # the BLE module, its own unit: no static data shared with the other units. Long calls, as the libraries
        # (large-program): the BLE unit calls RAM code (__wrap_memcpy, the delays). No global merging, as the
        # object Hortator's device tests ran. -Oz (PORTING.md section 7, a fit tool): its delays are calibrated
        # against TIMER4 at start-up, so the counted loops keep their length
        tc("cc", *[f if f != "-Os" else BLE_OPT for f in flags], "-mllvm", "-enable-global-merge=false",
           "-mllvm", "-pi32v2-large-program=true", "-c", ROOT / "firmware" / "src" / "ble" / "ble.c",
           "-o", OUT / "ble.o")
        objs.append(OUT / "ble.o")
        extra = [OUT / "fm1_ctx.o", *cfg_objs, *objs, "--start-group", *[libdir / Path(p).name for p in libs],
                 "--end-group"]
        # the BT libraries are LLVM bitcode: link them as the SDK does (apps/demo/demo_ble/board/wl82/Makefile
        # LFLAGS), through the gold plugin with its codegen options; our objects stay native
        link = (*link, "--plugin", "/opt/jieli/pi32v2/bin/LLVMgold.so", "--plugin-opt=mcpu=r3",
                "--plugin-opt=-pi32v2-large-program=true", "--plugin-opt=-pi32v2-always-use-itblock=false",
                "--plugin-opt=-dont-used-symbol-list=" + ",".join(ble_libs.LOG_FUNCS),
                "--orphan-handling=error",          # every library section placed on purpose
                # the libraries' (and the BLE unit's) memcpy calls go to ble_port.c's RAM copy, __wrap_memcpy: the
                # radio calibration calls memcpy while the flash must not be read. --wrap redirects undefined
                # references only, so the glue's unit, which defines memcpy, keeps calling its own
                "--wrap=memcpy")
        script = FW / "app_ble.ld"
        if ld_regions(script) != ld_regions():
            raise SystemExit("build: firmware/fm1/app_ble.ld's MEMORY block differs from firmware/fm1/app.ld's")
        if os.environ.get("BLE_MEASURE") == "1":
            # measuring only (never packaged, main()): the XIP to the flash's end, the RAM region to 160 KiB and
            # the pool out of the way (an address range the chip has not: sizes only), so an image that does not fit
            # yet still links and can be measured
            lds = script.read_text()
            lds, n = re.subn(r"(XIP\s+\(rx\)\s*: ORIGIN = 0x02000120, LENGTH = )\w+", r"\g<1>0xFFEE0", lds)
            lds, k = re.subn(r"(POOL\s+\(rw\)\s*: ORIGIN = )\w+(, LENGTH = )\w+", r"\g<1>0x01D00000\g<2>0x100000", lds)
            lds, j = re.subn(r"(RAM\s+\(rwx\)\s*: ORIGIN = \w+, LENGTH = )\w+", r"\g<1>160K", lds)
            if n != 1 or k != 1 or j != 1:
                raise SystemExit("build: BLE_MEASURE: app_ble.ld's XIP / POOL / RAM lines not found")
            script = OUT / "app_ble_measure.ld"
            script.write_text(lds)
    units = [OUT / "mono2k.o", OUT / "core.o"]
    if ble:                         # the glue's and the core's units as one object first: the core's memcpy calls
        # (struct copies) then reach the glue's libc.c memcpy as a defined symbol, which --wrap=memcpy leaves alone
        # (it redirects undefined references only: to ble_port.c's RAM copy, out of a call's reach from XIP)
        tc("pi32v2/bin/ld", "-r", *units, "-o", OUT / "app_units.o")
        units = [OUT / "app_units.o"]
    tc(*link, "--gc-sections", "-e", "_start", "-T", script, OUT / "crt0.o", OUT / "fm1_vec.o",
       OUT / "fm1_isr.o", *units, *extra, "-o", elf, "-Map", OUT / "mono2k.map")
    ref_dis = None
    if ble:                         # library code meant for RAM would have landed in XIP via *(.*_code): refuse
        # .volatile_ram_code (wl_rf_common's wf_rf_trim) is placed in .ram_text with the trim (app_ble.ld);
        # .bt_updata_ram_code = bredr_frame's BT-over-the-air-update helpers, never run, allowed in XIP (fm1-lsdj
        # docs/ble/LINK_NOTES.md: checked by hand)
        ok = {".volatile_ram_code", ".bt_updata_ram_code"}
        ram_code = sorted(set(re.findall(r"^\s*(\.\S*ram_code\S*)", (OUT / "mono2k.map").read_text(), re.M)) - ok)
        if ram_code:
            raise SystemExit(f"build: BT library RAM code sections linked (not placed in RAM): {ram_code}")
        # gate 2b's reference: the same objects, empty BLE entry points, app.ld
        tc("cc", *flags, "-c", ROOT / "firmware" / "src" / "ble" / "ble_stub.c", "-o", OUT / "ble_stub.o")
        tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", FW / "app.ld", OUT / "crt0.o", OUT / "fm1_vec.o",
           OUT / "fm1_isr.o", OUT / "mono2k.o", OUT / "core.o", OUT / "ble_stub.o", "-o", OUT / "mono2k_ref.elf")
        ref = tc("common/bin/objdump", "-d", OUT / "mono2k_ref.elf")
        (OUT / "mono2k_ref.dis").write_text(ref)
        ref_dis = (ref, *gate2c_listings(flags))
    for sect in ("text.bin", "data.bin", "ramtext.bin"):
        (OUT / sect).unlink(missing_ok=True)
    *_, syms, dis, rt = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, OUT / "text.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".data", elf, OUT / "data.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".ram_text", elf, OUT / "ramtext.bin"),
                               ("common/bin/objdump", "-t", elf),
                               ("common/bin/objdump", "-d", elf),
                               ("common/bin/objdump", "-d", "-j", ".ram_text", elf))
    (OUT / "mono2k.dis").write_text(dis)
    (OUT / "mono2k.sym").write_text(syms)

    def symv(name):
        return int(re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M).group(1), 16)
    img = bytearray((OUT / "text.bin").read_bytes())
    for sect, lname in (("ramtext.bin", "_rt_load"), ("data.bin", "_data_load")):   # after .text, where crt0
        load = symv(lname)                                                        # copies them from, by words
        if load % 4:
            raise SystemExit(f"{lname} {load:#x} is not word aligned")
        blob = (OUT / sect).read_bytes() if (OUT / sect).exists() else b""
        if blob:
            if load - APP_XIP < len(img):
                raise SystemExit(f"{lname} overlaps the image")
            img += b"\xff" * (load - APP_XIP - len(img))
            img += blob
    img += b"\xff" * (-len(img) % 4)
    (OUT / "mono2k.bin").write_bytes(img)
    return bytes(img), syms, dis, rt, ref_dis


def rt_reach(rt):
    """Bluetooth builds (the radio calibration runs from .ram_text and calls within it; fm1-lsdj 548ce73): RAM code
    may call and jump only inside .ram_text, never through a register, and use no XIP address as data"""
    def in_rt(a): return 0x01C00000 <= a < 0x01C06000
    bad, calls = [], 0
    for ln in rt.splitlines():
        if not LINE.match(ln):
            continue
        if re.search(r"\b(call|goto)\s+r\d+\b", ln):
            bad.append(ln)
            continue
        for a in re.findall(r"<[^>]*:\s*([0-9a-f]+)\s*>", ln):
            if 0x02000000 <= int(a, 16) < 0x02100000 or (re.search(r"\b(call|goto)\b", ln) and not in_rt(int(a, 16))):
                bad.append(ln)
        calls += bool(re.search(r"\bcall\b", ln))
    return bad, calls


# the BT memory's place (OberVave's gate, after SLOOP's Bluetooth build lost its sound): every BT library section with
# data the controller or the radio uses, and the BLE unit's heap and task stacks, where Felucca keeps them (the layout
# the FM-1 has run Bluetooth with): the libraries' zeroed data (.bd_base, *_bss) in .bss of the RAM region, the heap
# and stacks in .pool, nothing of either in the SRAM below the RAM region (0x01C00000..0x01C08000: RAM code only),
# and no second core (no CPU1 entry, loop or stacks: the BLE unit runs on one core)
BT_RAM_SECTIONS = re.compile(r"^ (\.bd_base|\.\w+_bss|\.btstack_pool|\.\w+_bulk)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)", re.M)
CPU1_SYMS = re.compile(r"\s(fm1_core1_start|fm1_core1_main|audio_worker_loop|cpu1_stacks|_cpu1_stacks_lo)$", re.M)


def ble_placement(mapfile, syms, regions):
    """-> errors: a BT library's RAM section, the BLE heap or a task stack outside its region, or a second core"""
    errors = []
    ram_org, ram_len = regions["RAM"]
    pool_org, pool_len = regions["POOL"]
    for name, at, size in BT_RAM_SECTIONS.findall(mapfile):
        a, n = int(at, 16), int(size, 16)
        if n and not (ram_org <= a and a + n <= ram_org + ram_len):
            errors.append(f"BT section {name} at {a:#x} ({n} B) is outside the RAM region")

    def sym(name):
        m = re.search(r"^([0-9a-f]+) .*\s" + re.escape(name) + r"$", syms, re.M)
        return int(m.group(1), 16) if m else None
    for name in ("ble_heap", "tasks"):           # (ble.c, ble_os.c: both in .pool)
        a = sym(name)
        if a is None:
            errors.append(f"BLE unit's {name} not found")
        elif not pool_org <= a < pool_org + pool_len:
            errors.append(f"BLE unit's {name} at {a:#x} is outside the pool")
    c1 = CPU1_SYMS.findall(syms)
    if c1:
        errors.append(f"second-core code or stack linked ({', '.join(sorted(set(c1)))}): the BLE unit runs on one core")
    return errors


def check(img, syms, dis, rt, ble=False, measure=False):
    errors, notes = [], []
    soft = notes if measure else errors          # BLE_MEASURE: the budgets are reported, not enforced

    def sym(name):
        mm = re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M)
        return int(mm.group(1), 16) if mm else 0
    if sym("_start") != APP_XIP:
        errors.append(f"_start is not at {APP_XIP:#x}")
    if img[:4] != bytes.fromhex("04818000"):
        errors.append(f"the image starts with {img[:4].hex()}, not the entry stub")
    rt_calls = []                   # code in RAM runs with the flash off: no calls out of RAM (and calls between XIP
    for ln in rt.splitlines():      # and RAM are out of a direct call's reach: through a pointer, FL_FAR)
        mc = re.search(r"\bcall\s+-?\d+\s+<([^:>]*):\s+([0-9a-f]+)\s*>", ln)
        if mc:
            if not 0x01C00000 <= int(mc.group(2), 16) < sym("_rt_end"):
                rt_calls.append(ln)
        elif re.search(r"\bcall\s+-?\d+", ln):
            rt_calls.append(ln)
    if rt_calls:
        errors.append(f".ram_text calls out of RAM: {rt_calls[:3]}")
    if ble:                         # gate 4: the radio calibration's RAM code calls inside .ram_text only
        rt_bad, n_calls = rt_reach(rt)
        if rt_bad:
            errors.append(f".ram_text reaches outside RAM: {rt_bad[:3]}")
        else:
            notes.append(f"gate 4: .ram_text {len([ln for ln in rt.splitlines() if LINE.match(ln)])} insns, {n_calls} "
                         "calls, all inside .ram_text, no XIP address")
    for ln in dis.splitlines():     # nothing may call or load an address in the chip's ROM
        mm = LINE.match(ln)
        if not mm:
            continue
        for v in re.findall(r"= (-?\d+) <|call -?\d+ <[^:>]*: ([0-9a-f]+) >", mm.group(3)):
            val = (int(v[0]) & 0xFFFFFFFF) if v[0] else int(v[1], 16) & 0xFFFFFFFF
            if 0xFFC00000 <= val < 0xFFD00000:
                errors.append(f"a reference to ROM address {val:#010x}")
    if sym("fm1_core1_start"):      # two cores: CPU1's code in RAM, its own stacks, its loop calling nothing (CS-1's)
        for name in ("fm1_core1_main", "audio_worker_loop"):
            if not 0x01C00000 <= sym(name) < sym("_rt_end"):
                errors.append(f"{name} must be in .ram_text so CPU1 idles with the flash off")
        if sym("_cpu1_stacks_hi") - sym("_cpu1_stacks_lo") != 8192:
            errors.append("CPU1 needs its 4 KiB user + 4 KiB supervisor stacks (.cpu1_stacks)")
        body = dis.split("\naudio_worker_loop:", 1)[-1].split("\n\n", 1)[0]
        if re.search(r"\bcall -?\d+", body):
            errors.append("audio_worker_loop contains a direct call; its helpers must inline (it runs with XIP off)")
        notes.append("CPU1: a polling loop in RAM, its own 8 KiB of stacks, bounded start and join")
    if len(img) > APP_SLOT:
        soft.append(f"the image, {len(img)} B, is bigger than the app slot ({APP_SLOT} B)")
    ram = sym("_bss_end") - 0x01C08000
    pool = sym("_pool_end") - (sym("_pool_start") if measure else 0x01C20000)
    if ble:
        notes.append(f"image {len(img)} B of {APP_SLOT} ({APP_SLOT - len(img)} B free); RAM .data+.bss {ram} B of "
                     f"98304 ({98304 - ram} B spare); pool {pool} B of 344064 ({344064 - pool} B spare); "
                     f"RAM code {sym('_rt_end') - 0x01C00000} B of 24576")
        if 98304 - ram < BLE_RAM_FLOOR:
            soft.append(f"RAM headroom {98304 - ram} B < {BLE_RAM_FLOOR} B")
        if 344064 - pool < BLE_POOL_FLOOR:
            soft.append(f"pool headroom {344064 - pool} B < {BLE_POOL_FLOOR} B")
        placed = ble_placement((OUT / "mono2k.map").read_text(), syms, ld_regions())
        (notes if measure else errors).extend(placed)
        if not placed:
            notes.append("BT memory: the libraries' zeroed data in .bss of the RAM region, the heap and task stacks "
                         "in .pool, nothing below 0x01C08000 but RAM code, one core")
    else:
        notes.append(f"image {len(img)} B of {APP_SLOT}; RAM .data+.bss {ram} B of 98304; pool {pool} B of 344064; "
                     f"RAM code {sym('_rt_end') - 0x01C00000} B of 24576")
    if sym("_rt_end") > 0x01C06000:
        errors.append(f"RAM code ends at {sym('_rt_end'):#x}, past 0x1c06000")
    if ram > 96 * 1024:
        soft.append(f"RAM .data+.bss {ram} B > 96 KiB")
    if pool > 0x54000:
        soft.append(f"pool {pool} B > {0x54000} B")
    return errors, notes


def main():
    global VERSION
    ap = argparse.ArgumentParser()
    ap.add_argument("--release", metavar="X.Y[.Z]", help="a release: version X.Y[.Z], mono2k-X.Y[.Z].fwsc (identity FM-1_980)")
    ap.add_argument("--sdk", type=Path, help="the JieLi AC79 SDK (default: $AC79_SDK or ~/fw-AC79_AIoT_SDK)")
    ap.add_argument("--diag", action="store_true", help="a test build: the render's load along the top edge")
    a = ap.parse_args()
    global DUAL
    ble = is_ble()
    measure = ble and os.environ.get("BLE_MEASURE") == "1"
    if ble:
        DUAL = 0
    name = "mono2k.fwsc"
    if a.diag:
        CORE.extend(["-DM2K_FM1_DIAG=1"])
        name = "mono2k-test.fwsc"
    if a.release:
        if not re.fullmatch(r"\d+\.\d+(\.\d+)?", a.release):
            raise SystemExit(f"--release {a.release}: X.Y or X.Y.Z")
        VERSION, name = a.release, f"mono2k-{a.release}.fwsc"
    if ble:                         # a gated Bluetooth build: its own version and name, never the release's
        VERSION = (a.release or VERSION) + "-ble"
        name = f"mono2k-{VERSION}{'-test' if a.diag else ''}.fwsc"
    fm1pkg_make.SDK = a.sdk or os.environ.get("AC79_SDK") or str(Path.home() / "fw-AC79_AIoT_SDK")
    for rel, sha in SDK_SHA256.items():
        if hashlib.sha256(fm1pkg_make.sdk_file(rel)).hexdigest() != sha:
            print(f"warning: the SDK's {rel} is not AC79NN_SDK_V1.2.1's")
    OUT.mkdir(parents=True, exist_ok=True)
    ota = build_loader()
    img, syms, dis, rt, ref_dis = build_app()
    errors, notes = check(img, syms, dis, rt, ble, measure)
    if ble and not measure:         # gates 2-3: every Bluetooth build stops on them
        if not (ROOT / "tools" / "check_update_path.py").exists():
            raise SystemExit("FELUCCA_BLE=1: gates 2-3 are not in this tree yet (BLE_MEASURE=1 measures only)")
        import check_update_path as cup
        import check_ble_boot as cbb
        base = ble_base("UPSTREAM")
        ref_dis, plain_nm, ble_nm = ref_dis
        ref = cup.selected(cup.functions(ref_dis))
        e2a = cup.source_errors(base)
        e2b = cup.compare(dis, cup.render(ref))
        e2c, same2c, known2c = cup.compare_builds(plain_nm, ble_nm)
        e3 = cbb.reachable_starts(dis)
        errors += [f"gate 2a: {e}" for e in e2a] + [f"gate 2b: {e}" for e in e2b] + [f"gate 2c: {e}" for e in e2c]
        errors += [f"gate 3: BT start-up reachable at boot: {', '.join(e3)}"] if e3 else []
        notes.append("gate 1: the update loader = the pinned one (tools/ble_base.txt)")
        if not e2a:
            notes.append(f"gate 2a: the update path's sources = {base}'s (platform.c: its plain view)")
        if not e2b:
            notes.append(f"gate 2b: the update path's machine code = the reference build's ({len(ref)} functions)")
        if not e2c:
            notes.append(f"gate 2c: the update path compiled for the Bluetooth build = the plain build's "
                         f"({same2c} functions identical; differing as listed in check_update_path.py: "
                         f"{', '.join(known2c) or 'none'})")
        if not e3:
            notes.append("gate 3: no BT start-up reachable at boot (only through ble_user_start)")
    for n in notes:
        print("  ok   ", n)
    for e in errors:
        print("  FAIL ", e)
    if errors:
        raise SystemExit("build: the checks failed")
    if measure:
        print("measured only (BLE_MEASURE=1): no package")
        return 0
    pkg = fm1pkg_make.ufw(fm1pkg_make.flash_image(img, fm1pkg_make.KEY), ota, PRODUCT)
    (OUT / name).write_bytes(pkg)
    print(f"package  {OUT / name}  {len(pkg)} B, identity {PRODUCT}, sha256 {hashlib.sha256(pkg).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
