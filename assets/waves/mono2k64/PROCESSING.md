# The 64-wave bank: processing

From each original (`OSC-N.wav`, kept as it is) to the device's cycle of 256 16-bit samples, by
`tools/gen_waves.py` (its header lists the steps). Per wave: the file's length; its whole cycles (one,
or m: the spectrum read at multiples of m, a correction); the harmonics kept (up to 127); the energy of
those dropped above them; the band limit's overshoot of the original peak; the level the bank's 32000
peak gives against the file's (dB).

| Wave | Length | Cycles | Harmonics kept | Dropped energy | Overshoot | Level | Note |
|---|---|---|---|---|---|---|---|
| OS01 (OSC-1) | 254 | 1 | 126 | -79.9 dB | -0.00 dB | -0.20 dB |  |
| OS02 (OSC-2) | 255 | 1 | 127 | none | +0.00 dB | -0.21 dB |  |
| OS03 (OSC-3) | 246 | 1 | 122 | -60.1 dB | -0.04 dB | -0.17 dB |  |
| OS04 (OSC-4) | 253 | 1 | 126 | none | +0.01 dB | -0.21 dB |  |
| OS05 (OSC-5) | 253 | 1 | 126 | none | -0.05 dB | -0.16 dB |  |
| OS06 (OSC-6) | 254 | 1 | 126 | -55.5 dB | -0.02 dB | -0.19 dB |  |
| OS07 (OSC-7) | 253 | 1 | 126 | none | +0.06 dB | -0.26 dB |  |
| OS08 (OSC-8) | 254 | 1 | 126 | -55.6 dB | -0.01 dB | -0.20 dB |  |
| OS09 (OSC-9) | 254 | 1 | 126 | -60.5 dB | -0.02 dB | -0.19 dB |  |
| OS10 (OSC-10) | 254 | 1 | 126 | -68.0 dB | -0.00 dB | -0.20 dB |  |
| OS11 (OSC-11) | 254 | 1 | 126 | -64.4 dB | -0.01 dB | -0.20 dB |  |
| OS12 (OSC-12) | 254 | 1 | 126 | -77.9 dB | +0.00 dB | -0.21 dB |  |
| OS13 (OSC-13) | 258 | 1 | 127 | -76.3 dB | +0.00 dB | -0.21 dB |  |
| OS14 (OSC-14) | 504 | 1 | 127 | -61.8 dB | -0.00 dB | -0.20 dB |  |
| OS15 (OSC-15) | 337 | 1 | 127 | -68.9 dB | +0.01 dB | -0.21 dB |  |
| OS16 (OSC-16) | 505 | 1 | 127 | -47.0 dB | +0.02 dB | -0.23 dB |  |
| OS17 (OSC-17) | 270 | 1 | 127 | -63.2 dB | -0.03 dB | -0.18 dB |  |
| OS18 (OSC-18) | 226 | 1 | 112 | -72.6 dB | +0.01 dB | -0.21 dB |  |
| OS19 (OSC-19) | 335 | 1 | 127 | -39.2 dB | -0.16 dB | -0.04 dB |  |
| OS20 (OSC-20) | 253 | 5 | 25 | -36.7 dB | -0.03 dB | -0.18 dB | **corrected**: 5 whole cycles in the file, one taken |
| OS21 (OSC-21) | 280 | 1 | 127 | -75.0 dB | -0.00 dB | -0.20 dB |  |
| OS22 (OSC-22) | 384 | 1 | 127 | -17.7 dB | -1.53 dB | +1.32 dB |  |
| OS23 (OSC-23) | 254 | 1 | 126 | -96.2 dB | +0.02 dB | -0.23 dB |  |
| OS24 (OSC-24) | 256 | 1 | 127 | -50.8 dB | +0.01 dB | -0.21 dB |  |
| OS25 (OSC-25) | 273 | 1 | 127 | -42.3 dB | -0.12 dB | -0.08 dB |  |
| OS26 (OSC-26) | 254 | 1 | 126 | -83.8 dB | -0.00 dB | -0.20 dB |  |
| OS27 (OSC-27) | 228 | 1 | 113 | -75.7 dB | -0.00 dB | -0.20 dB |  |
| OS28 (OSC-28) | 261 | 1 | 127 | -45.2 dB | -0.34 dB | +0.14 dB |  |
| OS29 (OSC-29) | 260 | 1 | 127 | -86.4 dB | -0.00 dB | -0.21 dB |  |
| OS30 (OSC-30) | 261 | 1 | 127 | -87.6 dB | -0.00 dB | -0.21 dB |  |
| OS31 (OSC-31) | 264 | 1 | 127 | -80.0 dB | +0.00 dB | -0.21 dB |  |
| OS32 (OSC-32) | 230 | 1 | 114 | -74.5 dB | +0.00 dB | -0.21 dB |  |
| OS33 (OSC-33) | 231 | 1 | 115 | none | -0.01 dB | -0.20 dB |  |
| OS34 (OSC-34) | 306 | 1 | 127 | -43.9 dB | -0.02 dB | -0.19 dB |  |
| OS35 (OSC-35) | 265 | 1 | 127 | -53.1 dB | +0.04 dB | -0.25 dB |  |
| OS36 (OSC-36) | 257 | 1 | 127 | -53.4 dB | -0.02 dB | -0.19 dB |  |
| OS37 (OSC-37) | 302 | 1 | 127 | -39.6 dB | +0.02 dB | -0.23 dB |  |
| OS38 (OSC-38) | 252 | 1 | 125 | -78.5 dB | -0.00 dB | -0.20 dB |  |
| OS39 (OSC-39) | 513 | 1 | 127 | -66.6 dB | -0.04 dB | -0.17 dB |  |
| OS40 (OSC-40) | 457 | 1 | 127 | -34.6 dB | -0.03 dB | -0.17 dB |  |
| OS41 (OSC-41) | 228 | 1 | 113 | -65.0 dB | -0.05 dB | -0.15 dB |  |
| OS42 (OSC-42) | 256 | 1 | 127 | -71.5 dB | -0.00 dB | -0.21 dB |  |
| OS43 (OSC-43) | 256 | 1 | 127 | -57.9 dB | -0.00 dB | -0.20 dB |  |
| OS44 (OSC-44) | 228 | 1 | 113 | -48.2 dB | -0.02 dB | -0.18 dB |  |
| OS45 (OSC-45) | 259 | 1 | 127 | -55.3 dB | +0.05 dB | -0.26 dB |  |
| OS46 (OSC-46) | 343 | 1 | 127 | -59.4 dB | +0.00 dB | -0.21 dB |  |
| OS47 (OSC-47) | 401 | 1 | 127 | -71.2 dB | -0.00 dB | -0.20 dB |  |
| OS48 (OSC-48) | 287 | 1 | 127 | -62.1 dB | -0.01 dB | -0.19 dB |  |
| OS49 (OSC-49) | 225 | 1 | 112 | none | +0.00 dB | -0.21 dB |  |
| OS50 (OSC-50) | 350 | 1 | 127 | -48.1 dB | -0.01 dB | -0.20 dB |  |
| OS51 (OSC-51) | 456 | 1 | 127 | -51.7 dB | +0.05 dB | -0.26 dB |  |
| OS52 (OSC-52) | 362 | 1 | 127 | -45.5 dB | +0.03 dB | -0.24 dB |  |
| OS53 (OSC-53) | 286 | 1 | 127 | -64.2 dB | -0.01 dB | -0.20 dB |  |
| OS54 (OSC-54) | 271 | 1 | 127 | -52.7 dB | -0.02 dB | -0.19 dB |  |
| OS55 (OSC-55) | 286 | 1 | 127 | -50.9 dB | -0.02 dB | -0.18 dB |  |
| OS56 (OSC-56) | 275 | 1 | 127 | -67.3 dB | +0.01 dB | -0.21 dB |  |
| OS57 (OSC-57) | 301 | 1 | 127 | -60.0 dB | -0.02 dB | -0.18 dB |  |
| OS58 (OSC-58) | 257 | 1 | 127 | -77.5 dB | +0.02 dB | -0.22 dB |  |
| OS59 (OSC-59) | 267 | 1 | 127 | -56.7 dB | +0.01 dB | -0.22 dB |  |
| OS60 (OSC-60) | 511 | 1 | 127 | -45.3 dB | -0.04 dB | -0.17 dB |  |
| OS61 (OSC-61) | 304 | 1 | 127 | -52.7 dB | +0.00 dB | -0.21 dB |  |
| OS62 (OSC-62) | 223 | 1 | 111 | none | -0.00 dB | -0.20 dB |  |
| OS63 (OSC-63) | 251 | 1 | 125 | none | -0.03 dB | -0.18 dB |  |
| OS64 (OSC-64) | 227 | 1 | 113 | none | +0.02 dB | -0.22 dB |  |
