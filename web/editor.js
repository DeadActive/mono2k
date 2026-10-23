// SPDX-License-Identifier: GPL-3.0-only
// The patch editor. The firmware runs in worklet.js as on the emulator page, from the same saves (and saving to
// them); a second copy of the core here, with no sound, names and formats each control as the screen does. Each
// slider sets one parameter of the open pattern's track; a chosen MIDI output gets the Monomachine's CC for it.
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);
  const cstr = (x, p) => { const m = new Uint8Array(x.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
  const css = () => getComputedStyle(document.documentElement);
  const hex = (c) => { const m = c.replace("#", ""); return [parseInt(m.slice(0, 2), 16), parseInt(m.slice(2, 4), 16), parseInt(m.slice(4, 6), 16)]; };
  const status = (s) => { $("ed-status").textContent = s; };

  /* the saves, shared with the emulator page */
  const STORE_KEY = "mono2k.store.v2";
  const loadStore = () => {
    try {
      const s = localStorage.getItem(STORE_KEY);
      if (!s) return null;
      const b = atob(s), u = new Uint8Array(b.length);
      for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i);
      return u.buffer;
    } catch (e) { return null; }
  };
  const saveStore = (bytes) => {
    try {
      let s = "";
      for (let i = 0; i < bytes.length; i += 4096) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 4096));
      localStorage.setItem(STORE_KEY, btoa(s));
    } catch (e) { status("Could not save in this browser (a private window?)."); }
  };

  let pre = null, wasmBytes = null, node = null, ctx = null;
  let kit = null, n = 0, machines = [0, 0, 0, 0, 0, 0], trk = 0, curPat = -1, playing = false, oct = 0;
  let midiOut = null, midiAccess = null;
  const send = (m) => { if (node) node.port.postMessage(m); };

  // ------------------------------------------------------------------------------------- the pages' knobs
  const PG = { HOME: 0, SYN: 1, AMP: 2, FILT: 3, FX: 4, LFO: 5, ARP: 6 };
  function blocks() {
    const x = pre;
    const knobs = (page, subs, only) => subs.flatMap((sub) => [0, 1, 2, 3].map((k) => x.w_kcode(page, sub, k))).filter((c) => c >= 0 && c < 255 && (!only || only.includes(c)));
    const LEVL = x.w_kcode(PG.HOME, 0, 0), PTCH = x.w_kcode(PG.HOME, 0, 1);
    return [
      ["SYN", "EDIT", knobs(PG.SYN, [0, 1])],
      ["TRACK", "HOME", [LEVL, PTCH]],
      ["AMP", "ENV", knobs(PG.AMP, [0, 1])],
      ["FILTER", "SEL", knobs(PG.FILT, [0, 1])],
      ...[0, 1, 2, 3].map((sub) => [cstr(x, x.w_page_label(PG.FX, sub)), "FX", knobs(PG.FX, [sub])]),
      ...[0, 1, 2].map((l) => [`LFO ${l + 1}`, "LFO", knobs(PG.LFO, [2 * l, 2 * l + 1])]),
      ["ARP", "ARP", knobs(PG.ARP, [0])],
    ];
  }
  /* a control's short and long name, as the screen has them for this track's machine */
  const names = (code) => {
    const x = pre;
    for (const [page, subs] of [[PG.SYN, [0, 1]], [PG.HOME, [0]], [PG.AMP, [0, 1]], [PG.FILT, [0, 1]], [PG.FX, [0, 1, 2, 3]], [PG.LFO, [0, 1, 2, 3, 4, 5]], [PG.ARP, [0]]])
      for (const sub of subs) for (let k = 0; k < 4; k++)
        if (x.w_kcode(page, sub, k) === code) return [cstr(x, x.w_knob(page, sub, machines[trk], k)), cstr(x, x.w_kname(page, sub, machines[trk], k))];
    return ["", ""];
  };
  const val = (code) => kit[trk * n + code];
  const label = (code, v) => cstr(pre, pre.w_fmt(trk, code, v));

  function render() {
    if (!kit) return;
    for (let t = 0; t < 6; t++) pre.w_mirror_machine(t, machines[t]);
    $("ed-pat").textContent = "ABCD"[curPat >> 4] + String((curPat & 15) + 1).padStart(2, "0");
    /* the tracks, as the emulator lists them */
    $("ed-tracks").innerHTML = machines.map((m, t) => `<li class="${t === trk ? "on" : ""}"><button class="tsel" type="button" data-t="${t}" aria-pressed="${t === trk}"><b>T${t + 1}</b>${esc(cstr(pre, pre.w_mi(m, 0)))}</button></li>`).join("");
    $("ed-machine").value = String(machines[trk]);
    $("ed-mach-text").textContent = `${cstr(pre, pre.w_mi(machines[trk], 3))} · ${cstr(pre, pre.w_mi(machines[trk], 2))}`;
    drawMark();
    /* the pages: a card each, a slider for each knob */
    $("ed-pages").innerHTML = blocks().map(([title, btn, codes]) => {
      const rows = codes.map((code) => {
        const [s4, ln] = names(code);
        if (!ln || s4 === "-") return "";
        const ch = pre.w_kchoices(trk, code), v = val(code);
        const [mn, mx, sv] = ch ? [0, ch - 1, pre.w_choice(trk, code, v)] : [pre.w_kmin(trk, code), pre.w_kmax(trk, code), v];
        return `<div class="ectl"><label for="c${code}"><b>${esc(s4)}</b><span>${esc(ln)}</span></label><output id="o${code}">${esc(label(code, v))}</output>` +
               `<input type="range" id="c${code}" data-code="${code}" data-ch="${ch}" min="${mn}" max="${mx}" step="1" value="${sv}"></div>`;
      }).join("");
      return rows ? `<section class="ecard"><h3>${esc(title)}<span>${esc(btn)}</span></h3>${rows}</section>` : "";
    }).join("");
  }
  function drawMark() {
    const cv = $("ed-mark"), g = cv.getContext("2d"), img = g.createImageData(72, 72);
    const I = hex(css().getPropertyValue("--ink").trim() || "#0b0b0b"), P = hex(css().getPropertyValue("--paper").trim() || "#f2f1ec");
    const bits = new Uint8Array(pre.memory.buffer, pre.w_mark(machines[trk]), 512);
    for (let p = 0; p < 72 * 72; p++) { const o = p * 4; img.data[o] = P[0]; img.data[o + 1] = P[1]; img.data[o + 2] = P[2]; img.data[o + 3] = 255; }
    for (let i = 0; i < 512; i++) for (let j = 0; j < 8; j++) {
      if (!((bits[i] >> (7 - j)) & 1)) continue;
      const o = (((i >> 3) + 4) * 72 + (i & 7) * 8 + j + 4) * 4;
      img.data[o] = I[0]; img.data[o + 1] = I[1]; img.data[o + 2] = I[2];
    }
    g.putImageData(img, 0, 0);
  }

  /* a slider moved: the value to the core (and to MIDI), its label as the screen would show it */
  $("ed-pages").addEventListener("input", (ev) => {
    const el = ev.target;
    if (!el.dataset || el.dataset.code === undefined) return;
    const code = +el.dataset.code, ch = +el.dataset.ch;
    const v = ch ? pre.w_enc(+el.value, ch) : +el.value;
    kit[trk * n + code] = v;
    send({ t: "param", trk, code, v });
    $("o" + code).textContent = label(code, v);
    if (midiOut) {
      const cc = pre.w_cc_of(code);
      if (cc >= 0) midiOut.send([0xb0 | trk, cc, Math.min(127, v)]);
    }
  });
  $("ed-tracks").addEventListener("click", (ev) => {
    const b = ev.target.closest("[data-t]");
    if (!b) return;
    trk = +b.dataset.t;
    render();
  });
  $("ed-machine").addEventListener("change", (ev) => {
    const m = +ev.target.value;
    send({ t: "machine", trk, m });
    status(`T${trk + 1}: ${cstr(pre, pre.w_mi(m, 0))}.`);
  });
  $("ed-pattern").addEventListener("change", (ev) => {
    send({ t: "pattern", n: +ev.target.value });
    setTimeout(() => send({ t: "kit" }), 60);
  });
  const tap = (b) => { send({ t: "btn", b, d: 1 }); send({ t: "btn", b, d: 0 }); };
  $("ed-play").addEventListener("click", () => tap(10));
  $("ed-save").addEventListener("click", () => { tap(7); status(`Saved ${$("ed-pat").textContent} in the project.`); });

  // -------------------------------------------------------------------------------------------- audition
  const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const base = () => 48 + 12 * oct;
  function keys() {
    const kb = $("ed-kb");
    kb.innerHTML = "";
    for (let i = 0; i < 24; i++) {
      const note = base() + i, black = [1, 3, 6, 8, 10].includes(i % 12);
      const b = document.createElement("button");
      b.type = "button";
      b.className = black ? "k b" : "k";
      b.dataset.note = String(note);
      b.setAttribute("aria-label", NOTE[note % 12] + (Math.floor(note / 12) - 1));
      if (!black && i % 12 === 0) b.textContent = NOTE[note % 12] + (Math.floor(note / 12) - 1);
      kb.appendChild(b);
    }
    $("ed-octv").textContent = "C" + (Math.floor(base() / 12) - 1);
  }
  const on = new Map();
  const noteOn = (note, id) => { if (on.has(id)) return; on.set(id, note); send({ t: "midi", s: 0x90 | trk, a: note, b: 100 }); if (midiOut) midiOut.send([0x90 | trk, note, 100]); };
  const noteOff = (id) => { const note = on.get(id); if (note === undefined) return; on.delete(id); send({ t: "midi", s: 0x80 | trk, a: note, b: 0 }); if (midiOut) midiOut.send([0x80 | trk, note, 0]); };
  $("ed-kb").addEventListener("pointerdown", (ev) => { const k = ev.target.closest("[data-note]"); if (!k) return; ev.preventDefault(); k.setPointerCapture(ev.pointerId); k.classList.add("down"); noteOn(+k.dataset.note, "p" + ev.pointerId); });
  const up = (ev) => { const k = ev.target.closest("[data-note]"); if (k) k.classList.remove("down"); noteOff("p" + ev.pointerId); };
  $("ed-kb").addEventListener("pointerup", up);
  $("ed-kb").addEventListener("pointercancel", up);
  const KEYS = { KeyZ: 0, KeyS: 1, KeyX: 2, KeyD: 3, KeyC: 4, KeyV: 5, KeyG: 6, KeyB: 7, KeyH: 8, KeyN: 9, KeyJ: 10, KeyM: 11,
                 KeyQ: 12, Digit2: 13, KeyW: 14, Digit3: 15, KeyE: 16, KeyR: 17, Digit5: 18, KeyT: 19, Digit6: 20, KeyY: 21, Digit7: 22, KeyU: 23 };
  window.addEventListener("keydown", (ev) => {
    if (!node || ev.repeat || ev.metaKey || ev.ctrlKey || ev.altKey) return;
    if (ev.target.closest && ev.target.closest("select, input")) return;
    if (KEYS[ev.code] === undefined) return;
    ev.preventDefault();
    noteOn(base() + KEYS[ev.code], ev.code);
  });
  window.addEventListener("keyup", (ev) => { if (KEYS[ev.code] !== undefined) noteOff(ev.code); });
  window.addEventListener("blur", () => { for (const id of Array.from(on.keys())) noteOff(id); });
  $("ed-octd").addEventListener("click", () => { oct = Math.max(-2, oct - 1); keys(); });
  $("ed-octu").addEventListener("click", () => { oct = Math.min(2, oct + 1); keys(); });

  // ---------------------------------------------------------------------------------------------- MIDI out
  /* asked for only when the list is opened: the browser's permission comes with a gesture */
  async function midiList() {
    if (midiAccess || !navigator.requestMIDIAccess) return;
    try {
      midiAccess = await navigator.requestMIDIAccess();
      const sel = $("ed-midi");
      const fill = () => {
        const keep = sel.value;
        sel.innerHTML = '<option value="">None</option>' + Array.from(midiAccess.outputs.values()).map((o) => `<option value="${esc(o.id)}">${esc(o.name)}</option>`).join("");
        sel.value = keep;
      };
      fill();
      midiAccess.onstatechange = fill;
    } catch (e) { status("MIDI is not available in this browser."); }
  }
  $("ed-midi").addEventListener("pointerdown", midiList);
  $("ed-midi").addEventListener("focus", midiList);
  $("ed-midi").addEventListener("change", (ev) => {
    midiOut = midiAccess && ev.target.value ? midiAccess.outputs.get(ev.target.value) : null;
    status(midiOut ? `Changes go to ${midiOut.name} too, on the Monomachine's CC numbers, channel = track.` : "");
  });

  // -------------------------------------------------------------------------------------------- the core
  async function boot() {
    const r = await fetch("m2k.wasm?v=68");
    wasmBytes = await r.arrayBuffer();
    pre = new WebAssembly.Instance(new WebAssembly.Module(wasmBytes), {}).exports;
    if (pre._initialize) pre._initialize();
    pre.w_init(2000);
    n = pre.w_pcount();
    $("ed-machine").innerHTML = Array.from({ length: pre.w_machines() }, (_, m) => `<option value="${m}">${esc(cstr(pre, pre.w_mi(m, 0)))}</option>`).join("");
    keys();
  }
  async function start() {
    $("ed-go").disabled = true;
    try {
      if (!pre) await boot();
      try { ctx = new AudioContext({ sampleRate: 44100, latencyHint: "interactive" }); }
      catch (e) { ctx = new AudioContext({ latencyHint: "interactive" }); }
      await ctx.audioWorklet.addModule("worklet.js?v=33");
      node = new AudioWorkletNode(ctx, "mono2k", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
      node.connect(ctx.destination);
      node.port.onmessage = (ev) => {
        const m = ev.data;
        if (m.t === "frame") {
          if (m.store) saveStore(m.store);
          if (m.names) send({ t: "names" });
          const info = m.info, p = info[3], pl = !!info[4];
          if (p !== curPat || machines.some((v, t) => v !== info[17 + t])) send({ t: "kit" });
          if (pl !== playing) { playing = pl; $("ed-play").setAttribute("aria-pressed", String(pl)); $("ed-play").textContent = pl ? "Stop" : "Play"; }
        } else if (m.t === "kit") {
          kit = m.kits; n = m.n; machines = m.machines; curPat = m.pat;
          $("ed-pattern").value = String(curPat);
          render();
        } else if (m.t === "names") {
          const sel = $("ed-pattern"), keep = curPat;
          sel.innerHTML = m.pats.map((nm, i) => `<option value="${i}">${"ABCD"[i >> 4]}${String((i & 15) + 1).padStart(2, "0")} ${esc(nm)}</option>`).join("");
          if (keep >= 0) sel.value = String(keep);
        } else if (m.t === "store") {
          saveStore(m.bytes);
        } else if (m.t === "ready") {
          send({ t: "set", i: 0, v: window.M2K_SITE && window.M2K_SITE.dark() ? 0 : 127 });
          send({ t: "kit" });
        }
      };
      node.port.postMessage({ t: "wasm", bytes: wasmBytes, seed: (Math.random() * 4294967295) >>> 0, store: loadStore() });
      await ctx.resume();
      $("ed-start").hidden = true;
      $("ed-main").hidden = false;
    } catch (e) {
      status("Could not start: " + e.message);
      $("ed-go").disabled = false;
    }
  }
  $("ed-go").addEventListener("click", start);
  window.addEventListener("m2k-theme", () => { if (kit) drawMark(); });
  boot().catch((e) => { $("ed-go").disabled = true; $("ed-go").innerHTML = "Unavailable<small>" + esc(e.message) + "</small>"; });
})();
