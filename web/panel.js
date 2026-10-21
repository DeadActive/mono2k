// SPDX-License-Identifier: GPL-3.0-only
// The FM-1's panel, as measured for the community's emulators (CoralFlower's emu.js), and what MONO2K puts on each
// control: one table for the emulator (app.js) and the manual's drawings (manual.js).
window.M2K_PANEL = (() => {
  "use strict";
  const WHITE_X = [71, 119, 167, 215, 264, 312, 360, 408, 456, 504, 553, 601, 649, 697, 745, 794];
  const BLACK_X = [95, 143, 191, 288, 336, 432, 480, 529, 625, 673, 769];
  const isBlack = (k) => (0x54a >> ((k + 5) % 12)) & 1;
  const KEYR = [];
  for (let k = 0, w = 0, b = 0; k < 27; k++) KEYR.push(isBlack(k) ? [BLACK_X[b++], 326, 42, 76] : [WHITE_X[w++], 408, 42, 76]);
  const BTNR = [];
  for (let i = 0; i < 12; i++) BTNR.push([[500, 554, 608, 661, 715, 768][i % 6], [177, 231][(i / 6) | 0], 36, 36]);
  BTNR.push([87, 250, 42, 22], [153, 250, 42, 22]);
  return {
    WHITE_X, BLACK_X, isBlack, KEYR, BTNR,
    ENC_CX: [186.5, 186.5, 95, 508, 604, 701, 797, 95], ENC_CY: [101, 191, 191, 101, 101, 101, 101, 101],
    BTN: ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"],
    ROLE: ["FX", "FILT", "AMP", "LFO", "SYN", "GLO", "HOME", "SAVE", "ARP", "GRID", "PLAY", "REC/FILL", "", ""],
    ENC: ["SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4", "MASTER"],
    EROLE: ["TRACK", "MACHINE", "PATTERN", "K1", "K2", "K3", "K4", "VOLUME"],
    NOTE: ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"],
    BLACK_ROLE: ["T1", "T2", "T3", "T4", "T5", "T6", "P1", "P2", "P3", "P4", "MUTE"],
  };
})();
