#include "notifications.h"
#include <libnotify/notify.h>
#include <cstdio>

void notifications_init() {
    notify_init("Teams for Linux");
    fprintf(stderr, "[tfl] Notifications initialized\n");
}

void notifications_show(const std::string& title, const std::string& body) {
    NotifyNotification* n = notify_notification_new(
        title.c_str(), body.c_str(), "teams-for-linux");
    notify_notification_set_urgency(n, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(n, 5000);

    GError* error = nullptr;
    if (!notify_notification_show(n, &error)) {
        fprintf(stderr, "[tfl] Notification error: %s\n",
                error ? error->message : "unknown");
        if (error) g_error_free(error);
    }
    g_object_unref(G_OBJECT(n));
}

void notifications_shutdown() {
    notify_uninit();
}
