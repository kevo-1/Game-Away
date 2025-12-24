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
            } catch (...) {
                // Ignore parse errors
            }
        }
        else if (msg->type == ix::WebSocketMessageType::Close) {
            m_connected.store(false);
            m_inputHook->stop();
            sendStatus("Disconnected from server");
            connectionDone.store(true);
        }
        else if (msg->type == ix::WebSocketMessageType::Error) {
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
    m_inputHook->stop();
    
    if (m_webSocket) {
        m_webSocket->stop();
        m_webSocket.reset();
    }
    
    m_connected.store(false);
}

void Client::pause() {
    m_paused.store(true);
    m_inputHook->pause();
    
    if (m_webSocket && m_connected.load()) {
        json msg;
        msg["type"] = MsgType::PAUSE;
        m_webSocket->send(msg.dump());
    }
}

void Client::resume() {
    m_paused.store(false);
    m_inputHook->resume();
    
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
