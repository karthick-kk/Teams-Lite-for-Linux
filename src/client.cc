#include "client.h"
#include "tray.h"
#include "idle.h"
#include "notifications.h"
#include "webrtc_fix.h"
#include "theme.h"
#include "include/cef_app.h"
#include "include/views/cef_browser_view.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

// Extract hostname from URL (e.g. "https://foo.bar.com/path?q=1" -> "foo.bar.com")
static std::string get_hostname(const std::string& url) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) return "";
    auto host_start = scheme_end + 3;
    auto host_end = url.find_first_of(":/?#", host_start);
    if (host_end == std::string::npos) host_end = url.size();
    return url.substr(host_start, host_end - host_start);
}

static bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool is_teams_domain(const std::string& url) {
    std::string host = get_hostname(url);
    return ends_with(host, "teams.cloud.microsoft") ||
           ends_with(host, "teams.microsoft.com") ||
           ends_with(host, "teams.live.com") ||
           ends_with(host, "teams.cdn.office.net") ||
           ends_with(host, ".office.com") ||
           ends_with(host, "login.microsoftonline.com") ||
           ends_with(host, "login.live.com") ||
           ends_with(host, "login.microsoft.com") ||
           ends_with(host, "microsoftonline.com") ||
           ends_with(host, "aadcdn.msftauth.net") ||
           ends_with(host, "aadcdn.msauth.net");
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

    std::string url = target_url.ToString();
    fprintf(stderr, "[tfl] OnBeforePopup: host=%s url=%s (gesture=%d)\n", get_hostname(url).c_str(), url.substr(0, 120).c_str(), user_gesture);

    // Detect Teams safelinks — extract the real URL and open externally
    std::string host = get_hostname(url);
    if (host.find("teams.cdn.office.net") != std::string::npos &&
        url.find("safelinks") != std::string::npos) {
        // Extract url= parameter
        auto pos = url.find("url=");
        if (pos != std::string::npos) {
            pos += 4;
            auto end = url.find("&", pos);
            std::string encoded = (end != std::string::npos)
                ? url.substr(pos, end - pos) : url.substr(pos);
            // URL-decode
            std::string decoded;
            for (size_t i = 0; i < encoded.size(); i++) {
                if (encoded[i] == '%' && i + 2 < encoded.size()) {
                    int hi = 0, lo = 0;
                    if (sscanf(encoded.c_str() + i + 1, "%1x%1x", &hi, &lo) == 2) {
                        decoded += (char)((hi << 4) | lo);
                        i += 2;
                        continue;
                    }
                } else if (encoded[i] == '+') {
                    decoded += ' ';
                    continue;
                }
                decoded += encoded[i];
            }
            if (!decoded.empty()) {
                std::string cmd = "xdg-open '" + decoded + "' &";
                system(cmd.c_str());
                fprintf(stderr, "[tfl] Safelink opened: %s\n", decoded.c_str());
                return true;
            }
        }
    }

    // Teams-internal popups (OAuth, etc.) — allow in CEF
    if (is_teams_domain(url)) {
        return false;
    }

    // Other external links — open in default browser
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

void TflClient::OnLoadStart(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             CefLoadHandler::TransitionType transition_type) {
    if (!frame->IsMain()) return;

    std::string url = frame->GetURL().ToString();
    if (!is_teams_domain(url)) return;

    // Inject insertRule hook BEFORE Teams' JS runs — this catches all Griffel
    // CSS rules as they're inserted, rewriting brand colors inline.
    if (config_.theme != "none" && !config_.theme.empty()) {
        std::string theme_css = theme_load_css(config_.theme);
        if (!theme_css.empty()) {
            std::string hook_js = theme_get_hook_js(theme_css);
            if (!hook_js.empty()) {
                frame->ExecuteJavaScript(hook_js.c_str(), url, 0);
            }
        }
    }
}

void TflClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int httpStatusCode) {
    if (!frame->IsMain()) return;

    std::string url = frame->GetURL().ToString();
    if (!is_teams_domain(url)) return;

    // Track the main Teams frame for idle monitor JS injection
    teams_frame_ = frame;

    // Inject visibility override to prevent "Away" status when window is unfocused
    frame->ExecuteJavaScript(idle_get_visibility_override_js(), url, 0);

    // Inject theme CSS (before custom.css so user overrides win)
    if (config_.theme != "none" && !config_.theme.empty()) {
        std::string theme_css = theme_load_css(config_.theme);
        if (!theme_css.empty()) {
            frame->ExecuteJavaScript(theme_get_inject_js(theme_css).c_str(), url, 0);
            fprintf(stderr, "[tfl] Theme injected: %s\n", config_.theme.c_str());
        }
    }
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

    // Inject WebRTC screen share fix (monitors tracks, restarts ICE if needed)
    frame->ExecuteJavaScript(webrtc_get_screenshare_fix_js(), url, 0);
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
    std::string tooltip = "Teams Lite for Linux";
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
bool TflClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               CefRefPtr<CefRequest> request,
                               bool user_gesture,
                               bool is_redirect) {
    if (!frame->IsMain()) return false;

    std::string url = request->GetURL().ToString();
    if (is_teams_domain(url)) return false;

    // External URL clicked by user — open in default browser
    std::string cmd = "xdg-open '" + url + "' &";
    system(cmd.c_str());
    fprintf(stderr, "[tfl] External link opened: %s\n", url.c_str());
    return true;  // cancel navigation in CEF
}

bool TflClient::OnOpenURLFromTab(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 const CefString& target_url,
                                 cef_window_open_disposition_t target_disposition,
                                 bool user_gesture) {
    std::string url = target_url.ToString();
    if (is_teams_domain(url)) return false;

    // External URL (e.g. ctrl+click, middle-click) — open in default browser
    std::string cmd = "xdg-open '" + url + "' &";
    system(cmd.c_str());
    fprintf(stderr, "[tfl] External link opened (tab): %s\n", url.c_str());
    return true;  // cancel navigation in CEF
}


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

// --- JS Injection (for idle monitor) ---

void TflClient::ApplyTheme(const std::string& theme_name) {
    config_.theme = theme_name;
    // Reload the page so OnLoadEnd fires fresh and injects the theme
    // into the correct JS context
    if (!browsers_.empty()) {
        auto browser = browsers_.front();
        browser->Reload();
        fprintf(stderr, "[tfl] Theme changed to '%s' — reloading\n", theme_name.c_str());
    }
}

void TflClient::InjectJS(const char* js) {
    if (!browsers_.empty()) {
        auto browser = browsers_.front();
        auto frame = browser->GetMainFrame();
        if (frame && frame->IsValid()) {
            frame->ExecuteJavaScript(js, frame->GetURL(), 0);
        }
    }
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

    // Ctrl+Q or Alt+F4 — quit
    if ((event.windows_key_code == 'Q' && (event.modifiers & EVENTFLAG_CONTROL_DOWN)) ||
        (event.windows_key_code == 0x73 && (event.modifiers & EVENTFLAG_ALT_DOWN))) {  // 0x73 = F4
        tray_request_quit();
        return true;
    }

    return false;
}
