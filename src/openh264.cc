#include "openh264.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Cisco's pre-built OpenH264 binary — royalty-free under Cisco's MPEG-LA license
static const char* OPENH264_VERSION = "2.6.0";
static const char* OPENH264_URL = "http://ciscobinary.openh264.org/libopenh264-2.6.0-linux64.8.so.bz2";
static const char* OPENH264_SO = "libopenh264.so.8";

std::string openh264_ensure_available(const std::string& cache_dir) {
    std::string lib_dir = cache_dir + "/openh264";
    std::string lib_path = lib_dir + "/" + OPENH264_SO;

    if (fs::exists(lib_path)) {
        fprintf(stderr, "[tfl] OpenH264 found: %s\n", lib_path.c_str());
        return lib_dir;
    }

    fprintf(stderr, "[tfl] Downloading Cisco OpenH264 %s (royalty-free binary)...\n", OPENH264_VERSION);
    fs::create_directories(lib_dir);

    std::string bz2_path = lib_dir + "/libopenh264.so.bz2";

    // Download
    std::string cmd = "curl -fSL '" + std::string(OPENH264_URL) + "' -o '" + bz2_path + "' 2>/dev/null";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        fprintf(stderr, "[tfl] OpenH264 download failed (code %d)\n", ret);
        return "";
    }

    // Decompress
    cmd = "bzip2 -df '" + bz2_path + "' 2>/dev/null";
    ret = system(cmd.c_str());
    if (ret != 0) {
        fprintf(stderr, "[tfl] OpenH264 decompression failed\n");
        return "";
    }

    // Rename to expected name
    std::string decompressed = lib_dir + "/libopenh264.so";
    if (fs::exists(decompressed) && !fs::exists(lib_path)) {
        fs::rename(decompressed, lib_path);
    }

    // Create libopenh264.so symlink
    std::string symlink_path = lib_dir + "/libopenh264.so";
    if (!fs::exists(symlink_path)) {
        fs::create_symlink(OPENH264_SO, symlink_path);
    }

    if (fs::exists(lib_path)) {
        fprintf(stderr, "[tfl] OpenH264 installed: %s\n", lib_path.c_str());
        return lib_dir;
    }

    fprintf(stderr, "[tfl] OpenH264 installation failed\n");
    return "";
}
