#pragma once

// Embedded web UI for the dub siren configuration portal
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

/* Themes */
[data-theme="midnight"]{--bg:#0a0a0a;--surface:#141414;--border:#2a2a2a;--accent:#ff6600;--accent-dim:#993d00;--led-green:#00ff88;--led-off:#1a1a1a;--text:#ccc;--text-hi:#fff;--text-lo:#555;--danger:#ff3333;--card:#141414}
[data-theme="deep-dub"]{--bg:#05080f;--surface:#0c1220;--border:#1a2744;--accent:#00ccff;--accent-dim:#007799;--led-green:#00ff88;--led-off:#0a1020;--text:#8899bb;--text-hi:#ccddef;--text-lo:#3a4a66;--danger:#ff4466;--card:#0c1220}
[data-theme="reggae"]{--bg:#0a0f06;--surface:#141e0c;--border:#2a3a18;--accent:#ffcc00;--accent-dim:#997a00;--led-green:#00ff44;--led-off:#111a08;--text:#b0c890;--text-hi:#e0f0c0;--text-lo:#4a5a38;--danger:#ff4444;--card:#141e0c}
[data-theme="steppers"]{--bg:#0f0808;--surface:#1a1010;--border:#332020;--accent:#ff2222;--accent-dim:#991414;--led-green:#ff4444;--led-off:#1a0e0e;--text:#cc9999;--text-hi:#ffdddd;--text-lo:#664444;--danger:#ff6644;--card:#1a1010}
[data-theme="silver"]{--bg:#f0f0f0;--surface:#ffffff;--border:#d0d0d0;--accent:#ff6600;--accent-dim:#cc5200;--led-green:#00cc66;--led-off:#e0e0e0;--text:#444;--text-hi:#111;--text-lo:#999;--danger:#dd2222;--card:#ffffff}
[data-theme="concrete"]{--bg:#e8e4e0;--surface:#f5f2ee;--border:#c8c4c0;--accent:#2a6b4f;--accent-dim:#1a4a35;--led-green:#2a9b5f;--led-off:#d8d4d0;--text:#4a4640;--text-hi:#1a1816;--text-lo:#9a9690;--danger:#cc3333;--card:#f5f2ee}
[data-theme="roots"]{--bg:#0e0a06;--surface:#1a1408;--border:#302410;--accent:#cc9933;--accent-dim:#8a6622;--led-green:#66cc44;--led-off:#14100a;--text:#aa9070;--text-hi:#ddccaa;--text-lo:#554433;--danger:#cc4422;--card:#1a1408}
[data-theme="irie"]{--bg:#0a060e;--surface:#140e1a;--border:#28183a;--accent:#bb44ff;--accent-dim:#7722aa;--led-green:#44ff88;--led-off:#100a16;--text:#9980bb;--text-hi:#ddccff;--text-lo:#443366;--danger:#ff4466;--card:#140e1a}

/* Theme Picker */
.theme-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(100px,1fr));gap:8px}
.theme-swatch{position:relative;cursor:pointer;border:2px solid var(--border);padding:10px 8px;text-align:center;transition:all 0.2s}
.theme-swatch:hover{border-color:var(--text-lo)}
.theme-swatch.active{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent)}
.theme-swatch .swatch-bar{height:6px;display:flex;gap:2px;margin-bottom:8px}
.theme-swatch .swatch-bar span{flex:1;display:block}
.theme-swatch .swatch-label{font-size:0.55rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;color:var(--text-lo)}
.theme-swatch.active .swatch-label{color:var(--accent)}

html{font-size:14px}
body{font-family:var(--font);background:var(--bg);color:var(--text);min-height:100vh;-webkit-tap-highlight-color:transparent;-webkit-font-smoothing:antialiased}

.header{display:flex;justify-content:space-between;align-items:center;padding:12px 16px;border-bottom:2px solid var(--accent)}
.header .brand{font-size:0.85rem;font-weight:700;color:var(--text-hi);letter-spacing:3px;text-transform:uppercase}
.header .sub{display:flex;align-items:center;gap:8px;font-size:0.7rem;color:var(--text-lo);letter-spacing:2px;text-transform:uppercase}
.conn-dot{width:8px;height:8px;border-radius:50%;background:var(--led-green);flex-shrink:0;transition:background 0.3s}
.conn-dot.offline{background:var(--danger);animation:blinker 1.5s ease-in-out infinite}
@keyframes blinker{0%,100%{opacity:1}50%{opacity:0.3}}

.offline-banner{display:none;background:var(--danger);color:#fff;text-align:center;padding:8px;font-size:0.7rem;font-weight:700;letter-spacing:2px;text-transform:uppercase}
.offline-banner.show{display:block}

.tabs{display:flex;border-bottom:1px solid var(--border);overflow-x:auto}
.tab{flex:1;padding:10px 4px;text-align:center;cursor:pointer;font-family:var(--font);font-size:0.6rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;color:var(--text-lo);background:var(--bg);border-right:1px solid var(--border);transition:all 0.15s;white-space:nowrap}
.tab:last-child{border-right:none}
.tab.active{color:var(--bg);background:var(--accent)}
.tab:not(.active):hover{color:var(--text)}

.content{padding:16px;margin:0 auto}
.panel{display:none}.panel.active{display:block}
@media(max-width:767px){.content{max-width:540px}}
@media(min-width:768px){.content{max-width:960px}}

.section{font-size:0.65rem;font-weight:700;color:var(--accent);letter-spacing:3px;text-transform:uppercase;padding:12px 0 8px;border-bottom:1px solid var(--border);margin-bottom:8px}

@media(min-width:768px){.banks-row{display:grid;grid-template-columns:1fr 1fr 1fr;gap:16px;margin-bottom:16px}.bank-col .slot-grid{grid-template-columns:1fr 1fr}}
@media(max-width:767px){.banks-row{display:block}.bank-col{margin-bottom:12px}}

.slot-grid{display:grid;grid-template-columns:1fr 1fr;gap:1px;background:var(--border);border:1px solid var(--border);margin-bottom:8px}
.slot{background:var(--surface);padding:10px;position:relative;min-height:100px;display:flex;flex-direction:column;transition:all 0.2s}
.slot.active-slot{border:2px solid var(--led-green)}
.slot.target-slot{border:2px solid var(--accent)}
.target-hint{font-size:0.65rem;color:var(--text-lo);letter-spacing:1px;text-transform:uppercase;padding:8px 0;text-align:center}
.lib-category{font-size:0.6rem;font-weight:700;color:var(--accent);letter-spacing:2px;text-transform:uppercase;padding:10px 0 6px;border-bottom:1px solid var(--border);margin-top:4px}
.lib-preset{display:flex;align-items:center;justify-content:space-between;padding:8px 6px;border-bottom:1px solid var(--border);transition:background 0.15s}
.lib-preset:hover{background:var(--border)}
.lib-preset .lib-name{font-size:0.8rem;color:var(--text-hi);flex:1}
.lib-preset .lib-actions{display:flex;gap:4px}
.lib-preset .lib-btn{font-family:var(--font);font-size:0.55rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;padding:4px 8px;background:var(--bg);color:var(--text-lo);border:1px solid var(--border);cursor:pointer;transition:all 0.15s}
.lib-preset .lib-btn:hover{color:var(--text-hi);border-color:var(--text-lo)}
.lib-preset .lib-btn.primary{background:var(--accent);color:var(--bg);border-color:var(--accent)}
.lib-preset .lib-btn.previewing{background:var(--danger);border-color:var(--danger);color:#fff}
.slot-num{display:flex;align-items:center;gap:6px;margin-bottom:6px}
.slot-num span{font-size:0.65rem;font-weight:700;color:var(--text-lo);letter-spacing:2px}
.led{width:6px;height:6px;border-radius:50%;background:var(--led-off);flex-shrink:0}
.led.active{background:var(--led-green);box-shadow:0 0 6px var(--led-green)}
.slot-name{font-size:0.85rem;font-weight:700;color:var(--text-hi);margin-bottom:2px;word-break:break-word;flex:1}
.slot-cat{font-size:0.55rem;color:var(--text-lo);letter-spacing:1px;text-transform:uppercase;margin-bottom:6px}
.slot-actions{display:flex;gap:3px;margin-top:auto;flex-wrap:wrap}
.slot-btn{font-family:var(--font);font-size:0.55rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;padding:3px 6px;background:var(--bg);color:var(--text-lo);border:1px solid var(--border);cursor:pointer;transition:all 0.15s}
.slot-btn:hover{color:var(--text-hi);border-color:var(--text-lo)}
.slot-btn.active{color:var(--accent);border-color:var(--accent)}
.save-inline{display:none;margin-top:6px}
.save-inline.show{display:flex;gap:4px}
.save-inline input{font-family:var(--font);font-size:0.7rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--accent);padding:3px 6px;flex:1;outline:none;min-width:0}
.save-inline button{font-family:var(--font);font-size:0.55rem;font-weight:700;background:var(--accent);color:var(--bg);border:none;padding:3px 8px;cursor:pointer;letter-spacing:1px}
.preset-loader{background:var(--surface);border:1px solid var(--border);padding:16px;margin-bottom:16px;max-height:60vh;overflow-y:auto}
.preset-loader h3{font-size:0.7rem;font-weight:700;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:12px}

.card{background:var(--surface);border:1px solid var(--border);padding:16px;margin-bottom:8px}
.card h3{font-size:0.7rem;font-weight:700;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:12px}
.form-group{margin-bottom:12px}
.form-group label{display:block;font-size:0.6rem;font-weight:700;color:var(--text-lo);letter-spacing:2px;text-transform:uppercase;margin-bottom:4px}
.form-group input,.form-group select{width:100%;padding:8px;font-family:var(--font);font-size:0.8rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);outline:none}
.form-group input:focus,.form-group select:focus{border-color:var(--accent)}
.form-group input[type=range]{padding:4px 0;-webkit-appearance:none;background:transparent}
input[type=range]::-webkit-slider-runnable-track{height:4px;background:var(--border)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:14px;height:14px;background:var(--accent);margin-top:-5px;cursor:pointer}
input[type=range]::-moz-range-track{height:4px;background:var(--border);border:none}
input[type=range]::-moz-range-thumb{width:14px;height:14px;background:var(--accent);border:none;cursor:pointer}

.seg-ctrl{display:flex;border:1px solid var(--border)}
.seg-btn{flex:1;padding:8px 4px;text-align:center;cursor:pointer;font-family:var(--font);font-size:0.65rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;color:var(--text-lo);background:var(--bg);border-right:1px solid var(--border);transition:all 0.15s}
.seg-btn:last-child{border-right:none}
.seg-btn.active{color:var(--bg);background:var(--accent)}
.seg-btn:not(.active):hover{color:var(--text)}

.toggle-row{display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid var(--border)}
.toggle-row span{font-size:0.7rem;letter-spacing:1px;text-transform:uppercase}
.toggle{position:relative;width:36px;height:18px;flex-shrink:0}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;cursor:pointer;inset:0;background:var(--border);transition:0.2s}
.toggle .slider:before{content:'';position:absolute;height:12px;width:12px;left:3px;bottom:3px;background:var(--text-lo);transition:0.2s}
.toggle input:checked+.slider{background:var(--accent)}
.toggle input:checked+.slider:before{transform:translateX(18px);background:var(--bg)}
.range-val{color:var(--accent);font-weight:700;font-size:0.75rem;min-width:36px;display:inline-block;text-align:right}

.btn{font-family:var(--font);font-size:0.7rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;padding:10px 20px;border:none;cursor:pointer;transition:all 0.15s}
.btn:disabled{opacity:0.5;cursor:not-allowed}
.btn-sm{font-size:0.6rem;padding:8px 12px}
.btn-primary{background:var(--accent);color:var(--bg)}
.btn-primary:hover:not(:disabled){background:var(--accent-dim)}
.btn-secondary{background:var(--bg);color:var(--text);border:1px solid var(--border)}
.btn-secondary:hover:not(:disabled){border-color:var(--text-lo)}
.btn-danger{background:var(--danger);color:var(--bg)}
.btn-danger:hover:not(:disabled){background:#cc2222}
.btn-block{width:100%;display:block}

.network-list{max-height:200px;overflow-y:auto}
.network-item{display:flex;justify-content:space-between;align-items:center;padding:8px 4px;cursor:pointer;border-bottom:1px solid var(--border);font-size:0.8rem}
.network-item:hover{background:var(--border)}
.signal{color:var(--text-lo);font-size:0.7rem}

.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);font-family:var(--font);font-size:0.75rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;background:var(--accent);color:var(--bg);padding:10px 24px;opacity:0;transition:opacity 0.3s;z-index:999}
.toast.show{opacity:1}
.loading{text-align:center;padding:20px;color:var(--text-lo)}
.spinner{display:inline-block;width:16px;height:16px;border:2px solid var(--border);border-top-color:var(--accent);animation:spin 0.8s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}

.exit-screen{text-align:center;padding:60px 20px}
.exit-screen h2{color:var(--accent);font-size:1rem;letter-spacing:3px;text-transform:uppercase;margin-bottom:8px}
.exit-screen p{color:var(--text-lo);font-size:0.8rem}

/* Live DSP */
.dsp-group{margin-bottom:12px}
.dsp-group h4{font-size:0.6rem;font-weight:700;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:6px;padding-bottom:4px;border-bottom:1px solid var(--border)}
.dsp-slider{display:flex;align-items:center;gap:8px;padding:4px 0}
.dsp-slider label{font-size:0.6rem;font-weight:700;color:var(--text-lo);letter-spacing:1px;text-transform:uppercase;min-width:70px;flex-shrink:0}
.dsp-slider input[type=range]{flex:1}
.dsp-slider .dsp-val{font-size:0.65rem;color:var(--accent);font-weight:700;min-width:55px;text-align:right;flex-shrink:0}

/* Encoder mapping */
.enc-table{width:100%;border-collapse:collapse;font-size:0.7rem}
.enc-table th{font-size:0.6rem;font-weight:700;color:var(--accent);letter-spacing:1px;text-transform:uppercase;padding:6px 4px;border-bottom:1px solid var(--border);text-align:left}
.enc-table td{padding:6px 4px;border-bottom:1px solid var(--border)}
.enc-table select{font-family:var(--font);font-size:0.7rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px;width:100%}

/* System stats */
.sys-stat{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--border);font-size:0.75rem}
.sys-stat .sys-label{color:var(--text-lo);text-transform:uppercase;letter-spacing:1px;font-size:0.6rem;font-weight:700}
.sys-stat .sys-value{color:var(--text-hi);font-weight:700}
</style>
</head>
<body>
<div class="header">
  <div class="brand">Poorhouse Lane</div>
  <div class="sub"><div class="conn-dot" id="conn-dot"></div> Siren Config</div>
</div>

<div class="offline-banner" id="offline-banner">Connection Lost &mdash; Reconnecting...</div>

<div class="tabs">
  <div class="tab" onclick="showTab('live')">Live</div>
  <div class="tab active" onclick="showTab('presets')">Presets</div>
  <div class="tab" onclick="showTab('options')">Options</div>
  <div class="tab" onclick="showTab('wifi')">WiFi</div>
  <div class="tab" onclick="showTab('system')">System</div>
</div>

<div class="content">

<!-- LIVE TAB -->
<div id="live" class="panel">
  <div class="card">
    <h3>Sound Controls</h3>
    <div class="dsp-group">
      <h4>Oscillator</h4>
      <div class="dsp-slider">
        <label>Waveform</label>
        <select id="dsp-waveform" onchange="setDspParam('waveform',this.value)" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="0">Sine</option><option value="1">Square</option><option value="2">Saw</option><option value="3">Triangle</option>
        </select>
      </div>
      <div class="dsp-slider">
        <label>Frequency</label>
        <input type="range" id="dsp-freq" min="30" max="8000" value="440" oninput="onDspSlider('freq',this.value)">
        <span class="dsp-val" id="dsp-freq-val">440 Hz</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>LFO</h4>
      <div class="dsp-slider">
        <label>Shape</label>
        <select id="dsp-lfo-waveform" onchange="setDspParam('lfo_waveform',this.value)" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="0">Sine</option><option value="1">Triangle</option><option value="2">Square</option>
          <option value="3">Ramp Up</option><option value="4">Ramp Down</option><option value="5">S&amp;H</option>
          <option value="6">Exp Rise</option><option value="7">Exp Fall</option>
        </select>
      </div>
      <div class="dsp-slider">
        <label>Rate</label>
        <input type="range" id="dsp-lfo_rate" min="1" max="200" value="35" oninput="onDspSliderLog('lfo_rate',this.value,0.1,20)">
        <span class="dsp-val" id="dsp-lfo_rate-val">0.35 Hz</span>
      </div>
      <div class="dsp-slider">
        <label>Depth</label>
        <input type="range" id="dsp-lfo_depth" min="0" max="100" value="35" oninput="onDspSliderPct('lfo_depth',this.value)">
        <span class="dsp-val" id="dsp-lfo_depth-val">35%</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Filter</h4>
      <div class="dsp-slider">
        <label>Cutoff</label>
        <input type="range" id="dsp-filter_cutoff" min="0" max="1000" value="500" oninput="onDspSliderLog('filter_cutoff',this.value,20,20000)">
        <span class="dsp-val" id="dsp-filter_cutoff-val">8000 Hz</span>
      </div>
      <div class="dsp-slider">
        <label>Resonance</label>
        <input type="range" id="dsp-filter_reso" min="0" max="95" value="0" oninput="onDspSliderPct('filter_reso',this.value,0.95)">
        <span class="dsp-val" id="dsp-filter_reso-val">0%</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Delay</h4>
      <div class="dsp-slider">
        <label>Time</label>
        <input type="range" id="dsp-delay_time" min="0" max="1000" value="375" oninput="onDspSliderLog('delay_time',this.value,0.001,1.0)">
        <span class="dsp-val" id="dsp-delay_time-val">375 ms</span>
      </div>
      <div class="dsp-slider">
        <label>Feedback</label>
        <input type="range" id="dsp-delay_feedback" min="0" max="95" value="55" oninput="onDspSliderPct('delay_feedback',this.value,0.95)">
        <span class="dsp-val" id="dsp-delay_feedback-val">55%</span>
      </div>
      <div class="dsp-slider">
        <label>Mix</label>
        <input type="range" id="dsp-delay_mix" min="0" max="100" value="30" oninput="onDspSliderPct('delay_mix',this.value)">
        <span class="dsp-val" id="dsp-delay_mix-val">30%</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Envelope</h4>
      <div class="dsp-slider">
        <label>Release</label>
        <input type="range" id="dsp-release_time" min="0" max="1000" value="50" oninput="onDspSliderLog('release_time',this.value,0.01,5.0)">
        <span class="dsp-val" id="dsp-release_time-val">50 ms</span>
      </div>
      <div class="dsp-slider">
        <label>Pitch Env</label>
        <div class="seg-ctrl" style="flex:1">
          <div class="seg-btn" onclick="setDspParam('pitch_env',-1);updatePitchEnvUI(-1)">Fall</div>
          <div class="seg-btn active" onclick="setDspParam('pitch_env',0);updatePitchEnvUI(0)">Off</div>
          <div class="seg-btn" onclick="setDspParam('pitch_env',1);updatePitchEnvUI(1)">Rise</div>
        </div>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Reverb</h4>
      <div class="dsp-slider">
        <label>Mix</label>
        <input type="range" id="dsp-reverb_mix" min="0" max="100" value="35" oninput="onDspSliderPct('reverb_mix',this.value)">
        <span class="dsp-val" id="dsp-reverb_mix-val">35%</span>
      </div>
    </div>
    <div style="display:flex;gap:8px;margin-top:12px">
      <button class="btn btn-secondary" onclick="loadDspState()" style="flex:1">Refresh</button>
      <button class="btn btn-danger" onclick="confirmAction('Reset all parameters to factory defaults?',resetDefaults)" style="flex:1">Reset Defaults</button>
    </div>
  </div>
</div>

<!-- PRESETS TAB -->
<div id="presets" class="panel active">
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
  <div id="library-browser" class="preset-loader" style="display:none">
    <h3>Preset Library <span id="target-label" style="color:var(--text-lo);font-size:0.6rem;letter-spacing:1px"></span></h3>
    <div id="library-list"></div>
  </div>
</div>

<!-- OPTIONS TAB -->
<div id="options" class="panel">
  <div class="card">
    <h3>Reverb Type</h3>
    <div class="form-group">
      <select id="reverb-type" onchange="onReverbTypeChange()">
        <option value="0">Spring</option><option value="1">Plate</option>
        <option value="2">Hall</option><option value="3">Schroeder</option>
      </select>
    </div>
  </div>
  <div class="card">
    <h3>Delay Type</h3>
    <div class="form-group">
      <select id="delay-type" onchange="onDelayTypeChange()">
        <option value="0">Tape</option><option value="1">Digital</option>
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
    <div class="toggle-row"><span>Linked Pitch / LFO</span><label class="toggle"><input type="checkbox" id="lfo-link" checked><span class="slider"></span></label></div>
    <div class="toggle-row"><span>Super Drip Reverb</span><label class="toggle"><input type="checkbox" id="super-drip" checked><span class="slider"></span></label></div>
    <div class="toggle-row"><span>Filter Sweep</span>
      <select id="sweep-dir" style="width:auto;padding:6px;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border)">
        <option value="-1">Down (Dub)</option><option value="0">Flat</option><option value="1">Up (Bright)</option>
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
    <div class="form-group"><label>Phaser <span class="range-val" id="phaser-mix-val">0%</span></label><input type="range" id="phaser-mix" min="0" max="100" value="0" oninput="updateRange('phaser-mix')"></div>
    <div class="form-group"><label>Chorus <span class="range-val" id="chorus-mix-val">0%</span></label><input type="range" id="chorus-mix" min="0" max="100" value="0" oninput="updateRange('chorus-mix')"></div>
    <div class="form-group"><label>Flanger <span class="range-val" id="flanger-mix-val">0%</span></label><input type="range" id="flanger-mix" min="0" max="100" value="0" oninput="updateRange('flanger-mix')"></div>
  </div>
  <div class="card">
    <h3>Encoder Mapping</h3>
    <table class="enc-table" id="enc-table"><tr><td>Loading...</td></tr></table>
    <div style="display:flex;gap:8px;margin-top:8px">
      <button class="btn btn-primary btn-sm" onclick="saveEncoderMap()">Save Mapping</button>
      <button class="btn btn-secondary btn-sm" onclick="resetEncoderMap()">Reset Default</button>
    </div>
  </div>
  <button class="btn btn-primary btn-block" onclick="applyOptions()" style="margin-top:8px">Apply Options</button>
</div>

<!-- WIFI TAB -->
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

<!-- SYSTEM TAB -->
<div id="system" class="panel">
  <div class="card">
    <h3>Device Status</h3>
    <div id="sys-stats">
      <div class="sys-stat"><span class="sys-label">CPU Temp</span><span class="sys-value" id="sys-cpu">--</span></div>
      <div class="sys-stat"><span class="sys-label">Uptime</span><span class="sys-value" id="sys-uptime">--</span></div>
      <div class="sys-stat"><span class="sys-label">Memory</span><span class="sys-value" id="sys-mem">--</span></div>
      <div class="sys-stat"><span class="sys-label">WiFi</span><span class="sys-value" id="sys-wifi">--</span></div>
    </div>
    <div style="display:flex;gap:8px;margin-top:12px">
      <button class="btn btn-secondary btn-sm" onclick="confirmAction('Restart the siren service?',restartService)" style="flex:1">Restart Siren</button>
      <button class="btn btn-danger btn-sm" onclick="confirmAction('Reboot the device? This will take ~30 seconds.',rebootDevice)" style="flex:1">Reboot Device</button>
    </div>
  </div>
  <div class="card">
    <h3>Updates</h3>
    <p style="color:var(--text-lo);margin-bottom:8px;font-size:0.75rem">Version 0.6.0</p>
    <div class="form-group">
      <label>Branch</label>
      <select id="update-branch" style="width:100%;padding:8px;border:1px solid var(--border);background:var(--bg);color:var(--text);font-size:0.85rem">
        <option value="main">main</option>
      </select>
    </div>
    <button class="btn btn-secondary" onclick="checkUpdate()" id="btn-check">Check for Updates</button>
    <div id="update-result" style="margin-top:12px"></div>
    <div id="update-progress" style="display:none;margin-top:12px">
      <div style="font-size:0.8rem;color:var(--text-lo);margin-bottom:6px" id="update-stage-text">Preparing...</div>
      <div style="background:var(--card);border:1px solid var(--border);height:22px;overflow:hidden">
        <div id="update-bar" style="height:100%;width:0%;background:var(--accent);transition:width 0.4s ease"></div>
      </div>
      <div style="font-size:0.7rem;color:var(--text-lo);margin-top:4px;text-align:right" id="update-pct-text">0%</div>
    </div>
    <div id="update-log-section" style="display:none;margin-top:12px">
      <div style="display:flex;align-items:center;justify-content:space-between;cursor:pointer;padding:6px 0" onclick="toggleUpdateLog()">
        <span style="font-size:0.65rem;font-weight:700;color:var(--text-lo);letter-spacing:2px;text-transform:uppercase">Verbose Log <span id="log-arrow">&#9654;</span></span>
        <button class="btn btn-secondary btn-sm" onclick="event.stopPropagation();copyUpdateLog()" style="font-size:0.55rem;padding:4px 8px">Copy</button>
      </div>
      <pre id="update-log" style="display:none;background:var(--bg);border:1px solid var(--border);padding:10px;margin-top:4px;max-height:300px;overflow:auto;font-family:var(--font);font-size:0.65rem;line-height:1.5;color:var(--text);white-space:pre-wrap;word-break:break-all"></pre>
    </div>
    <div id="update-reboot" style="display:none;margin-top:12px">
      <button class="btn btn-danger" onclick="confirmAction('Reboot the device? This will take ~30 seconds.',rebootDevice)" style="width:100%">Reboot Device</button>
    </div>
  </div>
  <div class="card">
    <h3>Theme</h3>
    <div class="theme-grid" id="theme-grid"></div>
  </div>
  <div class="card">
    <h3>Backup &amp; Restore</h3>
    <button class="btn btn-secondary" onclick="downloadBackup()" style="margin-bottom:8px">Download Backup</button>
    <div class="form-group">
      <label>Restore from file</label>
      <input type="file" id="restore-file" accept=".json" style="font-size:0.75rem">
    </div>
    <button class="btn btn-secondary" onclick="confirmAction('Restore will overwrite all presets. Continue?',restoreBackup)">Restore</button>
  </div>
  <div class="card">
    <button class="btn btn-danger btn-block" onclick="exitAP()">Return to Siren Mode</button>
  </div>
</div>
</div>

<!-- Confirm Dialog -->
<div class="confirm-overlay" id="confirm-overlay" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,0.7);z-index:998;align-items:center;justify-content:center">
  <div style="background:var(--surface);border:2px solid var(--accent);padding:24px;max-width:320px;text-align:center">
    <p id="confirm-msg" style="font-size:0.8rem;color:var(--text-hi);margin-bottom:16px"></p>
    <div style="display:flex;gap:8px;justify-content:center">
      <button class="btn btn-secondary btn-sm" onclick="closeConfirm()">Cancel</button>
      <button class="btn btn-danger btn-sm" id="confirm-ok" onclick="doConfirm()">Confirm</button>
    </div>
  </div>
</div>

<div id="toast" class="toast"></div>

<script>
// State
let allPresets = {};
let libraryByCategory = {};
let activeBank = 0, activePreset = 0;
let targetBank = null, targetSlot = null;
const BANK_NAMES = ['user', 'standard', 'experimental'];
let isOnline = true;
let heartbeatTimer = null;
let reconnectDelay = 3000;
let previewingIndex = -1;
let confirmCallback = null;
let dspThrottle = {};
let encoderParams = [];

// Connection resilience
function startHeartbeat() {
  if (heartbeatTimer) clearInterval(heartbeatTimer);
  heartbeatTimer = setInterval(async () => {
    try {
      const r = await fetch('/api/dsp/state', {signal: AbortSignal.timeout(5000)});
      if (r.ok) { setOnline(true); reconnectDelay = 3000; }
      else setOnline(false);
    } catch(e) { setOnline(false); }
  }, 3000);
}
function setOnline(online) {
  if (isOnline === online) return;
  isOnline = online;
  document.getElementById('conn-dot').className = 'conn-dot' + (online ? '' : ' offline');
  document.getElementById('offline-banner').className = 'offline-banner' + (online ? '' : ' show');
  if (online) { loadPresets(); loadDspState(); toast('Reconnected'); }
}

// Confirm dialog
function confirmAction(msg, cb) {
  document.getElementById('confirm-msg').textContent = msg;
  document.getElementById('confirm-overlay').style.display = 'flex';
  confirmCallback = cb;
}
function closeConfirm() { document.getElementById('confirm-overlay').style.display = 'none'; confirmCallback = null; }
function doConfirm() { closeConfirm(); if (confirmCallback) confirmCallback(); }

// Debounced button helper
function debounceBtn(btn, ms) {
  if (!ms) ms = 500;
  btn.disabled = true;
  setTimeout(() => btn.disabled = false, ms);
}

// Tab Navigation
function showTab(id) {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  const tabs = ['live','presets','options','wifi','system'];
  const tabEls = document.querySelectorAll('.tab');
  tabs.forEach((t, i) => { if (t === id && tabEls[i]) tabEls[i].classList.add('active'); });
  if (id === 'live') loadDspState();
  if (id === 'wifi') { loadWifiStatus(); }
  if (id === 'system') { loadSystemInfo(); loadBranches(); }
  if (id === 'options') { loadEncoderMap(); }
}

// Toast
function toast(msg, err) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.style.background = err ? 'var(--danger)' : 'var(--accent)';
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2500);
}

