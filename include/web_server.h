#pragma once

#include <functional>
#include <string>

// ─── Web configuration server ──────────────────────────────────────
//
// HTTP server for the AP mode captive portal.  Serves the web UI
// and provides REST API endpoints for device configuration.

namespace web_server {

// Callbacks for interacting with the siren state
struct Callbacks {
    // Get current siren state as JSON
    std::function<std::string()> get_preset_state;

    // Get all presets (user + library) as JSON
    std::function<std::string()> get_all_presets;

    // Apply a preset by category and index
    std::function<bool(const std::string& category, int index)> apply_preset;

    // Save current state to user slot with a name
    std::function<bool(int slot, const std::string& name)> save_to_slot;

    // Swap two user preset slots
    std::function<bool(int a, int b)> swap_slots;

    // Load a library/factory preset into a user slot
    std::function<bool(int slot, const std::string& category, int index)> load_to_slot;

    // Get/set siren options as JSON
    std::function<std::string()> get_siren_options;
    std::function<bool(const std::string& json)> set_siren_options;

    // Preview control
    std::function<void(const std::string& preset_json)> preview_start;
    std::function<void()> preview_stop;

    // WiFi operations
    std::function<std::string()> wifi_scan;
    std::function<bool(const std::string& ssid, const std::string& password)> wifi_connect;
    std::function<std::string()> wifi_status;

    // Update operations
    std::function<std::string()> update_check;
    std::function<bool()> update_install;
    std::function<std::string()> update_status;

    // Backup/restore
    std::function<std::string()> backup_create;
    std::function<bool(const std::string& data)> backup_restore;

    // Exit AP mode
    std::function<void()> exit_ap;
};

// Start the web server on the given port (usually 80)
bool start(int port, const Callbacks& cb);

// Stop the web server
void stop();

// Check if server is running
bool is_running();

} // namespace web_server
