#pragma once

// Embedded web UI for the dub siren AP mode configuration portal
// Design language: Teenage Engineering / Elektron inspired
namespace web_ui {

static const char* const INDEX_HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Poorhouse Lane Siren</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&display=swap');
*{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#0a0a0a;--surface:#141414;--border:#2a2a2a;
  --accent:#ff6600;--accent-dim:#993d00;
  --led-green:#00ff88;--led-off:#1a1a1a;
  --text:#ccc;--text-hi:#fff;--text-lo:#555;
  --danger:#ff3333;--card:var(--surface);
  --font:'Space Mono',monospace;
}

/* ── Themes ──────────────────────────────────────────────── */
[data-theme="midnight"]{
  --bg:#0a0a0a;--surface:#141414;--border:#2a2a2a;
  --accent:#ff6600;--accent-dim:#993d00;
  --led-green:#00ff88;--led-off:#1a1a1a;
  --text:#ccc;--text-hi:#fff;--text-lo:#555;
  --danger:#ff3333;--card:#141414;
}
[data-theme="deep-dub"]{
  --bg:#05080f;--surface:#0c1220;--border:#1a2744;
  --accent:#00ccff;--accent-dim:#007799;
  --led-green:#00ff88;--led-off:#0a1020;
  --text:#8899bb;--text-hi:#ccddef;--text-lo:#3a4a66;
  --danger:#ff4466;--card:#0c1220;
}
[data-theme="reggae"]{
  --bg:#0a0f06;--surface:#141e0c;--border:#2a3a18;
  --accent:#ffcc00;--accent-dim:#997a00;
  --led-green:#00ff44;--led-off:#111a08;
  --text:#b0c890;--text-hi:#e0f0c0;--text-lo:#4a5a38;
  --danger:#ff4444;--card:#141e0c;
}
[data-theme="steppers"]{
  --bg:#0f0808;--surface:#1a1010;--border:#332020;
  --accent:#ff2222;--accent-dim:#991414;
  --led-green:#ff4444;--led-off:#1a0e0e;
  --text:#cc9999;--text-hi:#ffdddd;--text-lo:#664444;
  --danger:#ff6644;--card:#1a1010;
}
[data-theme="silver"]{
  --bg:#f0f0f0;--surface:#ffffff;--border:#d0d0d0;
  --accent:#ff6600;--accent-dim:#cc5200;
  --led-green:#00cc66;--led-off:#e0e0e0;
  --text:#444;--text-hi:#111;--text-lo:#999;
  --danger:#dd2222;--card:#ffffff;
}
[data-theme="concrete"]{
  --bg:#e8e4e0;--surface:#f5f2ee;--border:#c8c4c0;
  --accent:#2a6b4f;--accent-dim:#1a4a35;
  --led-green:#2a9b5f;--led-off:#d8d4d0;
  --text:#4a4640;--text-hi:#1a1816;--text-lo:#9a9690;
  --danger:#cc3333;--card:#f5f2ee;
}

/* ── Theme Picker Styles ─────────────────────────────────── */
.theme-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(100px,1fr));gap:8px}
.theme-swatch{position:relative;cursor:pointer;border:2px solid var(--border);
  padding:10px 8px;text-align:center;transition:all 0.2s}
.theme-swatch:hover{border-color:var(--text-lo)}
.theme-swatch.active{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent)}
.theme-swatch .swatch-bar{height:6px;display:flex;gap:2px;margin-bottom:8px}
.theme-swatch .swatch-bar span{flex:1;display:block}
.theme-swatch .swatch-label{font-size:0.55rem;font-weight:700;letter-spacing:1px;
  text-transform:uppercase;color:var(--text-lo)}
.theme-swatch.active .swatch-label{color:var(--accent)}

html{font-size:14px}
body{font-family:var(--font);background:var(--bg);color:var(--text);
  min-height:100vh;-webkit-tap-highlight-color:transparent;
  -webkit-font-smoothing:antialiased}

/* ── Header ─────────────────────────────────────────────── */
.header{display:flex;justify-content:space-between;align-items:center;
  padding:12px 16px;border-bottom:2px solid var(--accent)}
.header .brand{font-size:0.85rem;font-weight:700;color:var(--text-hi);
  letter-spacing:3px;text-transform:uppercase}
.header .sub{font-size:0.7rem;color:var(--text-lo);letter-spacing:2px;
  text-transform:uppercase}

/* ── Tab Bar ────────────────────────────────────────────── */
.tabs{display:flex;border-bottom:1px solid var(--border)}
.tab{flex:1;padding:10px 4px;text-align:center;cursor:pointer;
  font-family:var(--font);font-size:0.7rem;font-weight:700;
  letter-spacing:2px;text-transform:uppercase;
  color:var(--text-lo);background:var(--bg);
  border-right:1px solid var(--border);transition:all 0.15s}
.tab:last-child{border-right:none}
.tab.active{color:var(--bg);background:var(--accent)}
.tab:not(.active):hover{color:var(--text)}

/* ── Content ────────────────────────────────────────────── */
.content{padding:16px;margin:0 auto}
.panel{display:none}.panel.active{display:block}

/* Mobile: single column, max-width for readability */
@media (max-width: 767px) {
  .content{max-width:540px}
}
/* Desktop: wider container */
@media (min-width: 768px) {
  .content{max-width:960px}
}

