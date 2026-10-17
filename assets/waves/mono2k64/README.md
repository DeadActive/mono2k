# MONO2K's 64-wave bank

64 single-cycle waveforms, `OSC-1.wav` to `OSC-64.wav`: the project owner's own, made for MONO2K's DigiPro-style
engines. Licensed with the project, GPL-3.0-only (see `LICENSE` at the root), unless the owner says otherwise.

The files here are the originals, untouched (16-bit mono PCM, 44.1 kHz, 223 to 513 samples). The device plays them
as 64 cycles of 256 samples, made from these by `tools/gen_waves.py` (into `firmware/gen/m2k_waves64.h`, never edited
by hand): `PROCESSING.md` has every step and its numbers per wave, `AUDIT.md` what the originals are.

They are not the Monomachine's: its factory DigiPro waves are never copied into MONO2K.
