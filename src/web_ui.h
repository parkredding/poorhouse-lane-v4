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
  --danger:#ff3333;
  --font:'Space Mono',monospace;
}
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

/* ── Preset Loader (dropdown-based) ────────────────────── */
.preset-loader{background:var(--surface);border:1px solid var(--border);
  padding:16px;margin-bottom:16px}
.preset-loader h3{font-size:0.7rem;font-weight:700;color:var(--accent);
  letter-spacing:2px;text-transform:uppercase;margin-bottom:12px}
.loader-row{display:flex;gap:8px;flex-wrap:wrap;align-items:flex-end}
.loader-group{flex:1;min-width:120px}
.loader-group label{display:block;font-size:0.55rem;font-weight:700;
  color:var(--text-lo);letter-spacing:2px;text-transform:uppercase;
  margin-bottom:4px}
.loader-group select{width:100%;padding:8px;font-family:var(--font);
  font-size:0.75rem;background:var(--bg);color:var(--text-hi);
  border:1px solid var(--border);outline:none}
.loader-group select:focus{border-color:var(--accent)}
.loader-actions{display:flex;gap:6px;padding-bottom:1px}

/* Desktop: loader row is horizontal */
@media (min-width: 768px) {
  .loader-row{flex-wrap:nowrap}
  .loader-group{min-width:0}
}
/* Mobile: loader wraps */
@media (max-width: 767px) {
  .loader-group{min-width:100%}
  .loader-actions{width:100%;margin-top:4px}
  .loader-actions .btn{flex:1}
}

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
.btn-primary:hover{background:#e55a00}
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
.network-item:hover{background:#1a1a1a}
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

  <!-- Preset Loader -->
  <div class="preset-loader">
    <h3>Load Preset</h3>
    <div class="loader-row">
      <div class="loader-group">
        <label>Category</label>
        <select id="load-category" onchange="onCategoryChange()">
          <option value="">-- Select --</option>
        </select>
      </div>
      <div class="loader-group">
        <label>Preset</label>
        <select id="load-preset">
          <option value="">-- Select --</option>
        </select>
      </div>
      <div class="loader-group">
        <label>Target Bank</label>
        <select id="load-bank" onchange="onTargetBankChange()">
          <option value="user">User</option>
          <option value="standard">Standard</option>
          <option value="experimental">Experimental</option>
        </select>
      </div>
      <div class="loader-group">
        <label>Slot</label>
        <select id="load-slot">
          <option value="0">1</option>
          <option value="1">2</option>
          <option value="2">3</option>
          <option value="3">4</option>
        </select>
      </div>
      <div class="loader-actions">
        <button class="btn btn-primary btn-sm" onclick="doLoadPreset()">Load</button>
        <button class="btn btn-secondary btn-sm" onclick="doPreviewPreset()">Preview</button>
      </div>
    </div>
  </div>

  <!-- Bank Grids -->
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
    <h3>Modulation FX</h3>
    <div class="toggle-row">
      <span>Phaser</span>
      <label class="toggle"><input type="checkbox" id="phaser"><span class="slider"></span></label>
    </div>
    <div class="toggle-row">
      <span>Chorus</span>
      <label class="toggle"><input type="checkbox" id="chorus"><span class="slider"></span></label>
    </div>
    <div class="toggle-row">
      <span>Flanger</span>
      <label class="toggle"><input type="checkbox" id="flanger"><span class="slider"></span></label>
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
    <p style="color:var(--text-lo);margin-bottom:12px;font-size:0.75rem">Version 0.5.0</p>
    <button class="btn btn-secondary" onclick="checkUpdate()">Check for Updates</button>
    <div id="update-result" style="margin-top:12px"></div>
  </div>
  <div class="card">
    <h3>Device</h3>
    <div id="sys-info" style="font-size:0.8rem;color:var(--text-lo)">Loading...</div>
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
const BANK_NAMES = ['user', 'standard', 'experimental'];

// ─── Tab Navigation ─────────────────────────────────────────────────
function showTab(id) {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  const tabs = ['presets','options','wifi','system'];
  const tabEls = document.querySelectorAll('.tab');
  tabs.forEach((t, i) => { if (t === id && tabEls[i]) tabEls[i].classList.add('active'); });
  if (id === 'system') loadSystemInfo();
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
function onReverbTypeChange() {
  // No extra logic needed — value captured on apply
}
function onDelayTypeChange() {
  const val = parseInt(document.getElementById('delay-type').value);
  document.getElementById('tape-params').style.display = val === 0 ? 'block' : 'none';
}

function updateRange(id) {
  document.getElementById(id + '-val').textContent = document.getElementById(id).value + '%';
}

// ─── Bank Slot Rendering ────────────────────────────────────────────
function renderBankSlots(bankName, presets, gridId) {
  const grid = document.getElementById(gridId);
  const bankIdx = BANK_NAMES.indexOf(bankName);
  grid.innerHTML = presets.map((p, i) => {
    const isActive = activeBank === bankIdx && activePreset === i;
    return `<div class="slot${isActive ? ' active-slot' : ''}" id="slot-${bankName}-${i}">
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
        <input type="text" placeholder="Name" id="save-name-${bankName}-${i}" value="${esc(p.name || '')}" onkeydown="if(event.key==='Enter')doSave('${bankName}',${i})">
        <button onclick="doSave('${bankName}',${i})">OK</button>
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

// ─── Preset Loader (dropdown-based) ─────────────────────────────────
function buildCategoryDropdown(data) {
  libraryByCategory = {};
  (data.library || []).forEach(p => {
    const cat = p.category || 'Other';
    if (!libraryByCategory[cat]) libraryByCategory[cat] = [];
    libraryByCategory[cat].push(p);
  });

  const sel = document.getElementById('load-category');
  sel.innerHTML = '<option value="">-- Select Category --</option>';
  Object.keys(libraryByCategory).forEach(cat => {
    const opt = document.createElement('option');
    opt.value = cat;
    opt.textContent = cat + ' (' + libraryByCategory[cat].length + ')';
    sel.appendChild(opt);
  });
  // Clear preset dropdown
  document.getElementById('load-preset').innerHTML = '<option value="">-- Select Category First --</option>';
}

function onCategoryChange() {
  const cat = document.getElementById('load-category').value;
  const sel = document.getElementById('load-preset');
  sel.innerHTML = '';
  if (!cat || !libraryByCategory[cat]) {
    sel.innerHTML = '<option value="">-- Select Category First --</option>';
    return;
  }
  libraryByCategory[cat].forEach(p => {
    const opt = document.createElement('option');
    opt.value = p.index;
    opt.textContent = p.name;
    sel.appendChild(opt);
  });
}

function onTargetBankChange() {
  // All banks have 4 slots, so no change needed
}

async function doLoadPreset() {
  const category = document.getElementById('load-category').value;
  const presetSel = document.getElementById('load-preset');
  const index = presetSel.value;
  const bank = document.getElementById('load-bank').value;
  const slot = document.getElementById('load-slot').value;

  if (!category || index === '') { toast('Select a preset first', true); return; }

  try {
    const r = await fetch('/api/presets/bank/load', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `bank=${bank}&slot=${slot}&category=library&index=${index}` });
    if (r.ok) {
      const name = presetSel.options[presetSel.selectedIndex].textContent;
      toast('Loaded "' + name + '" to ' + bank + ' slot ' + (parseInt(slot) + 1));
      loadPresets();
    } else toast('Load failed', true);
  } catch (e) { toast('Error: ' + e, true); }
}

async function doPreviewPreset() {
  const category = document.getElementById('load-category').value;
  const index = document.getElementById('load-preset').value;
  if (!category || index === '') { toast('Select a preset first', true); return; }

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
    document.getElementById('phaser').checked = !!d.phaser;
    document.getElementById('chorus').checked = !!d.chorus;
    document.getElementById('flanger').checked = !!d.flanger;
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
    'phaser=' + (document.getElementById('phaser').checked ? '1' : '0'),
    'chorus=' + (document.getElementById('chorus').checked ? '1' : '0'),
    'flanger=' + (document.getElementById('flanger').checked ? '1' : '0')
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
async function checkUpdate() {
  document.getElementById('update-result').innerHTML =
    '<div class="loading"><div class="spinner"></div> Checking...</div>';
  try {
    const r = await fetch('/api/update/check', { method: 'POST' });
    const d = await r.json();
    if (d.available) {
      document.getElementById('update-result').innerHTML =
        `<p style="color:var(--accent);font-size:0.8rem">${d.count} update(s) available</p>
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
  document.getElementById('update-result').innerHTML =
    '<div class="loading"><div class="spinner"></div> Installing...</div>';
  try {
    const r = await fetch('/api/update/install', { method: 'POST' });
    if (r.ok) toast('Update installed — restart to apply');
    else toast('Update failed', true);
  } catch (e) { toast('Error: ' + e, true); }
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

// ─── Init ───────────────────────────────────────────────────────────
loadPresets();
loadOptions();
</script>
</body>
</html>
)HTML";

} // namespace web_ui
