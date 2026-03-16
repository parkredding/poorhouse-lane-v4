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
static const char* HOSTAPD_PID  = "/tmp/dubsiren_hostapd.pid";
static const char* DNSMASQ_CONF = "/tmp/dubsiren_dnsmasq.conf";
static const char* DNSMASQ_PID  = "/tmp/dubsiren_dnsmasq.pid";

static bool  g_active       = false;
static std::string g_ssid;
static std::string g_iface;   // actual interface used (ap0 or wlan0)
static bool g_using_wlan0 = false;  // true if we fell back to wlan0

// "sudo " when not root, "" when root — prefixed to privileged commands
static const char* SUDO = (geteuid() == 0) ? "" : "sudo ";

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
        "wpa=0\n",
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
    snprintf(cmd, sizeof(cmd), "%siw dev ap0 del 2>/dev/null", SUDO);
    system(cmd);
    usleep(300000);

    // Kill our own stale hostapd if it holds the phy lock (PID-file based)
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, HOSTAPD_PID);
    system(cmd);
    usleep(200000);

    // Try iw dev first, then iw phy as fallback
    snprintf(cmd, sizeof(cmd), "%siw dev %s interface add ap0 type __ap", SUDO, AP_PHY_IFACE);
    int ret = system(cmd);
    if (ret != 0) {
        slog("AP: 'iw dev' failed — trying 'iw phy'");
        // Find the phy name for wlan0 and try phy-based creation
        snprintf(cmd, sizeof(cmd), "%siw phy $(%siw dev wlan0 info | awk '/wiphy/{print \"phy\"$2}') "
                      "interface add ap0 type __ap 2>/dev/null", SUDO, SUDO);
        ret = system(cmd);
        if (ret != 0) {
            slog("AP: 'iw phy' also failed (exit %d)", ret);
            return false;
        }
    }

    // Tell NetworkManager to ignore the virtual interface
    snprintf(cmd, sizeof(cmd), "%snmcli device set ap0 managed no 2>/dev/null", SUDO);
    system(cmd);
    usleep(200000);

    // Configure IP
    snprintf(cmd, sizeof(cmd), "%sip addr add %s/24 dev ap0", SUDO, AP_IP);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%sip link set ap0 up", SUDO);
    system(cmd);

    // Verify ap0 actually came up
    ret = system("ip link show ap0 2>/dev/null | grep -q 'state UP\\|state UNKNOWN'");
    if (ret != 0) {
        slog("AP: ap0 created but failed to come up");
        snprintf(cmd, sizeof(cmd), "%siw dev ap0 del 2>/dev/null", SUDO);
        system(cmd);
        return false;
    }

    return true;
}

// Fall back to using wlan0 directly — disconnects from WiFi
static bool setup_wlan0_ap()
{
    slog("AP: Using wlan0 directly (WiFi will disconnect)");

    // Stop anything managing wlan0
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%swpa_cli -i wlan0 disconnect 2>/dev/null", SUDO);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%snmcli device disconnect wlan0 2>/dev/null", SUDO);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%ssystemctl stop wpa_supplicant 2>/dev/null", SUDO);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%snmcli device set wlan0 managed no 2>/dev/null", SUDO);
    system(cmd);
    usleep(500000);

    // Flush existing IP and configure AP IP
    snprintf(cmd, sizeof(cmd), "%sip addr flush dev wlan0", SUDO);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%sip addr add %s/24 dev wlan0", SUDO, AP_IP);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%sip link set wlan0 up", SUDO);
    system(cmd);
    usleep(200000);

    return true;
}

// Restore wlan0 to managed mode after AP shutdown
static void restore_wlan0()
{
    slog("AP: Restoring wlan0 to managed mode");

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%sip addr flush dev wlan0 2>/dev/null", SUDO);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%snmcli device set wlan0 managed yes 2>/dev/null", SUDO);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%ssystemctl start wpa_supplicant 2>/dev/null", SUDO);
    system(cmd);
    // Give NetworkManager a kick to reconnect
    snprintf(cmd, sizeof(cmd), "%snmcli device connect wlan0 2>/dev/null", SUDO);
    system(cmd);
}

// ─── Public interface ───────────────────────────────────────────────

bool ap_mode::start_ap()
{
    if (g_active) return true;

    std::string suffix = get_mac_suffix();
    g_ssid = "Poorhouse-Siren-Config-" + suffix;

    // 1. Clean up any previous AP state
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, HOSTAPD_PID);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, DNSMASQ_PID);
    system(cmd);
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
    snprintf(cmd, sizeof(cmd), "%shostapd %s -B -P %s", SUDO, HOSTAPD_CONF, HOSTAPD_PID);
    int ret = system(cmd);
    if (ret != 0) {
        slog("AP: Failed to start hostapd (exit %d)", ret);
        if (g_using_wlan0) restore_wlan0();
        return false;
    }
    usleep(1000000);  // 1s — let hostapd fully settle

    // 5. Start dnsmasq
    snprintf(cmd, sizeof(cmd), "%sdnsmasq -C %s --pid-file=%s",
             SUDO, DNSMASQ_CONF, DNSMASQ_PID);
    ret = system(cmd);
    if (ret != 0) {
        slog("AP: Failed to start dnsmasq (exit %d)", ret);
        snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, HOSTAPD_PID);
        system(cmd);
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
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, HOSTAPD_PID);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "%spkill -F %s 2>/dev/null", SUDO, DNSMASQ_PID);
    system(cmd);
    usleep(500000);

    // Clean up config and PID files
    unlink(HOSTAPD_CONF);
    unlink(DNSMASQ_CONF);
    unlink(HOSTAPD_PID);
    unlink(DNSMASQ_PID);

    if (g_using_wlan0) {
        // Restore wlan0 to managed mode so it reconnects to WiFi
        restore_wlan0();
    } else {
        // Remove the virtual AP interface
        snprintf(cmd, sizeof(cmd), "%siw dev ap0 del", SUDO);
        system(cmd);
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
