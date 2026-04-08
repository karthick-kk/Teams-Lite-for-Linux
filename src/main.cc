#include "app.h"
#include "client.h"
#include "window.h"
#include "config.h"
#include "idle.h"
#include "notifications.h"
#include "theme.h"
#include "openh264.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include <cstdio>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);

    // Subprocesses don't need full config — just enough for CefExecuteProcess
    TflConfig config;
    CefRefPtr<TflApp> app(new TflApp(config));

    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    // --- Main browser process: load full config ---
    config = load_config();

    // Single-instance lock
    if (!acquire_instance_lock(config.config_dir)) {
        fprintf(stderr, "[tfl] Another instance is already running\n");
        return 0;
    }

    // Download Cisco's OpenH264 if not present (royalty-free binary)
    std::string openh264_dir = openh264_ensure_available(config.cache_dir);
    if (!openh264_dir.empty()) {
        // Add to library path so CEF's WebRTC can dlopen libopenh264.so
        std::string ld_path = openh264_dir;
        const char* existing = std::getenv("LD_LIBRARY_PATH");
        if (existing && existing[0]) {
            ld_path += ":" + std::string(existing);
        }
        setenv("LD_LIBRARY_PATH", ld_path.c_str(), 1);
    }

    fprintf(stderr, "[tfl] Starting Teams Lite for Linux (CEF)\n");

    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;
    settings.windowless_rendering_enabled = false;

    CefString(&settings.root_cache_path) = config.cache_dir;
    CefString(&settings.cache_path) = config.cache_dir + "/Default";
    CefString(&settings.locale) = "en-US";
    CefString(&settings.accept_language_list) = "en-US,en";
    CefString(&settings.log_file) = config.cache_dir + "/cef.log";
    settings.log_severity = LOGSEVERITY_WARNING;
    settings.remote_debugging_port = 9222;
    CefString(&settings.user_agent) = config.user_agent;

    // Memory optimizations: limit background renderer priority
    settings.background_color = 0;  // transparent — avoids extra compositing

    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    std::string exe_dir;
    if (len > 0) {
        exe_path[len] = '\0';
        CefString(&settings.browser_subprocess_path) = exe_path;
        exe_dir = fs::path(exe_path).parent_path().string();
    }

    if (!exe_dir.empty()) {
        CefString(&settings.resources_dir_path) = exe_dir;
        CefString(&settings.locales_dir_path) = exe_dir + "/locales";
    }

    if (!CefInitialize(main_args, settings, app, nullptr)) {
        fprintf(stderr, "[tfl] CefInitialize failed\n");
        return 1;
    }

    fprintf(stderr, "[tfl] CEF initialized, cache: %s\n", config.cache_dir.c_str());

    notifications_init();

    // Create browser using Views framework (no browser chrome — clean app window)
    CefRefPtr<TflClient> client(new TflClient(config));

    CefBrowserSettings browser_settings;
    browser_settings.javascript_access_clipboard = STATE_ENABLED;
    browser_settings.local_storage = STATE_ENABLED;

    CefRefPtr<TflBrowserViewDelegate> bv_delegate(new TflBrowserViewDelegate(config));
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        client, config.url, browser_settings, nullptr, nullptr, bv_delegate);

    auto available_themes = theme_list_available();
    CefWindow::CreateTopLevelWindow(
        new TflWindowDelegate(browser_view, config, client, available_themes));

    // Start idle-aware presence monitor
    idle_monitor_start(config.idle_timeout, [client](const char* js) {
        client->InjectJS(js);
    });

    fprintf(stderr, "[tfl] Browser loading: %s\n", config.url.c_str());

    CefRunMessageLoop();
    idle_monitor_stop();

    // Release all CEF refs before shutdown
    browser_view = nullptr;
    client = nullptr;
    bv_delegate = nullptr;
    app = nullptr;

    notifications_shutdown();
    CefShutdown();
    fprintf(stderr, "[tfl] Shutdown complete\n");
    return 0;
}
