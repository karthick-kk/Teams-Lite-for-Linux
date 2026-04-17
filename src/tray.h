#pragma once

#include "include/cef_browser.h"
#include "include/views/cef_window.h"
#include "config.h"
#include <string>
#include <functional>
#include <vector>

// Callback type for theme changes
using ThemeChangeCallback = std::function<void(const std::string& theme_name)>;

// Callback type for app restart (e.g. VAAPI toggle)
using RestartCallback = std::function<void()>;

// Initialize the system tray icon
void tray_init(CefRefPtr<CefBrowser> browser, CefRefPtr<CefWindow> window,
               const TflConfig& config, const std::vector<std::string>& themes,
               ThemeChangeCallback on_theme_change,
               RestartCallback on_restart);

// Update the tray tooltip text
void tray_set_tooltip(const std::string& text);

// Check if quit was requested from tray
bool tray_quit_requested();

// Set the quit flag (used by Ctrl+Q and tray Quit)
void tray_request_quit();

// Cleanup tray on shutdown
void tray_shutdown();