/* ── Section Labels ─────────────────────────────────────── */
.section{font-size:0.65rem;font-weight:700;color:var(--accent);
  letter-spacing:3px;text-transform:uppercase;padding:12px 0 8px;
  border-bottom:1px solid var(--border);margin-bottom:8px}

/* ── Banks Layout ──────────────────────────────────────── */
/* Desktop: 3 banks side-by-side */
@media (min-width: 768px) {
  .banks-row{display:grid;grid-template-columns:1fr 1fr 1fr;gap:16px;margin-bottom:16px}
  .bank-col .slot-grid{grid-template-columns:1fr 1fr}
}
/* Mobile: stacked */
@media (max-width: 767px) {
  .banks-row{display:block}
  .bank-col{margin-bottom:12px}
}

/* ── Slot Grid ──────────────────────────────────────────── */
.slot-grid{display:grid;grid-template-columns:1fr 1fr;gap:1px;
  background:var(--border);border:1px solid var(--border);margin-bottom:8px}
.slot{background:var(--surface);padding:10px;position:relative;
  min-height:100px;display:flex;flex-direction:column;transition:all 0.2s}
.slot.active-slot{border:2px solid var(--led-green)}
.slot.target-slot{border:2px solid var(--accent);background:var(--surface)}
.target-hint{font-size:0.65rem;color:var(--text-lo);letter-spacing:1px;
  text-transform:uppercase;padding:8px 0;text-align:center}
/* Library browser */
.lib-category{font-size:0.6rem;font-weight:700;color:var(--accent);
  letter-spacing:2px;text-transform:uppercase;padding:10px 0 6px;
  border-bottom:1px solid var(--border);margin-top:4px}
.lib-preset{display:flex;align-items:center;justify-content:space-between;
  padding:8px 6px;border-bottom:1px solid var(--border);cursor:pointer;
  transition:background 0.15s}
.lib-preset:hover{background:var(--border)}
.lib-preset .lib-name{font-size:0.8rem;color:var(--text-hi);flex:1}
.lib-preset .lib-actions{display:flex;gap:4px}
.lib-preset .lib-btn{font-family:var(--font);font-size:0.55rem;font-weight:700;
  letter-spacing:1px;text-transform:uppercase;padding:4px 8px;
  background:var(--bg);color:var(--text-lo);border:1px solid var(--border);
  cursor:pointer;transition:all 0.15s}
.lib-preset .lib-btn:hover{color:var(--text-hi);border-color:var(--text-lo)}
.lib-preset .lib-btn.primary{background:var(--accent);color:var(--bg);border-color:var(--accent)}
.slot-num{display:flex;align-items:center;gap:6px;margin-bottom:6px}
.slot-num span{font-size:0.65rem;font-weight:700;color:var(--text-lo);
  letter-spacing:2px}
.led{width:6px;height:6px;border-radius:50%;background:var(--led-off);
  flex-shrink:0}
.led.active{background:var(--led-green);box-shadow:0 0 6px var(--led-green)}
.slot-name{font-size:0.85rem;font-weight:700;color:var(--text-hi);
  margin-bottom:2px;word-break:break-word;flex:1}
.slot-cat{font-size:0.55rem;color:var(--text-lo);letter-spacing:1px;
  text-transform:uppercase;margin-bottom:6px}
.slot-actions{display:flex;gap:3px;margin-top:auto;flex-wrap:wrap}
.slot-btn{font-family:var(--font);font-size:0.55rem;font-weight:700;
  letter-spacing:1px;text-transform:uppercase;padding:3px 6px;
  background:var(--bg);color:var(--text-lo);border:1px solid var(--border);
  cursor:pointer;transition:all 0.15s}
.slot-btn:hover{color:var(--text-hi);border-color:var(--text-lo)}
.slot-btn.active{color:var(--accent);border-color:var(--accent)}

/* Inline save */
.save-inline{display:none;margin-top:6px}
.save-inline.show{display:flex;gap:4px}
.save-inline input{font-family:var(--font);font-size:0.7rem;
  background:var(--bg);color:var(--text-hi);border:1px solid var(--accent);
  padding:3px 6px;flex:1;outline:none;min-width:0}
.save-inline button{font-family:var(--font);font-size:0.55rem;font-weight:700;
  background:var(--accent);color:var(--bg);border:none;padding:3px 8px;
  cursor:pointer;letter-spacing:1px}

/* ── Preset Library Browser ───────────────────────────── */
.preset-loader{background:var(--surface);border:1px solid var(--border);
  padding:16px;margin-bottom:16px;max-height:60vh;overflow-y:auto}
.preset-loader h3{font-size:0.7rem;font-weight:700;color:var(--accent);
  letter-spacing:2px;text-transform:uppercase;margin-bottom:12px}

/* ── Cards / Form Elements ──────────────────────────────── */
.card{background:var(--surface);border:1px solid var(--border);
  padding:16px;margin-bottom:8px}
.card h3{font-size:0.7rem;font-weight:700;color:var(--accent);
  letter-spacing:2px;text-transform:uppercase;margin-bottom:12px}

.form-group{margin-bottom:12px}
.form-group label{display:block;font-size:0.6rem;font-weight:700;
  color:var(--text-lo);letter-spacing:2px;text-transform:uppercase;
  margin-bottom:4px}
.form-group input,.form-group select{width:100%;padding:8px;
  font-family:var(--font);font-size:0.8rem;
  background:var(--bg);color:var(--text-hi);
  border:1px solid var(--border);outline:none}
.form-group input:focus,.form-group select:focus{border-color:var(--accent)}
.form-group input[type=range]{padding:4px 0;-webkit-appearance:none;
  background:transparent}
