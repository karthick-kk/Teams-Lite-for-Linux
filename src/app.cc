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

    // GPU acceleration
    command_line->AppendSwitch("enable-gpu");
    command_line->AppendSwitch("enable-gpu-rasterization");
    command_line->AppendSwitch("in-process-gpu");

    // Custom user-agent
    command_line->AppendSwitchWithValue("user-agent", config_.user_agent);

    // Disable sandbox (we're not shipping chrome-sandbox suid)
    command_line->AppendSwitch("no-sandbox");

    // Feature flags: Wayland, PipeWire, VAAPI, echo cancellation
    std::string features =
        "UseOzonePlatform,WaylandWindowDecorations,"
        "WebRTCPipeWireCapturer,ChromeWideEchoCancellation,WebRtcAllowInputVolumeAdjustment";
    if (config_.vaapi) {
        features += ",VaapiVideoDecoder,VaapiVideoEncoder,VaapiVideoDecodeLinuxGL";
    }
    command_line->AppendSwitchWithValue("enable-features", features);
    fprintf(stderr, "[tfl] VAAPI: %s\n", config_.vaapi ? "enabled" : "disabled");

    // ponytail: disable-features crashes CEF 151 (segfault with any value, even nonsense).
    // Workaround: use individual switches. Revisit when upgrading CEF.
    command_line->AppendSwitch("disable-spell-checking");        // was: SpellCheck
    command_line->AppendSwitch("disable-back-forward-cache");    // was: BackForwardCache

    // Disable H.264 simulcast — OpenH264 only supports single layer encoding
    command_line->AppendSwitchWithValue("force-fieldtrials",
        "WebRTC-H264Simulcast/Disabled/");

    // Allow up to 30fps for video capture
    command_line->AppendSwitchWithValue("max-gum-fps", "30");

    // Memory: limit renderer processes (avoids spare renderer overhead)
    command_line->AppendSwitchWithValue("renderer-process-limit", "1");

    // Memory: cap V8 heap per renderer
    command_line->AppendSwitchWithValue("js-flags", "--max-old-space-size=512");

    // Memory: discard GPU shader disk cache on exit
    command_line->AppendSwitch("disable-gpu-shader-disk-cache");
    command_line->AppendSwitch("aggressive-cache-discard");

    // Media/stream permissions
    command_line->AppendSwitch("enable-media-stream");
    command_line->AppendSwitch("use-fake-ui-for-media-stream");
    command_line->AppendSwitch("enable-webrtc-pipewire-capturer");
    command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
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
