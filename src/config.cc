#include "config.h"
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

namespace fs = std::filesystem;

// Trim whitespace from both ends
static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Parse a simple INI-style config file (key = value, # comments)
static void parse_config_file(const std::string& path, TflConfig& cfg) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        // Strip quotes
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }

        if (key == "url") cfg.url = val;
        else if (key == "user_agent") cfg.user_agent = val;
        else if (key == "width") cfg.width = std::atoi(val.c_str());
        else if (key == "height") cfg.height = std::atoi(val.c_str());
        else if (key == "dev_tools") cfg.enable_dev_tools = (val == "true" || val == "1");
        else if (key == "start_minimized") cfg.minimized_to_tray = (val == "true" || val == "1");
        else if (key == "close_to_tray") cfg.close_to_tray = (val == "true" || val == "1");
        else if (key == "x") cfg.x = std::atoi(val.c_str());
        else if (key == "y") cfg.y = std::atoi(val.c_str());
    }
}

// Write default config file if it doesn't exist
static void write_default_config(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return;

    file << "# tfl — Teams Lite for Linux configuration\n"
         << "# See https://github.com/karthick-kk/Teams-Lite-for-Linux for documentation\n"
         << "\n"
         << "# Teams URL\n"
         << "# url = https://teams.cloud.microsoft\n"
         << "\n"
         << "# Window size\n"
         << "# width = 1280\n"
         << "# height = 800\n"
         << "\n"
         << "# Start minimized to tray\n"
         << "# start_minimized = false\n"
         << "\n"
         << "# Close to tray (X button hides instead of quitting)\n"
         << "# close_to_tray = true\n"
         << "\n"
         << "# Enable developer tools (F12)\n"
         << "# dev_tools = false\n";
}

TflConfig load_config() {
    TflConfig cfg;

    // XDG directories
    const char* home = std::getenv("HOME");
    std::string home_dir = home ? home : "/tmp";

    const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
    cfg.cache_dir = xdg_cache && xdg_cache[0]
        ? std::string(xdg_cache) + "/tfl"
        : home_dir + "/.cache/tfl";

    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    cfg.config_dir = xdg_config && xdg_config[0]
        ? std::string(xdg_config) + "/tfl"
        : home_dir + "/.config/tfl";

    fs::create_directories(cfg.cache_dir);
    fs::create_directories(cfg.config_dir);

    // Load config file
    std::string config_path = cfg.config_dir + "/config";
    if (fs::exists(config_path)) {
        parse_config_file(config_path, cfg);
        fprintf(stderr, "[tfl] Config loaded: %s\n", config_path.c_str());
    } else {
        write_default_config(config_path);
        fprintf(stderr, "[tfl] Default config written: %s\n", config_path.c_str());
    }

    // Environment overrides (highest priority)
    if (const char* url = std::getenv("TFL_URL")) cfg.url = url;
    if (const char* w = std::getenv("TFL_WIDTH")) cfg.width = std::atoi(w);
    if (const char* h = std::getenv("TFL_HEIGHT")) cfg.height = std::atoi(h);
    if (std::getenv("TFL_DEV_TOOLS")) cfg.enable_dev_tools = true;

    // Load saved window state (overrides config width/height)
    std::string state_path = cfg.config_dir + "/window-state";
    if (fs::exists(state_path)) {
        TflConfig state;
        parse_config_file(state_path, state);
        if (state.width > 0) cfg.width = state.width;
        if (state.height > 0) cfg.height = state.height;
        cfg.x = state.x;
        cfg.y = state.y;
    }

    return cfg;
}

void save_window_state(const TflConfig& config, int x, int y, int w, int h) {
    std::string path = config.config_dir + "/window-state";
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "x = " << x << "\n"
         << "y = " << y << "\n"
         << "width = " << w << "\n"
         << "height = " << h << "\n";
}

static int g_lock_fd = -1;

bool acquire_instance_lock(const std::string& config_dir) {
    std::string lock_path = config_dir + "/tfl.lock";
    g_lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
    if (g_lock_fd < 0) return false;

    if (flock(g_lock_fd, LOCK_EX | LOCK_NB) != 0) {
        close(g_lock_fd);
        g_lock_fd = -1;
        return false;  // another instance holds the lock
    }
    return true;
}
