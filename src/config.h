#pragma once

#include <string>

struct TflConfig {
    std::string url = "https://teams.cloud.microsoft";
    std::string user_agent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36";
    std::string cache_dir;   // XDG_CACHE_HOME/tfl
    std::string config_dir;  // XDG_CONFIG_HOME/tfl
    int width = 1280;
    int height = 800;
    bool enable_dev_tools = false;
    bool minimized_to_tray = false;  // start minimized
};

TflConfig load_config();
