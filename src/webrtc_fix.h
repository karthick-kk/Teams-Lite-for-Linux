#pragma once

// Get JS to inject: monitors WebRTC screen share tracks and triggers
// ICE restart if frames aren't being decoded (works around a Teams
// renegotiation bug when receiving calls on Linux).
const char* webrtc_get_screenshare_fix_js();
