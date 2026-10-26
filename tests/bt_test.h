/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* The Bluetooth build on the host, for the tests/bt_*_test.c: MONO2K's core as the FM-1's Bluetooth build has it
 * (firmware/src/m2k_core.c, M2K_BT) and the FM-1's Bluetooth glue (firmware/fm1/bt_*.c, BT_HOST) in one program,
 * talking through m2k.h's M2K_BT part as on the FM-1, with the BLE unit and the platform as tests/bt_host.h has them.
 * Build (tests/run_bt_tests.sh, which makes the marks' header first):
 *   cc -DM2K_BT=1 -DBT_HOST -Ifirmware/src -Ifirmware/gen -Ifirmware/fm1 -Ibuild/host/bt -o build/host/bt/x tests/x.c */
#if !M2K_BT
#error "build with -DM2K_BT=1 -DBT_HOST"
#endif
#include <stdio.h>
#include <string.h>
#include "m2k_core.c"
#include "bt_host.h"
#include "../firmware/fm1/bt_store.c"
#include "../firmware/fm1/bt_set.c"
#include "../firmware/fm1/bt_midi.c"
#include "../firmware/fm1/bt_fml.c"
#include "../firmware/fm1/bt_devs.c"
#include "../firmware/fm1/bt_page.c"
#include "../firmware/fm1/core_ble_api.c"

static int fails;
static void ok(const char *what, int good)
{
    printf("%s  %s\n", good ? "ok  " : "FAIL", what);
    fails += !good;
}
static int str_eq(const char *a, const char *b) { return !strcmp(a, b); }
static uint32_t slen(const char *s) { return (uint32_t)strlen(s); }

/* the Bluetooth state as at a power-on with an empty store (or the store as the test left it: keep 1); the core as
 * at its start */
static void bt_fresh(int keep_store)
{
    if (!keep_store)
        memset(host_bts, 0, sizeof host_bts);
    memset(&bts, 0, sizeof bts);
    bts_dirty = bts_copy = 0;
    bts_load();                                     /* (app_init's) */
    memset(&bs, 0, sizeof bs);
    memset(&bh, 0, sizeof bh);
    memset(&bt_dv, 0, sizeof bt_dv);
    memset(&btp, 0, sizeof btp);
    memset(&bt_in, 0, sizeof bt_in);
    bt_rx_w = bt_rx_r = bt_out_w = bt_out_r = 0;
    memset(&bt_auto, 0, sizeof bt_auto);
    memset(&bt_fml, 0, sizeof bt_fml);
    memset(&bt_fo, 0, sizeof bt_fo);
    conn_up = 0;
    bs_role = -1;
    bs_fml_ready = 0;
    host_ble_out_n = 0;
    host_ble_room = 64;
    fm1_ms = 100000u;
    bt_blk_t = 0;
    m2k_init(1);
    m2k_smc_link(0);
    smc.dark = 0;
}
static uint32_t ble_out_find(uint8_t st, uint8_t d1, uint8_t d2)   /* how many sent to the device */
{
    uint32_t i, n = 0;
    for (i = 0; i < host_ble_out_n && i < 256u; i++)
        n += host_ble_out[i][0] == st && host_ble_out[i][1] == d1 && host_ble_out[i][2] == d2;
    return n;
}
static void flush(void)                             /* the main loop's store pass, a second after the change */
{
    fm1_ms += 1500u;
    bt_store_frame(1);
}
