#include "notifications.h"
#include <libnotify/notify.h>
#include <cstdio>
#include <functional>

static std::function<void()> g_click_handler;

static void on_notification_action(NotifyNotification* n, char* action, gpointer data) {
    if (g_click_handler) {
        g_click_handler();
    }
}

void notifications_init() {
    notify_init("Teams Lite for Linux");
    fprintf(stderr, "[tfl] Notifications initialized\n");
}

void notifications_set_click_handler(std::function<void()> handler) {
    g_click_handler = std::move(handler);
}

void notifications_show(const std::string& title, const std::string& body) {
    NotifyNotification* n = notify_notification_new(
        title.c_str(), body.c_str(), "tfl-app");
    notify_notification_set_urgency(n, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(n, 5000);

    // Add "Show" action — clicking the notification shows the window
    notify_notification_add_action(n, "default", "Show",
        on_notification_action, nullptr, nullptr);

    GError* error = nullptr;
    if (!notify_notification_show(n, &error)) {
        fprintf(stderr, "[tfl] Notification error: %s\n",
                error ? error->message : "unknown");
        if (error) g_error_free(error);
    }
    g_object_unref(G_OBJECT(n));
}

void notifications_shutdown() {
    g_click_handler = nullptr;
    notify_uninit();
}
