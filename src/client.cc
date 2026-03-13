#include "client.h"
#include "tray.h"
#include "idle.h"
#include "notifications.h"
#include "include/cef_app.h"
#include "include/views/cef_browser_view.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

static bool is_teams_domain(const std::string& url) {
    return url.find("teams.cloud.microsoft") != std::string::npos ||
           url.find("teams.microsoft.com") != std::string::npos ||
           url.find("teams.live.com") != std::string::npos ||
           url.find("login.microsoftonline.com") != std::string::npos ||
           url.find("login.live.com") != std::string::npos ||
           url.find("login.microsoft.com") != std::string::npos ||
           url.find("microsoft.com/devicelogin") != std::string::npos ||
           url.find("aadcdn.msftauth.net") != std::string::npos ||
           url.find("aadcdn.msauth.net") != std::string::npos;
}

TflClient::TflClient(const TflConfig& config) : config_(config) {}

// --- LifeSpan ---

void TflClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    browsers_.push_back(browser);
    fprintf(stderr, "[tfl] Browser created (total: %zu)\n", browsers_.size());
}

bool TflClient::DoClose(CefRefPtr<CefBrowser> browser) {
    if (browsers_.size() == 1) {
        is_closing_ = true;
    }
    return false;  // allow default close
}

void TflClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    for (auto it = browsers_.begin(); it != browsers_.end(); ++it) {
        if ((*it)->IsSame(browser)) {
            browsers_.erase(it);
            break;
        }
    }
    if (browsers_.empty()) {
        CefQuitMessageLoop();
    }
}

bool TflClient::OnBeforePopup(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int popup_id,
    const CefString& target_url,
    const CefString& target_frame_name,
    CefLifeSpanHandler::WindowOpenDisposition target_disposition,
    bool user_gesture,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue>& extra_info,
    bool* no_javascript_access) {

    // Load popups in the main browser instead of opening new windows
    // Exception: allow OAuth popups that need separate windows
    std::string url = target_url.ToString();
    if (is_teams_domain(url)) {
        return false;
    }

    // Open external links in default browser
    std::string cmd = "xdg-open '" + url + "' &";
    system(cmd.c_str());
    return true;  // cancel popup
}

// --- Context Menu ---

void TflClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefContextMenuParams> params,
                                     CefRefPtr<CefMenuModel> model) {
    CefString misspelled = params->GetMisspelledWord();
    if (misspelled.length() > 0) {
        std::vector<CefString> suggestions;
        params->GetDictionarySuggestions(suggestions);

        model->Clear();

        if (suggestions.empty()) {
            model->AddItem(MENU_ID_NO_SPELLING_SUGGESTIONS, "No suggestions");
            model->SetEnabled(MENU_ID_NO_SPELLING_SUGGESTIONS, false);
        } else {
            for (size_t i = 0; i < suggestions.size() && i <= 4; i++) {
                model->AddItem(MENU_ID_SPELLCHECK_SUGGESTION_0 + (int)i, suggestions[i]);
            }
        }
        model->AddSeparator();
    }

    if (params->IsEditable()) {
        model->AddItem(MENU_ID_UNDO, "Undo");
        model->AddItem(MENU_ID_REDO, "Redo");
        model->AddSeparator();
        model->AddItem(MENU_ID_CUT, "Cut");
        model->AddItem(MENU_ID_COPY, "Copy");
        model->AddItem(MENU_ID_PASTE, "Paste");
        model->AddItem(MENU_ID_SELECT_ALL, "Select All");
    } else if (params->GetSelectionText().length() > 0) {
        model->AddItem(MENU_ID_COPY, "Copy");
    }
}

