/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE (from Felucca's ble-kit core_ble_api.c, as OberVave's) */
/* FELUCCA_BLE: the core functions the BLE unit calls (firmware/src/ble/ble_core.h), on MONO2K's platform and glue
 * (m2k_fm1.c includes this file last, after the Bluetooth features). MONO2K has no serial console: built with
 * BLE_CONSOLE=0 (tools/fm1/build.py), the console's hooks (core_audio_late, core_audio_gap_us, core_ui_frames,
 * core_ble_in_drops) are not declared and not defined, and the page's dim line shows the audio's timing instead. The
 * serial test build (--diag-serial: FELUCCA_CDC=1, BLE_CONSOLE=1) has the USB serial port: a line `ble ...` typed
 * there is the BLE unit's console (ble_console; Felucca's words: ble, ble status, ble list, ...), its answers go out
 * on the port. */
#if BLE_CONSOLE
static uint32_t bt_frames;                          /* main-loop passes (core_ui_frames: the BLE unit's loop period) */
#endif
#ifndef BT_HOST                                     /* (the host's tests: tests/bt_host.h stands in for the BLE unit) */
#include "fm1_ble_hal.h"                            /* fm1_guard_stack_window, fm1_irq_unmask / _is_masked */
extern uint32_t _pool_start[];
uint32_t core_ms(void) { return fm1_ms; }
void core_wdt_feed(void) { fm1_wdt_feed(); }
void core_reboot(void) { fm1_reboot(); }
void core_audio_stop(void) { fm1_audio_stop(); }
/* IRQs off (the caller). The BT libraries' requests come at priority 2 at most (ble_port.c request_irq lowers any
 * above): under the audio interrupt (3, platform.c fm1_audio_init) and TIMER5 (4). One core in this build (no second
 * core waits inside the render), so the libraries' long interrupts-off stretches can only delay the audio interrupt
 * (m2k_fm1.c bt_a_gap, platform.c audio_lates: the Bluetooth page shows them) */
void core_irq_attach(uint32_t n, void (*h)(void), uint32_t prio)
{
    fm1_guard_unlock_top();                         /* the vector table is write-protected after boot */
    fm1_irq_attach(n, h, prio);
    fm1_guard_lock_top();
}
void core_ble_stack_window(void)                    /* the BLE tasks' stacks are in .pool, below the main stacks */
{
    fm1_guard_stack_window((uint32_t)(uintptr_t)_pool_start, (uint32_t)(uintptr_t)_sstack_top);
}
void core_irq_mask(uint32_t n) { fm1_irq_mask(n); }
void core_irq_unmask(uint32_t n) { fm1_irq_unmask(n); }
uint32_t core_irq_is_masked(uint32_t n) { return fm1_irq_is_masked(n); }
void core_midi_out(uint32_t pkt) { midi_out_event(pkt); }
int core_flash_read(uint32_t off, void *buf, uint32_t n)    /* plain flash only (0x93000..), read only */
{
    if (!flash_ok || off < 0x93000u || off > 0x100000u || n > 0x100000u - off)
        return -1;
    return st_read(off, buf, n);
}
int core_ble_slow(void) { return bt_set_slow(m2k_playing()); }
#if FELUCCA_CDC
void core_con_puts(const char *s)                   /* the serial test build: into the USB serial port's out ring (usb.c:
                                                     * TIMER5 sends it while a host has the port open); what does not
                                                     * fit is dropped */
{
    while (*s && co_w - co_r < CO_N) {
        cdc_out[co_w % CO_N] = (uint8_t)*s++;
        RING_PUBLISH();
        co_w++;
    }
}
#else
void core_con_puts(const char *s) { (void)s; }      /* (MONO2K has no serial console: FELUCCA_CDC 0) */
#endif
#if BLE_CONSOLE
uint32_t core_audio_late(void) { return audio_lates; }
uint32_t core_audio_gap_us(void)                    /* the longest gap between two audio halves since the last call */
{
    uint32_t g = bt_a_gap_con;
    bt_a_gap_con = 0;
    return g / FM1_TICKS_PER_US;
}
uint32_t core_ui_frames(void) { return bt_frames; }
uint32_t core_ble_in_drops(void) { return bt_in.drops; }
#endif
void core_ble_mac(uint8_t mac[6]) { bt_mac(mac); }
int core_rf_keep_get(uint8_t rec[68]) { return bt_rf_io(rec, 0); }
void core_rf_keep_put(const uint8_t rec[68]) { (void)bt_rf_io((uint8_t *)rec, 1); }
void core_ble_midi_in(uint32_t pkt) { bt_midi_in(pkt); }
void core_fml_rx(const uint8_t *p, uint32_t len, uint32_t t) { bt_fml_rx(p, len, t); }
void core_ble_link(int up)
{
    bt_link(up);
    m2k_toast(up ? "BT CONNECTED" : "BT LOST", 0);
}
#endif
#if BLE_CONSOLE && FELUCCA_CDC && !defined(BT_HOST)
/* the serial test build: a line from the USB serial port; `ble ...` goes to the BLE unit's console */
static void bt_con_poll(void)
{
    static char line[48];
    static uint32_t n;
    while (ci_r != ci_w) {
        char c = (char)cdc_in[ci_r % CI_N];
        ci_r++;
        if (c == '\r' || c == '\n') {
            line[n] = 0;
            if (n >= 3u && line[0] == 'b' && line[1] == 'l' && line[2] == 'e' && (line[3] == ' ' || !line[3]))
                ble_console(line[3] ? line + 4 : line + 3);
            n = 0;
        } else if (n + 1u < sizeof line) {
            line[n++] = c;
        }
    }
}
#endif
/* the main loop's hook (app_loop, every pass): the BT stack's slice (at most 5 ms, ble/ble.c), then the Bluetooth
 * features' own work, cheap while nothing changed */
__attribute__((noinline)) static void bt_service(void)
{
#if BLE_CONSOLE
    bt_frames++;
#endif
#if BLE_CONSOLE && FELUCCA_CDC && !defined(BT_HOST)
    bt_con_poll();
#endif
    ble_service();
    bt_page_auto();                                 /* AUTOSTART: once, a few seconds after a clean boot (bt_page.c) */
    bt_devs_frame();                                /* the saved devices: each link's target, the scan wanted */
    bt_midi_frame();                                /* MIDI mode: the core's MIDI out to the device; the SMC layout */
    bt_fml_frame();                                 /* FM-Link: the role from SYNC IN, its messages to the other FM-1 */
}
