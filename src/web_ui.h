#pragma once

// Embedded web UI for the dub siren AP mode configuration portal
namespace web_ui {

static const char* const INDEX_HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Poorhouse Lane Siren Config</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
background:#1a1a2e;color:#e0e0e0;min-height:100vh}
.header{background:#16213e;padding:16px;text-align:center;border-bottom:2px solid #00ff88}
.header h1{color:#00ff88;font-size:1.4em;margin-bottom:4px}
.header .subtitle{color:#888;font-size:0.85em}
.tabs{display:flex;background:#0f3460;overflow-x:auto}
.tab{flex:1;padding:12px 8px;text-align:center;cursor:pointer;color:#888;
border-bottom:3px solid transparent;font-size:0.85em;white-space:nowrap;
min-width:60px;transition:all 0.2s}
.tab.active{color:#00ff88;border-bottom-color:#00ff88;background:#16213e}
.tab:hover{color:#fff}
.content{padding:16px;max-width:600px;margin:0 auto}
.panel{display:none}
.panel.active{display:block}
.card{background:#16213e;border-radius:8px;padding:16px;margin-bottom:12px;
border:1px solid #0f3460}
.card h3{color:#00ff88;margin-bottom:8px;font-size:1em}
.btn{background:#00ff88;color:#1a1a2e;border:none;padding:10px 20px;
border-radius:6px;cursor:pointer;font-weight:bold;font-size:0.9em;
transition:background 0.2s}
.btn:hover{background:#00cc6a}
.btn-danger{background:#ff4444}
.btn-danger:hover{background:#cc3333}
.btn-secondary{background:#0f3460;color:#e0e0e0}
.form-group{margin-bottom:12px}
.form-group label{display:block;color:#888;margin-bottom:4px;font-size:0.85em}
.form-group input,.form-group select{width:100%;padding:8px;background:#0f3460;
color:#e0e0e0;border:1px solid #333;border-radius:4px;font-size:0.9em}
.form-group input[type=range]{padding:4px 0}
.toggle{position:relative;display:inline-block;width:48px;height:24px}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;
background:#333;border-radius:24px;transition:0.3s}
.toggle .slider:before{content:'';position:absolute;height:18px;width:18px;
left:3px;bottom:3px;background:#fff;border-radius:50%;transition:0.3s}
.toggle input:checked+.slider{background:#00ff88}
.toggle input:checked+.slider:before{transform:translateX(24px)}
.radio-group{display:flex;gap:8px;flex-wrap:wrap}
.radio-btn{padding:8px 16px;background:#0f3460;border:1px solid #333;
border-radius:4px;cursor:pointer;font-size:0.85em;transition:all 0.2s}
.radio-btn.active{background:#00ff88;color:#1a1a2e;border-color:#00ff88}
.preset-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:8px}
.preset-card{background:#0f3460;border-radius:6px;padding:10px;cursor:pointer;
border:1px solid #333;transition:all 0.2s;font-size:0.85em}
.preset-card:hover{border-color:#00ff88}
.preset-card .name{font-weight:bold;margin-bottom:4px}
.preset-card .cat{color:#ff8800;font-size:0.75em}
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);
background:#00ff88;color:#1a1a2e;padding:10px 24px;border-radius:6px;
font-weight:bold;opacity:0;transition:opacity 0.3s;z-index:999}
.toast.show{opacity:1}
.range-val{color:#00ff88;font-weight:bold;min-width:40px;display:inline-block;text-align:right}
.option-row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;
border-bottom:1px solid #0f3460}
.section-title{color:#ff8800;font-size:0.9em;margin:16px 0 8px;text-transform:uppercase;
letter-spacing:1px}
.network-list{max-height:200px;overflow-y:auto}
.network-item{display:flex;justify-content:space-between;align-items:center;
padding:8px;cursor:pointer;border-bottom:1px solid #0f3460}
.network-item:hover{background:#0f3460}
.signal-bars{display:flex;gap:2px;align-items:flex-end}
.signal-bar{width:4px;background:#333;border-radius:1px}
.signal-bar.active{background:#00ff88}
.loading{text-align:center;padding:20px;color:#888}
.spinner{display:inline-block;width:20px;height:20px;border:2px solid #333;
border-top-color:#00ff88;border-radius:50%;animation:spin 0.8s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
</style>
</head>
<body>
<div class="header">
<h1>POORHOUSE LANE SIREN</h1>
<div class="subtitle">Configuration Portal</div>
</div>

<div class="tabs">
<div class="tab active" onclick="showTab('presets')">Presets</div>
<div class="tab" onclick="showTab('options')">Options</div>
<div class="tab" onclick="showTab('wifi')">WiFi</div>
<div class="tab" onclick="showTab('updates')">Updates</div>
<div class="tab" onclick="showTab('system')">System</div>
</div>

<!-- PRESETS TAB -->
<div class="content">
<div id="presets" class="panel active">
<div class="card">
<h3>Save Current State</h3>
<div class="form-group"><label>Name</label><input type="text" id="save-name" placeholder="My Preset"></div>
<div class="form-group"><label>Slot</label>
<select id="save-slot"><option value="0">Slot 1</option><option value="1">Slot 2</option>
<option value="2">Slot 3</option><option value="3">Slot 4</option></select></div>
<button class="btn" onclick="savePreset()">Save to Slot</button>
</div>

<div class="section-title">User Presets</div>
<div id="user-presets" class="preset-grid"></div>

<div class="section-title">Standard Presets</div>
<div id="standard-presets" class="preset-grid"></div>

<div class="section-title">Preset Library</div>
<div id="library-presets" class="preset-grid"></div>
</div>

<!-- OPTIONS TAB -->
<div id="options" class="panel">
<div class="card">
<h3>Reverb Type</h3>
<div class="radio-group" id="reverb-type">
<div class="radio-btn active" data-val="0" onclick="setRadio('reverb-type',0)">Spring</div>
<div class="radio-btn" data-val="1" onclick="setRadio('reverb-type',1)">Plate</div>
<div class="radio-btn" data-val="2" onclick="setRadio('reverb-type',2)">Hall</div>
<div class="radio-btn" data-val="3" onclick="setRadio('reverb-type',3)">Schroeder</div>
</div>
</div>

<div class="card">
<h3>Delay Type</h3>
<div class="radio-group" id="delay-type">
<div class="radio-btn active" data-val="0" onclick="setRadio('delay-type',0)">Tape</div>
<div class="radio-btn" data-val="1" onclick="setRadio('delay-type',1)">Digital</div>
</div>
</div>

<div class="card" id="tape-params">
<h3>Tape Delay Parameters</h3>
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
<h3>Effects Chain Order</h3>
<div class="form-group">
<select id="fx-chain">
<option value="0">Filter → Delay → Reverb (Classic)</option>
<option value="1">Filter → Reverb → Delay</option>
<option value="2">Delay → Filter → Reverb</option>
<option value="3">Delay → Reverb → Filter</option>
<option value="4">Reverb → Filter → Delay</option>
<option value="5">Reverb → Delay → Filter</option>
</select>
</div>
</div>

<div class="card">
<h3>Siren Settings</h3>
<div class="option-row">
<span>Linked Pitch/LFO Rate</span>
<label class="toggle"><input type="checkbox" id="lfo-link" checked><span class="slider"></span></label>
</div>
<div class="option-row">
<span>Super Drip Reverb</span>
<label class="toggle"><input type="checkbox" id="super-drip" checked><span class="slider"></span></label>
</div>
<div class="option-row">
<span>Filter Sweep Direction</span>
<select id="sweep-dir" style="width:auto;padding:6px">
<option value="-1">Down (Dub)</option>
<option value="0">Flat</option>
<option value="1">Up (Bright)</option>
</select>
</div>
</div>
<button class="btn" onclick="applyOptions()" style="width:100%;margin-top:8px">Apply Options</button>
</div>

<!-- WIFI TAB -->
<div id="wifi" class="panel">
<div class="card">
<h3>WiFi Networks</h3>
<button class="btn btn-secondary" onclick="scanWifi()" style="margin-bottom:12px">Scan Networks</button>
<div id="wifi-networks" class="network-list"></div>
</div>
<div class="card">
<h3>Connect to Network</h3>
<div class="form-group"><label>SSID</label><input type="text" id="wifi-ssid"></div>
<div class="form-group"><label>Password</label><input type="password" id="wifi-pass"></div>
<button class="btn" onclick="connectWifi()">Save Credentials</button>
</div>
<div class="card">
<h3>Status</h3>
<div id="wifi-status">Not connected</div>
</div>
</div>

<!-- UPDATES TAB -->
<div id="updates" class="panel">
<div class="card">
<h3>Software Updates</h3>
<p style="color:#888;margin-bottom:12px">Current version: v0.5.0</p>
<button class="btn btn-secondary" onclick="checkUpdate()">Check for Updates</button>
<div id="update-result" style="margin-top:12px"></div>
</div>
</div>

<!-- SYSTEM TAB -->
<div id="system" class="panel">
<div class="card">
<h3>Device Info</h3>
<div id="sys-info" style="color:#888">Loading...</div>
</div>
<div class="card">
<h3>Backup & Restore</h3>
<button class="btn btn-secondary" onclick="downloadBackup()" style="margin-bottom:8px">Download Backup</button>
<div class="form-group">
<label>Restore from file</label>
<input type="file" id="restore-file" accept=".json">
</div>
<button class="btn btn-secondary" onclick="restoreBackup()">Restore</button>
</div>
<div class="card">
<button class="btn btn-danger" onclick="exitAP()" style="width:100%">Return to Siren Mode</button>
</div>
</div>
</div>

<div id="toast" class="toast"></div>

<script>
function showTab(id){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  document.querySelectorAll('.tab').forEach(t=>{
    if(t.textContent.toLowerCase().replace(/\s/g,'')===id||
       (id==='presets'&&t.textContent==='Presets')||
       (id==='options'&&t.textContent==='Options')||
       (id==='wifi'&&t.textContent==='WiFi')||
       (id==='updates'&&t.textContent==='Updates')||
       (id==='system'&&t.textContent==='System'))
      t.classList.add('active');
  });
  if(id==='system')loadSystemInfo();
}

function toast(msg,err){
  const t=document.getElementById('toast');
  t.textContent=msg;
  t.style.background=err?'#ff4444':'#00ff88';
  t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'),2500);
}

function setRadio(group,val){
  document.querySelectorAll('#'+group+' .radio-btn').forEach(b=>{
    b.classList.toggle('active',parseInt(b.dataset.val)===val);
  });
  if(group==='delay-type'){
    document.getElementById('tape-params').style.display=val===0?'block':'none';
  }
}

function updateRange(id){
  document.getElementById(id+'-val').textContent=document.getElementById(id).value+'%';
}

async function loadPresets(){
  try{
    const r=await fetch('/api/presets');
    const d=await r.json();
    renderPresets('user-presets',d.user||[],'user');
    renderPresets('standard-presets',d.standard||[],'standard');
    const lib=(d.library||[]).concat(d.experimental||[]);
    renderPresets('library-presets',lib,'library');
  }catch(e){console.error(e)}
}

function renderPresets(containerId,presets,category){
  const c=document.getElementById(containerId);
  c.innerHTML=presets.map((p,i)=>`
    <div class="preset-card" onclick="applyPreset('${category}',${p.index!==undefined?p.index:p.slot})">
      <div class="name">${p.name}</div>
      <div class="cat">${p.category||category}</div>
    </div>`).join('');
}

async function applyPreset(cat,idx){
  try{
    const r=await fetch('/api/presets/apply',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:`category=${cat}&index=${idx}`});
    if(r.ok)toast('Preset loaded');else toast('Failed',true);
  }catch(e){toast('Error: '+e,true)}
}

async function savePreset(){
  const name=document.getElementById('save-name').value||'User Preset';
  const slot=document.getElementById('save-slot').value;
  try{
    const r=await fetch('/api/presets/save',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:`slot=${slot}&name=${encodeURIComponent(name)}`});
    if(r.ok){toast('Saved!');loadPresets();}else toast('Save failed',true);
  }catch(e){toast('Error: '+e,true)}
}

async function loadOptions(){
  try{
    const r=await fetch('/api/siren/options');
    const d=await r.json();
    setRadio('reverb-type',d.reverb_type||0);
    setRadio('delay-type',d.delay_type||0);
    document.getElementById('wobble').value=Math.round((d.tape_wobble||1)*100);
    document.getElementById('flutter').value=Math.round((d.tape_flutter||1)*100);
    updateRange('wobble');updateRange('flutter');
    document.getElementById('fx-chain').value=d.fx_chain||0;
    document.getElementById('lfo-link').checked=d.lfo_pitch_link!==false;
    document.getElementById('super-drip').checked=d.super_drip!==false;
    document.getElementById('sweep-dir').value=d.sweep_dir||-1;
  }catch(e){console.error(e)}
}

async function applyOptions(){
  const rt=document.querySelector('#reverb-type .radio-btn.active').dataset.val;
  const dt=document.querySelector('#delay-type .radio-btn.active').dataset.val;
  const body=[
    'reverb_type='+rt,'delay_type='+dt,
    'tape_wobble='+(document.getElementById('wobble').value/100),
    'tape_flutter='+(document.getElementById('flutter').value/100),
    'fx_chain='+document.getElementById('fx-chain').value,
    'lfo_pitch_link='+(document.getElementById('lfo-link').checked?'1':'0'),
    'super_drip='+(document.getElementById('super-drip').checked?'1':'0'),
    'sweep_dir='+document.getElementById('sweep-dir').value
  ].join('&');
  try{
    const r=await fetch('/api/siren/options',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if(r.ok)toast('Options applied');else toast('Failed',true);
  }catch(e){toast('Error: '+e,true)}
}

async function scanWifi(){
  document.getElementById('wifi-networks').innerHTML='<div class="loading"><div class="spinner"></div> Scanning...</div>';
  try{
    const r=await fetch('/api/wifi/scan');
    const d=await r.json();
    document.getElementById('wifi-networks').innerHTML=(d.networks||[]).map(n=>`
      <div class="network-item" onclick="document.getElementById('wifi-ssid').value='${n.ssid}'">
        <span>${n.ssid}</span>
        <span style="color:#888">${n.signal} dBm</span>
      </div>`).join('')||'<div style="color:#888;padding:8px">No networks found</div>';
  }catch(e){document.getElementById('wifi-networks').innerHTML='<div style="color:#ff4444;padding:8px">Scan failed</div>'}
}

async function connectWifi(){
  const ssid=document.getElementById('wifi-ssid').value;
  const pass=document.getElementById('wifi-pass').value;
  if(!ssid){toast('Enter SSID',true);return}
  try{
    const r=await fetch('/api/wifi/connect',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:`ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(pass)}`});
    if(r.ok)toast('Credentials saved');else toast('Failed',true);
  }catch(e){toast('Error: '+e,true)}
}

async function checkUpdate(){
  document.getElementById('update-result').innerHTML='<div class="loading"><div class="spinner"></div> Checking...</div>';
  try{
    const r=await fetch('/api/update/check',{method:'POST'});
    const d=await r.json();
    if(d.available){
      document.getElementById('update-result').innerHTML=
        `<p style="color:#00ff88">${d.count} update(s) available</p>
         <button class="btn" onclick="installUpdate()" style="margin-top:8px">Install Update</button>`;
    }else{
      document.getElementById('update-result').innerHTML='<p style="color:#888">Up to date</p>';
    }
  }catch(e){document.getElementById('update-result').innerHTML='<p style="color:#ff4444">Check failed</p>'}
}

async function installUpdate(){
  document.getElementById('update-result').innerHTML='<div class="loading"><div class="spinner"></div> Installing...</div>';
  try{
    const r=await fetch('/api/update/install',{method:'POST'});
    if(r.ok)toast('Update installed! Restart to apply.');else toast('Update failed',true);
  }catch(e){toast('Error: '+e,true)}
}

async function loadSystemInfo(){
  document.getElementById('sys-info').innerHTML='Loading...';
  try{
    const r=await fetch('/api/wifi/status');
    const d=await r.json();
    document.getElementById('sys-info').innerHTML=
      `<p>WiFi: ${d.connected?'Connected to '+d.ssid:'Not connected'}</p>
       <p>AP Mode: Active</p>`;
  }catch(e){document.getElementById('sys-info').innerHTML='Error loading info'}
}

async function downloadBackup(){
  try{
    const r=await fetch('/api/backup');
    const blob=await r.blob();
    const url=URL.createObjectURL(blob);
    const a=document.createElement('a');
    a.href=url;a.download='dubsiren-backup.json';a.click();
    URL.revokeObjectURL(url);
    toast('Backup downloaded');
  }catch(e){toast('Backup failed',true)}
}

async function restoreBackup(){
  const file=document.getElementById('restore-file').files[0];
  if(!file){toast('Select a file',true);return}
  const data=await file.text();
  try{
    const r=await fetch('/api/restore',{method:'POST',
      headers:{'Content-Type':'application/json'},body:data});
    if(r.ok)toast('Restored!');else toast('Restore failed',true);
  }catch(e){toast('Error: '+e,true)}
}

async function exitAP(){
  if(!confirm('Return to siren mode?'))return;
  try{
    await fetch('/api/exit',{method:'POST'});
    toast('Returning to siren mode...');
    setTimeout(()=>{document.body.innerHTML='<div style="text-align:center;padding:40px;color:#00ff88"><h2>Siren Mode Active</h2><p>You can disconnect from this WiFi network.</p></div>'},1000);
  }catch(e){toast('Error: '+e,true)}
}

// Initialize
loadPresets();
loadOptions();
</script>
</body>
</html>
)HTML";

} // namespace web_ui