bool TflClient::OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefContextMenuParams> params,
                                      int command_id,
                                      CefContextMenuHandler::EventFlags event_flags) {
    if (command_id >= MENU_ID_SPELLCHECK_SUGGESTION_0 &&
        command_id <= MENU_ID_SPELLCHECK_SUGGESTION_LAST) {
        std::vector<CefString> suggestions;
        params->GetDictionarySuggestions(suggestions);
        int idx = command_id - MENU_ID_SPELLCHECK_SUGGESTION_0;
        if (idx >= 0 && idx < (int)suggestions.size()) {
            CefString replacement = suggestions[idx];
            frame->ExecuteJavaScript(
                "document.execCommand('insertText', false, '" +
                replacement.ToString() + "');",
                frame->GetURL(), 0);
        }
        return true;
    }
    return false;
}

// --- Downloads ---

bool TflClient::OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefDownloadItem> download_item,
                                  const CefString& suggested_name,
                                  CefRefPtr<CefBeforeDownloadCallback> callback) {
    const char* home = std::getenv("HOME");
    std::string downloads_dir = std::string(home ? home : "/tmp") + "/Downloads";
    std::string path = downloads_dir + "/" + suggested_name.ToString();

    fprintf(stderr, "[tfl] Downloading: %s\n", path.c_str());
    callback->Continue(path, false);
    return true;
}

void TflClient::OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefDownloadItem> download_item,
                                   CefRefPtr<CefDownloadItemCallback> callback) {
    if (download_item->IsComplete()) {
        std::string filename = download_item->GetFullPath().ToString();
        auto slash = filename.rfind('/');
        std::string name = (slash != std::string::npos) ? filename.substr(slash + 1) : filename;
        notifications_show("Download complete", name);
        fprintf(stderr, "[tfl] Download complete: %s\n", filename.c_str());
    } else if (download_item->IsCanceled()) {
        fprintf(stderr, "[tfl] Download canceled\n");
    }
}

// --- Load ---

void TflClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int httpStatusCode) {
    if (!frame->IsMain()) return;

    std::string url = frame->GetURL().ToString();
    if (!is_teams_domain(url)) return;

    // Inject visibility override to prevent "Away" status when window is unfocused
    frame->ExecuteJavaScript(idle_get_visibility_override_js(), url, 0);

    // Inject custom CSS from ~/.config/tfl/custom.css
    std::string css_path = config_.config_dir + "/custom.css";
    std::ifstream css_file(css_path);
    if (css_file.is_open()) {
        std::ostringstream ss;
        ss << css_file.rdbuf();
        std::string css = ss.str();
        if (!css.empty()) {
            // Escape for JS string
            std::string escaped;
            for (char c : css) {
                if (c == '\\') escaped += "\\\\";
                else if (c == '\'') escaped += "\\'";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\r') continue;
                else escaped += c;
            }
            std::string js = "(function(){var s=document.createElement('style');"
                             "s.textContent='" + escaped + "';"
                             "document.head.appendChild(s);})();";
            frame->ExecuteJavaScript(js, url, 0);
            fprintf(stderr, "[tfl] Custom CSS injected from %s\n", css_path.c_str());
        }
    }
}

// --- Display ---

void TflClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    std::string t = title.ToString();

    // Parse badge count from title — Teams uses "(N) Microsoft Teams"
    int badge = 0;
    if (t.size() > 2 && t[0] == '(') {
        auto close = t.find(')');
        if (close != std::string::npos && close > 1) {
            std::string num = t.substr(1, close - 1);
            try { badge = std::stoi(num); } catch (...) {}
        }
    }

    // Update tray tooltip with badge info
    std::string tooltip = "Teams for Linux";
    if (badge > 0) {
        tooltip += " (" + std::to_string(badge) + " unread)";
    }
    tray_set_tooltip(tooltip);

    // Show desktop notification when badge count increases
    if (badge > last_badge_ && badge > 0) {
        int new_msgs = badge - last_badge_;
        std::string body = std::to_string(new_msgs) + " new message" +
                           (new_msgs > 1 ? "s" : "");
        notifications_show("Microsoft Teams", body);
    }
    last_badge_ = badge;

    // Set window title for GNOME titlebar
    auto views = CefBrowserView::GetForBrowser(browser);
    if (views) {
        auto window = views->GetWindow();
        if (window) {
            window->SetTitle(title);
        }
    }
}

