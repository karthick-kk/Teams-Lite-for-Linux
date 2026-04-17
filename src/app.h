#pragma once

#include "include/cef_app.h"
#include "config.h"

// Browser process handler — sets up command line flags for Wayland, HiDPI, etc.
class TflApp : public CefApp, public CefBrowserProcessHandler {
public:
    explicit TflApp(const TflConfig& config);

    // CefApp
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override;

    // Update config after load_config() — must be called before CefInitialize
    void UpdateConfig(const TflConfig& config) { config_ = config; }

private:
    TflConfig config_;
    IMPLEMENT_REFCOUNTING(TflApp);
    DISALLOW_COPY_AND_ASSIGN(TflApp);
};
