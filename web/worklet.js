// SPDX-License-Identifier: GPL-3.0-only
// MONO2K's AudioWorklet: the firmware's core runs here, in one WebAssembly instance, as it runs on the FM-1 (audio
// in the ISR, the screen in the main loop). The page sends the controls and gets the screen, LEDs and state.
class Mono2k extends AudioWorkletProcessor {
  constructor() {
    super();
    this.x = null;
    this.lastFrame = -1e9;
    this.port.onmessage = (ev) => this.msg(ev.data);
  }

  /* a saved blob back into the core; a blob that is not ours leaves the factory as it is */
  unpack(bytes) {
    const x = this.x, n = Math.min(bytes.length, x.w_store_max());
    new Uint8Array(x.memory.buffer, x.w_store_buf(), n).set(bytes.subarray(0, n));
    return x.w_store_unpack(n);
  }

  kit() {
    const x = this.x, n = x.w_pcount(), kits = new Uint8Array(6 * n), machines = [];
    for (let t = 0; t < 6; t++) {
      machines.push(x.w_machine_of(t));
      kits.set(new Uint8Array(x.memory.buffer, x.w_kit(t), n), t * n);
    }
    this.port.postMessage({ t: "kit", pat: x.w_cur_pattern(), machines, kits, n });
  }

  names() {
    const x = this.x, mem = new Uint8Array(x.memory.buffer), s = (p) => { let r = ""; while (mem[p]) r += String.fromCharCode(mem[p++]); return r; };
    const pats = [], projs = [];
    for (let i = 0; i < x.w_patterns(); i++) pats.push(s(x.w_pat_name(i)));
    for (let i = 0; i < x.w_projects(); i++) projs.push(s(x.w_project_name(i)));
    this.port.postMessage({ t: "names", pats, projs });
  }

  msg(m) {
    if (m.t === "wasm") {
      const inst = new WebAssembly.Instance(new WebAssembly.Module(m.bytes), {});
      this.x = inst.exports;
      if (this.x._initialize) this.x._initialize();
      this.x.w_init(m.seed >>> 0);
      if (m.store) this.unpack(new Uint8Array(m.store));
      this.audio = new Int16Array(this.x.memory.buffer, this.x.w_audio(), 256);
      this.port.postMessage({ t: "ready" });
      this.names();
      return;
    }
    const x = this.x;
    if (!x) return;
    switch (m.t) {
      case "key": x.w_key(m.k, m.d); break;
      case "btn": x.w_button(m.b, m.d); break;
      case "enc": x.w_encoder(m.e, m.d); break;
      case "midi": x.w_midi(m.s, m.a, m.b); break;
      case "set": x.w_setting(m.i, m.v); break;
      case "pattern": x.w_pattern(m.n); break;
      case "track": x.w_track(m.n); break;
      case "mute": x.w_mute(m.n); break;
      case "tempo": x.w_tempo(m.d); break;
      case "reset": x.w_init(m.seed >>> 0); this.names(); break;
      case "names": this.names(); break;
      /* the patch editor: a parameter or a machine set on the open pattern, and the pattern's six tracks sent back */
      case "param": x.w_param(m.trk, m.code, m.v); break;
      case "machine": x.w_set_machine(m.trk, m.m); this.kit(); break;
      case "kit": this.kit(); break;
      /* the patch editor's sessions: a store loaded in place of the open one (none: the factory's), and the open one
       * packed, whole or as the FM-1 keeps it (16 patterns, one project) */
      case "load": {
        x.w_init(m.seed >>> 0);
        const ok = m.bytes ? this.unpack(new Uint8Array(m.bytes)) : 1;
        const n = x.w_store_pack();
        this.port.postMessage({ t: "loaded", ok, bytes: new Uint8Array(x.memory.buffer, x.w_store_buf(), n).slice() });
        this.names();
        this.kit();
        break;
      }
      case "pack": {
        const n = m.fm1 ? x.w_store_pack_as(16, 1) : x.w_store_pack();
        this.port.postMessage({ t: "packed", fm1: !!m.fm1, why: m.why, bytes: new Uint8Array(x.memory.buffer, x.w_store_buf(), n).slice() });
        break;
      }
      case "project": {                       /* load (save 0) or save (save 1) a project, then keep it */
        x.w_project(m.slot, m.save ? 1 : 0);
        const n = x.w_store_pack();
        this.port.postMessage({ t: "store", bytes: new Uint8Array(x.memory.buffer, x.w_store_buf(), n).slice() });
        this.names();
        break;
      }
      case "export": {
        const n = x.w_proj_export(m.slot);
        this.port.postMessage({ t: "export", slot: m.slot, bytes: new Uint8Array(x.memory.buffer, x.w_store_buf(), n).slice() });
        break;
      }
      case "import": {
        const b = new Uint8Array(m.bytes), n = Math.min(b.length, x.w_store_max());
        new Uint8Array(x.memory.buffer, x.w_store_buf(), n).set(b.subarray(0, n));
        const ok = x.w_proj_import(m.slot, n);
        if (ok) {
          const k = x.w_store_pack();
          this.port.postMessage({ t: "store", bytes: new Uint8Array(x.memory.buffer, x.w_store_buf(), k).slice() });
        }
        this.port.postMessage({ t: "imported", ok, slot: m.slot });
        this.names();
        break;
      }
      default: break;
    }
  }

