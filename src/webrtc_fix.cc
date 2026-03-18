#include "webrtc_fix.h"

// JS that monkey-patches RTCPeerConnection to monitor incoming video tracks.
// When a screen share track arrives but frames aren't being decoded after 3s,
// it triggers restartIce() to force renegotiation.
//
// Safety: restartIce() is a no-op on healthy connections. If Microsoft fixes
// the underlying bug, the health check always passes and this never fires.

static const char* kScreenShareFixJS = R"JS(
(function() {
    if (window.__tfl_webrtc_patched) return;
    window.__tfl_webrtc_patched = true;

    const origAddEventListener = RTCPeerConnection.prototype.addEventListener;

    RTCPeerConnection.prototype.addEventListener = function(type, listener, options) {
        origAddEventListener.call(this, type, listener, options);

        // Only hook 'track' events, and only once per connection
        if (type !== 'track' || this.__tfl_track_hooked) return;
        this.__tfl_track_hooked = true;

        const pc = this;

        origAddEventListener.call(this, 'track', function(event) {
            const track = event.track;
            if (track.kind !== 'video') return;

            // Count video receivers — first is camera, second+ is screen share
            const videoReceivers = pc.getReceivers().filter(
                r => r.track && r.track.kind === 'video'
            );
            if (videoReceivers.length < 2) return;

            const receiver = event.receiver;
            if (!receiver) return;

            console.log('[tfl-webrtc] Screen share track detected, starting health check...');

            // Wait 3 seconds then check if frames are being decoded
            setTimeout(function() {
                if (track.readyState === 'ended' || pc.connectionState === 'closed') return;

                receiver.getStats().then(function(stats) {
                    let framesDecoded = 0;
                    stats.forEach(function(report) {
                        if (report.type === 'inbound-rtp' && report.kind === 'video') {
                            framesDecoded = report.framesDecoded || 0;
                        }
                    });

                    if (framesDecoded === 0) {
                        console.log('[tfl-webrtc] Health check: framesDecoded=0 after 3s — triggering restartIce()');
                        try {
                            pc.restartIce();
                        } catch (e) {
                            console.warn('[tfl-webrtc] restartIce() failed:', e);
                        }
                    } else {
                        console.log('[tfl-webrtc] Health check: framesDecoded=' + framesDecoded + ' — stream healthy, no action needed');
                    }
                }).catch(function(err) {
                    console.warn('[tfl-webrtc] getStats() failed:', err);
                });
            }, 3000);
        });
    };

    console.log('[tfl-webrtc] Screen share fix installed');
})();
)JS";

const char* webrtc_get_screenshare_fix_js() {
    return kScreenShareFixJS;
}