function esc(s) { const d = document.createElement('div'); d.textContent = s; return d.innerHTML; }

// Live DSP Controls
async function loadDspState() {
  try {
    const r = await fetch('/api/dsp/state');
    const d = await r.json();
    // Sliders
    document.getElementById('dsp-freq').value = d.freq || 440;
    document.getElementById('dsp-freq-val').textContent = Math.round(d.freq) + ' Hz';
    setLogSlider('lfo_rate', d.lfo_rate, 0.1, 20);
    document.getElementById('dsp-lfo_rate-val').textContent = (d.lfo_rate||0).toFixed(2) + ' Hz';
    document.getElementById('dsp-lfo_depth').value = Math.round((d.lfo_depth||0)*100);
    document.getElementById('dsp-lfo_depth-val').textContent = Math.round((d.lfo_depth||0)*100)+'%';
    setLogSlider('filter_cutoff', d.filter_cutoff, 20, 20000);
    document.getElementById('dsp-filter_cutoff-val').textContent = Math.round(d.filter_cutoff)+' Hz';
    document.getElementById('dsp-filter_reso').value = Math.round((d.filter_reso||0)*100);
    document.getElementById('dsp-filter_reso-val').textContent = Math.round((d.filter_reso||0)*100)+'%';
    setLogSlider('delay_time', d.delay_time, 0.001, 1.0);
    document.getElementById('dsp-delay_time-val').textContent = Math.round((d.delay_time||0)*1000)+' ms';
    document.getElementById('dsp-delay_feedback').value = Math.round((d.delay_feedback||0)*100);
    document.getElementById('dsp-delay_feedback-val').textContent = Math.round((d.delay_feedback||0)*100)+'%';
    document.getElementById('dsp-delay_mix').value = Math.round((d.delay_mix||0)*100);
    document.getElementById('dsp-delay_mix-val').textContent = Math.round((d.delay_mix||0)*100)+'%';
    setLogSlider('release_time', d.release_time, 0.01, 5.0);
    document.getElementById('dsp-release_time-val').textContent = Math.round((d.release_time||0)*1000)+' ms';
    document.getElementById('dsp-reverb_mix').value = Math.round((d.reverb_mix||0)*100);
    document.getElementById('dsp-reverb_mix-val').textContent = Math.round((d.reverb_mix||0)*100)+'%';
    // Selects
    document.getElementById('dsp-waveform').value = d.waveform || 0;
    document.getElementById('dsp-lfo-waveform').value = d.lfo_waveform || 0;
    updatePitchEnvUI(d.pitch_env || 0);
  } catch(e) { console.error('loadDspState', e); }
}

