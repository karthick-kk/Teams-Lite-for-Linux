#include "app.h"

TflApp::TflApp(const TflConfig& config) : config_(config) {}

void TflApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {

    // Native Wayland support
    command_line->AppendSwitchWithValue("ozone-platform", "wayland");
    command_line->AppendSwitch("enable-wayland-ime");

    // HiDPI + spellcheck + VAAPI hardware video encoding
    command_line->AppendSwitchWithValue("enable-features",
        "UseOzonePlatform,WaylandWindowDecorations,SpellcheckServiceMultilingual,"
        "VaapiVideoDecoder,VaapiVideoEncoder,VaapiVideoDecodeLinuxGL,"
        "WebRTCPipeWireCapturer,WebRtcUnifiedPlanByDefault");

    // Disable H.264 simulcast — OpenH264 only supports single layer encoding
    command_line->AppendSwitchWithValue("force-fieldtrials",
        "WebRTC-H264Simulcast/Disabled/");

    // GPU acceleration
    command_line->AppendSwitch("enable-gpu");
    command_line->AppendSwitch("enable-gpu-rasterization");

    // Allow up to 30fps for video capture
    command_line->AppendSwitchWithValue("max-gum-fps", "30");

    // Custom user-agent
    command_line->AppendSwitchWithValue("user-agent", config_.user_agent);

    // Disable sandbox (we're not shipping chrome-sandbox suid)
    command_line->AppendSwitch("no-sandbox");

    // Enable media stream (camera, mic) — required for getUserMedia
    command_line->AppendSwitch("enable-media-stream");
    // Auto-select default devices without showing picker UI (needed for mic/speaker)
    command_line->AppendSwitch("use-fake-ui-for-media-stream");

    // Use PipeWire for audio/screen capture on Wayland
    command_line->AppendSwitch("enable-webrtc-pipewire-capturer");

    // Autoplay for notification sounds
    command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");

    // Enable screen sharing via xdg-desktop-portal (Wayland native)
    command_line->AppendSwitch("enable-usermedia-screen-capturing");

    // Spellcheck
    command_line->AppendSwitch("enable-spelling-auto-correct");
}
