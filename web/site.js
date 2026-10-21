// SPDX-License-Identifier: GPL-3.0-only
// The pages around the emulator (the manual, the installer, the patch editor): the theme switch, as the emulator's
// (Auto, Light, Dark, remembered under the same key), and a "m2k-theme" event for pages that draw in the theme.
(() => {
  "use strict";
  const THEMES = ["auto", "light", "dark"];
  const btn = document.getElementById("theme");
  let theme = "auto";
  try { theme = localStorage.getItem("mono2k.theme") || "auto"; } catch (e) { theme = "auto"; }
  const label = () => { if (btn) btn.textContent = theme === "auto" ? "Auto" : theme === "light" ? "Light" : "Dark"; };
  function apply() {
    if (theme === "auto") document.documentElement.removeAttribute("data-theme");
    else document.documentElement.setAttribute("data-theme", theme);
    try { if (theme === "auto") localStorage.removeItem("mono2k.theme"); else localStorage.setItem("mono2k.theme", theme); } catch (e) { /* private mode */ }
    label();
    window.dispatchEvent(new Event("m2k-theme"));
  }
  /* the dithered textures the style sheet draws with (as app.js makes them on the emulator page), in the theme's ink */
  const BAYER = (() => {
    const b = (n) => {
      if (n === 1) return [[0]];
      const q = b(n / 2), m = n / 2, r = [];
      for (let y = 0; y < n; y++) { r.push([]); for (let x = 0; x < n; x++) r[y].push(4 * q[y % m][x % m] + [[0, 2], [3, 1]][(y / m) | 0][(x / m) | 0]); }
      return r;
    };
    return b(8);
  })();
  function textures() {
    const cs = getComputedStyle(document.documentElement);
    const ink = cs.getPropertyValue("--ink").trim() || "#0b0b0b", paper = cs.getPropertyValue("--paper").trim() || "#f2f1ec";
    const make = (w, h, f, col = ink) => {
      const c = document.createElement("canvas");
      c.width = w; c.height = h;
      const g = c.getContext("2d");
      g.fillStyle = col;
      for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) if (f(x, y)) g.fillRect(x, y, 1, 1);
      return `url(${c.toDataURL()})`;
    };
    const r = document.documentElement.style;
    r.setProperty("--d-sparse", make(4, 4, (x, y) => x === 0 && y === 0));
    r.setProperty("--d-dots", make(2, 2, (x, y) => x === 0 && y === 0));
    r.setProperty("--d-check", make(2, 2, (x, y) => (x + y) % 2 === 0));
    r.setProperty("--g-across", make(256, 8, (x, y) => BAYER[y & 7][x & 7] < 64 - x / 4));
    r.setProperty("--g-down", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < 64 - y));
    r.setProperty("--g-down-inv", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < 64 - y, paper));
    r.setProperty("--g-up", make(8, 64, (x, y) => BAYER[y & 7][x & 7] < y));
  }
  window.addEventListener("m2k-theme", textures);
  textures();
  if (btn) btn.addEventListener("click", () => { theme = THEMES[(THEMES.indexOf(theme) + 1) % THEMES.length]; apply(); });
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", () => { if (theme === "auto") window.dispatchEvent(new Event("m2k-theme")); });
  label();
  window.M2K_SITE = { dark: () => { const t = document.documentElement.getAttribute("data-theme"); return t ? t === "dark" : matchMedia("(prefers-color-scheme: dark)").matches; } };
})();