function setLogSlider(name, value, min, max) {
  // Map log value to 0-1000 slider position
  const logMin = Math.log(min), logMax = Math.log(max);
  const pos = Math.round((Math.log(value) - logMin) / (logMax - logMin) * 1000);
  const el = document.getElementById('dsp-' + name);
  if (el) el.value = Math.max(0, Math.min(1000, pos));
}

function logSliderToValue(sliderVal, min, max) {
  const logMin = Math.log(min), logMax = Math.log(max);
  return Math.exp(logMin + (sliderVal / 1000) * (logMax - logMin));
}

function setDspParam(name, value) {
  fetch('/api/dsp/param', { method: 'POST',
    headers: {'Content-Type':'application/x-www-form-urlencoded'},
    body: 'name='+name+'&value='+value }).catch(e => {});
}

function throttledSetParam(name, value) {
  if (dspThrottle[name]) cancelAnimationFrame(dspThrottle[name]);
  dspThrottle[name] = requestAnimationFrame(() => { setDspParam(name, value); });
}

function onDspSlider(name, val) {
  val = parseFloat(val);
  document.getElementById('dsp-'+name+'-val').textContent = Math.round(val)+' Hz';
  throttledSetParam(name, val);
}

function onDspSliderLog(name, val, min, max) {
  const v = logSliderToValue(parseFloat(val), min, max);
  const el = document.getElementById('dsp-'+name+'-val');
  if (v >= 1) el.textContent = Math.round(v) + (max > 100 ? ' Hz' : ' ms');
  else if (v >= 0.01) el.textContent = (v*1000).toFixed(0) + ' ms';
  else el.textContent = v.toFixed(3);
  if (name === 'lfo_rate') el.textContent = v.toFixed(2) + ' Hz';
  if (name === 'release_time') el.textContent = Math.round(v*1000) + ' ms';
  if (name === 'delay_time') el.textContent = Math.round(v*1000) + ' ms';
  throttledSetParam(name, v);
}

