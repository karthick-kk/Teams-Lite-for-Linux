#include "app.h"

TflApp::TflApp(const TflConfig& config) : config_(config) {}

void TflApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {

    // Native Wayland support
    command_line->AppendSwitchWithValue("ozone-platform", "wayland");
    command_line->AppendSwitch("enable-wayland-ime");

    // HiDPI — let CEF auto-detect scale factor
    command_line->AppendSwitch("enable-features=UseOzonePlatform,WaylandWindowDecorations");

    // GPU acceleration
    command_line->AppendSwitch("enable-gpu");
    command_line->AppendSwitch("enable-gpu-rasterization");

    // Custom user-agent
    command_line->AppendSwitchWithValue("user-agent", config_.user_agent);

    // Disable sandbox (we're not shipping chrome-sandbox suid)
    command_line->AppendSwitch("no-sandbox");

    // Enable screen sharing
    command_line->AppendSwitch("enable-usermedia-screen-capturing");
    command_line->AppendSwitchWithValue("auto-select-desktop-capture-source", "Entire screen");
}
