#pragma once

// Start the idle activity monitor
void idle_monitor_start();

// Stop the idle monitor
void idle_monitor_stop();

// Get system idle time in seconds (via GNOME Mutter DBus)
int get_system_idle_seconds();

// Get the JS to inject for visibility override
const char* idle_get_visibility_override_js();
