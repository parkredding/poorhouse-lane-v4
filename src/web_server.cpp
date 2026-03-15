// web_server.cpp — HTTP server for AP mode configuration portal
//
// Uses cpp-httplib (single-header) to serve the web UI and REST API.
// Runs in its own thread.  Handles captive portal detection for
// Android, iOS, and Windows.

#include "web_server.h"
#include "web_ui.h"

#include <httplib.h>

#include <cstdio>
#include <thread>
#include <atomic>
#include <memory>

static std::unique_ptr<httplib::Server> g_server;
static std::thread g_thread;
static std::atomic<bool> g_running{false};
static web_server::Callbacks g_callbacks;  // persistent copy of callbacks

// ─── Captive portal detection responses ─────────────────────────────
//
// Different OS vendors probe specific URLs to detect captive portals.
// We respond with redirects or minimal content to trigger the portal.

static void setup_captive_portal(httplib::Server& svr)
{
    // Android captive portal detection
    svr.Get("/generate_204", [](const httplib::Request&, httplib::Response& res) {
        res.status = 302;
        res.set_header("Location", "http://192.168.4.1/");
    });

    // Additional Android check
    svr.Get("/gen_204", [](const httplib::Request&, httplib::Response& res) {
        res.status = 302;
        res.set_header("Location", "http://192.168.4.1/");
    });

    // Apple captive portal detection
    svr.Get("/hotspot-detect.html", [](const httplib::Request&, httplib::Response& res) {
        res.status = 302;
        res.set_header("Location", "http://192.168.4.1/");
    });

    // Windows captive portal detection
    svr.Get("/connecttest.txt", [](const httplib::Request&, httplib::Response& res) {
        res.status = 302;
        res.set_header("Location", "http://192.168.4.1/");
    });

    // Windows NCSI
    svr.Get("/ncsi.txt", [](const httplib::Request&, httplib::Response& res) {
        res.status = 302;
        res.set_header("Location", "http://192.168.4.1/");
    });

    // Firefox captive portal detection
    svr.Get("/success.txt", [](const httplib::Request&, httplib::Response& res) {
        res.status = 302;
        res.set_header("Location", "http://192.168.4.1/");
    });
}

// ─── JSON helpers ───────────────────────────────────────────────────

static void json_response(httplib::Response& res, const std::string& json)
{
    res.set_content(json, "application/json");
}

static void json_ok(httplib::Response& res, const std::string& msg = "ok")
{
    res.set_content("{\"status\":\"" + msg + "\"}", "application/json");
}

static void json_error(httplib::Response& res, const std::string& msg, int status = 400)
{
    res.status = status;
    res.set_content("{\"error\":\"" + msg + "\"}", "application/json");
}

// ─── Setup API routes ───────────────────────────────────────────────

