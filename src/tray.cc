#include "tray.h"
#include "include/cef_task.h"
#include <libayatana-appindicator/app-indicator.h>
#include <gtk/gtk.h>
#include <cstdio>
#include <functional>
#include <filesystem>
#include <unistd.h>

static AppIndicator* g_indicator = nullptr;
static GtkWidget* g_menu = nullptr;
static CefRefPtr<CefBrowser> g_browser;
static CefRefPtr<CefWindow> g_window;
static bool g_quit_requested = false;

// Helper to run a lambda on CEF's UI thread
class CefLambdaTask : public CefTask {
public:
    explicit CefLambdaTask(std::function<void()> fn) : fn_(std::move(fn)) {}
    void Execute() override { fn_(); }
private:
    std::function<void()> fn_;
    IMPLEMENT_REFCOUNTING(CefLambdaTask);
};

static void on_show_activate(GtkMenuItem*, gpointer) {
    CefRefPtr<CefWindow> win = g_window;
    if (win) {
        CefPostTask(TID_UI, new CefLambdaTask([win]() {
            win->Show();
            win->Activate();
        }));
    }
}

static void on_quit_activate(GtkMenuItem*, gpointer) {
    g_quit_requested = true;
    // Close the window — CanClose will see quit_requested and return true
    CefRefPtr<CefWindow> win = g_window;
    if (win) {
        CefPostTask(TID_UI, new CefLambdaTask([win]() {
            win->Close();
        }));
    }
}

void tray_init(CefRefPtr<CefBrowser> browser, CefRefPtr<CefWindow> window) {
    g_browser = browser;
    g_window = window;

    // Find icon path relative to executable
    char exe_path[4096];
    std::string icon_path;
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        std::string exe_dir = std::filesystem::path(exe_path).parent_path().string();
        // Check next to binary, ../data/, ../share/icons/, and system icon path
        for (const auto& candidate : {
            exe_dir + "/tfl-tray.png",
            exe_dir + "/../data/tfl-tray.png",
            exe_dir + "/../share/icons/tfl-tray.png",
            std::string("/usr/share/icons/hicolor/48x48/apps/tfl-tray.png"),
        }) {
            if (std::filesystem::exists(candidate)) {
                icon_path = std::filesystem::canonical(candidate).string();
                break;
            }
        }
    }

    if (icon_path.empty()) {
        // Fallback: use icon name from theme
        g_indicator = app_indicator_new(
            "tfl-teams-for-linux", "teams-for-linux",
            APP_INDICATOR_CATEGORY_COMMUNICATIONS);
    } else {
        // Use icon from file path — appindicator needs dir + icon name (no extension)
        std::string icon_dir = std::filesystem::path(icon_path).parent_path().string();
        g_indicator = app_indicator_new_with_path(
            "tfl-teams-for-linux", "tfl-tray",
            APP_INDICATOR_CATEGORY_COMMUNICATIONS,
            icon_dir.c_str());
    }

    app_indicator_set_status(g_indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(g_indicator, "Teams Lite for Linux");

    g_menu = gtk_menu_new();

    GtkWidget* show_item = gtk_menu_item_new_with_label("Show");
    g_signal_connect(show_item, "activate", G_CALLBACK(on_show_activate), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), show_item);

    GtkWidget* sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), sep);

    GtkWidget* quit_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_quit_activate), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), quit_item);

    gtk_widget_show_all(g_menu);
    app_indicator_set_menu(g_indicator, GTK_MENU(g_menu));

    fprintf(stderr, "[tfl] Tray initialized\n");
}

void tray_set_tooltip(const std::string& text) {
    if (g_indicator) {
        app_indicator_set_title(g_indicator, text.c_str());
    }
}

bool tray_quit_requested() {
    return g_quit_requested;
}

void tray_request_quit() {
    g_quit_requested = true;
    CefRefPtr<CefWindow> win = g_window;
    if (win) {
        CefPostTask(TID_UI, new CefLambdaTask([win]() {
            win->Close();
        }));
    }
}

void tray_shutdown() {
    if (g_indicator) {
        g_object_unref(g_indicator);
        g_indicator = nullptr;
    }
    if (g_menu) {
        gtk_widget_destroy(g_menu);
        g_menu = nullptr;
    }
    g_browser = nullptr;
    g_window = nullptr;
    fprintf(stderr, "[tfl] Tray shutdown\n");
}
