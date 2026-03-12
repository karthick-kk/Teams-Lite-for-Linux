#pragma once

#include "include/cef_browser.h"
#include "include/views/cef_window.h"
#include <string>

// Initialize the system tray icon
void tray_init(CefRefPtr<CefBrowser> browser, CefRefPtr<CefWindow> window);

// Update the tray tooltip text
void tray_set_tooltip(const std::string& text);

// Check if quit was requested from tray
bool tray_quit_requested();

// Cleanup tray on shutdown
void tray_shutdown();
