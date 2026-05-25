#pragma once

#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"
#include "config.h"

// Browser + Render process handler — same executable handles both processes
class TflApp : public CefApp,
               public CefBrowserProcessHandler,
               public CefRenderProcessHandler {
public:
    explicit TflApp(const TflConfig& config);

    // CefApp
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override;

    // CefRenderProcessHandler — for message router render-side
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override;
    void OnContextReleased(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context) override;
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override;

    // Update config after load_config() — must be called before CefInitialize
    void UpdateConfig(const TflConfig& config) { config_ = config; }

private:
    TflConfig config_;
    CefRefPtr<CefMessageRouterRendererSide> renderer_router_;
    IMPLEMENT_REFCOUNTING(TflApp);
    DISALLOW_COPY_AND_ASSIGN(TflApp);
};
