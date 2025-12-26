#include "config.hpp"
#include "utils/token.hpp"
#include "server/server.hpp"
#include "client/client.hpp"
#include "client/overlay_window.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <Windows.h>

using namespace GameAway;

std::atomic<bool> g_paused{false};
std::atomic<bool> g_running{true};

// Global hotkey IDs
constexpr int HOTKEY_PAUSE = 1;
constexpr int HOTKEY_OVERLAY = 2;

void printHeader() {
    std::cout << "\n";
    std::cout << "  ╔═══════════════════════════════════════╗\n";
    std::cout << "  ║         GAME-AWAY v1.0.0              ║\n";
    std::cout << "  ║   Input Mirroring for Windows         ║\n";
    std::cout << "  ╚═══════════════════════════════════════╝\n";
    std::cout << "\n";
}

void printStatus(bool isServer, bool paused, uint64_t events) {
    static uint64_t lastEvents = 0;
    static bool lastPaused = false;
    
    // Only update if something changed
    if (events != lastEvents || paused != lastPaused) {
        lastEvents = events;
        lastPaused = paused;
        
        std::cout << "\r[" << (paused ? "PAUSED" : "ACTIVE") << "] ";
        std::cout << (isServer ? "Events received: " : "Events sent: ") << events;
        std::cout << " | Ctrl+Shift+P to " << (paused ? "resume" : "pause");
        std::cout << "          " << std::flush;
    }
}

void runServer() {
    std::string token = generateToken(TOKEN_LENGTH);
    
    std::cout << "╔═══════════════════════════════════════╗\n";
    std::cout << "║              SERVER MODE              ║\n";
    std::cout << "╠═══════════════════════════════════════╣\n";
    std::cout << "║  Connection Token: " << token << "    ║\n";
    std::cout << "╚═══════════════════════════════════════╝\n";
    std::cout << "\nShare this token with the client.\n";
    std::cout << "Waiting for connection on port " << DEFAULT_PORT << "...\n\n";
    
    Server server(DEFAULT_PORT);
    server.setToken(token);
    
    server.setApprovalCallback([](const std::string& pcName) {
        std::cout << "\n[CONNECTION REQUEST]\n";
        std::cout << "PC Name: " << pcName << "\n";
        std::cout << "Accept connection? (y/n): ";
        
        char response;
        std::cin >> response;
        
        return (response == 'y' || response == 'Y');
    });
    
    if (!server.start()) {
        std::cerr << "Failed to start server!\n";
        return;
    }
    
    std::cout << "Server started. Press Ctrl+C to exit.\n\n";
    
    // Register hotkey
    RegisterHotKey(nullptr, HOTKEY_PAUSE, MOD_CONTROL | MOD_SHIFT, 'P');
    
    MSG msg;
    while (g_running.load() && server.isRunning()) {
        // Check for hotkey
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_PAUSE) {
                if (g_paused.load()) {
                    g_paused.store(false);
                    server.resume();
                } else {
                    g_paused.store(true);
                    server.pause();
                }
            }
        }
        
        printStatus(true, g_paused.load(), server.getEventsReceived());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    UnregisterHotKey(nullptr, HOTKEY_PAUSE);
    server.stop();
}

void runClient() {
    std::cout << "╔═══════════════════════════════════════╗\n";
    std::cout << "║              CLIENT MODE              ║\n";
    std::cout << "╚═══════════════════════════════════════╝\n\n";
    
    std::string serverIp;
    std::string token;
    
    std::cout << "Enter server IP address: ";
    std::getline(std::cin, serverIp);
    
    if (serverIp.empty()) {
        serverIp = "localhost";
    }
    
    std::cout << "Enter connection token: ";
    std::getline(std::cin, token);
    
    if (token.length() != TOKEN_LENGTH) {
        std::cerr << "Invalid token length. Expected " << TOKEN_LENGTH << " characters.\n";
        return;
    }
    
    Client client;
    OverlayWindow overlay;
    
    client.setStatusCallback([](const std::string& status) {
        std::cout << "\n[STATUS] " << status << "\n";
    });
    
    std::cout << "\nConnecting to " << serverIp << ":" << DEFAULT_PORT << "...\n";
    
    if (!client.connect(serverIp, DEFAULT_PORT, token)) {
        std::cerr << "Failed to connect to server!\n";
        return;
    }
    
    std::cout << "\nConnected! Input mirroring active.\n";
    std::cout << "Press Ctrl+Shift+P to pause/resume.\n";
    std::cout << "Press Ctrl+Shift+L to toggle transparent overlay. Ctrl+C to exit.\n\n";
    
    // Register hotkeys
    RegisterHotKey(nullptr, HOTKEY_PAUSE, MOD_CONTROL | MOD_SHIFT, 'P');
    RegisterHotKey(nullptr, HOTKEY_OVERLAY, MOD_CONTROL | MOD_SHIFT, 'L');
    
    MSG msg;
    while (g_running.load() && client.isConnected()) {
        // Check for hotkey
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) {
                if (msg.wParam == HOTKEY_PAUSE) {
                    if (g_paused.load()) {
                        g_paused.store(false);
                        client.resume();
                    } else {
                        g_paused.store(true);
                        client.pause();
                    }
                } else if (msg.wParam == HOTKEY_OVERLAY) {
                    overlay.toggle();
                }
            }
        }
        
        printStatus(false, g_paused.load(), client.getEventsSent());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    UnregisterHotKey(nullptr, HOTKEY_PAUSE);
    UnregisterHotKey(nullptr, HOTKEY_OVERLAY);
    client.disconnect();
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        g_running.store(false);
        return TRUE;
    }
    return FALSE;
}

int main() {
    // Initialize network system (required for IXWebSocket on Windows)
    ix::initNetSystem();
    
    // Setup console handling
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    SetConsoleOutputCP(CP_UTF8);
    
    printHeader();
    
    std::cout << "Select mode:\n";
    std::cout << "  [1] Server (receive input)\n";
    std::cout << "  [2] Client (send input)\n";
    std::cout << "\nChoice: ";
    
    int choice;
    std::cin >> choice;
    std::cin.ignore(); // Clear newline
    
    std::cout << "\n";
    
    switch (choice) {
        case 1:
            runServer();
            break;
        case 2:
            runClient();
            break;
        default:
            std::cerr << "Invalid choice.\n";
            return 1;
    }
    
    std::cout << "\nExiting...\n";
    ix::uninitNetSystem();
    return 0;
}