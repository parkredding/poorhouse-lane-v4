// ap_mode.cpp — WiFi Access Point management
//
// Manages the lifecycle of hostapd + dnsmasq to create a WiFi AP
// with captive portal DNS redirection.  All DNS queries resolve to
// the device IP (192.168.4.1) so connected clients get redirected
// to the configuration portal automatically.

#include "ap_mode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

static const char* AP_IP        = "192.168.4.1";
static const char* AP_IFACE     = "wlan0";
static const char* HOSTAPD_CONF = "/tmp/dubsiren_hostapd.conf";
static const char* DNSMASQ_CONF = "/tmp/dubsiren_dnsmasq.conf";

static pid_t g_hostapd_pid  = 0;
static pid_t g_dnsmasq_pid  = 0;
static bool  g_active       = false;
static std::string g_ssid;

// ─── Read MAC address from sysfs ────────────────────────────────────

static std::string read_mac()
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", AP_IFACE);

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

static bool write_hostapd_conf(const std::string& ssid)
{
    FILE* f = fopen(HOSTAPD_CONF, "w");
    if (!f) {
        fprintf(stderr, "AP: Failed to write %s\n", HOSTAPD_CONF);
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
        "wpa=0\n",    // open network (no password needed for config)
        AP_IFACE, ssid.c_str());

    fclose(f);
    return true;
}

static bool write_dnsmasq_conf()
{
    FILE* f = fopen(DNSMASQ_CONF, "w");
    if (!f) {
        fprintf(stderr, "AP: Failed to write %s\n", DNSMASQ_CONF);
        return false;
    }

    fprintf(f,
        "interface=%s\n"
        "dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h\n"
        "address=/#/%s\n"      // captive portal: all DNS → device
        "no-resolv\n"
        "no-poll\n"
        "log-queries\n"
        "log-dhcp\n",
        AP_IFACE, AP_IP);

    fclose(f);
    return true;
}

// ─── Process management ─────────────────────────────────────────────

static pid_t start_process(const char* program, const char* arg)
{
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        // Redirect stdout/stderr to /dev/null to avoid cluttering siren output
        freopen("/dev/null", "w", stdout);
        // Keep stderr for debugging
        execlp(program, program, arg, nullptr);
        _exit(127);  // exec failed
    }
    return pid;
}

static void stop_process(pid_t& pid)
{
    if (pid > 0) {
        kill(pid, SIGTERM);
        int status;
        waitpid(pid, &status, 0);
        pid = 0;
    }
}

// ─── Public interface ───────────────────────────────────────────────

bool ap_mode::start_ap()
{
    if (g_active) return true;

    std::string suffix = get_mac_suffix();
    g_ssid = "Poorhouse-Siren-Config-" + suffix;

    printf("AP: Starting access point '%s' on %s\n", g_ssid.c_str(), AP_IFACE);

    // 1. Stop any existing networking on the interface
    system("sudo killall wpa_supplicant 2>/dev/null");
    usleep(500000);  // 500ms

    // 2. Configure the interface
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "sudo ip addr flush dev %s 2>/dev/null", AP_IFACE);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "sudo ip addr add %s/24 dev %s", AP_IP, AP_IFACE);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "sudo ip link set %s up", AP_IFACE);
    system(cmd);

    // 3. Write config files
    if (!write_hostapd_conf(g_ssid)) return false;
    if (!write_dnsmasq_conf()) return false;

    // 4. Start hostapd
    g_hostapd_pid = start_process("sudo", "hostapd");
    if (g_hostapd_pid <= 0) {
        // Try launching with full config path
        pid_t pid = fork();
        if (pid == 0) {
            execlp("sudo", "sudo", "hostapd", HOSTAPD_CONF, nullptr);
            _exit(127);
        }
        g_hostapd_pid = pid;
    } else {
        // The simple start_process doesn't work for sudo + args, use system
        stop_process(g_hostapd_pid);
    }

    // Use system() + background for reliability
    snprintf(cmd, sizeof(cmd), "sudo hostapd %s -B 2>/dev/null", HOSTAPD_CONF);
    if (system(cmd) != 0) {
        fprintf(stderr, "AP: Failed to start hostapd\n");
        return false;
    }
    g_hostapd_pid = 1;  // sentinel — managed by hostapd -B

    usleep(500000);  // let hostapd settle

    // 5. Start dnsmasq
    snprintf(cmd, sizeof(cmd), "sudo dnsmasq -C %s --pid-file=/tmp/dubsiren_dnsmasq.pid 2>/dev/null",
             DNSMASQ_CONF);
    if (system(cmd) != 0) {
        fprintf(stderr, "AP: Failed to start dnsmasq\n");
        return false;
    }
    g_dnsmasq_pid = 1;  // sentinel

    g_active = true;
    printf("AP: Access point active — SSID: %s  IP: %s\n", g_ssid.c_str(), AP_IP);
    return true;
}

void ap_mode::stop_ap()
{
    if (!g_active) return;

    printf("AP: Stopping access point\n");

    // Kill hostapd and dnsmasq
    system("sudo killall hostapd 2>/dev/null");
    system("sudo killall dnsmasq 2>/dev/null");
    usleep(500000);

    g_hostapd_pid = 0;
    g_dnsmasq_pid = 0;

    // Clean up config files
    unlink(HOSTAPD_CONF);
    unlink(DNSMASQ_CONF);
    unlink("/tmp/dubsiren_dnsmasq.pid");

    // Flush interface
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "sudo ip addr flush dev %s 2>/dev/null", AP_IFACE);
    system(cmd);

    // Try to restore normal WiFi
    system("sudo systemctl restart wpa_supplicant 2>/dev/null");

    g_active = false;
    printf("AP: Access point stopped\n");
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