input[type=range]::-webkit-slider-runnable-track{height:4px;background:var(--border)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;
  width:14px;height:14px;background:var(--accent);margin-top:-5px;cursor:pointer}
input[type=range]::-moz-range-track{height:4px;background:var(--border);border:none}
input[type=range]::-moz-range-thumb{width:14px;height:14px;background:var(--accent);
  border:none;cursor:pointer}

/* Segmented control (radio replacement) */
.seg-ctrl{display:flex;border:1px solid var(--border)}
.seg-btn{flex:1;padding:8px 4px;text-align:center;cursor:pointer;
  font-family:var(--font);font-size:0.65rem;font-weight:700;
  letter-spacing:1px;text-transform:uppercase;
  color:var(--text-lo);background:var(--bg);
  border-right:1px solid var(--border);transition:all 0.15s}
.seg-btn:last-child{border-right:none}
.seg-btn.active{color:var(--bg);background:var(--accent)}
.seg-btn:not(.active):hover{color:var(--text)}

/* Toggle (square style) */
.toggle-row{display:flex;align-items:center;justify-content:space-between;
  padding:10px 0;border-bottom:1px solid var(--border)}
.toggle-row span{font-size:0.7rem;letter-spacing:1px;text-transform:uppercase}
.toggle{position:relative;width:36px;height:18px;flex-shrink:0}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;cursor:pointer;inset:0;
  background:var(--border);transition:0.2s}
.toggle .slider:before{content:'';position:absolute;height:12px;width:12px;
  left:3px;bottom:3px;background:var(--text-lo);transition:0.2s}
.toggle input:checked+.slider{background:var(--accent)}
.toggle input:checked+.slider:before{transform:translateX(18px);background:var(--bg)}

.range-val{color:var(--accent);font-weight:700;font-size:0.75rem;
  min-width:36px;display:inline-block;text-align:right}

/* ── Buttons ────────────────────────────────────────────── */
.btn{font-family:var(--font);font-size:0.7rem;font-weight:700;
  letter-spacing:1px;text-transform:uppercase;padding:10px 20px;
  border:none;cursor:pointer;transition:all 0.15s}
.btn-sm{font-size:0.6rem;padding:8px 12px}
.btn-primary{background:var(--accent);color:var(--bg)}
.btn-primary:hover{background:var(--accent-dim)}
.btn-secondary{background:var(--bg);color:var(--text);border:1px solid var(--border)}
.btn-secondary:hover{border-color:var(--text-lo)}
.btn-danger{background:var(--danger);color:var(--bg)}
.btn-danger:hover{background:#cc2222}
.btn-block{width:100%;display:block}

/* ── WiFi ───────────────────────────────────────────────── */
.network-list{max-height:200px;overflow-y:auto}
.network-item{display:flex;justify-content:space-between;align-items:center;
  padding:8px 4px;cursor:pointer;border-bottom:1px solid var(--border);
  font-size:0.8rem}
.network-item:hover{background:var(--border)}
.signal{color:var(--text-lo);font-size:0.7rem}

/* ── Toast ──────────────────────────────────────────────── */
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);
  font-family:var(--font);font-size:0.75rem;font-weight:700;
  letter-spacing:1px;text-transform:uppercase;
  background:var(--accent);color:var(--bg);padding:10px 24px;
  opacity:0;transition:opacity 0.3s;z-index:999}
.toast.show{opacity:1}

/* ── Loading ────────────────────────────────────────────── */
.loading{text-align:center;padding:20px;color:var(--text-lo)}
.spinner{display:inline-block;width:16px;height:16px;border:2px solid var(--border);
  border-top-color:var(--accent);animation:spin 0.8s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}

/* ── Exit Screen ────────────────────────────────────────── */
.exit-screen{text-align:center;padding:60px 20px}
.exit-screen h2{color:var(--accent);font-size:1rem;letter-spacing:3px;
  text-transform:uppercase;margin-bottom:8px}
.exit-screen p{color:var(--text-lo);font-size:0.8rem}
</style>
</head>
<body>

<div class="header">
  <div class="brand">Poorhouse Lane</div>
  <div class="sub">Siren Config</div>
</div>

<div class="tabs">
  <div class="tab active" onclick="showTab('presets')">Presets</div>
  <div class="tab" onclick="showTab('options')">Options</div>
  <div class="tab" onclick="showTab('wifi')">WiFi</div>
  <div class="tab" onclick="showTab('system')">System</div>
</div>

<!-- ═══ PRESETS TAB ═══ -->
<div class="content">
<div id="presets" class="panel active">

  <!-- Bank Grids — tap a slot to select as target -->
  <div id="target-hint" class="target-hint">Tap a slot to select it, then pick a preset below</div>
  <div class="banks-row">
    <div class="bank-col">
      <div class="section">User Bank</div>
      <div class="slot-grid" id="slot-grid-user"></div>
    </div>
    <div class="bank-col">
      <div class="section">Standard Bank</div>
      <div class="slot-grid" id="slot-grid-standard"></div>
    </div>
    <div class="bank-col">
      <div class="section">Experimental Bank</div>
      <div class="slot-grid" id="slot-grid-experimental"></div>
    </div>
  </div>

  <!-- Preset Library Browser — shown after selecting a target slot -->
  <div id="library-browser" class="preset-loader" style="display:none">
    <h3>Preset Library <span id="target-label" style="color:var(--text-lo);font-size:0.6rem;letter-spacing:1px"></span></h3>
    <div id="library-list"></div>
  </div>

