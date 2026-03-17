#pragma once

#include <functional>

// Callback type: idle monitor calls this to inject JS into the Teams frame
using IdleJsCallback = std::function<void(const char* js)>;

// Start the idle activity monitor.
// idle_timeout_sec: seconds of inactivity before pausing the override (0 = never pause).
// js_callback: called on the main thread to inject JS into the Teams frame.
void idle_monitor_start(int idle_timeout_sec, IdleJsCallback js_callback);

// Stop the idle monitor and clean up the timer.
void idle_monitor_stop();

// Get system idle time in seconds (via GNOME Mutter DBus).
int get_system_idle_seconds();

// Check if the screen is locked (via freedesktop/GNOME ScreenSaver DBus).
bool is_screen_locked();

// JS to inject: override Page Visibility API so Teams thinks window is always visible.
const char* idle_get_visibility_override_js();

// JS to inject: restore native visibility state so Teams can detect inactivity.
const char* idle_get_visibility_restore_js();
