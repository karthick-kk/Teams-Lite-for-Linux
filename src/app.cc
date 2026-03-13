#include "app.h"

TflApp::TflApp(const TflConfig& config) : config_(config) {}

void TflApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {

    // Native Wayland support
    command_line->AppendSwitchWithValue("ozone-platform", "wayland");
    command_line->AppendSwitch("enable-wayland-ime");

    // HiDPI + spellcheck features
    command_line->AppendSwitchWithValue("enable-features",
        "UseOzonePlatform,WaylandWindowDecorations,SpellcheckServiceMultilingual");

    // GPU acceleration
    command_line->AppendSwitch("enable-gpu");
    command_line->AppendSwitch("enable-gpu-rasterization");

    // Custom user-agent
    command_line->AppendSwitchWithValue("user-agent", config_.user_agent);

    // Disable sandbox (we're not shipping chrome-sandbox suid)
    command_line->AppendSwitch("no-sandbox");

    // Enable media stream (camera, mic) — required for getUserMedia
    command_line->AppendSwitch("enable-media-stream");

    // Use PipeWire for audio/screen capture on Wayland
    command_line->AppendSwitch("enable-webrtc-pipewire-capturer");

    // Enable screen sharing
    command_line->AppendSwitch("enable-usermedia-screen-capturing");
    command_line->AppendSwitchWithValue("auto-select-desktop-capture-source", "Entire screen");

    // Spellcheck
    command_line->AppendSwitch("enable-spelling-auto-correct");
}