</div>

<!-- ═══ OPTIONS TAB ═══ -->
<div id="options" class="panel">

  <div class="card">
    <h3>Reverb Type</h3>
    <div class="form-group">
      <select id="reverb-type" onchange="onReverbTypeChange()">
        <option value="0">Spring</option>
        <option value="1">Plate</option>
        <option value="2">Hall</option>
        <option value="3">Schroeder</option>
      </select>
    </div>
  </div>

  <div class="card">
    <h3>Delay Type</h3>
    <div class="form-group">
      <select id="delay-type" onchange="onDelayTypeChange()">
        <option value="0">Tape</option>
        <option value="1">Digital</option>
      </select>
    </div>
  </div>

  <div class="card" id="tape-params">
    <h3>Tape Parameters</h3>
    <div class="form-group">
      <label>Wobble <span class="range-val" id="wobble-val">100%</span></label>
      <input type="range" id="wobble" min="0" max="100" value="100" oninput="updateRange('wobble')">
    </div>
    <div class="form-group">
      <label>Flutter <span class="range-val" id="flutter-val">100%</span></label>
      <input type="range" id="flutter" min="0" max="100" value="100" oninput="updateRange('flutter')">
    </div>
  </div>

  <div class="card">
    <h3>Effects Chain</h3>
    <div class="form-group">
      <select id="fx-chain">
        <option value="0">Filter &rarr; Delay &rarr; Reverb</option>
        <option value="1">Filter &rarr; Reverb &rarr; Delay</option>
        <option value="2">Delay &rarr; Filter &rarr; Reverb</option>
        <option value="3">Delay &rarr; Reverb &rarr; Filter</option>
        <option value="4">Reverb &rarr; Filter &rarr; Delay</option>
        <option value="5">Reverb &rarr; Delay &rarr; Filter</option>
      </select>
    </div>
  </div>

  <div class="card">
    <h3>Settings</h3>
    <div class="toggle-row">
      <span>Linked Pitch / LFO</span>
      <label class="toggle"><input type="checkbox" id="lfo-link" checked><span class="slider"></span></label>
    </div>
    <div class="toggle-row">
      <span>Super Drip Reverb</span>
      <label class="toggle"><input type="checkbox" id="super-drip" checked><span class="slider"></span></label>
    </div>
    <div class="toggle-row">
      <span>Filter Sweep</span>
      <select id="sweep-dir" style="width:auto;padding:6px;font-family:var(--font);
        font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border)">
        <option value="-1">Down (Dub)</option>
        <option value="0">Flat</option>
        <option value="1">Up (Bright)</option>
      </select>
    </div>
  </div>

  <div class="card">
    <h3>Tape Saturator</h3>
    <div class="form-group">
      <label>Mix <span class="range-val" id="saturator-mix-val">0%</span></label>
      <input type="range" id="saturator-mix" min="0" max="100" value="0" oninput="updateRange('saturator-mix')">
    </div>
    <div class="form-group">
      <label>Drive <span class="range-val" id="saturator-drive-val">50%</span></label>
      <input type="range" id="saturator-drive" min="0" max="100" value="50" oninput="updateRange('saturator-drive')">
    </div>
  </div>

  <div class="card">
    <h3>Modulation FX</h3>
    <div class="form-group">
      <label>Phaser <span class="range-val" id="phaser-mix-val">0%</span></label>
      <input type="range" id="phaser-mix" min="0" max="100" value="0" oninput="updateRange('phaser-mix')">
    </div>
    <div class="form-group">
      <label>Chorus <span class="range-val" id="chorus-mix-val">0%</span></label>
      <input type="range" id="chorus-mix" min="0" max="100" value="0" oninput="updateRange('chorus-mix')">
    </div>
    <div class="form-group">
      <label>Flanger <span class="range-val" id="flanger-mix-val">0%</span></label>
      <input type="range" id="flanger-mix" min="0" max="100" value="0" oninput="updateRange('flanger-mix')">
    </div>
  </div>

  <button class="btn btn-primary btn-block" onclick="applyOptions()" style="margin-top:8px">
    Apply Options
  </button>
</div>

<!-- ═══ WIFI TAB ═══ -->
<div id="wifi" class="panel">
  <div class="card">
    <h3>Networks</h3>
    <button class="btn btn-secondary" onclick="scanWifi()" style="margin-bottom:12px">Scan</button>
    <div id="wifi-networks" class="network-list"></div>
  </div>
  <div class="card">
    <h3>Connect</h3>
    <div class="form-group"><label>SSID</label><input type="text" id="wifi-ssid"></div>
    <div class="form-group"><label>Password</label><input type="password" id="wifi-pass"></div>
    <button class="btn btn-primary" onclick="connectWifi()">Save Credentials</button>
  </div>
  <div class="card">
    <h3>Status</h3>
    <div id="wifi-status" style="font-size:0.8rem;color:var(--text-lo)">Not connected</div>
  </div>
</div>

