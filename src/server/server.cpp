#include "server.hpp"
#include "config.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace GameAway {

Server::Server(uint16_t port) : m_port(port) {
    m_replay = std::make_unique<InputReplay>();
}

Server::~Server() {
    stop();
}

void Server::setToken(const std::string& token) {
    m_token = token;
    m_crypto = std::make_unique<Crypto>(token);
}

void Server::setApprovalCallback(ApprovalCallback callback) {
    m_approvalCallback = std::move(callback);
}

bool Server::start() {
    if (m_running.load() || !m_crypto || !m_crypto->isValid()) {
        return false;
    }
    
    m_server = std::make_unique<ix::WebSocketServer>(m_port, "0.0.0.0");
    
    m_server->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> connectionState,
               ix::WebSocket& webSocket,
               const ix::WebSocketMessagePtr& msg) {
            handleMessage(connectionState, webSocket, msg);
        }
    );
    
    auto result = m_server->listen();
    if (!result.first) {
        std::cerr << "Error: " << result.second << std::endl;
        return false;
    }
    
    m_server->start();
    m_running.store(true);
    
    return true;
}

void Server::stop() {
    if (!m_running.load()) return;
    
    // Send graceful disconnect notification to connected client
    if (m_clientWebSocket && m_connected.load()) {
        try {
            json msg;
            msg["type"] = MsgType::DISCONNECT;
            m_clientWebSocket->send(msg.dump());
        } catch (...) {
            // Ignore send errors during shutdown
        }
    }
    
    m_running.store(false);
    m_connected.store(false);
    m_clientWebSocket = nullptr;
    
    if (m_server) {
        m_server->stop();
        m_server.reset();
    }
}

void Server::pause() {
    m_paused.store(true);
    
    // Notify client that server has paused
    if (m_clientWebSocket && m_connected.load()) {
        json msg;
        msg["type"] = MsgType::PAUSE;
        m_clientWebSocket->send(msg.dump());
    }
}

void Server::resume() {
    m_paused.store(false);
    
    // Notify client that server has resumed
    if (m_clientWebSocket && m_connected.load()) {
        json msg;
        msg["type"] = MsgType::RESUME;
        m_clientWebSocket->send(msg.dump());
    }
}

void Server::handleMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                           ix::WebSocket& webSocket,
                           const ix::WebSocketMessagePtr& msg) {
    if (msg->type == ix::WebSocketMessageType::Open) {
        std::cout << "\n[INFO] Client connected" << std::endl;
    }
    else if (msg->type == ix::WebSocketMessageType::Message) {
        try {
            json j = json::parse(msg->str);
            std::string type = j["type"].get<std::string>();
            
            if (type == MsgType::CONNECT) {
                std::string encData = j["d"].get<std::string>();
                std::string pcName;
                
                if (validateConnection(encData, pcName)) {
                    bool approved = true;
                    
                    if (m_approvalCallback) {
                        approved = m_approvalCallback(pcName);
                    }
                    
                    if (approved) {
                        m_connected.store(true);
                        m_clientWebSocket = &webSocket; // Store reference to client
                        json response;
                        response["type"] = MsgType::ACCEPT;
                        webSocket.send(response.dump());
                        std::cout << "[INFO] Connection accepted" << std::endl;
                    } else {
                        json response;
                        response["type"] = MsgType::REJECT;
                        webSocket.send(response.dump());
                        webSocket.close();
                        std::cout << "[INFO] Connection rejected by user" << std::endl;
                    }
                } else {
                    std::cout << "[WARN] Invalid token - connection rejected" << std::endl;
                    json response;
                    response["type"] = MsgType::REJECT;
                    response["reason"] = "Invalid token";
                    webSocket.send(response.dump());
                    webSocket.close();
                }
            }
            else if (type == MsgType::KEY || type == MsgType::MOUSE) {
                if (!m_connected.load() || m_paused.load()) return;
                
                std::string encData = j["d"].get<std::string>();
                std::string decrypted = m_crypto->decrypt(encData);
                
                if (!decrypted.empty()) {
                    InputEvent event = parseInputEvent(decrypted);
                    m_replay->replay(event);
                    m_eventsReceived.fetch_add(1);
                }
            }
            else if (type == MsgType::PAUSE) {
                m_paused.store(true);
                std::cout << "\n[INFO] Paused by client" << std::endl;
            }
            else if (type == MsgType::RESUME) {
                m_paused.store(false);
                std::cout << "\n[INFO] Resumed by client" << std::endl;
            }
            else if (type == MsgType::DISCONNECT) {
                // Client is closing gracefully
                std::cout << "\n[INFO] Client disconnecting gracefully..." << std::endl;
                m_connected.store(false);
                m_clientWebSocket = nullptr;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] " << e.what() << std::endl;
        }
    }
    else if (msg->type == ix::WebSocketMessageType::Close) {
        std::cout << "\n[INFO] Client disconnected" << std::endl;
        m_connected.store(false);
        m_clientWebSocket = nullptr;
    }
}

bool Server::validateConnection(const std::string& encryptedData, std::string& pcName) {
    std::string decrypted = m_crypto->decrypt(encryptedData);
    if (decrypted.empty()) return false;
    
    try {
        json j = json::parse(decrypted);
        pcName = j["pcName"].get<std::string>();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse connection data: " << e.what() << std::endl;
        return false;
    }
}

InputEvent Server::parseInputEvent(const std::string& jsonStr) {
    InputEvent event{};
    
    try {
        json j = json::parse(jsonStr);
        
        int typeInt = j["t"].get<int>();
        event.type = static_cast<InputEventType>(typeInt);
        event.vkCode = j.value("vk", 0);
        event.scanCode = j.value("sc", 0);
        event.x = j.value("x", 0);
        event.y = j.value("y", 0);
        event.button = j.value("btn", 0);
        event.wheelDelta = j.value("wd", 0);
        event.timestamp = j.value("ts", 0ULL);
        
    } catch (...) {
        // Return empty event on parse error
    }
    
    return event;
}

} // namespace GameAway
