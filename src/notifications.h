#pragma once

#include <string>

// Initialize libnotify
void notifications_init();

// Show a desktop notification
void notifications_show(const std::string& title, const std::string& body);

// Cleanup
void notifications_shutdown();
