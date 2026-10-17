# The 64-wave bank: the originals audited

`tools/wave_audit.py` on the files as they are. DC is the mean against the peak; the loop jump is |last - first|
and, in brackets, that jump in the wave's median sample-to-sample steps (a large one is a reset, as a saw's: kept);
cycles from the spectrum (the largest m whose harmonics, bins m, 2m, ..., hold 99.5 % of the energy over DC), and a
repeat that is not a whole number of cycles from the wave's correlation with itself shifted (over 0.97, over at least
a quarter of the file).

64 files, numbered 1..64; gaps: none

| File | Format | Length | DC (of peak) | Peak | RMS | Loop jump (steps) | Cycles | One cycle |
|---|---|---|---|---|---|---|---|---|
| OSC-1.wav | 16-bit mono 44100 Hz | 254 | -0.1 % | -0.1 dBFS | -3.4 dBFS | 0.052 (3) | 1 | yes |
| OSC-2.wav | 16-bit mono 44100 Hz | 255 | +0.0 % | -0.1 dBFS | -4.3 dBFS | 0.074 (8) | 1 | yes |
| OSC-3.wav | 16-bit mono 44100 Hz | 246 | +0.9 % | -0.1 dBFS | -9.5 dBFS | 0.115 (1) | 1 | yes |
| OSC-4.wav | 16-bit mono 44100 Hz | 253 | -0.2 % | -0.1 dBFS | -7.3 dBFS | 0.471 (251) | 1 | yes |
| OSC-5.wav | 16-bit mono 44100 Hz | 253 | -0.4 % | -0.1 dBFS | -10.2 dBFS | 0.370 (209) | 1 | yes |
| OSC-6.wav | 16-bit mono 44100 Hz | 254 | +0.0 % | -0.1 dBFS | -10.9 dBFS | 0.206 (53) | 1 | yes |
| OSC-7.wav | 16-bit mono 44100 Hz | 253 | +2.3 % | -0.1 dBFS | -3.5 dBFS | 0.744 (2) | 1 | yes |
| OSC-8.wav | 16-bit mono 44100 Hz | 254 | +0.3 % | -0.1 dBFS | -6.7 dBFS | 0.314 (49) | 1 | yes |
| OSC-9.wav | 16-bit mono 44100 Hz | 254 | +0.3 % | -0.1 dBFS | -2.6 dBFS | 0.286 (109) | 1 | yes |
| OSC-10.wav | 16-bit mono 44100 Hz | 254 | +0.1 % | -0.1 dBFS | -9.8 dBFS | 0.082 (150) | 1 | yes |
| OSC-11.wav | 16-bit mono 44100 Hz | 254 | +1.1 % | -0.1 dBFS | -3.8 dBFS | 0.273 (118) | 1 | yes |
| OSC-12.wav | 16-bit mono 44100 Hz | 254 | -0.1 % | -0.1 dBFS | -17.4 dBFS | 0.074 (268) | 1 | yes |
| OSC-13.wav | 16-bit mono 44100 Hz | 258 | -16.6 % | -0.1 dBFS | -6.7 dBFS | 0.033 (1) | 1 | yes |
| OSC-14.wav | 16-bit mono 44100 Hz | 504 | +0.1 % | -0.1 dBFS | -5.4 dBFS | 0.010 (2) | 1 | yes |
| OSC-15.wav | 16-bit mono 44100 Hz | 337 | -0.1 % | -0.1 dBFS | -5.7 dBFS | 0.002 (0) | 1 | yes |
| OSC-16.wav | 16-bit mono 44100 Hz | 505 | -0.2 % | -0.1 dBFS | -7.7 dBFS | 0.249 (92) | 1 | yes |
| OSC-17.wav | 16-bit mono 44100 Hz | 270 | -0.6 % | -0.1 dBFS | -5.5 dBFS | 0.646 (104) | 1 | yes |
| OSC-18.wav | 16-bit mono 44100 Hz | 226 | +1.2 % | -0.1 dBFS | -8.1 dBFS | 0.132 (6) | 1 | yes |
| OSC-19.wav | 16-bit mono 44100 Hz | 335 | -0.9 % | -0.1 dBFS | -8.5 dBFS | 0.666 (255) | 1 | yes |
| OSC-20.wav | 16-bit mono 44100 Hz | 253 | +0.1 % | -0.1 dBFS | -3.9 dBFS | 0.041 (1) | 5 (repeats at 101, r 1.00) | NO |
| OSC-21.wav | 16-bit mono 44100 Hz | 280 | -2.1 % | -0.1 dBFS | -5.4 dBFS | 0.007 (0) | 1 (repeats at 128, r 1.00) | NO |
| OSC-22.wav | 16-bit mono 44100 Hz | 384 | +1.6 % | -0.1 dBFS | -14.7 dBFS | 0.008 (1) | 1 | yes |
| OSC-23.wav | 16-bit mono 44100 Hz | 254 | +0.1 % | -0.1 dBFS | -11.6 dBFS | 0.016 (1) | 1 | yes |
| OSC-24.wav | 16-bit mono 44100 Hz | 256 | -0.8 % | -0.1 dBFS | -10.1 dBFS | 0.017 (0) | 1 (repeats at 21, r 0.99) | NO |
| OSC-25.wav | 16-bit mono 44100 Hz | 273 | +0.8 % | -0.1 dBFS | -10.0 dBFS | 0.128 (1) | 1 | yes |
| OSC-26.wav | 16-bit mono 44100 Hz | 254 | -0.5 % | -0.1 dBFS | -8.0 dBFS | 0.146 (3) | 1 (repeats at 95, r 0.98) | NO |
| OSC-27.wav | 16-bit mono 44100 Hz | 228 | -0.0 % | -0.1 dBFS | -7.9 dBFS | 0.181 (4) | 1 | yes |
| OSC-28.wav | 16-bit mono 44100 Hz | 261 | +0.4 % | -0.1 dBFS | -8.8 dBFS | 0.868 (4) | 1 | yes |
| OSC-29.wav | 16-bit mono 44100 Hz | 260 | -4.5 % | -0.1 dBFS | -5.8 dBFS | 0.048 (2) | 1 | yes |
| OSC-30.wav | 16-bit mono 44100 Hz | 261 | +0.5 % | -0.1 dBFS | -5.0 dBFS | 0.026 (2) | 1 | yes |
| OSC-31.wav | 16-bit mono 44100 Hz | 264 | -0.9 % | -0.1 dBFS | -3.1 dBFS | 0.014 (1) | 1 | yes |
| OSC-32.wav | 16-bit mono 44100 Hz | 230 | -0.0 % | -0.1 dBFS | -7.2 dBFS | 0.105 (2) | 1 | yes |
| OSC-33.wav | 16-bit mono 44100 Hz | 231 | -1.0 % | -0.1 dBFS | -9.3 dBFS | 0.244 (1) | 1 | yes |
| OSC-34.wav | 16-bit mono 44100 Hz | 306 | +11.5 % | -0.1 dBFS | -8.0 dBFS | 0.516 (2) | 1 | yes |
| OSC-35.wav | 16-bit mono 44100 Hz | 265 | +11.5 % | -0.1 dBFS | -11.6 dBFS | 0.622 (4) | 1 | yes |
| OSC-36.wav | 16-bit mono 44100 Hz | 257 | -28.6 % | -0.1 dBFS | -8.0 dBFS | 0.280 (2) | 1 | yes |
| OSC-37.wav | 16-bit mono 44100 Hz | 302 | -8.4 % | -0.1 dBFS | -7.1 dBFS | 0.107 (0) | 1 | yes |
| OSC-38.wav | 16-bit mono 44100 Hz | 252 | +5.6 % | -0.1 dBFS | -9.3 dBFS | 0.034 (1) | 1 | yes |
| OSC-39.wav | 16-bit mono 44100 Hz | 513 | +0.2 % | -0.1 dBFS | -9.1 dBFS | 0.033 (1) | 1 | yes |
| OSC-40.wav | 16-bit mono 44100 Hz | 457 | -0.0 % | -0.1 dBFS | -10.2 dBFS | 0.307 (2) | 1 | yes |
| OSC-41.wav | 16-bit mono 44100 Hz | 228 | +0.3 % | -0.1 dBFS | -6.0 dBFS | 0.180 (1) | 1 | yes |
| OSC-42.wav | 16-bit mono 44100 Hz | 256 | -0.1 % | -0.1 dBFS | -8.2 dBFS | 0.559 (4) | 1 | yes |
| OSC-43.wav | 16-bit mono 44100 Hz | 256 | -0.1 % | -0.1 dBFS | -10.4 dBFS | 0.583 (4) | 1 | yes |
| OSC-44.wav | 16-bit mono 44100 Hz | 228 | -1.2 % | -0.1 dBFS | -10.1 dBFS | 0.277 (2) | 1 | yes |
| OSC-45.wav | 16-bit mono 44100 Hz | 259 | -0.3 % | -0.1 dBFS | -10.4 dBFS | 0.018 (0) | 1 | yes |
| OSC-46.wav | 16-bit mono 44100 Hz | 343 | -0.2 % | -0.1 dBFS | -6.6 dBFS | 0.033 (1) | 1 | yes |
| OSC-47.wav | 16-bit mono 44100 Hz | 401 | +2.9 % | -0.1 dBFS | -6.6 dBFS | 0.053 (3) | 1 | yes |
| OSC-48.wav | 16-bit mono 44100 Hz | 287 | -17.4 % | -0.1 dBFS | -5.8 dBFS | 0.036 (1) | 1 | yes |
| OSC-49.wav | 16-bit mono 44100 Hz | 225 | -1.7 % | -0.1 dBFS | -7.1 dBFS | 0.068 (2) | 1 | yes |
| OSC-50.wav | 16-bit mono 44100 Hz | 350 | -0.2 % | -0.1 dBFS | -7.3 dBFS | 0.535 (22) | 1 | yes |
| OSC-51.wav | 16-bit mono 44100 Hz | 456 | -0.0 % | -0.1 dBFS | -6.4 dBFS | 0.234 (128) | 1 | yes |
| OSC-52.wav | 16-bit mono 44100 Hz | 362 | +0.8 % | -0.1 dBFS | -10.9 dBFS | 0.387 (8) | 1 | yes |
| OSC-53.wav | 16-bit mono 44100 Hz | 286 | -0.4 % | -0.1 dBFS | -8.0 dBFS | 0.018 (18) | 1 | yes |
| OSC-54.wav | 16-bit mono 44100 Hz | 271 | -0.3 % | -0.1 dBFS | -8.3 dBFS | 0.081 (3) | 1 | yes |
| OSC-55.wav | 16-bit mono 44100 Hz | 286 | -0.2 % | -0.1 dBFS | -7.9 dBFS | 0.062 (2) | 1 | yes |
| OSC-56.wav | 16-bit mono 44100 Hz | 275 | -1.9 % | -0.1 dBFS | -6.4 dBFS | 0.006 (0) | 1 | yes |
| OSC-57.wav | 16-bit mono 44100 Hz | 301 | -0.0 % | -0.1 dBFS | -7.7 dBFS | 0.099 (3) | 1 | yes |
| OSC-58.wav | 16-bit mono 44100 Hz | 257 | +7.1 % | -0.1 dBFS | -7.1 dBFS | 0.049 (1) | 1 | yes |
| OSC-59.wav | 16-bit mono 44100 Hz | 267 | +0.9 % | -0.1 dBFS | -18.7 dBFS | 0.110 (157) | 1 | yes |
| OSC-60.wav | 16-bit mono 44100 Hz | 511 | -0.3 % | -0.1 dBFS | -8.3 dBFS | 0.246 (244) | 1 | yes |
| OSC-61.wav | 16-bit mono 44100 Hz | 304 | +1.4 % | -0.1 dBFS | -14.1 dBFS | 0.111 (331) | 1 | yes |
| OSC-62.wav | 16-bit mono 44100 Hz | 223 | +15.7 % | -0.1 dBFS | -6.9 dBFS | 0.007 (0) | 1 | yes |
| OSC-63.wav | 16-bit mono 44100 Hz | 251 | -7.1 % | -0.1 dBFS | -6.3 dBFS | 0.192 (2) | 1 | yes |
| OSC-64.wav | 16-bit mono 44100 Hz | 227 | +17.8 % | -0.1 dBFS | -6.7 dBFS | 0.143 (2) | 1 | yes |

## Findings

- 64 files, OSC-1 to OSC-64, no gaps; all 16-bit mono 44.1 kHz; each peaks at -0.1 dBFS.
- 60 are one cycle. OSC-20 holds five whole cycles (its harmonics all on multiples of 5): played as one it would sound
  two octaves and a major third high, so the device's copy takes one cycle of the five (`PROCESSING.md`), the file
  kept as it is. OSC-21 (repeats at 128 of 280), OSC-24 (at 21 of 256) and OSC-26 (at 95 of 254) loop as whole
  files: a weak sub under alternating humps, a buzz, a damped ringing inside one period; kept as they are.
- DC offsets reach -28.6 % (OSC-36) and +17.8 % (OSC-64): the waves' own asymmetry, kept in the files and the table.
  DIGI-WAV plays them without it (`PROCESSING.md`): a DC stepping in at every trig measured 13-17 dB more under 60 Hz
  at the onset on OSC-13, OSC-36 and OSC-48 (`tools/wave_check.py`, thump), not the track's HP's to take.
- Loop jumps up to 0.87 of full scale (OSC-28): resets, kept; the band limit draws them.
