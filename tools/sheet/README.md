# Sound sheets and bank A's scores

How bank A was made (`docs/bank-a.md`), and how to make it again. Build from the repo's root:

    cc -O2 -Itools/sheet -I. -Ifirmware/src -Ifirmware/gen -o build/sheet_gen tools/sheet/gen.c -lm
    cc -O2 -Itools/sheet -I. -Ifirmware/src -Ifirmware/gen -o build/sheet_mel tools/sheet/mel.c -lm

- `gen.c`: sheet 1's search. A machine at random settings over their whole ranges with motion, two bars at 103 BPM;
  deterministic by candidate id, so a picked sound is rebuilt from its number (`gen OUTDIR FIRST COUNT [EDIT..]`).
- `feat.py`, `pick_sheet.py`: sheet 1's scorer (trained on the rejected banks against the Model:Cycles demos; it only
  told them apart below 250 Hz, so it filtered, it did not choose) and its pick for variety. `pick.txt` maps the
  sheet's numbers to candidates; `verdict1.tsv` is the owner's verdict; `refine.py` and `refine1.tsv` the eight
  refinements (before, A, B) the owner asked for.
- `mel.c`: a small score language over the engine (`track`/`orig`/`mtrack`/`ptrack` a sound, `trig`, `ens`, `lock`,
  `cond`, `glob`, `sync`, `solo`, `out`), and `cdump`, which writes a finished score as `factory()`'s code.
- `mus.h`: sheet 3's draw, instruments in musical ranges (harmonic FM ratios, the filter around the note, little crush,
  slow motion; pad, stab, keys, lead envelopes). `mel.py`, `mus.py`, `harm.py`: sheets 2, 3 and 4.
- `compose.py`: the parts, written to `docs/composing.md` (drafts the owner rejected are kept in it, named, for the
  record). `loud.py`: BS.1770 loudness. `fit.py`: each track alone, its share on the bar's chord notes.
- `scores/`: the four parts as rendered and approved. To write them into the factory again:

      for p in circuit:0:CIRCUIT:126 tide:1:TIDE:122 suspend:2:SUSPEND:110 lantern:3:LANTERN:100; do
          set -- $(echo $p | tr : ' ')
          (echo "len 64"; grep -v "^out " tools/sheet/scores/$1.txt; echo "cdump $2 $3 $4 build/$1.c") > build/$1.spec
          build/sheet_mel build/$1.spec
      done

  then paste each `build/NAME.c` under its comment in `factory()`.
