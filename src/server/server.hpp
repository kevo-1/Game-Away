#pragma once

#include "input_replay.hpp"
#include "utils/crypto.hpp"
#include <ixwebsocket/IXWebSocketServer.h>
#include <functional>
#include <atomic>
#include <string>
#include <memory>

namespace GameAway {

class Server {
public:
    Server(uint16_t port = 8765);
    ~Server();
    
    // Set the connection token
    void setToken(const std::string& token);
    
    // Start the server
    bool start();
    
    // Stop the server
    void stop();
    
    // Set callback for connection approval
    using ApprovalCallback = std::function<bool(const std::string& pcName)>;
    void setApprovalCallback(ApprovalCallback callback);
    
    // Pause/resume input replay
    void pause();
    void resume();
    bool isPaused() const { return m_paused.load(); }
    
    // Check if running
    bool isRunning() const { return m_running.load(); }
    
    // Get statistics
    uint64_t getEventsReceived() const { return m_eventsReceived.load(); }

private:
    uint16_t m_port;
    std::string m_token;
    std::unique_ptr<Crypto> m_crypto;
    std::unique_ptr<ix::WebSocketServer> m_server;
    std::unique_ptr<InputReplay> m_replay;
    ApprovalCallback m_approvalCallback;
    
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_connected{false};
    std::atomic<uint64_t> m_eventsReceived{0};
    ix::WebSocket* m_clientWebSocket = nullptr; // Pointer to connected client
    
    void handleMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                       ix::WebSocket& webSocket,
                       const ix::WebSocketMessagePtr& msg);
    
    InputEvent parseInputEvent(const std::string& json);
    bool validateConnection(const std::string& encryptedData, std::string& pcName);
};

} // namespace GameAway
