#pragma once

#include "input_hook.hpp"
#include "utils/crypto.hpp"
#include <ixwebsocket/IXWebSocket.h>
#include <functional>
#include <atomic>
#include <string>
#include <memory>

namespace GameAway {

class Client {
public:
    Client();
    ~Client();
    
    // Connect to server
    bool connect(const std::string& serverIp, uint16_t port, const std::string& token);
    
    // Disconnect
    void disconnect();
    
    // Pause/resume input capture
    void pause();
    void resume();
    bool isPaused() const { return m_paused.load(); }
    
    // Check connection status
    bool isConnected() const { return m_connected.load(); }
    
    // Set callback for status updates
    using StatusCallback = std::function<void(const std::string& status)>;
    void setStatusCallback(StatusCallback callback);
    
    // Get statistics
    uint64_t getEventsSent() const { return m_eventsSent.load(); }

private:
    std::string m_token;
    std::unique_ptr<Crypto> m_crypto;
    std::unique_ptr<ix::WebSocket> m_webSocket;
    std::unique_ptr<InputHook> m_inputHook;
    StatusCallback m_statusCallback;
    
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_paused{false};
    std::atomic<uint64_t> m_eventsSent{0};
    
    void onInputEvent(const InputEvent& event);
    std::string serializeInputEvent(const InputEvent& event);
    void sendStatus(const std::string& status);
};

} // namespace GameAway
