#include "client.hpp"
#include "utils/token.hpp"
#include "config.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace GameAway {

Client::Client() {
    m_inputHook = std::make_unique<InputHook>();
}

Client::~Client() {
    disconnect();
}

void Client::setStatusCallback(StatusCallback callback) {
    m_statusCallback = std::move(callback);
}

void Client::sendStatus(const std::string& status) {
    if (m_statusCallback) {
        m_statusCallback(status);
    }
}

bool Client::connect(const std::string& serverIp, uint16_t port, const std::string& token) {
    m_token = token;
    m_crypto = std::make_unique<Crypto>(token);
    
    if (!m_crypto->isValid()) {
        sendStatus("Error: Failed to initialize encryption");
        return false;
    }
    
    std::string url = "ws://" + serverIp + ":" + std::to_string(port);
    
    m_webSocket = std::make_unique<ix::WebSocket>();
    m_webSocket->setUrl(url);
    
    std::atomic<bool> connectionResult{false};
    std::atomic<bool> connectionDone{false};
    
    m_webSocket->setOnMessageCallback([this, &connectionResult, &connectionDone](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            sendStatus("Connected, sending authentication...");
            
            // Send encrypted connection request
            json connectData;
            connectData["pcName"] = getPcName();
            
            std::string encrypted = m_crypto->encrypt(connectData.dump());
            
            json request;
            request["type"] = MsgType::CONNECT;
            request["d"] = encrypted;
            
            m_webSocket->send(request.dump());
        }
        else if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                json j = json::parse(msg->str);
                std::string type = j["type"].get<std::string>();
                
                if (type == MsgType::ACCEPT) {
                    m_connected.store(true);
                    connectionResult.store(true);
                    connectionDone.store(true);
                    sendStatus("Connection accepted! Starting input capture...");
                    
                    // Start input hook
                    m_inputHook->start([this](const InputEvent& event) {
                        onInputEvent(event);
                    });
                }
                else if (type == MsgType::REJECT) {
                    sendStatus("Connection rejected by server");
                    connectionResult.store(false);
                    connectionDone.store(true);
                }
                else if (type == MsgType::PAUSE) {
                    // Server requested pause
                    m_paused.store(true);
                    if (m_inputHook && m_inputHook->isRunning()) {
                        m_inputHook->pause();
                    }
                    sendStatus("Server paused input mirroring");
                }
                else if (type == MsgType::RESUME) {
                    // Server requested resume
                    m_paused.store(false);
                    if (m_inputHook && m_inputHook->isRunning()) {
                        m_inputHook->resume();
                    }
                    sendStatus("Server resumed input mirroring");
                }
                else if (type == MsgType::DISCONNECT) {
                    // Server is closing gracefully
                    m_connected.store(false);
                    if (m_inputHook && m_inputHook->isRunning()) {
                        m_inputHook->stop();
                    }
                    sendStatus("Server closed the connection");
                    // Close our end of the WebSocket to complete the disconnect
                    if (m_webSocket) {
                        m_webSocket->close();
                    }
                }
            } catch (const std::exception& e) {
                // Log parse errors for debugging (silent in production)
                std::cerr << "[DEBUG] Message parse error: " << e.what() << std::endl;
            }
        }
        else if (msg->type == ix::WebSocketMessageType::Close) {
            m_connected.store(false);
            if (m_inputHook) {
                m_inputHook->stop();
            }
            sendStatus("Disconnected from server");
            connectionDone.store(true);
        }
        else if (msg->type == ix::WebSocketMessageType::Error) {
            m_connected.store(false);
            sendStatus("Connection error: " + msg->errorInfo.reason);
            connectionDone.store(true);
        }
    });
    
    m_webSocket->start();
    
    // Wait for connection result with timeout
    int timeout = CONNECTION_TIMEOUT_MS / 100;
    while (!connectionDone.load() && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout--;
    }
    
    if (!connectionDone.load()) {
        sendStatus("Connection timeout");
        m_webSocket->stop();
        return false;
    }
    
    return connectionResult.load();
}

void Client::disconnect() {
    std::cerr << "[DEBUG] Client::disconnect() called" << std::endl;
    
    // Send graceful disconnect notification to server
    if (m_webSocket && m_connected.load()) {
        try {
            std::cerr << "[DEBUG] Sending DISCONNECT message to server..." << std::endl;
            json msg;
            msg["type"] = MsgType::DISCONNECT;
            m_webSocket->send(msg.dump());
            std::cerr << "[DEBUG] DISCONNECT message sent" << std::endl;
        } catch (...) {
            std::cerr << "[DEBUG] Failed to send DISCONNECT message" << std::endl;
        }
    }
    
    if (m_inputHook && m_inputHook->isRunning()) {
        std::cerr << "[DEBUG] Stopping input hook..." << std::endl;
        m_inputHook->stop();
        std::cerr << "[DEBUG] Input hook stopped" << std::endl;
    }
    
    if (m_webSocket) {
        std::cerr << "[DEBUG] Stopping WebSocket..." << std::endl;
        m_webSocket->stop();
        std::cerr << "[DEBUG] WebSocket stopped, resetting..." << std::endl;
        m_webSocket.reset();
        std::cerr << "[DEBUG] WebSocket reset complete" << std::endl;
    }
    
    m_connected.store(false);
    std::cerr << "[DEBUG] Client::disconnect() complete" << std::endl;
}

void Client::pause() {
    m_paused.store(true);
    if (m_inputHook && m_inputHook->isRunning()) {
        m_inputHook->pause();
    }
    
    if (m_webSocket && m_connected.load()) {
        json msg;
        msg["type"] = MsgType::PAUSE;
        m_webSocket->send(msg.dump());
    }
}

void Client::resume() {
    m_paused.store(false);
    if (m_inputHook && m_inputHook->isRunning()) {
        m_inputHook->resume();
    }
    
    if (m_webSocket && m_connected.load()) {
        json msg;
        msg["type"] = MsgType::RESUME;
        m_webSocket->send(msg.dump());
    }
}

void Client::onInputEvent(const InputEvent& event) {
    if (!m_connected.load() || m_paused.load()) return;
    
    std::string serialized = serializeInputEvent(event);
    std::string encrypted = m_crypto->encrypt(serialized);
    
    json msg;
    msg["type"] = (event.type == InputEventType::KeyDown || event.type == InputEventType::KeyUp) 
        ? MsgType::KEY : MsgType::MOUSE;
    msg["d"] = encrypted;
    
    m_webSocket->send(msg.dump());
    m_eventsSent.fetch_add(1);
}

std::string Client::serializeInputEvent(const InputEvent& event) {
    json j;
    j["t"] = static_cast<int>(event.type);
    j["vk"] = event.vkCode;
    j["sc"] = event.scanCode;
    j["x"] = event.x;
    j["y"] = event.y;
    j["btn"] = event.button;
    j["wd"] = event.wheelDelta;
    j["ts"] = event.timestamp;
    return j.dump();
}

} // namespace GameAway
