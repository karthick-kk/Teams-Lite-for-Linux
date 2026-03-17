#include "idle.h"
#include <gio/gio.h>
#include <cstdio>
#include <utility>

// --- Module state ---
static int s_idle_timeout_sec = 0;
static IdleJsCallback s_js_callback;
static guint s_timer_id = 0;
static bool s_override_active = true;  // starts active (injected on page load)

// --- DBus helpers ---

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
            // Not on GNOME or DBus unavailable — assume active
            g_error_free(error);
        }
    }

    g_object_unref(conn);
    return idle_sec;
}

// Try a single screensaver DBus interface, return true if screen is locked
static bool try_screensaver_get_active(GDBusConnection* conn, const char* bus_name, const char* object_path) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn,
        bus_name,
        object_path,
        bus_name,  // interface name matches bus name for both freedesktop and GNOME
        "GetActive",
        nullptr,
        G_VARIANT_TYPE("(b)"),
        G_DBUS_CALL_FLAGS_NONE,
        500,
        nullptr,
        &error);

    bool active = false;
    if (result) {
        g_variant_get(result, "(b)", &active);
        g_variant_unref(result);
    } else {
        if (error) g_error_free(error);
    }
    return active;
}

bool is_screen_locked() {
    GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!conn) return false;

    // Try freedesktop ScreenSaver first (works on most DEs)
    bool locked = try_screensaver_get_active(conn,
        "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver");

    // Fallback to GNOME ScreenSaver
    if (!locked) {
        locked = try_screensaver_get_active(conn,
            "org.gnome.ScreenSaver", "/org/gnome/ScreenSaver");
    }

    g_object_unref(conn);
    return locked;
}

// --- JS snippets ---

static const char* kVisibilityOverrideJS = R"JS(
(function() {
    // Clear any previous restore state
    if (window.__tfl_mousemove_interval) {
        clearInterval(window.__tfl_mousemove_interval);
    }

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
    if (!window.__tfl_visibility_suppressor) {
        window.__tfl_visibility_suppressor = function(e) {
            e.stopImmediatePropagation();
        };
        document.addEventListener('visibilitychange', window.__tfl_visibility_suppressor, true);
    }

    // Periodically dispatch a fake mousemove to keep Teams "active"
    window.__tfl_mousemove_interval = setInterval(function() {
        document.dispatchEvent(new MouseEvent('mousemove', {
            bubbles: true, clientX: 100, clientY: 100
        }));
    }, 30000);

    console.log('[tfl] Visibility override active — Teams will stay Available');
})();
)JS";

static const char* kVisibilityRestoreJS = R"JS(
(function() {
    // Stop fake mousemove
    if (window.__tfl_mousemove_interval) {
        clearInterval(window.__tfl_mousemove_interval);
        window.__tfl_mousemove_interval = null;
    }

    // Remove the visibilitychange suppressor
    if (window.__tfl_visibility_suppressor) {
        document.removeEventListener('visibilitychange', window.__tfl_visibility_suppressor, true);
        window.__tfl_visibility_suppressor = null;
    }

    // Restore native document.hidden and document.visibilityState
    delete document.hidden;
    delete document.visibilityState;

    // Notify Teams of the real visibility state
    document.dispatchEvent(new Event('visibilitychange'));

    console.log('[tfl] Visibility override paused — native status active');
})();
)JS";

const char* idle_get_visibility_override_js() {
    return kVisibilityOverrideJS;
}

const char* idle_get_visibility_restore_js() {
    return kVisibilityRestoreJS;
}

// --- Idle monitor timer ---

static gboolean idle_check_cb(gpointer) {
    if (s_idle_timeout_sec <= 0 || !s_js_callback) return G_SOURCE_CONTINUE;

    int idle_sec = get_system_idle_seconds();
    bool locked = is_screen_locked();
    bool should_be_active = (idle_sec < s_idle_timeout_sec) && !locked;

    if (s_override_active && !should_be_active) {
        // User went idle or locked — pause the override
        fprintf(stderr, "[tfl] User idle (%ds) or locked — pausing visibility override\n", idle_sec);
        s_js_callback(kVisibilityRestoreJS);
        s_override_active = false;
    } else if (!s_override_active && should_be_active) {
        // User came back — re-enable the override
        fprintf(stderr, "[tfl] User active again — re-enabling visibility override\n");
        s_js_callback(kVisibilityOverrideJS);
        s_override_active = true;
    }

    return G_SOURCE_CONTINUE;
}

void idle_monitor_start(int idle_timeout_sec, IdleJsCallback js_callback) {
    s_idle_timeout_sec = idle_timeout_sec;
    s_js_callback = std::move(js_callback);
    s_override_active = true;

    if (idle_timeout_sec <= 0) {
        fprintf(stderr, "[tfl] Idle monitor disabled (idle_timeout = 0) — always Available\n");
        return;
    }

    // Poll every 15 seconds
    s_timer_id = g_timeout_add_seconds(15, idle_check_cb, nullptr);
    fprintf(stderr, "[tfl] Idle monitor started (timeout: %ds, poll: 15s)\n", idle_timeout_sec);
}

void idle_monitor_stop() {
    if (s_timer_id > 0) {
        g_source_remove(s_timer_id);
        s_timer_id = 0;
    }
    s_js_callback = nullptr;
    fprintf(stderr, "[tfl] Idle monitor stopped\n");
}
