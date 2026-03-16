// ap_mode.cpp — WiFi Access Point management
//
// Manages the lifecycle of hostapd + dnsmasq to create a WiFi AP
// with captive portal DNS redirection.  All DNS queries resolve to
// the device IP (192.168.4.1) so connected clients get redirected
// to the configuration portal automatically.
//
// Strategy:
//   1. Try creating a virtual ap0 interface (concurrent STA+AP — keeps SSH)
//   2. If that fails, fall back to using wlan0 directly (drops WiFi, but
//      works on all Pi models including Pi Zero 2W)

#include "ap_mode.h"
#include "siren_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>

static const char* AP_IP        = "192.168.4.1";
static const char* AP_PHY_IFACE = "wlan0";     // physical interface
static const char* HOSTAPD_CONF = "/tmp/dubsiren_hostapd.conf";
static const char* DNSMASQ_CONF = "/tmp/dubsiren_dnsmasq.conf";

static bool  g_active       = false;
static std::string g_ssid;
static std::string g_iface;   // actual interface used (ap0 or wlan0)
static bool g_using_wlan0 = false;  // true if we fell back to wlan0

// ─── Read MAC address from sysfs ────────────────────────────────────

static std::string read_mac()
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", AP_PHY_IFACE);

    std::ifstream f(path);
    if (!f.is_open()) return "00:00:00:00:00:00";

    std::string mac;
    std::getline(f, mac);
    return mac;
}

std::string ap_mode::get_mac_suffix()
{
    std::string mac = read_mac();
    // MAC format: "aa:bb:cc:dd:ee:ff" — take last 4 hex chars (ee + ff, no colons)
    std::string suffix;
    for (char c : mac) {
        if (c != ':') suffix += c;
    }
    // Last 4 characters, uppercase
    if (suffix.size() >= 4) {
        suffix = suffix.substr(suffix.size() - 4);
        for (auto& c : suffix) c = static_cast<char>(toupper(c));
    } else {
        suffix = "0000";
    }
    return suffix;
}

// ─── Write config files ─────────────────────────────────────────────

static bool write_hostapd_conf(const std::string& ssid, const char* iface)
{
    FILE* f = fopen(HOSTAPD_CONF, "w");
    if (!f) {
        slog("AP: Failed to write %s", HOSTAPD_CONF);
        return false;
    }

    fprintf(f,
        "interface=%s\n"
        "driver=nl80211\n"
        "ssid=%s\n"
        "hw_mode=g\n"
        "channel=6\n"
        "wmm_enabled=0\n"
        "macaddr_acl=0\n"
        "auth_algs=1\n"
        "ignore_broadcast_ssid=0\n"
        "wpa=0\n"
        "pid_file=/tmp/dubsiren_hostapd.pid\n",
        iface, ssid.c_str());

    fclose(f);
    return true;
}

static bool write_dnsmasq_conf(const char* iface)
{
    FILE* f = fopen(DNSMASQ_CONF, "w");
    if (!f) {
        slog("AP: Failed to write %s", DNSMASQ_CONF);
        return false;
    }

    fprintf(f,
        "interface=%s\n"
        "bind-interfaces\n"
        "listen-address=%s\n"
        "dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h\n"
        "address=/#/%s\n"            // captive portal: all DNS → device
        "no-resolv\n"
        "no-poll\n"
        "log-queries\n"
        "log-dhcp\n",
        iface, AP_IP, AP_IP);

    fclose(f);
    return true;
}

// ─── Interface setup helpers ─────────────────────────────────────────

// Try creating a virtual ap0 interface for concurrent STA+AP
static bool try_virtual_ap()
{
    char cmd[512];

    // Remove stale virtual interface from previous run
    system("iw dev ap0 del 2>/dev/null");
    usleep(200000);

    snprintf(cmd, sizeof(cmd), "iw dev %s interface add ap0 type __ap", AP_PHY_IFACE);
    int ret = system(cmd);
    if (ret != 0) return false;

    // Tell NetworkManager to ignore the virtual interface
    system("nmcli device set ap0 managed no 2>/dev/null");
    usleep(200000);

    // Configure IP
    snprintf(cmd, sizeof(cmd), "ip addr add %s/24 dev ap0", AP_IP);
    system(cmd);
    system("ip link set ap0 up");

    return true;
}

