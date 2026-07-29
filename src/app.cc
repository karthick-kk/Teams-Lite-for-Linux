#include "app.h"
#include <string>
#include <cstdio>

TflApp::TflApp(const TflConfig& config) : config_(config) {}

void TflApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {

    // Native Wayland support
    command_line->AppendSwitchWithValue("ozone-platform", "wayland");
    command_line->AppendSwitch("enable-wayland-ime");

    // Wayland + optional VAAPI hardware video decode
    // ChromeWideEchoCancellation / WebRtcAllowInputVolumeAdjustment: acoustic AEC for mic
    // path. These do NOT reliably cancel desktop-loopback feedback during screen share.
    std::string features =
        "UseOzonePlatform,WaylandWindowDecorations,"
        "WebRTCPipeWireCapturer,ChromeWideEchoCancellation,WebRtcAllowInputVolumeAdjustment";
    if (config_.vaapi) {
        features += ",VaapiVideoDecoder,VaapiVideoEncoder,VaapiVideoDecodeLinuxGL";
    }
    command_line->AppendSwitchWithValue("enable-features", features);
    command_line->AppendSwitchWithValue("disable-features",
        "SpareRendererForSitePerProcess,SpellCheck,BackForwardCache");
    fprintf(stderr, "[tfl] VAAPI: %s\n", config_.vaapi ? "enabled" : "disabled");

    // Disable H.264 simulcast — OpenH264 only supports single layer encoding
    command_line->AppendSwitchWithValue("force-fieldtrials",
        "WebRTC-H264Simulcast/Disabled/BackForwardCache/Disabled/");

    // GPU acceleration
    command_line->AppendSwitch("enable-gpu");
    command_line->AppendSwitch("enable-gpu-rasterization");
    command_line->AppendSwitch("in-process-gpu");

    // Allow up to 30fps for video capture
    command_line->AppendSwitchWithValue("max-gum-fps", "30");

    // Custom user-agent
    command_line->AppendSwitchWithValue("user-agent", config_.user_agent);

    // Limit renderer processes — Teams only needs one tab
    command_line->AppendSwitchWithValue("renderer-process-limit", "1");
    command_line->AppendSwitchWithValue("js-flags", "--max-old-space-size=512");
    command_line->AppendSwitch("disable-gpu-shader-disk-cache");
    command_line->AppendSwitch("aggressive-cache-discard");

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
}

// --- Render Process Handler (runs in renderer subprocess) ---

void TflApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               CefRefPtr<CefV8Context> context) {
    // Create renderer-side message router on first context
    if (!renderer_router_) {
        CefMessageRouterConfig config;
        renderer_router_ = CefMessageRouterRendererSide::Create(config);
    }
    renderer_router_->OnContextCreated(browser, frame, context);
}

void TflApp::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) {
    if (renderer_router_) {
        renderer_router_->OnContextReleased(browser, frame, context);
    }
}

bool TflApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       CefProcessId source_process,
                                       CefRefPtr<CefProcessMessage> message) {
    if (renderer_router_) {
        return renderer_router_->OnProcessMessageReceived(browser, frame, source_process, message);
    }
    return false;
}
