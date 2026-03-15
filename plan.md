# Implementation Plan: Pro Features

## Features to implement:
1. **Live DSP Controls** — real-time sliders for all 10 encoder parameters + waveform selectors
2. **Connection & Resilience** — status indicator, auto-reconnect, offline banner, debouncing
3. **Restart Button + System Info** — reboot/restart, CPU temp, uptime, memory
4. **Preview System Fix** — wire up dead preview start/stop callbacks
5. **Control Surface Reassignment** — remap encoders to different parameters via web UI
6. **Revert to Defaults** — factory reset button

---

## Phase 1: Backend API (web_server.h, web_server.cpp, main.cpp)

### New Callbacks in `web_server::Callbacks`:
- `set_parameter` — set a single DSP parameter by name (for live sliders)
- `get_live_state` — return all current DSP parameter values (for slider positions)
- `preview_start` / `preview_stop` — already defined, need implementation in main.cpp
- `get_system_info` — return CPU temp, uptime, memory usage
- `reboot_system` — trigger system reboot
- `restart_service` — restart dubsiren service
- `reset_defaults` — reset all parameters to factory defaults
- `get_encoder_map` — return current encoder→parameter assignments
- `set_encoder_map` — update encoder→parameter assignments

### New API Endpoints:
- `POST /api/dsp/param?name=freq&value=880` — set single parameter
- `GET /api/dsp/state` — get all current DSP values (for slider init)
- `POST /api/preview/start` — already exists, wire callback
- `POST /api/preview/stop` — already exists, wire callback
- `GET /api/system/info` — CPU temp, uptime, memory, version
- `POST /api/system/reboot` — reboot Pi
- `POST /api/system/restart` — restart service
- `POST /api/dsp/reset` — factory reset all parameters
- `GET /api/encoders/map` — get encoder assignments
- `POST /api/encoders/map` — set encoder assignments

## Phase 2: Main.cpp Backend Logic

### Live DSP parameter setting:
- Map parameter names to atomic globals (g_freq, g_lfo_rate, etc.)
- Validate ranges, clamp values
- Return updated value for UI confirmation

### Preview system:
- `preview_start`: apply a library preset temporarily, set a "previewing" flag
- `preview_stop`: restore the previous state from a snapshot taken before preview

### System info:
- Read `/sys/class/thermal/thermal_zone0/temp` for CPU temp
- Read `/proc/uptime` for uptime
- Read `/proc/meminfo` for memory
- `system("sudo reboot")` for reboot
- `system("sudo systemctl restart dubsiren.service")` for restart

### Factory reset:
- Store hardcoded defaults in a const struct
- Apply them to all atomics
- Don't clear saved presets — just reset current live state

### Encoder reassignment:
- Replace hardcoded switch/case encoder handling with table-driven approach
- Store mapping in config file (siren_config.txt)
- Mapping: encoder_id × bank → parameter_name
- Load on startup, save on change

## Phase 3: Web UI (web_ui.h)

### New "Live" Tab (primary feature):
- 10 sliders matching encoder parameters (Bank A + Bank B)
- Real-time parameter updates via POST /api/dsp/param on `input` event (debounced)
- Waveform selector dropdowns (oscillator + LFO)
- Pitch envelope toggle (rise/off/fall)
- Visual grouping: Oscillator, LFO, Filter, Delay, Reverb
- "Reset to Defaults" button

### Connection status:
- Heartbeat polling (GET /api/dsp/state every 3s)
- Status indicator in header (green dot = connected, red = disconnected)
- Offline banner overlay when disconnected
- Auto-reconnect with exponential backoff
- Re-fetch state on reconnect

### System Tab additions:
- CPU temperature display
- Uptime display
- Memory usage display
- "Restart Siren" button (with confirm)
- "Reboot Device" button (with confirm)
- Auto-refresh system info every 10s

### Encoder Mapping UI (in Options or dedicated section):
- Table showing: Encoder 1-5 × Bank A/B
- Dropdown for each cell: parameter to assign
- Save button to persist mapping
- Reset to default mapping button

### UX improvements:
- Debounce all slider inputs (16ms requestAnimationFrame)
- Double-click guard on buttons (disable for 500ms after click)
- Confirm dialogs on destructive actions (reboot, factory reset, restore backup)

## Phase 4: Preview System

### Backend:
- On preview_start: snapshot current state, apply library preset
- On preview_stop: restore snapshot
- Auto-stop preview after 10s timeout

### Frontend:
- Preview button in library triggers POST /api/preview/start
- Visual indicator showing "previewing" state
- Stop button appears during preview
- Auto-stop on navigation away

---

## File changes:
1. `include/web_server.h` — new callback fields
2. `src/web_server.cpp` — new endpoint handlers
3. `src/main.cpp` — callback implementations, encoder table, defaults, preview, system info
4. `src/web_ui.h` — new Live tab, system info, connection status, encoder mapping UI

## Implementation order:
1. Backend API + callbacks first (testable via curl)
2. Live DSP controls UI (highest impact feature)
3. Connection resilience (improves all features)
4. System info + restart
5. Preview system
6. Encoder reassignment (most complex)
7. Factory reset (simplest, can do anytime)
