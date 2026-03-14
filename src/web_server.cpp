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

static void setup_api(httplib::Server& svr, const web_server::Callbacks& cb)
{
    // Main page
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web_ui::INDEX_HTML, "text/html");
    });

    // ── Preset endpoints ────────────────────────────────────────────

    svr.Get("/api/presets", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.get_all_presets) {
            json_response(res, cb.get_all_presets());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Get("/api/presets/current", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.get_preset_state) {
            json_response(res, cb.get_preset_state());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/presets/apply", [&cb](const httplib::Request& req, httplib::Response& res) {
        if (!cb.apply_preset) { json_error(res, "not implemented", 501); return; }
        // Parse category and index from body (simple key=value)
        auto category = req.get_param_value("category");
        auto index_str = req.get_param_value("index");
        int index = 0;
        try { index = std::stoi(index_str); } catch (...) {}
        if (cb.apply_preset(category, index)) {
            json_ok(res);
        } else {
            json_error(res, "failed to apply preset");
        }
    });

    svr.Post("/api/presets/save", [&cb](const httplib::Request& req, httplib::Response& res) {
        if (!cb.save_to_slot) { json_error(res, "not implemented", 501); return; }
        auto slot_str = req.get_param_value("slot");
        auto name = req.get_param_value("name");
        int slot = 0;
        try { slot = std::stoi(slot_str); } catch (...) {}
        if (cb.save_to_slot(slot, name)) {
            json_ok(res, "saved");
        } else {
            json_error(res, "failed to save");
        }
    });

    // ── Siren options ───────────────────────────────────────────────

    svr.Get("/api/siren/options", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.get_siren_options) {
            json_response(res, cb.get_siren_options());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/siren/options", [&cb](const httplib::Request& req, httplib::Response& res) {
        if (!cb.set_siren_options) { json_error(res, "not implemented", 501); return; }
        if (cb.set_siren_options(req.body)) {
            json_ok(res, "applied");
        } else {
            json_error(res, "failed to apply options");
        }
    });

    // ── Preview control ─────────────────────────────────────────────

    svr.Post("/api/preview/start", [&cb](const httplib::Request& req, httplib::Response& res) {
        if (cb.preview_start) {
            cb.preview_start(req.body);
            json_ok(res, "previewing");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/preview/stop", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.preview_stop) {
            cb.preview_stop();
            json_ok(res, "stopped");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── WiFi operations ─────────────────────────────────────────────

    svr.Get("/api/wifi/scan", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.wifi_scan) {
            json_response(res, cb.wifi_scan());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/wifi/connect", [&cb](const httplib::Request& req, httplib::Response& res) {
        if (!cb.wifi_connect) { json_error(res, "not implemented", 501); return; }
        auto ssid = req.get_param_value("ssid");
        auto password = req.get_param_value("password");
        if (cb.wifi_connect(ssid, password)) {
            json_ok(res, "credentials saved");
        } else {
            json_error(res, "failed to save credentials");
        }
    });

    svr.Get("/api/wifi/status", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.wifi_status) {
            json_response(res, cb.wifi_status());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── Update operations ───────────────────────────────────────────

    svr.Post("/api/update/check", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.update_check) {
            json_response(res, cb.update_check());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/update/install", [&cb](const httplib::Request&, httplib::Response& res) {
        if (!cb.update_install) { json_error(res, "not implemented", 501); return; }
        if (cb.update_install()) {
            json_ok(res, "update started");
        } else {
            json_error(res, "update failed");
        }
    });

    svr.Get("/api/update/status", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.update_status) {
            json_response(res, cb.update_status());
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    // ── Backup/restore ──────────────────────────────────────────────

    svr.Get("/api/backup", [&cb](const httplib::Request&, httplib::Response& res) {
        if (cb.backup_create) {
            auto data = cb.backup_create();
            res.set_header("Content-Disposition",
                           "attachment; filename=\"dubsiren-backup.json\"");
            res.set_content(data, "application/json");
        } else {
            json_error(res, "not implemented", 501);
        }
    });

    svr.Post("/api/restore", [&cb](const httplib::Request& req, httplib::Response& res) {
        if (!cb.backup_restore) { json_error(res, "not implemented", 501); return; }
        if (cb.backup_restore(req.body)) {
            json_ok(res, "restored");
        } else {
            json_error(res, "restore failed");
        }
    });

    // ── Exit AP mode ────────────────────────────────────────────────

    svr.Post("/api/exit", [&cb](const httplib::Request&, httplib::Response& res) {
        json_ok(res, "exiting AP mode");
        if (cb.exit_ap) {
            // Schedule exit after response is sent
            std::thread([&cb]() {
                usleep(500000);  // 500ms delay so response reaches client
                cb.exit_ap();
            }).detach();
        }
    });
}

// ─── Public interface ───────────────────────────────────────────────

bool web_server::start(int port, const Callbacks& cb)
{
    if (g_running) return true;

    g_server = std::make_unique<httplib::Server>();

    setup_captive_portal(*g_server);
    setup_api(*g_server, cb);

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
    g_running = false;
}

bool web_server::is_running()
{
    return g_running;
}
