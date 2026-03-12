#include "tray.h"
#include <libayatana-appindicator/app-indicator.h>
#include <gtk/gtk.h>
#include <cstdio>

static AppIndicator* g_indicator = nullptr;
static GtkWidget* g_menu = nullptr;
static CefRefPtr<CefBrowser> g_browser;
static CefRefPtr<CefWindow> g_window;
static bool g_quit_requested = false;

static void on_show_activate(GtkMenuItem*, gpointer) {
    if (g_window) {
        g_window->Show();
        g_window->Activate();
    }
}

static void on_quit_activate(GtkMenuItem*, gpointer) {
    g_quit_requested = true;
    if (g_window) {
        g_window->Close();
    }
}

void tray_init(CefRefPtr<CefBrowser> browser, CefRefPtr<CefWindow> window) {
    g_browser = browser;
    g_window = window;

    g_indicator = app_indicator_new(
        "tfl-teams-for-linux",
        "teams-for-linux",
        APP_INDICATOR_CATEGORY_COMMUNICATIONS);

    app_indicator_set_status(g_indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(g_indicator, "Teams for Linux");

    // Build context menu
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
