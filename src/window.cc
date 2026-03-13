#include "window.h"
#include "tray.h"
#include "notifications.h"
#include <cstdio>

// --- TflWindowDelegate ---

TflWindowDelegate::TflWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                                     const TflConfig& config)
    : browser_view_(browser_view), config_(config) {}

void TflWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
    window->AddChildView(browser_view_);
    window->Show();
    browser_view_->RequestFocus();

    // Init tray after browser is ready
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
        tray_init(browser, window);
    }

    // Re-show window when notification is clicked
    CefRefPtr<CefWindow> win = window;
    notifications_set_click_handler([win]() {
        if (win) {
            win->Show();
            win->Activate();
        }
    });

    fprintf(stderr, "[tfl] Window created\n");
}

void TflWindowDelegate::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
    tray_shutdown();
    browser_view_ = nullptr;
    fprintf(stderr, "[tfl] Window destroyed\n");
}

bool TflWindowDelegate::CanClose(CefRefPtr<CefWindow> window) {
    if (tray_quit_requested()) {
        // Actually quit — close the browser
        CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
        if (browser) {
            browser->GetHost()->CloseBrowser(false);
        }
        return false;  // let OnBeforeClose handle final quit
    }
    // Minimize to tray instead of closing
    window->Hide();
    return false;
}

CefRect TflWindowDelegate::GetInitialBounds(CefRefPtr<CefWindow> window) {
    return CefRect(0, 0, config_.width, config_.height);
}

// --- TflBrowserViewDelegate ---

TflBrowserViewDelegate::TflBrowserViewDelegate(const TflConfig& config)
    : config_(config) {}

bool TflBrowserViewDelegate::OnPopupBrowserViewCreated(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowserView> popup_browser_view,
    bool is_devtools) {
    // Create a new window for popups (OAuth login windows)
    CefWindow::CreateTopLevelWindow(
        new TflWindowDelegate(popup_browser_view, config_));
    return true;
}
