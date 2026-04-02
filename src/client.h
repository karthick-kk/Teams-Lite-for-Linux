#pragma once

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_permission_handler.h"
#include "include/cef_keyboard_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_jsdialog_handler.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_download_handler.h"
#include "config.h"
#include <list>

class TflClient : public CefClient,
                  public CefLifeSpanHandler,
                  public CefDisplayHandler,
                  public CefRequestHandler,
                  public CefPermissionHandler,
                  public CefKeyboardHandler,
                  public CefLoadHandler,
                  public CefJSDialogHandler,
                  public CefContextMenuHandler,
                  public CefDownloadHandler {
public:
    explicit TflClient(const TflConfig& config);

    // CefClient
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefPermissionHandler> GetPermissionHandler() override { return this; }
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }

    // CefLifeSpanHandler
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int popup_id,
                       const CefString& target_url,
                       const CefString& target_frame_name,
                       cef_window_open_disposition_t target_disposition,
                       bool user_gesture,
                       const CefPopupFeatures& popupFeatures,
                       CefWindowInfo& windowInfo,
                       CefRefPtr<CefClient>& client,
                       CefBrowserSettings& settings,
                       CefRefPtr<CefDictionaryValue>& extra_info,
                       bool* no_javascript_access) override;

    // CefDisplayHandler
    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

    // CefRequestHandler
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                    CefRefPtr<CefFrame> frame,
                    CefRefPtr<CefRequest> request,
                    bool user_gesture,
                    bool is_redirect) override;

    bool OnOpenURLFromTab(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          const CefString& target_url,
                          cef_window_open_disposition_t target_disposition,
                          bool user_gesture) override;

    bool OnCertificateError(CefRefPtr<CefBrowser> browser,
                            cef_errorcode_t cert_error,
                            const CefString& request_url,
                            CefRefPtr<CefSSLInfo> ssl_info,
                            CefRefPtr<CefCallback> callback) override;

    // CefPermissionHandler
    bool OnRequestMediaAccessPermission(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        const CefString& requesting_url,
                                        uint32_t requested_permissions,
                                        CefRefPtr<CefMediaAccessCallback> callback) override;

    bool OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser,
                                uint64_t prompt_id,
                                const CefString& requesting_origin,
                                uint32_t requested_permissions,
                                CefRefPtr<CefPermissionPromptCallback> callback) override;

    // CefKeyboardHandler
    bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyEvent& event,
                       CefEventHandle os_event,
                       bool* is_keyboard_shortcut) override;

    // CefLoadHandler
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;

    // CefJSDialogHandler
    bool OnJSDialog(CefRefPtr<CefBrowser> browser,
                    const CefString& origin_url,
                    CefJSDialogHandler::JSDialogType dialog_type,
                    const CefString& message_text,
                    const CefString& default_prompt_text,
                    CefRefPtr<CefJSDialogCallback> callback,
                    bool& suppress_message) override;

    bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser,
                              const CefString& message_text,
                              bool is_reload,
                              CefRefPtr<CefJSDialogCallback> callback) override;

    // CefContextMenuHandler — spellcheck suggestions in right-click menu
    void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             CefRefPtr<CefContextMenuParams> params,
                             CefRefPtr<CefMenuModel> model) override;

    bool OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefContextMenuParams> params,
                              int command_id,
                              CefContextMenuHandler::EventFlags event_flags) override;

    // CefDownloadHandler — auto-save to ~/Downloads
    bool OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefDownloadItem> download_item,
                          const CefString& suggested_name,
                          CefRefPtr<CefBeforeDownloadCallback> callback) override;

    void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefDownloadItem> download_item,
                           CefRefPtr<CefDownloadItemCallback> callback) override;

    bool IsClosing() const { return is_closing_; }

    // Inject JavaScript into the current Teams frame (used by idle monitor)
    void InjectJS(const char* js);

    // Apply a theme by name (called from tray menu)
    void ApplyTheme(const std::string& theme_name);

private:
    TflConfig config_;
    bool is_closing_ = false;
    int last_badge_ = 0;
    std::list<CefRefPtr<CefBrowser>> browsers_;
    CefRefPtr<CefFrame> teams_frame_;  // main Teams frame for JS injection

    IMPLEMENT_REFCOUNTING(TflClient);
    DISALLOW_COPY_AND_ASSIGN(TflClient);
};
