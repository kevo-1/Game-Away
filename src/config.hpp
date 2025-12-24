#pragma once

#include <string>
#include <cstdint>

namespace GameAway {

// Network configuration
constexpr uint16_t DEFAULT_PORT = 8765;
constexpr int CONNECTION_TIMEOUT_MS = 30000;  // 30 seconds to allow manual approval

// Token configuration
constexpr size_t TOKEN_LENGTH = 6;

// Pause shortcut: Ctrl+Shift+P
constexpr int PAUSE_MODIFIER_CTRL = 0x0002;  // MOD_CONTROL
constexpr int PAUSE_MODIFIER_SHIFT = 0x0004; // MOD_SHIFT
constexpr int PAUSE_KEY = 0x50;              // 'P' key

// Performance
constexpr int MAX_LATENCY_MS = 200;

// Message types
namespace MsgType {
    constexpr const char* CONNECT = "connect";
    constexpr const char* KEY = "key";
    constexpr const char* MOUSE = "mouse";
    constexpr const char* PAUSE = "pause";
    constexpr const char* RESUME = "resume";
    constexpr const char* ACCEPT = "accept";
    constexpr const char* REJECT = "reject";
}

} // namespace GameAway