static void setup_api(httplib::Server& svr)
{
    // Main page
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web_ui::INDEX_HTML, "text/html");
    });

    // ── Preset endpoints ────────────────────────────────────────────

    svr.Get("/api/presets", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_all_presets) {
            json_response(res, g_callbacks.get_all_presets());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Get("/api/presets/current", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_preset_state) {
            json_response(res, g_callbacks.get_preset_state());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/presets/apply", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.apply_preset) { json_error(res, "not implemented", 501); return; }
        auto category = req.get_param_value("category");
        auto index_str = req.get_param_value("index");
        int index = 0;
        try { index = std::stoi(index_str); } catch (...) {}
        if (g_callbacks.apply_preset(category, index)) {
            json_ok(res);
        } else {
            json_error(res, "failed to apply preset");
        }
    });

    svr.Post("/api/presets/save", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.save_to_slot) { json_error(res, "not implemented", 501); return; }
        auto slot_str = req.get_param_value("slot");
        auto name = req.get_param_value("name");
        int slot = 0;
        try { slot = std::stoi(slot_str); } catch (...) {}
        if (g_callbacks.save_to_slot(slot, name)) {
            json_ok(res, "saved");
        } else {
            json_error(res, "failed to save");
        }
    });

    // ── Siren options ───────────────────────────────────────────────

    svr.Get("/api/siren/options", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.get_siren_options) {
            json_response(res, g_callbacks.get_siren_options());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/siren/options", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.set_siren_options) { json_error(res, "not implemented", 501); return; }
        if (g_callbacks.set_siren_options(req.body)) {
            json_ok(res, "applied");
        } else {
            json_error(res, "failed to apply options");
        }
    });

    // ── Preview control ─────────────────────────────────────────────

    svr.Post("/api/preview/start", [](const httplib::Request& req, httplib::Response& res) {
        if (g_callbacks.preview_start) {
            g_callbacks.preview_start(req.body);
            json_ok(res, "previewing");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/preview/stop", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.preview_stop) {
            g_callbacks.preview_stop();
            json_ok(res, "stopped");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── WiFi operations ─────────────────────────────────────────────

    svr.Get("/api/wifi/scan", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.wifi_scan) {
            json_response(res, g_callbacks.wifi_scan());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/wifi/connect", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.wifi_connect) { json_error(res, "not implemented", 501); return; }
        auto ssid = req.get_param_value("ssid");
        auto password = req.get_param_value("password");
        if (g_callbacks.wifi_connect(ssid, password)) {
            json_ok(res, "credentials saved");
        } else {
            json_error(res, "failed to save credentials");
        }
    });

    svr.Get("/api/wifi/status", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.wifi_status) {
            json_response(res, g_callbacks.wifi_status());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── Update operations ───────────────────────────────────────────

    svr.Post("/api/update/check", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.update_check) {
            json_response(res, g_callbacks.update_check());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/update/install", [](const httplib::Request&, httplib::Response& res) {
        if (!g_callbacks.update_install) { json_error(res, "not implemented", 501); return; }
        if (g_callbacks.update_install()) {
            json_ok(res, "update started");
        } else {
            json_error(res, "update failed");
        }
    });

    svr.Get("/api/update/status", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.update_status) {
            json_response(res, g_callbacks.update_status());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── Backup/restore ──────────────────────────────────────────────

    svr.Get("/api/backup", [](const httplib::Request&, httplib::Response& res) {
        if (g_callbacks.backup_create) {
            auto data = g_callbacks.backup_create();
            res.set_header("Content-Disposition",
                           "attachment; filename=\"dubsiren-backup.json\"");
            res.set_content(data, "application/json");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/restore", [](const httplib::Request& req, httplib::Response& res) {
        if (!g_callbacks.backup_restore) { json_error(res, "not implemented", 501); return; }
        if (g_callbacks.backup_restore(req.body)) {
            json_ok(res, "restored");
        } else {
            json_error(res, "restore failed");
        }
    });

    // ── Exit AP mode ────────────────────────────────────────────────

    svr.Post("/api/exit", [](const httplib::Request&, httplib::Response& res) {
        json_ok(res, "exiting AP mode");
        if (g_callbacks.exit_ap) {
            // Schedule exit after response is sent
            std::thread([]() {
                usleep(500000);  // 500ms delay so response reaches client
                g_callbacks.exit_ap();
            }).detach();
        }
    });
}

// ─── Public interface ───────────────────────────────────────────────

bool web_server::start(int port, const Callbacks& cb)
{
    if (g_running) return true;

    g_callbacks = cb;  // persistent copy — outlives this function
    g_server = std::make_unique<httplib::Server>();

    setup_captive_portal(*g_server);
    setup_api(*g_server);

    g_running = true;
    g_thread = std::thread([port]() {
        printf("WEB: Server starting on port %d\n", port);
        if (!g_server->listen("0.0.0.0", port)) {
            fprintf(stderr, "WEB: Failed to start server on port %d\n", port);
        }
        g_running = false;
        printf("WEB: Server stopped\n");
    });

    // Wait briefly for server to start
    usleep(100000);
    return g_running;
}

void web_server::stop()
{
    if (!g_running) return;

    printf("WEB: Stopping server\n");
    if (g_server) {
        g_server->stop();
    }

    if (g_thread.joinable()) {
        g_thread.join();
    }

    g_server.reset();
    g_callbacks = {};
    g_running = false;
}

bool web_server::is_running()
{
    return g_running;
}
