#include "window.h"
#include "tray.h"
#include "notifications.h"
#include "config.h"
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
    // Save window state
    CefRect bounds = window->GetBounds();
    save_window_state(config_, bounds.x, bounds.y, bounds.width, bounds.height);

    if (tray_quit_requested()) {
        // Allow the window to close — OnBeforeClose will call CefQuitMessageLoop
        return true;
    }
    // Minimize to tray instead of closing
    window->Hide();
    return false;
}

CefRect TflWindowDelegate::GetInitialBounds(CefRefPtr<CefWindow> window) {
    if (config_.x >= 0 && config_.y >= 0) {
        return CefRect(config_.x, config_.y, config_.width, config_.height);
    }
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
