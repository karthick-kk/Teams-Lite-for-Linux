#pragma once

#include <string>
#include <vector>

// Find the path to a theme CSS file by name (searches exe-relative and system paths)
std::string theme_find_path(const std::string& theme_name);

// Load theme CSS content by name. Returns empty string if not found.
std::string theme_load_css(const std::string& theme_name);

// List available theme names (scans theme directories)
std::vector<std::string> theme_list_available();

// Build JS to inject or replace the theme <style id="tfl-theme"> element
std::string theme_get_inject_js(const std::string& css);

// Build JS to remove the theme style element (revert to Teams default)
std::string theme_get_remove_js();