// Fall back to using wlan0 directly — disconnects from WiFi
static bool setup_wlan0_ap()
{
    slog("AP: Using wlan0 directly (WiFi will disconnect)");

    // Stop anything managing wlan0
    system("wpa_cli -i wlan0 disconnect 2>/dev/null");
    system("nmcli device disconnect wlan0 2>/dev/null");
    system("systemctl stop wpa_supplicant 2>/dev/null");
    system("nmcli device set wlan0 managed no 2>/dev/null");
    usleep(500000);

    // Flush existing IP and configure AP IP
    char cmd[256];
    system("ip addr flush dev wlan0");
    snprintf(cmd, sizeof(cmd), "ip addr add %s/24 dev wlan0", AP_IP);
    system(cmd);
    system("ip link set wlan0 up");
    usleep(200000);

    return true;
}

// Restore wlan0 to managed mode after AP shutdown
static void restore_wlan0()
{
    slog("AP: Restoring wlan0 to managed mode");

    system("ip addr flush dev wlan0 2>/dev/null");
    system("nmcli device set wlan0 managed yes 2>/dev/null");
    system("systemctl start wpa_supplicant 2>/dev/null");
    // Give NetworkManager a kick to reconnect
    system("nmcli device connect wlan0 2>/dev/null");
}

// ─── Public interface ───────────────────────────────────────────────

bool ap_mode::start_ap()
{
    if (g_active) return true;

    std::string suffix = get_mac_suffix();
    g_ssid = "Poorhouse-Siren-Config-" + suffix;

    // 1. Clean up any previous AP state
    system("pkill -F /tmp/dubsiren_hostapd.pid 2>/dev/null");
    system("pkill -F /tmp/dubsiren_dnsmasq.pid 2>/dev/null");
    usleep(500000);

    // 2. Try virtual ap0 first, fall back to wlan0
    if (try_virtual_ap()) {
        g_iface = "ap0";
        g_using_wlan0 = false;
        slog("AP: Using virtual interface ap0 (WiFi stays connected)");
    } else {
        slog("AP: Virtual ap0 not supported — falling back to wlan0");
        if (!setup_wlan0_ap()) {
            slog("AP: Failed to configure wlan0 for AP mode");
            return false;
        }
        g_iface = "wlan0";
        g_using_wlan0 = true;
    }

    slog("AP: Starting '%s' on %s", g_ssid.c_str(), g_iface.c_str());

    // 3. Write config files
    if (!write_hostapd_conf(g_ssid, g_iface.c_str())) return false;
    if (!write_dnsmasq_conf(g_iface.c_str())) return false;

    // 4. Start hostapd (daemonized with -B)
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "hostapd %s -B", HOSTAPD_CONF);
    int ret = system(cmd);
    if (ret != 0) {
        slog("AP: Failed to start hostapd (exit %d)", ret);
        if (g_using_wlan0) restore_wlan0();
        return false;
    }
    usleep(1000000);  // 1s — let hostapd fully settle

    // 5. Start dnsmasq
    snprintf(cmd, sizeof(cmd), "dnsmasq -C %s --pid-file=/tmp/dubsiren_dnsmasq.pid",
             DNSMASQ_CONF);
    ret = system(cmd);
    if (ret != 0) {
        slog("AP: Failed to start dnsmasq (exit %d)", ret);
        system("pkill -F /tmp/dubsiren_hostapd.pid 2>/dev/null");
        if (g_using_wlan0) restore_wlan0();
        return false;
    }
    g_active = true;
    slog("AP: Access point active — SSID: %s  IP: %s", g_ssid.c_str(), AP_IP);
    return true;
}

void ap_mode::stop_ap()
{
    if (!g_active) return;

    slog("AP: Stopping access point");

    // Kill hostapd and dnsmasq using PID files (avoids killing unrelated processes)
    system("pkill -F /tmp/dubsiren_hostapd.pid 2>/dev/null");
    system("pkill -F /tmp/dubsiren_dnsmasq.pid 2>/dev/null");
    usleep(500000);

    // Clean up config and PID files
    unlink(HOSTAPD_CONF);
    unlink(DNSMASQ_CONF);
    unlink("/tmp/dubsiren_hostapd.pid");
    unlink("/tmp/dubsiren_dnsmasq.pid");

    if (g_using_wlan0) {
        // Restore wlan0 to managed mode so it reconnects to WiFi
        restore_wlan0();
    } else {
        // Remove the virtual AP interface
        system("iw dev ap0 del");
    }

    g_active = false;
    g_using_wlan0 = false;
    slog("AP: Access point stopped");
}

bool ap_mode::is_active()
{
    return g_active;
}

std::string ap_mode::get_ssid()
{
    return g_ssid;
}

const char* ap_mode::get_ip()
{
    return AP_IP;
}
