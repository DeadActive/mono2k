# MONO2K on the FM-1 Firmware Hub

What the hub asks for (<https://thefirmwarehub.com/submit/>, <https://thefirmwarehub.com/kb/>), ready to send.

## The card

- **Name:** MONO2K
- **By:** Sequencist
- **Kind:** Grooveboxes (`groove` in `fmhub.json`)
- **Text** (`fmhub.json`, 262 characters): Six mono machines in the spirit of Elektron's Monomachine: FM,
  superwaves, wavetables, a SID, a voice and four drum synths, one per track. A 64-step sequencer with parameter
  locks, trig conditions and ratchets, a lofi bus, four demo parts and a web patch editor.
- **Picture:** `docs/hub/screen.png` (the screen, 240 × 240, in dark mode, from the engine while A01 plays),
  `docs/hub/screen-4x.png` (the same, 960 × 960), `docs/hub/screens-wide.png` (HOME and the chord machine's page).
- **Animation:** `docs/hub/mono2k.gif` (240 × 240) and `docs/hub/mono2k-2x.gif` (480 × 480): four bars of A01 on
  HOME, then the chord machine's page, 12.5 s, looping; every frame the engine's own.
- **Play in the browser:** <https://inigohernandez.github.io/mono2k/>
- **Editor:** <https://inigohernandez.github.io/mono2k/editor.html>
- **Manual:** <https://inigohernandez.github.io/mono2k/manual.html>

## The package

- **Repository:** <https://github.com/InigoHernandez/mono2k>, licence GPL-3.0-only, the whole source public.
- **Release:** a GitHub release `v0.1` with `mono2k-0.1.fwsc` attached (a version under 1.0: the card says beta).
- **Identity:** FM-1_980, proposed, to be confirmed. No other firmware on the hub uses it (the hub's table,
  2026-10-24), and it is of the form FM-1_9NN, so the hub resumes an interrupted update with Felucca's loader, which
  the package carries. `tools/fm1/build.py --release 0.Y` gives FM-1_98Y today, and FM-1_981 (Pocket GammaWave) and
  FM-1_983 (DRIFT) are taken: the release should keep FM-1_980 instead.
- **Update loader:** Felucca's, from the CS-1 platform the FM-1 build is made on.
- **USB:** a MIDI device; its port name as the FM-1 shows it (to be read when it is tested).
- **What it keeps:** its own store in the last 128 KiB of the data area; it does not read other firmwares' projects
  or samples (the README's "On the FM-1").

## Before sending: the hub's tests, on a real FM-1, with the release's own file

1. Install `mono2k-0.1.fwsc` from the official V15: MONO2K starts, and after Connect the hub names it.
2. Install it from another custom firmware (Felucca, for one): the same.
3. Leave it: with MONO2K running, install another firmware from the hub. The update must start.
4. USB: the FM-1 shows as a MIDI device and answers the identity request with FM-1_980; note the port name.
5. An interrupted update: unplug during Writing once, plug in, press Install again: it must continue.
6. Play it: the four demos, saving, the Patch Editor's backup and restore.

## The mail

To zven@thefirmwarehub.com, links only, no attachments:

> **Subject:** Firmware for the hub: MONO2K
>
> Hello,
>
> I'd like to list my firmware on the hub.
>
> - Firmware: MONO2K
> - By: Sequencist
> - Repository and release: https://github.com/InigoHernandez/mono2k (release v0.1, mono2k-0.1.fwsc)
> - Identity: FM-1_980 (own; Felucca's update loader)
> - Licence: GPL-3.0-only, the complete source in the repository
> - USB port name: (as tested)
>
> MONO2K is six mono machines in the spirit of Elektron's Monomachine: each track one machine (FM, superwaves,
> wavetables, a SID, a voice, four drum synths, effects), a 64-step sequencer with parameter locks, trig conditions
> and ratchets, and a lofi bus. It comes with four demo parts, a sound library and a web patch editor that backs up
> and restores the FM-1 over USB.
>
> - Picture: https://raw.githubusercontent.com/InigoHernandez/mono2k/main/docs/hub/screen-4x.png
> - Animation (the screen while it plays): https://raw.githubusercontent.com/InigoHernandez/mono2k/main/docs/hub/mono2k-2x.gif
> - Play in the browser: https://inigohernandez.github.io/mono2k/
> - Editor: https://inigohernandez.github.io/mono2k/editor.html
> - Manual: https://inigohernandez.github.io/mono2k/manual.html
>
> Tested on a real FM-1 with the release's file: installed from V15 and from another firmware, left again to another
> firmware, USB MIDI with its identity, an interrupted update resumed. It keeps its own store and does not read other
> firmwares' projects or samples; the README says so ("On the FM-1"). The card's text is in fmhub.json.
>
> Thank you,
> Sequencist
