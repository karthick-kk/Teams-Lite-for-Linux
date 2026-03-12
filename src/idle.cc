#include "idle.h"
#include <gio/gio.h>
#include <cstdio>

// Query GNOME Mutter IdleMonitor for system-level idle time via DBus
int get_system_idle_seconds() {
    GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!conn) return 0;

    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn,
        "org.gnome.Mutter.IdleMonitor",
        "/org/gnome/Mutter/IdleMonitor/Core",
        "org.gnome.Mutter.IdleMonitor",
        "GetIdletime",
        nullptr,
        G_VARIANT_TYPE("(t)"),
        G_DBUS_CALL_FLAGS_NONE,
        500,  // timeout ms
        nullptr,
        &error);

    int idle_sec = 0;
    if (result) {
        guint64 idle_ms = 0;
        g_variant_get(result, "(t)", &idle_ms);
        idle_sec = (int)(idle_ms / 1000);
        g_variant_unref(result);
    } else {
        if (error) {
            // Fallback: not on GNOME or DBus unavailable — assume active
            g_error_free(error);
        }
    }

    g_object_unref(conn);
    return idle_sec;
}

// JS to inject: override Page Visibility API so Teams thinks window is always visible
static const char* kVisibilityOverrideJS = R"JS(
(function() {
    // Override document.hidden and document.visibilityState
    Object.defineProperty(document, 'hidden', {
        get: function() { return false; },
        configurable: true
    });
    Object.defineProperty(document, 'visibilityState', {
        get: function() { return 'visible'; },
        configurable: true
    });

    // Suppress visibilitychange events from firing
    document.addEventListener('visibilitychange', function(e) {
        e.stopImmediatePropagation();
    }, true);

    // Periodically dispatch a fake mousemove to keep Teams "active"
    setInterval(function() {
        document.dispatchEvent(new MouseEvent('mousemove', {
            bubbles: true, clientX: 100, clientY: 100
        }));
    }, 30000);  // every 30 seconds

    console.log('[tfl] Visibility override active — Teams will stay Available');
})();
)JS";

const char* idle_get_visibility_override_js() {
    return kVisibilityOverrideJS;
}

void idle_monitor_start() {
    fprintf(stderr, "[tfl] Idle monitor started (visibility override + DBus idle query)\n");
}

void idle_monitor_stop() {
    fprintf(stderr, "[tfl] Idle monitor stopped\n");
}
