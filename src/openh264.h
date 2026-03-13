#pragma once

#include <string>

// Download Cisco's pre-built OpenH264 library if not present.
// Returns the directory containing libopenh264.so, or empty on failure.
std::string openh264_ensure_available(const std::string& cache_dir);