  process(inputs, outputs) {
    const out = outputs[0];
    const x = this.x;
    if (!x) return true;
    const L = out[0], R = out[1] || out[0];
    const n = L.length;
    if (sampleRate === 44100) {
      for (let o = 0; o < n; o += 128) {
        x.w_render(128);
        const a = this.audio;
        for (let i = 0; i < 128 && o + i < n; i++) {
          L[o + i] = a[2 * i] / 32768;
          R[o + i] = a[2 * i + 1] / 32768;
        }
      }
    } else {
      /* the core runs at 44.1 kHz as on the FM-1; read it at this context's rate, linearly */
      if (!this.rs) this.rs = { pos: 0, step: 44100 / sampleRate, i: 128, prevL: 0, prevR: 0, curL: 0, curR: 0 };
      const s = this.rs;
      for (let o = 0; o < n; o++) {
        s.pos += s.step;
        while (s.pos >= 1) {
          s.pos -= 1;
          if (s.i >= 128) { x.w_render(128); s.i = 0; }
          s.prevL = s.curL; s.prevR = s.curR;
          s.curL = this.audio[2 * s.i] / 32768; s.curR = this.audio[2 * s.i + 1] / 32768;
          s.i++;
        }
        L[o] = s.prevL + (s.curL - s.prevL) * s.pos;
        R[o] = s.prevR + (s.curR - s.prevR) * s.pos;
      }
    }
    /* MIDI out (GLO > MIDI's clock and notes): what is due by the samples rendered goes to the page, which stamps it
     * with the sound's latency */
    if (x.w_midi_out) {
      const now = x.w_samples();
      let list = null, k;
      while ((k = x.w_midi_out(now)) > 0) (list || (list = [])).push(Array.from(new Uint8Array(x.memory.buffer, x.w_mout(), k)));
      if (list) this.port.postMessage({ t: "mout", list });
    }
    const ms = currentTime * 1000;
    if (ms - this.lastFrame >= 33) {
      this.lastFrame = ms;
      const mem = x.memory.buffer;
      const fb = new Uint8Array(mem, x.w_frame(ms >>> 0), 7200).slice();
      const leds = new Uint8Array(mem, x.w_leds(), 41).slice();
      const info = new Uint8Array(mem, x.w_info(), 32).slice();
      const msg = { t: "frame", fb, leds, info };
      if (x.w_dirty()) {
        const n = x.w_store_pack();
        msg.store = new Uint8Array(x.memory.buffer, x.w_store_buf(), n).slice();
        msg.names = true;
      }
      this.port.postMessage(msg, [fb.buffer, leds.buffer, info.buffer]);
    }
    return true;
  }
}
registerProcessor("mono2k", Mono2k);
