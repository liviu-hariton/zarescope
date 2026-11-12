// SPDX-License-Identifier: GPL-3.0-or-later
//
// A screenshot of a page with the installed Chrome, through its DevTools
// protocol (no other tool needed): the website's review captures and its social
// cards (tools/capture-site-og.sh). Full-page captures scroll through first, so
// lazy images load.
//
// Usage: node tools/site-shot.mjs <url> <out.png> <width> <height> [mobile 0|1] [fullPage 0|1] [theme dark|light] [css]
//   css: a stylesheet added to the page before the capture (the social card hides the controls)
import { spawn } from "node:child_process";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
const [url, out, w, h, mobile = "0", full = "0", theme = "", css = ""] = process.argv.slice(2);
const chrome = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const profile = mkdtempSync(join(tmpdir(), "cdp-"));
const proc = spawn(chrome, ["--headless=new", "--remote-debugging-port=0", `--user-data-dir=${profile}`, "--hide-scrollbars", "about:blank"], { stdio: ["ignore", "ignore", "pipe"] });
const wsUrl = await new Promise((res) => proc.stderr.on("data", (d) => { const m = String(d).match(/ws:\/\/\S+/); if (m) res(m[0]); }));
const port = new URL(wsUrl).port;
const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
const ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener("open", r));
let id = 0; const pending = new Map();
ws.addEventListener("message", (e) => { const m = JSON.parse(e.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result ?? m.error); pending.delete(m.id); } });
const send = (method, params = {}) => new Promise((r) => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const W = +w, H = +h, isMobile = mobile === "1";
await send("Emulation.setDeviceMetricsOverride", { width: W, height: H, deviceScaleFactor: isMobile ? 2 : 1, mobile: isMobile });
if (isMobile) await send("Emulation.setTouchEmulationEnabled", { enabled: true });
if (theme) await send("Emulation.setEmulatedMedia", { features: [{ name: "prefers-color-scheme", value: theme }] });
await send("Page.enable");
await send("Page.navigate", { url });
await new Promise((r) => setTimeout(r, 2500));
if (css) {
  await send("Runtime.evaluate", { expression: `(() => { const s = document.createElement("style"); s.textContent = ${JSON.stringify(css)}; document.head.append(s); })()` });
  await new Promise((r) => setTimeout(r, 400));
}
if (full === "1") {
  // Scroll through the page so lazy images load, then back to the top.
  await send("Runtime.evaluate", { expression: "(async () => { for (let y = 0; y < document.documentElement.scrollHeight; y += 600) { scrollTo(0, y); await new Promise(r => setTimeout(r, 120)); } scrollTo(0, 0); })()", awaitPromise: true });
  await new Promise((r) => setTimeout(r, 1500));
}
const metrics = await send("Runtime.evaluate", { expression: "JSON.stringify({sw: document.documentElement.scrollWidth, cw: document.documentElement.clientWidth, sh: document.documentElement.scrollHeight})", returnByValue: true });
const { sh } = JSON.parse(metrics.result.value);
const shot = await send("Page.captureScreenshot", full === "1"
  ? { format: "png", captureBeyondViewport: true, clip: { x: 0, y: 0, width: W, height: sh, scale: 1 } }
  : { format: "png" });
writeFileSync(out, Buffer.from(shot.data, "base64"));
ws.close(); proc.kill();