function onDspSliderPct(name, val, maxVal) {
  const mx = maxVal || 1.0;
  const v = (parseFloat(val)/100) * mx;
  document.getElementById('dsp-'+name+'-val').textContent = Math.round(parseFloat(val))+'%';
  throttledSetParam(name, v);
}

function updatePitchEnvUI(val) {
  const btns = document.querySelectorAll('#live .seg-btn');
  btns.forEach((b,i) => b.classList.toggle('active', (i-1) === val));
}

async function resetDefaults() {
  try {
    const r = await fetch('/api/dsp/reset', {method:'POST'});
    if (r.ok) { toast('Reset to defaults'); loadDspState(); loadOptions(); }
    else toast('Reset failed', true);
  } catch(e) { toast('Error: '+e, true); }
}

// Options
function onReverbTypeChange() {}
function onDelayTypeChange() {
  document.getElementById('tape-params').style.display =
    parseInt(document.getElementById('delay-type').value) === 0 ? 'block' : 'none';
}
function updateRange(id) {
  document.getElementById(id+'-val').textContent = document.getElementById(id).value+'%';
}

// Presets
function selectTargetSlot(bank, slot) {
  if (targetBank === bank && targetSlot === slot) {
    targetBank = null; targetSlot = null;
    document.querySelectorAll('.slot.target-slot').forEach(el => el.classList.remove('target-slot'));
    document.getElementById('library-browser').style.display = 'none';
    document.getElementById('target-hint').textContent = 'Tap a slot to select it, then pick a preset below';
    return;
  }
  targetBank = bank; targetSlot = slot;
  document.querySelectorAll('.slot.target-slot').forEach(el => el.classList.remove('target-slot'));
  const el = document.getElementById('slot-'+bank+'-'+slot);
  if (el) el.classList.add('target-slot');
  document.getElementById('target-hint').textContent = 'Target: '+bank.toUpperCase()+' slot '+(slot+1)+' \u2014 pick a preset below';
  document.getElementById('target-label').textContent = '\u2192 '+bank.toUpperCase()+' SLOT '+(slot+1);
  document.getElementById('library-browser').style.display = 'block';
  renderLibrary();
}

