#include "window.h"
#include "tray.h"
#include "theme.h"
#include "notifications.h"
#include "config.h"
#include <cstdio>

// --- TflWindowDelegate ---

TflWindowDelegate::TflWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                                     const TflConfig& config,
                                     CefRefPtr<TflClient> client,
                                     const std::vector<std::string>& themes,
                                     std::function<void()> on_restart)
    : browser_view_(browser_view), config_(config), client_(client),
      themes_(themes), on_restart_(std::move(on_restart)) {}

void TflWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
    window->AddChildView(browser_view_);
    window->Show();
    browser_view_->RequestFocus();

    // Init tray after browser is ready
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
        CefRefPtr<TflClient> cl = client_;
        auto restart_cb = on_restart_;
        tray_init(browser, window, config_, themes_,
            [cl](const std::string& theme_name) {
                if (cl) cl->ApplyTheme(theme_name);
            },
            restart_cb);
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
    client_ = nullptr;
    fprintf(stderr, "[tfl] Window destroyed\n");
}

bool TflWindowDelegate::CanClose(CefRefPtr<CefWindow> window) {
    // Save window state
    CefRect bounds = window->GetBounds();
    save_window_state(config_, bounds.x, bounds.y, bounds.width, bounds.height);

    if (tray_quit_requested()) {
        tray_shutdown();
        return true;
    }

    // Close-to-tray: hide window instead of quitting
    if (config_.close_to_tray) {
        window->Hide();
        return false;
    }

    tray_shutdown();
    return true;
}

CefRect TflWindowDelegate::GetInitialBounds(CefRefPtr<CefWindow> window) {
    if (config_.x >= 0 && config_.y >= 0) {
        return CefRect(config_.x, config_.y, config_.width, config_.height);
    }
    return CefRect(0, 0, config_.width, config_.height);
}

bool TflWindowDelegate::GetLinuxWindowProperties(
    CefRefPtr<CefWindow> window,
    CefLinuxWindowProperties& properties) {
    CefString(&properties.wayland_app_id) = "tfl";
    CefString(&properties.wm_class_class) = "tfl";
    CefString(&properties.wm_class_name) = "tfl";
    return true;
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
