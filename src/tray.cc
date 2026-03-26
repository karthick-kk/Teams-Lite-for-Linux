#include "tray.h"
#include "include/cef_task.h"
#include <libayatana-appindicator/app-indicator.h>
#include <gtk/gtk.h>
#include <cstdio>
#include <functional>
#include <filesystem>
#include <fstream>
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

    // Find the source SVG icon
    char exe_path[4096];
    std::string icon_svg_path;
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        std::string exe_dir = std::filesystem::path(exe_path).parent_path().string();
        for (const auto& candidate : {
            exe_dir + "/tfl.svg",
            exe_dir + "/../data/tfl.svg",
            exe_dir + "/../share/icons/tfl.svg",
            std::string("/usr/share/icons/hicolor/scalable/apps/tfl.svg"),
        }) {
            if (std::filesystem::exists(candidate)) {
                icon_svg_path = std::filesystem::canonical(candidate).string();
                break;
            }
        }
    }

    if (icon_svg_path.empty()) {
        // No icon found — use fallback icon name from system theme
        g_indicator = app_indicator_new(
            "tfl-teams-for-linux", "teams-for-linux",
            APP_INDICATOR_CATEGORY_COMMUNICATIONS);
    } else {
        // Build a proper hicolor icon theme in cache dir so the DE picks the right size
        const char* home = std::getenv("HOME");
        const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
        std::string cache_dir = (xdg_cache && xdg_cache[0])
            ? std::string(xdg_cache) + "/tfl"
            : std::string(home ? home : "/tmp") + "/.cache/tfl";

        std::string theme_base = cache_dir + "/tray-icons";
        std::string scalable_dir = theme_base + "/hicolor/scalable/apps";
        std::filesystem::create_directories(scalable_dir);

        // Copy SVG into the theme
        std::string dest_svg = scalable_dir + "/tfl.svg";
        std::filesystem::copy_file(icon_svg_path, dest_svg,
            std::filesystem::copy_options::overwrite_existing);

        // Write index.theme so the icon lookup works
        std::string index_path = theme_base + "/hicolor/index.theme";
        std::ofstream index(index_path);
        if (index.is_open()) {
            index << "[Icon Theme]\n"
                  << "Name=tfl\n"
                  << "Comment=TFL tray icons\n"
                  << "Directories=scalable/apps\n"
                  << "\n"
                  << "[scalable/apps]\n"
                  << "Size=48\n"
                  << "MinSize=16\n"
                  << "MaxSize=256\n"
                  << "Type=Scalable\n"
                  << "Context=Applications\n";
        }

        g_indicator = app_indicator_new(
            "tfl-teams-for-linux", "tfl",
            APP_INDICATOR_CATEGORY_COMMUNICATIONS);
        app_indicator_set_icon_theme_path(g_indicator, theme_base.c_str());
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