function renderLibrary() {
  const list = document.getElementById('library-list');
  let html = '';
  Object.keys(libraryByCategory).forEach(cat => {
    html += '<div class="lib-category">'+esc(cat)+'</div>';
    libraryByCategory[cat].forEach(p => {
      const isPreviewing = previewingIndex === p.index;
      html += '<div class="lib-preset">'+
        '<span class="lib-name">'+esc(p.name)+'</span>'+
        '<div class="lib-actions">'+
          '<div class="lib-btn'+(isPreviewing?' previewing':'')+'" onclick="event.stopPropagation();doPreviewPreset('+p.index+')">'+(isPreviewing?'Stop':'Preview')+'</div>'+
          '<div class="lib-btn primary" onclick="event.stopPropagation();doLoadPreset('+p.index+',\''+esc(p.name)+'\')">Load</div>'+
        '</div></div>';
    });
  });
  list.innerHTML = html;
}

function renderBankSlots(bankName, presets, gridId) {
  const grid = document.getElementById(gridId);
  const bankIdx = BANK_NAMES.indexOf(bankName);
  grid.innerHTML = presets.map((p,i) => {
    const isActive = activeBank === bankIdx && activePreset === i;
    const isTarget = targetBank === bankName && targetSlot === i;
    return '<div class="slot'+(isActive?' active-slot':'')+(isTarget?' target-slot':'')+'" id="slot-'+bankName+'-'+i+'" onclick="selectTargetSlot(\''+bankName+'\','+i+')">'+
      '<div class="slot-num"><div class="led'+(isActive?' active':'')+'"></div><span>0'+(i+1)+'</span></div>'+
      '<div class="slot-name">'+esc(p.name||'Empty')+'</div>'+
      '<div class="slot-cat">'+(p.saved?'Saved':'Factory')+'</div>'+
      '<div class="slot-actions">'+
        '<div class="slot-btn'+(isActive?' active':'')+'" onclick="event.stopPropagation();applyBankPreset(\''+bankName+'\','+i+')">Apply</div>'+
        '<div class="slot-btn" onclick="event.stopPropagation();showSaveInput(\''+bankName+'\','+i+')">Save</div>'+
      '</div>'+
      '<div class="save-inline" id="save-inline-'+bankName+'-'+i+'">'+
        '<input type="text" placeholder="Name" id="save-name-'+bankName+'-'+i+'" value="'+esc(p.name||'')+'" onclick="event.stopPropagation()" onkeydown="if(event.key===\'Enter\')doSave(\''+bankName+'\','+i+')">'+
        '<button onclick="event.stopPropagation();doSave(\''+bankName+'\','+i+')">OK</button>'+
      '</div></div>';
  }).join('');
}