<!-- ═══ SYSTEM TAB ═══ -->
<div id="system" class="panel">
  <div class="card">
    <h3>Updates</h3>
    <p style="color:var(--text-lo);margin-bottom:8px;font-size:0.75rem">Version 0.5.0</p>
    <div class="form-group">
      <label>Branch</label>
      <select id="update-branch" style="width:100%;padding:8px;border-radius:6px;border:1px solid var(--border);background:var(--bg);color:var(--text);font-size:0.85rem">
        <option value="main">main</option>
      </select>
    </div>
    <button class="btn btn-secondary" onclick="checkUpdate()" id="btn-check">Check for Updates</button>
    <div id="update-result" style="margin-top:12px"></div>
    <div id="update-progress" style="display:none;margin-top:12px">
      <div style="font-size:0.8rem;color:var(--text-lo);margin-bottom:6px" id="update-stage-text">Preparing...</div>
      <div style="background:var(--card);border:1px solid var(--border);border-radius:8px;height:22px;overflow:hidden">
        <div id="update-bar" style="height:100%;width:0%;background:var(--accent);border-radius:8px;transition:width 0.4s ease"></div>
      </div>
      <div style="font-size:0.7rem;color:var(--text-lo);margin-top:4px;text-align:right" id="update-pct-text">0%</div>
    </div>
  </div>
  <div class="card">
    <h3>Device</h3>
    <div id="sys-info" style="font-size:0.8rem;color:var(--text-lo)">Loading...</div>
  </div>
  <div class="card">
    <h3>Theme</h3>
    <div class="theme-grid" id="theme-grid"></div>
  </div>
  <div class="card">
    <h3>Backup &amp; Restore</h3>
    <button class="btn btn-secondary" onclick="downloadBackup()" style="margin-bottom:8px">
      Download Backup
    </button>
    <div class="form-group">
      <label>Restore from file</label>
      <input type="file" id="restore-file" accept=".json" style="font-size:0.75rem">
    </div>
    <button class="btn btn-secondary" onclick="restoreBackup()">Restore</button>
  </div>
  <div class="card">
    <button class="btn btn-danger btn-block" onclick="exitAP()">Return to Siren Mode</button>
  </div>
</div>
</div>

<div id="toast" class="toast"></div>

<script>
// ─── State ──────────────────────────────────────────────────────────
let allPresets = {};
let libraryByCategory = {};  // { category: [{name, index}, ...] }
let activeBank = 0;
let activePreset = 0;
let targetBank = null;   // selected target slot bank name
let targetSlot = null;   // selected target slot index
const BANK_NAMES = ['user', 'standard', 'experimental'];

// ─── Tab Navigation ─────────────────────────────────────────────────
function showTab(id) {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  const tabs = ['presets','options','wifi','system'];
  const tabEls = document.querySelectorAll('.tab');
  tabs.forEach((t, i) => { if (t === id && tabEls[i]) tabEls[i].classList.add('active'); });
  if (id === 'system') { loadSystemInfo(); loadBranches(); }
}

// ─── Toast ──────────────────────────────────────────────────────────
function toast(msg, err) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.style.background = err ? 'var(--danger)' : 'var(--accent)';
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2500);
}

function esc(s) {
  const d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}

// ─── Dropdown Handlers ──────────────────────────────────────────────
function onReverbTypeChange() {}
function onDelayTypeChange() {
  const val = parseInt(document.getElementById('delay-type').value);
  document.getElementById('tape-params').style.display = val === 0 ? 'block' : 'none';
}

function updateRange(id) {
  document.getElementById(id + '-val').textContent = document.getElementById(id).value + '%';
}

// ─── Target Slot Selection ──────────────────────────────────────────
function selectTargetSlot(bank, slot) {
  // Deselect if tapping same slot
  if (targetBank === bank && targetSlot === slot) {
    targetBank = null; targetSlot = null;
    document.querySelectorAll('.slot.target-slot').forEach(el => el.classList.remove('target-slot'));
    document.getElementById('library-browser').style.display = 'none';
    document.getElementById('target-hint').textContent = 'Tap a slot to select it, then pick a preset below';
    return;
  }
  targetBank = bank; targetSlot = slot;
  // Update visual
  document.querySelectorAll('.slot.target-slot').forEach(el => el.classList.remove('target-slot'));
  const el = document.getElementById('slot-' + bank + '-' + slot);
  if (el) el.classList.add('target-slot');
  document.getElementById('target-hint').textContent =
    'Target: ' + bank.toUpperCase() + ' slot ' + (slot + 1) + ' — pick a preset below';
  // Show library browser
  document.getElementById('target-label').textContent =
    '→ ' + bank.toUpperCase() + ' SLOT ' + (slot + 1);
  document.getElementById('library-browser').style.display = 'block';
  renderLibrary();
}

function renderLibrary() {
  const list = document.getElementById('library-list');
  let html = '';
  Object.keys(libraryByCategory).forEach(cat => {
    html += '<div class="lib-category">' + esc(cat) + '</div>';
    libraryByCategory[cat].forEach(p => {
      html += '<div class="lib-preset">' +
        '<span class="lib-name">' + esc(p.name) + '</span>' +
        '<div class="lib-actions">' +
          '<div class="lib-btn" onclick="event.stopPropagation();doPreviewPreset(' + p.index + ')">Preview</div>' +
          '<div class="lib-btn primary" onclick="event.stopPropagation();doLoadPreset(' + p.index + ',\'' + esc(p.name) + '\')">Load</div>' +
        '</div></div>';
    });
  });
  list.innerHTML = html;
}

