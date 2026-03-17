#pragma once

#include <string>

struct TflConfig {
    std::string url = "https://teams.cloud.microsoft";
    std::string user_agent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36";
    std::string cache_dir;   // XDG_CACHE_HOME/tfl
    std::string config_dir;  // XDG_CONFIG_HOME/tfl
    int width = 1280;
    int height = 800;
    int x = -1;  // -1 = auto center
    int y = -1;
    bool enable_dev_tools = false;
    bool minimized_to_tray = false;
    bool close_to_tray = true;  // X button hides to tray instead of quitting
    int idle_timeout = 300;          // seconds before allowing "Away" (0 = always available)
};

TflConfig load_config();

// Save window state (size + position) to config dir
void save_window_state(const TflConfig& config, int x, int y, int w, int h);

// Try to acquire single-instance lock. Returns true if we're the only instance.
bool acquire_instance_lock(const std::string& config_dir);
