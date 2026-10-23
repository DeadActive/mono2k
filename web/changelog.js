// SPDX-License-Identifier: GPL-3.0-only
// The change log the manual shows (manual.html#changelog): one entry per update pushed, newest first, each line
// under one of the kinds below. Plain text: what changed for whoever plays MONO2K, not how.
window.M2K_KINDS = {
  machines: "Machines", sound: "Sound", sequencer: "Sequencer", workflow: "Workflow",
  patterns: "Patterns", screen: "Screen", web: "Website", fixes: "Fixes",
};
window.M2K_CHANGES = [
  {
    date: "2026-10-23", title: "VO-6, the Monomachine's voice",
    items: [
      ["machines", "VOX-6 rebuilt on the Monomachine's VO-6 page, measured: VOC1 and VOC2 set the vowel's two formants, VOIC whispers it, its pitch and level as the Monomachine's (it fades in from each trig). CUTUP's voice keeps its vowels. The consonants come next."],
      ["web", "The Installer connects to the FM-1 over USB in Chrome or Edge and says what it runs: M-VAVE's firmware, MONO2K, another open firmware, or an update waiting to finish. It installs MONO2K once the FM-1 build is published (or a test package from a file), finishes an update cut short, and takes the FM-1 back to M-VAVE's V15 from the official file."],
      ["fixes", "VOX-6's CONS steps evenly through its 21 choices, as the Monomachine's screen does."],
    ],
  },
  {
    date: "2026-10-22", title: "Trig types, a new site, moving marks",
    items: [
      ["sequencer", "Trig types: each step can trig the amp, the filter's envelope and the LFOs on their own, in eight states, as on the Monomachine. Older patterns play as they did."],
      ["machines", "FM+: the 24 ratio steps fall where the Monomachine's sound puts them; eight values that sat a step off now match."],
      ["machines", "FX-RING rebuilt on the Monomachine's RINGMOD, measured: it hears the track before after its VOL and LEV, INP drives it on the Monomachine's curve into a clip of its own, and WAVE sweeps its carrier from a sine through a triangle to a square. A ring saved before at INP 100 now drives its input harder."],
      ["screen", "Each machine's mark moves in its own way, and jumps with the beat while a pattern plays."],
      ["sound", "HOLD counts in beats at the pattern's tempo, as on the Monomachine (32 is a beat, 8 a sixteenth); it was a fixed time. Saved patterns and bank A keep the holds they had at their own tempo."],
      ["screen", "SWAV-SAW's three peaks ride the music, each a little after the one before, each falling at its own pace."],
      ["screen", "A new boot splash: the brand's logotype resolves out of coarse blocks, its dither drifting as the site's hero does, and breaks once before the first page."],
      ["web", "A Manual, an Installer page and a Patch Editor, under a new sub-header."],
      ["web", "The brand's isotype in the header and on the panel; the logotype as the hero, a slowly drifting bit-crusher that breaks now and then and crushes on the beat."],
      ["web", "The Patch Editor's audition keys float at the bottom of the window; new link arrows."],
      ["web", "The header's isotype crushes and breaks on hover, then locks solid."],
      ["web", "Every button, tab, link and card glitches as you point at it: its rows slip sideways and its dither crushes, then it settles; a press inverts and slips it once, and an opened panel deals its contents in."],
      ["web", "This change log."],
    ],
  },
  {
    date: "2026-10-21", title: "MAMMOTH, and bank A in a new order",
    items: [
      ["patterns", "A new pattern, MAMMOTH, in place of PYLONS."],
      ["patterns", "Bank A ordered by energy: each row of four opens on a hard part, with a slow one in every row. MAMMOTH at A01, CUTUP at A02, RIVETS at A11."],
      ["screen", "The machine dots fit their column."],
      ["web", "Every parameter, page by page, under the machine cards."],
      ["web", "Support MONO2K on Ko-fi, from the header and the footer."],
    ],
  },
  {
    date: "2026-10-20", title: "FX-CHORS's feedback",
    items: [
      ["fixes", "FX-CHORS's feedback reaches ±0.9 as designed, so it rings all the way up."],
    ],
  },
  {
    date: "2026-10-19", title: "GND-SIN and GND-NOIS on the Monomachine's pages",
    items: [
      ["machines", "GND-SIN on the Monomachine's page: TUNE on H and a plain sine at init, with MONO2K's own fold, sub, noise, drop, click and harmonic on A to G. Older patterns are converted on load."],
      ["machines", "GND-NOIS is the Monomachine's noise (ST, RED, STON); MONO2K's own noise percussion is now GND-PERC."],
      ["machines", "SWAV-ENS's chorus spreads across the stereo, as the Monomachine's does."],
      ["sound", "MOD's chorus and flanger in stereo, the low end kept in the middle."],
      ["workflow", "MIDI notes play alike at any velocity, as on the Monomachine."],
      ["web", "Machine cards: each machine's name, class, style and stats."],
    ],
  },
  {
    date: "2026-10-18", title: "The FM and SID machines on the Monomachine's pages, three FX machines",
    items: [
      ["machines", "FM+STAT and FM+DYN on the Monomachine's pages, and a new FM+PAR: three modulators side by side on one carrier."],
      ["machines", "SID-6581 on the Monomachine's page."],
      ["machines", "SWAV-PUL, a new machine: a pulse with its unison, subs and a sweeping width."],
      ["machines", "Three FX machines that work on the track before them: FX-CHORS, FX-RING and FX-DYNMX."],
      ["sound", "DIST: each machine clips at its own ceiling, as on the Monomachine."],
      ["patterns", "New parts in bank A: PYLONS, RIVETS, ORCHARD, SPLINTR, TENFOUR, FOUNDRY, CAROM, VESPER, CUTUP and GRISTLE."],
    ],
  },
  {
    date: "2026-10-17", title: "Measured against the Monomachine",
    items: [
      ["sound", "The track chain rebuilt on the Monomachine's and measured on its OS: the amp envelope's times, the filters, the drive, the EQ, the rate reduction and the delay."],
      ["machines", "SWAV-SAW rebuilt on SWAVE-SAW, and SWAV-ENS on SWAVE-ENS, both measured on the Monomachine's OS."],
      ["machines", "DIGI-WAV plays the author's own bank of 64 waves, with a bank choice per track."],
      ["sound", "A reverb you can hear, and a MOD effect on every track: a chorus, a flanger or a phaser."],
      ["sound", "The bus's glitch, hiss, ADPCM, drive and tape on laws measured over their whole range; the tape's flutter is a quarter of its wow, not four times it."],
      ["patterns", "A new bank A on the new chain: HAZE, SPLICE, RIBBON, FACETS, SMOKE, TORQUE, TUNNEL, MOTEL and more; A16 INIT is a blank kit."],
      ["sequencer", "GRID's page keys say which steps they show, and a held page key ends the pattern there."],
      ["fixes", "Overflows found by the sanitizers, and the destroy page's bit preview."],
      ["web", "Bank A on the web; the side holds only the instrument's controls."],
    ],
  },
  {
    date: "2026-10-16", title: "MONO2K 0.1",
    items: [
      ["machines", "Six mono machines for the M-VAVE FM-1: the firmware's core, and the emulator that runs it in the browser."],
      ["sequencer", "16 patterns of up to 64 steps, with parameter locks, conditions and ratchets."],
      ["workflow", "Projects the modern Elektron way: four banks of 16 patterns, each with its own sounds; save, reload, copy, paste, clear, export and import."],
      ["patterns", "15 factory sound designs in bank A."],
      ["screen", "A light and a dark display (GLO > SET > SCRN), a mark for every machine, and screens that glitch into each other."],
      ["web", "A pattern picker, the readout's controls, and links to Sequencist."],
    ],
  },
];
