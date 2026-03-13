#pragma once

#include <string>
#include <functional>

// Initialize libnotify
void notifications_init();

// Set callback for when a notification is clicked
void notifications_set_click_handler(std::function<void()> handler);

// Show a desktop notification (clicking it triggers the click handler)
void notifications_show(const std::string& title, const std::string& body);

// Cleanup
void notifications_shutdown();