// ─── Bank Slot Rendering ────────────────────────────────────────────
function renderBankSlots(bankName, presets, gridId) {
  const grid = document.getElementById(gridId);
  const bankIdx = BANK_NAMES.indexOf(bankName);
  grid.innerHTML = presets.map((p, i) => {
    const isActive = activeBank === bankIdx && activePreset === i;
    const isTarget = targetBank === bankName && targetSlot === i;
    return `<div class="slot${isActive ? ' active-slot' : ''}${isTarget ? ' target-slot' : ''}"
                 id="slot-${bankName}-${i}" onclick="selectTargetSlot('${bankName}',${i})">
      <div class="slot-num">
        <div class="led${isActive ? ' active' : ''}"></div>
        <span>0${i + 1}</span>
      </div>
      <div class="slot-name">${esc(p.name || 'Empty')}</div>
      <div class="slot-cat">${p.saved ? 'Saved' : 'Factory'}</div>
      <div class="slot-actions">
        <div class="slot-btn${isActive ? ' active' : ''}" onclick="event.stopPropagation();applyBankPreset('${bankName}',${i})">Apply</div>
        <div class="slot-btn" onclick="event.stopPropagation();showSaveInput('${bankName}',${i})">Save</div>
      </div>
      <div class="save-inline" id="save-inline-${bankName}-${i}">
        <input type="text" placeholder="Name" id="save-name-${bankName}-${i}" value="${esc(p.name || '')}"
               onclick="event.stopPropagation()" onkeydown="if(event.key==='Enter')doSave('${bankName}',${i})">
        <button onclick="event.stopPropagation();doSave('${bankName}',${i})">OK</button>
      </div>
    </div>`;
  }).join('');
}

// ─── Apply Preset ───────────────────────────────────────────────────
async function applyBankPreset(bank, index) {
  try {
    const r = await fetch('/api/presets/apply', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `category=${bank}&index=${index}` });
    if (r.ok) { toast('Applied'); loadPresets(); }
    else toast('Apply failed', true);
  } catch (e) { toast('Error: ' + e, true); }
}

// ─── Save ───────────────────────────────────────────────────────────
function showSaveInput(bank, i) {
  document.querySelectorAll('.save-inline.show').forEach(el => el.classList.remove('show'));
  const el = document.getElementById('save-inline-' + bank + '-' + i);
  if (el) { el.classList.add('show'); el.querySelector('input').focus(); }
}

async function doSave(bank, slot) {
  const input = document.getElementById('save-name-' + bank + '-' + slot);
  const name = input ? input.value || 'Preset' : 'Preset';
  try {
    const r = await fetch('/api/presets/bank/save', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `bank=${bank}&slot=${slot}&name=${encodeURIComponent(name)}` });
    if (r.ok) { toast('Saved to ' + bank + ' slot ' + (slot + 1)); loadPresets(); }
    else toast('Save failed', true);
  } catch (e) { toast('Error: ' + e, true); }
}

// ─── Preset Library ─────────────────────────────────────────────────
function buildCategoryDropdown(data) {
  libraryByCategory = {};
  (data.library || []).forEach(p => {
    const cat = p.category || 'Other';
    if (!libraryByCategory[cat]) libraryByCategory[cat] = [];
    libraryByCategory[cat].push(p);
  });
  // Re-render library if browser is open
  if (targetBank !== null) renderLibrary();
}

async function doLoadPreset(index, name) {
  if (targetBank === null || targetSlot === null) {
    toast('Select a target slot first', true); return;
  }
  try {
    const r = await fetch('/api/presets/bank/load', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `bank=${targetBank}&slot=${targetSlot}&category=library&index=${index}` });
    if (r.ok) {
      toast('Loaded "' + name + '" → ' + targetBank.toUpperCase() + ' slot ' + (targetSlot + 1));
      loadPresets();
    } else toast('Load failed', true);
  } catch (e) { toast('Error: ' + e, true); }
}

async function doPreviewPreset(index) {
  try {
    const r = await fetch('/api/presets/apply', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `category=library&index=${index}` });
    if (r.ok) toast('Previewing'); else toast('Preview failed', true);
  } catch (e) { toast('Error: ' + e, true); }
}

// ─── Data Loading ───────────────────────────────────────────────────
async function loadPresets() {
  try {
    const r = await fetch('/api/presets');
    const d = await r.json();
    allPresets = d;
    activeBank = d.active_bank || 0;
    activePreset = d.active_preset || 0;
    renderBankSlots('user', d.user || [], 'slot-grid-user');
    renderBankSlots('standard', d.standard || [], 'slot-grid-standard');
    renderBankSlots('experimental', d.experimental || [], 'slot-grid-experimental');
    buildCategoryDropdown(d);
  } catch (e) { console.error(e); }
}

async function loadOptions() {
  try {
    const r = await fetch('/api/siren/options');
    const d = await r.json();
    document.getElementById('reverb-type').value = d.reverb_type || 0;
    document.getElementById('delay-type').value = d.delay_type || 0;
    onDelayTypeChange();
    document.getElementById('wobble').value = Math.round((d.tape_wobble || 1) * 100);
    document.getElementById('flutter').value = Math.round((d.tape_flutter || 1) * 100);
    updateRange('wobble'); updateRange('flutter');
    document.getElementById('fx-chain').value = d.fx_chain || 0;
    document.getElementById('lfo-link').checked = d.lfo_pitch_link !== false;
    document.getElementById('super-drip').checked = d.super_drip !== false;
    document.getElementById('sweep-dir').value = d.sweep_dir || -1;
    document.getElementById('saturator-mix').value = Math.round((d.saturator_mix || 0) * 100);
    document.getElementById('saturator-drive').value = Math.round((d.saturator_drive != null ? d.saturator_drive : 0.5) * 100);
    updateRange('saturator-mix'); updateRange('saturator-drive');
    document.getElementById('phaser-mix').value = Math.round((d.phaser_mix || 0) * 100);
    document.getElementById('chorus-mix').value = Math.round((d.chorus_mix || 0) * 100);
    document.getElementById('flanger-mix').value = Math.round((d.flanger_mix || 0) * 100);
    updateRange('phaser-mix'); updateRange('chorus-mix'); updateRange('flanger-mix');
  } catch (e) { console.error(e); }
}