// --- Request ---

bool TflClient::OnCertificateError(
    CefRefPtr<CefBrowser> browser,
    cef_errorcode_t cert_error,
    const CefString& request_url,
    CefRefPtr<CefSSLInfo> ssl_info,
    CefRefPtr<CefCallback> callback) {
    // Reject invalid certs by default
    fprintf(stderr, "[tfl] Certificate error for %s (code %d)\n",
            request_url.ToString().c_str(), cert_error);
    return false;
}

// --- Permission ---

bool TflClient::OnRequestMediaAccessPermission(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& requesting_url,
    uint32_t requested_permissions,
    CefRefPtr<CefMediaAccessCallback> callback) {
    // Grant all media permissions for Teams (camera, mic, screen share)
    std::string url = requesting_url.ToString();
    if (url.find("teams.cloud.microsoft") != std::string::npos ||
        url.find("teams.microsoft.com") != std::string::npos ||
        url.find("teams.live.com") != std::string::npos) {
        callback->Continue(requested_permissions);
        return true;
    }
    return false;  // deny for other origins
}

// Auto-grant permission prompts (notifications, clipboard, etc.) for Teams
bool TflClient::OnShowPermissionPrompt(
    CefRefPtr<CefBrowser> browser,
    uint64_t prompt_id,
    const CefString& requesting_origin,
    uint32_t requested_permissions,
    CefRefPtr<CefPermissionPromptCallback> callback) {
    std::string origin = requesting_origin.ToString();
    if (is_teams_domain(origin)) {
        callback->Continue(CEF_PERMISSION_RESULT_ACCEPT);
        return true;
    }
    callback->Continue(CEF_PERMISSION_RESULT_DENY);
    return true;
}

// --- JS Dialogs ---

bool TflClient::OnJSDialog(CefRefPtr<CefBrowser> browser,
                            const CefString& origin_url,
                            CefJSDialogHandler::JSDialogType dialog_type,
                            const CefString& message_text,
                            const CefString& default_prompt_text,
                            CefRefPtr<CefJSDialogCallback> callback,
                            bool& suppress_message) {
    // Suppress "save password" and other JS dialogs — auto-accept
    callback->Continue(true, CefString());
    return true;
}

bool TflClient::OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser,
                                      const CefString& message_text,
                                      bool is_reload,
                                      CefRefPtr<CefJSDialogCallback> callback) {
    // Auto-accept "leave page?" dialogs
    callback->Continue(true, CefString());
    return true;
}

// --- Keyboard ---

bool TflClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                               const CefKeyEvent& event,
                               CefEventHandle os_event,
                               bool* is_keyboard_shortcut) {
    if (event.type != KEYEVENT_RAWKEYDOWN) return false;

    // F5 — reload
    if (event.windows_key_code == 0x74) {
        browser->Reload();
        return true;
    }

    // F12 — toggle devtools
    if (event.windows_key_code == 0x7B && config_.enable_dev_tools) {
        if (browser->GetHost()->HasDevTools()) {
            browser->GetHost()->CloseDevTools();
        } else {
            CefWindowInfo devtools_info;
            CefBrowserSettings devtools_settings;
            browser->GetHost()->ShowDevTools(devtools_info, nullptr, devtools_settings, CefPoint());
        }
        return true;
    }

    // Ctrl+R — reload
    if (event.windows_key_code == 'R' && (event.modifiers & EVENTFLAG_CONTROL_DOWN)) {
        browser->Reload();
        return true;
    }

    // Ctrl+Shift+R — reload ignoring cache
    if (event.windows_key_code == 'R' &&
        (event.modifiers & EVENTFLAG_CONTROL_DOWN) &&
        (event.modifiers & EVENTFLAG_SHIFT_DOWN)) {
        browser->ReloadIgnoreCache();
        return true;
    }

    return false;
}
