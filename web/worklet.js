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

  msg(m) {
    if (m.t === "wasm") {
      const inst = new WebAssembly.Instance(new WebAssembly.Module(m.bytes), {});
      this.x = inst.exports;
      if (this.x._initialize) this.x._initialize();
      this.x.w_init(m.seed >>> 0);
      if (m.store) {
        const n = this.x.w_store_len();
        if (m.store.byteLength === n) {
          new Uint8Array(this.x.memory.buffer, this.x.w_store(), n).set(new Uint8Array(m.store));
          this.x.w_restore();
        }
      }
      this.audio = new Int16Array(this.x.memory.buffer, this.x.w_audio(), 256);
      this.port.postMessage({ t: "ready" });
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
      case "store": {
        const n = x.w_store_len();
        this.port.postMessage({ t: "store", bytes: new Uint8Array(x.memory.buffer, x.w_store(), n).slice() });
        break;
      }
      case "reset": { const keep = new Uint8Array(x.memory.buffer, x.w_store(), x.w_store_len()).slice(-16); x.w_init(m.seed >>> 0); new Uint8Array(x.memory.buffer, x.w_store(), x.w_store_len()).set(keep, x.w_store_len() - 16); break; }
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
    const ms = currentTime * 1000;
    if (ms - this.lastFrame >= 33) {
      this.lastFrame = ms;
      const mem = x.memory.buffer;
      const fb = new Uint8Array(mem, x.w_frame(ms >>> 0), 7200).slice();
      const leds = new Uint8Array(mem, x.w_leds(), 41).slice();
      const info = new Uint8Array(mem, x.w_info(), 32).slice();
      const msg = { t: "frame", fb, leds, info };
      if (x.w_dirty()) {
        msg.store = new Uint8Array(mem, x.w_store(), x.w_store_len()).slice();
      }
      this.port.postMessage(msg, [fb.buffer, leds.buffer, info.buffer]);
    }
    return true;
  }
}
registerProcessor("mono2k", Mono2k);