async function applyBankPreset(bank, index) {
  try {
    const r = await fetch('/api/presets/apply', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'category='+bank+'&index='+index});
    if (r.ok) { toast('Applied'); loadPresets(); loadDspState(); } else toast('Apply failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

function showSaveInput(bank, i) {
  document.querySelectorAll('.save-inline.show').forEach(el => el.classList.remove('show'));
  const el = document.getElementById('save-inline-'+bank+'-'+i);
  if (el) { el.classList.add('show'); el.querySelector('input').focus(); }
}

async function doSave(bank, slot) {
  const input = document.getElementById('save-name-'+bank+'-'+slot);
  const name = input ? input.value || 'Preset' : 'Preset';
  try {
    const r = await fetch('/api/presets/bank/save', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'bank='+bank+'&slot='+slot+'&name='+encodeURIComponent(name)});
    if (r.ok) { toast('Saved to '+bank+' slot '+(slot+1)); loadPresets(); } else toast('Save failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

function buildCategoryDropdown(data) {
  libraryByCategory = {};
  (data.library||[]).forEach(p => {
    const cat = p.category||'Other';
    if (!libraryByCategory[cat]) libraryByCategory[cat] = [];
    libraryByCategory[cat].push(p);
  });
  if (targetBank !== null) renderLibrary();
}

async function doLoadPreset(index, name) {
  if (targetBank === null || targetSlot === null) { toast('Select a target slot first',true); return; }
  try {
    const r = await fetch('/api/presets/bank/load', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'bank='+targetBank+'&slot='+targetSlot+'&category=library&index='+index});
    if (r.ok) { toast('Loaded "'+name+'" \u2192 '+targetBank.toUpperCase()+' slot '+(targetSlot+1)); loadPresets(); }
    else toast('Load failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

async function doPreviewPreset(index) {
  try {
    if (previewingIndex === index) {
      // Stop preview
      await fetch('/api/preview/stop', {method:'POST'});
      previewingIndex = -1;
      toast('Preview stopped');
      renderLibrary();
      return;
    }
    // Stop any existing preview first
    if (previewingIndex >= 0) await fetch('/api/preview/stop', {method:'POST'});
    const r = await fetch('/api/preview/start', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'category=library&index='+index});
    if (r.ok) { previewingIndex = index; toast('Previewing'); renderLibrary(); }
    else toast('Preview failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

async function loadPresets() {
  try {
    const r = await fetch('/api/presets');
    const d = await r.json();
    allPresets = d;
    activeBank = d.active_bank||0;
    activePreset = d.active_preset||0;
    renderBankSlots('user', d.user||[], 'slot-grid-user');
    renderBankSlots('standard', d.standard||[], 'slot-grid-standard');
    renderBankSlots('experimental', d.experimental||[], 'slot-grid-experimental');
    buildCategoryDropdown(d);
  } catch(e) { console.error(e); }
}

async function loadOptions() {
  try {
    const r = await fetch('/api/siren/options');
    const d = await r.json();
    document.getElementById('reverb-type').value = d.reverb_type||0;
    document.getElementById('delay-type').value = d.delay_type||0;
    onDelayTypeChange();
    document.getElementById('wobble').value = Math.round((d.tape_wobble||1)*100);
    document.getElementById('flutter').value = Math.round((d.tape_flutter||1)*100);
    updateRange('wobble'); updateRange('flutter');
    document.getElementById('fx-chain').value = d.fx_chain||0;
    document.getElementById('lfo-link').checked = d.lfo_pitch_link !== false;
    document.getElementById('super-drip').checked = d.super_drip !== false;
    document.getElementById('sweep-dir').value = d.sweep_dir||-1;
    document.getElementById('saturator-mix').value = Math.round((d.saturator_mix||0)*100);
    document.getElementById('saturator-drive').value = Math.round((d.saturator_drive!=null?d.saturator_drive:0.5)*100);
    updateRange('saturator-mix'); updateRange('saturator-drive');
    document.getElementById('phaser-mix').value = Math.round((d.phaser_mix||0)*100);
    document.getElementById('chorus-mix').value = Math.round((d.chorus_mix||0)*100);
    document.getElementById('flanger-mix').value = Math.round((d.flanger_mix||0)*100);
    updateRange('phaser-mix'); updateRange('chorus-mix'); updateRange('flanger-mix');
    // Sync theme from server
    if (d.theme) {
      document.documentElement.setAttribute('data-theme', d.theme);
      try { localStorage.setItem('dubsiren-theme', d.theme); } catch(e) {}
      renderThemeGrid();
    }
  } catch(e) { console.error(e); }
}

async function applyOptions() {
  const body = [
    'reverb_type='+document.getElementById('reverb-type').value,
    'delay_type='+document.getElementById('delay-type').value,
    'tape_wobble='+(document.getElementById('wobble').value/100),
    'tape_flutter='+(document.getElementById('flutter').value/100),
    'fx_chain='+document.getElementById('fx-chain').value,
    'lfo_pitch_link='+(document.getElementById('lfo-link').checked?'1':'0'),
    'super_drip='+(document.getElementById('super-drip').checked?'1':'0'),
    'sweep_dir='+document.getElementById('sweep-dir').value,
    'saturator_mix='+(document.getElementById('saturator-mix').value/100),
    'saturator_drive='+(document.getElementById('saturator-drive').value/100),
    'phaser_mix='+(document.getElementById('phaser-mix').value/100),
    'chorus_mix='+(document.getElementById('chorus-mix').value/100),
    'flanger_mix='+(document.getElementById('flanger-mix').value/100),
    'theme='+(document.documentElement.getAttribute('data-theme')||'midnight')
  ].join('&');
  try {
    const r = await fetch('/api/siren/options', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if (r.ok) toast('Options applied'); else toast('Failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

// Encoder mapping
async function loadEncoderMap() {
  try {
    const r = await fetch('/api/encoders/map');
    const d = await r.json();
    encoderParams = d.params || [];
    const ba = d.bank_a || [0,1,2,3,4];
    const bb = d.bank_b || [5,6,7,8,9];
    let html = '<tr><th>Encoder</th><th>Bank A</th><th>Bank B</th></tr>';
    for (let i = 0; i < 5; i++) {
      const opts = encoderParams.map((p,pi) =>
        '<option value="'+pi+'">'+esc(p.label)+'</option>').join('');
      html += '<tr><td style="color:var(--text-hi);font-weight:700">Enc '+(i+1)+'</td>'+
        '<td><select id="enc-a'+i+'">'+opts+'</select></td>'+
        '<td><select id="enc-b'+i+'">'+opts+'</select></td></tr>';
    }
    document.getElementById('enc-table').innerHTML = html;
    for (let i = 0; i < 5; i++) {
      document.getElementById('enc-a'+i).value = ba[i];
      document.getElementById('enc-b'+i).value = bb[i];
    }
  } catch(e) { console.error(e); }
}

async function saveEncoderMap() {
  let body = '';
  for (let i = 0; i < 5; i++) {
    if (i > 0) body += '&';
    body += 'a'+i+'='+document.getElementById('enc-a'+i).value;
    body += '&b'+i+'='+document.getElementById('enc-b'+i).value;
  }
  try {
    const r = await fetch('/api/encoders/map', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if (r.ok) toast('Encoder mapping saved'); else toast('Save failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

function resetEncoderMap() {
  for (let i = 0; i < 5; i++) {
    document.getElementById('enc-a'+i).value = i;
    document.getElementById('enc-b'+i).value = i+5;
  }
  saveEncoderMap();
}

// WiFi
async function loadWifiStatus() {
  try {
    const r = await fetch('/api/wifi/status');
    const wifi = await r.json();
    document.getElementById('wifi-status').textContent = wifi.connected ? 'Connected: '+wifi.ssid : 'Not connected';
  } catch(e) {}
}
async function scanWifi() {
  document.getElementById('wifi-networks').innerHTML = '<div class="loading"><div class="spinner"></div> Scanning...</div>';
  try {
    const r = await fetch('/api/wifi/scan');
    const d = await r.json();
    document.getElementById('wifi-networks').innerHTML =
      (d.networks||[]).map(n => '<div class="network-item" onclick="document.getElementById(\'wifi-ssid\').value=\''+esc(n.ssid)+'\'"><span>'+esc(n.ssid)+'</span><span class="signal">'+n.signal+' dBm</span></div>').join('') ||
      '<div style="color:var(--text-lo);padding:8px;font-size:0.8rem">No networks found</div>';
  } catch(e) { document.getElementById('wifi-networks').innerHTML = '<div style="color:var(--danger);padding:8px;font-size:0.8rem">Scan failed</div>'; }
}

async function connectWifi() {
  const ssid = document.getElementById('wifi-ssid').value;
  const pass = document.getElementById('wifi-pass').value;
  if (!ssid) { toast('Enter SSID',true); return; }
  try {
    const r = await fetch('/api/wifi/connect', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass)});
    if (r.ok) toast('Credentials saved'); else toast('Failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

// System
async function loadSystemInfo() {
  try {
    const [sysR, wifiR] = await Promise.all([fetch('/api/system/info'), fetch('/api/wifi/status')]);
    const sys = await sysR.json();
    const wifi = await wifiR.json();
    document.getElementById('sys-cpu').textContent = sys.cpu_temp ? sys.cpu_temp.toFixed(1)+'\u00b0C' : 'N/A';
    let up = '';
    if (sys.uptime_days > 0) up += sys.uptime_days+'d ';
    up += sys.uptime_hours+'h '+sys.uptime_mins+'m';
    document.getElementById('sys-uptime').textContent = up;
    document.getElementById('sys-mem').textContent = sys.mem_used_mb+'MB / '+sys.mem_total_mb+'MB';
    const wifiText = wifi.connected ? 'Connected: '+wifi.ssid : 'Not connected';
    document.getElementById('sys-wifi').textContent = wifiText;
    document.getElementById('wifi-status').textContent = wifiText;
  } catch(e) {
    document.getElementById('sys-cpu').textContent = 'Error';
  }
}

async function restartService() {
  try { await fetch('/api/system/restart',{method:'POST'}); toast('Restarting...'); } catch(e) { toast('Error',true); }
}
async function rebootDevice() {
  try { await fetch('/api/system/reboot',{method:'POST'}); toast('Rebooting...'); } catch(e) { toast('Error',true); }
}

const STAGE_LABELS = {idle:'Ready',backup:'Backing up...',fetch:'Fetching updates...',pull:'Pulling code...',build:'Building firmware...',deploy:'Deploying...',done:'Complete!',error:'Error'};
let updatePollTimer = null;

async function loadBranches() {
  try {
    const r = await fetch('/api/update/branches');
    const d = await r.json();
    const sel = document.getElementById('update-branch');
    const cur = sel.value;
    sel.innerHTML = (d.branches||['main']).map(b => '<option value="'+esc(b)+'"'+(b===cur?' selected':'')+'>'+esc(b)+'</option>').join('');
  } catch(e) {}
}

async function checkUpdate() {
  const branch = document.getElementById('update-branch').value;
  document.getElementById('update-result').innerHTML = '<div class="loading"><div class="spinner"></div> Checking...</div>';
  try {
    const r = await fetch('/api/update/check', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'branch='+encodeURIComponent(branch)});
    const d = await r.json();
    if (d.available) {
      document.getElementById('update-result').innerHTML = '<p style="color:var(--accent);font-size:0.8rem">'+d.count+' update(s) available on '+esc(branch)+'</p><button class="btn btn-primary" onclick="installUpdate()" style="margin-top:8px">Install</button>';
    } else {
      document.getElementById('update-result').innerHTML = '<p style="color:var(--text-lo);font-size:0.8rem">Up to date</p>';
    }
  } catch(e) { document.getElementById('update-result').innerHTML = '<p style="color:var(--danger);font-size:0.8rem">Check failed</p>'; }
}

async function installUpdate() {
  const branch = document.getElementById('update-branch').value;
  document.getElementById('update-result').innerHTML = '';
  document.getElementById('update-progress').style.display = 'block';
  document.getElementById('update-bar').style.width = '0%';
  document.getElementById('update-stage-text').textContent = 'Starting...';
  document.getElementById('update-pct-text').textContent = '0%';
  document.getElementById('btn-check').disabled = true;
  document.getElementById('update-log-section').style.display = 'block';
  document.getElementById('update-log').textContent = '';
  document.getElementById('update-reboot').style.display = 'none';
  try {
    const r = await fetch('/api/update/install', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'branch='+encodeURIComponent(branch)});
    if (!r.ok) { toast('Update failed',true); document.getElementById('update-progress').style.display='none'; document.getElementById('btn-check').disabled=false; return; }
    pollUpdateStatus();
  } catch(e) { toast('Error: '+e,true); document.getElementById('update-progress').style.display='none'; document.getElementById('btn-check').disabled=false; }
}

async function pollUpdateStatus() {
  if (updatePollTimer) clearTimeout(updatePollTimer);
  try {
    const [statusRes, logRes] = await Promise.all([fetch('/api/update/status'),fetch('/api/update/log')]);
    const d = await statusRes.json();
    const pct = d.progress||0, stage = d.stage||'idle';
    document.getElementById('update-bar').style.width = pct+'%';
    document.getElementById('update-pct-text').textContent = pct+'%';
    document.getElementById('update-stage-text').textContent = STAGE_LABELS[stage]||stage;
    try { const ld = await logRes.json(); if (ld.log) { const el = document.getElementById('update-log'); el.textContent = ld.log; if (el.scrollHeight-el.scrollTop-el.clientHeight<60) el.scrollTop=el.scrollHeight; } } catch(e) {}
    if (stage === 'error') {
      document.getElementById('update-bar').style.background = 'var(--danger)';
      document.getElementById('update-stage-text').textContent = 'Error: '+(d.error||'unknown');
      document.getElementById('btn-check').disabled = false;
      toast('Update failed',true);
      document.getElementById('update-log').style.display = 'block';
      document.getElementById('log-arrow').innerHTML = '&#9660;';
      return;
    }
    if (stage === 'done') {
      document.getElementById('update-bar').style.background = 'var(--accent)';
      document.getElementById('btn-check').disabled = false;
      document.getElementById('update-reboot').style.display = 'block';
      toast('Update installed \u2014 reboot to apply');
      return;
    }
    updatePollTimer = setTimeout(pollUpdateStatus, 1500);
  } catch(e) { updatePollTimer = setTimeout(pollUpdateStatus, 3000); }
}

function toggleUpdateLog() {
  const el = document.getElementById('update-log');
  const arrow = document.getElementById('log-arrow');
  if (el.style.display === 'none') { el.style.display='block'; arrow.innerHTML='&#9660;'; el.scrollTop=el.scrollHeight; }
  else { el.style.display='none'; arrow.innerHTML='&#9654;'; }
}

function copyUpdateLog() {
  const text = document.getElementById('update-log').textContent;
  if (!text) { toast('No log',true); return; }
  if (navigator.clipboard) { navigator.clipboard.writeText(text).then(()=>toast('Copied')); }
  else { const ta=document.createElement('textarea');ta.value=text;ta.style.position='fixed';ta.style.opacity='0';document.body.appendChild(ta);ta.select();document.execCommand('copy');document.body.removeChild(ta);toast('Copied'); }
}

async function downloadBackup() {
  try {
    const r = await fetch('/api/backup');
    const blob = await r.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href=url;a.download='dubsiren-backup.json';a.click();
    URL.revokeObjectURL(url);
    toast('Backup downloaded');
  } catch(e) { toast('Backup failed',true); }
}

async function restoreBackup() {
  const file = document.getElementById('restore-file').files[0];
  if (!file) { toast('Select a file',true); return; }
  const data = await file.text();
  try {
    const r = await fetch('/api/restore', {method:'POST',headers:{'Content-Type':'application/json'},body:data});
    if (r.ok) { toast('Restored'); loadPresets(); loadOptions(); } else toast('Restore failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

async function exitAP() {
  if (!confirm('Return to siren mode?')) return;
  try {
    await fetch('/api/exit', {method:'POST'});
    toast('Returning to siren mode...');
    setTimeout(() => { document.body.innerHTML = '<div class="exit-screen"><h2>Siren Mode Active</h2><p>You can disconnect from this network.</p></div>'; }, 1000);
  } catch(e) { toast('Error: '+e,true); }
}

// Theme
const THEMES = [
  {id:'midnight',name:'Midnight',colors:['#0a0a0a','#ff6600','#00ff88','#ccc']},
  {id:'deep-dub',name:'Deep Dub',colors:['#05080f','#00ccff','#00ff88','#8899bb']},
  {id:'reggae',name:'Reggae',colors:['#0a0f06','#ffcc00','#00ff44','#b0c890']},
  {id:'steppers',name:'Steppers',colors:['#0f0808','#ff2222','#ff4444','#cc9999']},
  {id:'silver',name:'Silver',colors:['#f0f0f0','#ff6600','#00cc66','#444']},
  {id:'concrete',name:'Concrete',colors:['#e8e4e0','#2a6b4f','#2a9b5f','#4a4640']},
  {id:'roots',name:'Roots',colors:['#0e0a06','#cc9933','#66cc44','#aa9070']},
  {id:'irie',name:'Irie',colors:['#0a060e','#bb44ff','#44ff88','#9980bb']},
];

function renderThemeGrid() {
  const cur = document.documentElement.getAttribute('data-theme')||'midnight';
  document.getElementById('theme-grid').innerHTML = THEMES.map(t =>
    '<div class="theme-swatch'+(t.id===cur?' active':'')+'" onclick="setTheme(\''+t.id+'\')">'+
      '<div class="swatch-bar">'+t.colors.map(c=>'<span style="background:'+c+'"></span>').join('')+'</div>'+
      '<div class="swatch-label">'+t.name+'</div></div>'
  ).join('');
}

async function setTheme(id) {
  document.documentElement.setAttribute('data-theme',id);
  try { localStorage.setItem('dubsiren-theme',id); } catch(e) {}
  renderThemeGrid();
  // Save to siren (server-side persistence)
  try {
    await fetch('/api/siren/options', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'theme='+id});
  } catch(e) {}
}

// Apply saved theme — try localStorage first for instant render, then sync from server
(function(){try{const s=localStorage.getItem('dubsiren-theme');document.documentElement.setAttribute('data-theme',s||'midnight');}catch(e){document.documentElement.setAttribute('data-theme','midnight');}})();

// Init
loadPresets();
loadOptions();
renderThemeGrid();
loadDspState();
startHeartbeat();
</script>
</body>
</html>
)HTML";

} // namespace web_ui
