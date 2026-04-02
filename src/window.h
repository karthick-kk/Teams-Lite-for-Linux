#pragma once

#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "config.h"
#include "client.h"
#include <vector>
#include <string>

// Window delegate — controls the app window (no tabs, no address bar)
class TflWindowDelegate : public CefWindowDelegate {
public:
    TflWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                      const TflConfig& config,
                      CefRefPtr<TflClient> client = nullptr,
                      const std::vector<std::string>& themes = {});

    // CefWindowDelegate
    void OnWindowCreated(CefRefPtr<CefWindow> window) override;
    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
    bool CanClose(CefRefPtr<CefWindow> window) override;
    CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override;
    bool IsFrameless(CefRefPtr<CefWindow> window) override { return true; }
    bool GetLinuxWindowProperties(CefRefPtr<CefWindow> window,
                                  CefLinuxWindowProperties& properties) override;

private:
    CefRefPtr<CefBrowserView> browser_view_;
    TflConfig config_;
    CefRefPtr<TflClient> client_;
    std::vector<std::string> themes_;

    IMPLEMENT_REFCOUNTING(TflWindowDelegate);
    DISALLOW_COPY_AND_ASSIGN(TflWindowDelegate);
};

// BrowserView delegate — minimal, just creates the window when the view is ready
class TflBrowserViewDelegate : public CefBrowserViewDelegate {
public:
    explicit TflBrowserViewDelegate(const TflConfig& config);

    // Called when a popup BrowserView is created (e.g. OAuth popups)
    bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                   CefRefPtr<CefBrowserView> popup_browser_view,
                                   bool is_devtools) override;

private:
    TflConfig config_;

    IMPLEMENT_REFCOUNTING(TflBrowserViewDelegate);
    DISALLOW_COPY_AND_ASSIGN(TflBrowserViewDelegate);
};