async function applyOptions() {
  const rt = document.getElementById('reverb-type').value;
  const dt = document.getElementById('delay-type').value;
  const body = [
    'reverb_type=' + rt, 'delay_type=' + dt,
    'tape_wobble=' + (document.getElementById('wobble').value / 100),
    'tape_flutter=' + (document.getElementById('flutter').value / 100),
    'fx_chain=' + document.getElementById('fx-chain').value,
    'lfo_pitch_link=' + (document.getElementById('lfo-link').checked ? '1' : '0'),
    'super_drip=' + (document.getElementById('super-drip').checked ? '1' : '0'),
    'sweep_dir=' + document.getElementById('sweep-dir').value,
    'saturator_mix=' + (document.getElementById('saturator-mix').value / 100),
    'saturator_drive=' + (document.getElementById('saturator-drive').value / 100),
    'phaser_mix=' + (document.getElementById('phaser-mix').value / 100),
    'chorus_mix=' + (document.getElementById('chorus-mix').value / 100),
    'flanger_mix=' + (document.getElementById('flanger-mix').value / 100)
  ].join('&');
  try {
    const r = await fetch('/api/siren/options', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
    if (r.ok) toast('Options applied'); else toast('Failed', true);
  } catch (e) { toast('Error: ' + e, true); }
}

// ─── WiFi ───────────────────────────────────────────────────────────
async function scanWifi() {
  document.getElementById('wifi-networks').innerHTML =
    '<div class="loading"><div class="spinner"></div> Scanning...</div>';
  try {
    const r = await fetch('/api/wifi/scan');
    const d = await r.json();
    document.getElementById('wifi-networks').innerHTML =
      (d.networks || []).map(n =>
        `<div class="network-item" onclick="document.getElementById('wifi-ssid').value='${esc(n.ssid)}'">
          <span>${esc(n.ssid)}</span>
          <span class="signal">${n.signal} dBm</span>
        </div>`).join('') ||
      '<div style="color:var(--text-lo);padding:8px;font-size:0.8rem">No networks found</div>';
  } catch (e) {
    document.getElementById('wifi-networks').innerHTML =
      '<div style="color:var(--danger);padding:8px;font-size:0.8rem">Scan failed</div>';
  }
}

async function connectWifi() {
  const ssid = document.getElementById('wifi-ssid').value;
  const pass = document.getElementById('wifi-pass').value;
  if (!ssid) { toast('Enter SSID', true); return; }
  try {
    const r = await fetch('/api/wifi/connect', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(pass)}` });
    if (r.ok) toast('Credentials saved'); else toast('Failed', true);
  } catch (e) { toast('Error: ' + e, true); }
}

// ─── System ─────────────────────────────────────────────────────────
const STAGE_LABELS = {
  idle: 'Ready', backup: 'Backing up settings...', fetch: 'Fetching updates...',
  pull: 'Pulling code...', build: 'Building firmware...', deploy: 'Deploying...',
  done: 'Complete!', error: 'Error'
};
let updatePollTimer = null;

async function loadBranches() {
  try {
    const r = await fetch('/api/update/branches');
    const d = await r.json();
    const sel = document.getElementById('update-branch');
    const cur = sel.value;
    sel.innerHTML = (d.branches || ['main']).map(b =>
      `<option value="${esc(b)}"${b === cur ? ' selected' : ''}>${esc(b)}</option>`
    ).join('');
  } catch (e) { /* keep default main */ }
}

async function checkUpdate() {
  const branch = document.getElementById('update-branch').value;
  document.getElementById('update-result').innerHTML =
    '<div class="loading"><div class="spinner"></div> Checking...</div>';
  try {
    const r = await fetch('/api/update/check', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'branch=' + encodeURIComponent(branch) });
    const d = await r.json();
    if (d.available) {
      document.getElementById('update-result').innerHTML =
        `<p style="color:var(--accent);font-size:0.8rem">${d.count} update(s) available on ${esc(branch)}</p>
         <button class="btn btn-primary" onclick="installUpdate()" style="margin-top:8px">Install</button>`;
    } else {
      document.getElementById('update-result').innerHTML =
        '<p style="color:var(--text-lo);font-size:0.8rem">Up to date</p>';
    }
  } catch (e) {
    document.getElementById('update-result').innerHTML =
      '<p style="color:var(--danger);font-size:0.8rem">Check failed</p>';
  }
}

async function installUpdate() {
  const branch = document.getElementById('update-branch').value;
  document.getElementById('update-result').innerHTML = '';
  document.getElementById('update-progress').style.display = 'block';
  document.getElementById('update-bar').style.width = '0%';
  document.getElementById('update-stage-text').textContent = 'Starting...';
  document.getElementById('update-pct-text').textContent = '0%';
  document.getElementById('btn-check').disabled = true;
  try {
    const r = await fetch('/api/update/install', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'branch=' + encodeURIComponent(branch) });
    if (!r.ok) {
      const d = await r.json().catch(() => ({}));
      toast(d.error || 'Update failed', true);
      document.getElementById('update-progress').style.display = 'none';
      document.getElementById('btn-check').disabled = false;
      return;
    }
    // Start polling for progress
    pollUpdateStatus();
  } catch (e) {
    toast('Error: ' + e, true);
    document.getElementById('update-progress').style.display = 'none';
    document.getElementById('btn-check').disabled = false;
  }
}

async function pollUpdateStatus() {
  if (updatePollTimer) clearTimeout(updatePollTimer);
  try {
    const r = await fetch('/api/update/status');
    const d = await r.json();
    const pct = d.progress || 0;
    const stage = d.stage || 'idle';
    document.getElementById('update-bar').style.width = pct + '%';
    document.getElementById('update-pct-text').textContent = pct + '%';
    document.getElementById('update-stage-text').textContent = STAGE_LABELS[stage] || stage;

    if (stage === 'error') {
      document.getElementById('update-bar').style.background = 'var(--danger)';
      document.getElementById('update-stage-text').textContent = 'Error: ' + (d.error || 'unknown');
      document.getElementById('btn-check').disabled = false;
      toast('Update failed: ' + (d.error || 'unknown'), true);
      return;
    }
    if (stage === 'done') {
      document.getElementById('update-bar').style.background = 'var(--accent)';
      document.getElementById('btn-check').disabled = false;
      toast('Update installed — restart to apply');
      return;
    }
    // Keep polling every 1.5s
    updatePollTimer = setTimeout(pollUpdateStatus, 1500);
  } catch (e) {
    // Network glitch — retry
    updatePollTimer = setTimeout(pollUpdateStatus, 3000);
  }
}

async function loadSystemInfo() {
  document.getElementById('sys-info').innerHTML = 'Loading...';
  try {
    const r = await fetch('/api/wifi/status');
    const d = await r.json();
    document.getElementById('sys-info').innerHTML =
      `<p>WiFi: ${d.connected ? 'Connected to ' + esc(d.ssid) : 'Not connected'}</p>
       <p>Mode: Access Point</p>`;
  } catch (e) {
    document.getElementById('sys-info').innerHTML = 'Error loading info';
  }
}

async function downloadBackup() {
  try {
    const r = await fetch('/api/backup');
    const blob = await r.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = 'dubsiren-backup.json'; a.click();
    URL.revokeObjectURL(url);
    toast('Backup downloaded');
  } catch (e) { toast('Backup failed', true); }
}

async function restoreBackup() {
  const file = document.getElementById('restore-file').files[0];
  if (!file) { toast('Select a file', true); return; }
  const data = await file.text();
  try {
    const r = await fetch('/api/restore', { method: 'POST',
      headers: { 'Content-Type': 'application/json' }, body: data });
    if (r.ok) { toast('Restored'); loadPresets(); loadOptions(); }
    else toast('Restore failed', true);
  } catch (e) { toast('Error: ' + e, true); }
}

async function exitAP() {
  if (!confirm('Return to siren mode?')) return;
  try {
    await fetch('/api/exit', { method: 'POST' });
    toast('Returning to siren mode...');
    setTimeout(() => {
      document.body.innerHTML =
        '<div class="exit-screen"><h2>Siren Mode Active</h2>' +
        '<p>You can disconnect from this network.</p></div>';
    }, 1000);
  } catch (e) { toast('Error: ' + e, true); }
}

// ─── Theme ──────────────────────────────────────────────────────────
const THEMES = [
  { id:'midnight',  name:'Midnight',  colors:['#0a0a0a','#ff6600','#00ff88','#ccc'] },
  { id:'deep-dub',  name:'Deep Dub',  colors:['#05080f','#00ccff','#00ff88','#8899bb'] },
  { id:'reggae',    name:'Reggae',    colors:['#0a0f06','#ffcc00','#00ff44','#b0c890'] },
  { id:'steppers',  name:'Steppers',  colors:['#0f0808','#ff2222','#ff4444','#cc9999'] },
  { id:'silver',    name:'Silver',    colors:['#f0f0f0','#ff6600','#00cc66','#444'] },
  { id:'concrete',  name:'Concrete',  colors:['#e8e4e0','#2a6b4f','#2a9b5f','#4a4640'] },
];

function renderThemeGrid() {
  const cur = document.documentElement.getAttribute('data-theme') || 'midnight';
  document.getElementById('theme-grid').innerHTML = THEMES.map(t =>
    `<div class="theme-swatch${t.id === cur ? ' active' : ''}" onclick="setTheme('${t.id}')">
       <div class="swatch-bar">${t.colors.map(c => `<span style="background:${c}"></span>`).join('')}</div>
       <div class="swatch-label">${t.name}</div>
     </div>`
  ).join('');
}

function setTheme(id) {
  document.documentElement.setAttribute('data-theme', id);
  try { localStorage.setItem('dubsiren-theme', id); } catch(e) {}
  renderThemeGrid();
}

// Apply saved theme on load (before paint)
(function() {
  try {
    const saved = localStorage.getItem('dubsiren-theme');
    if (saved) document.documentElement.setAttribute('data-theme', saved);
    else document.documentElement.setAttribute('data-theme', 'midnight');
  } catch(e) {
    document.documentElement.setAttribute('data-theme', 'midnight');
  }
})();

// ─── Init ───────────────────────────────────────────────────────────
loadPresets();
loadOptions();
renderThemeGrid();
</script>
</body>
</html>
)HTML";

} // namespace web_ui
