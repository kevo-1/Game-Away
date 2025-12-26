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
std::atomic<bool> g_waitingForInput{false}; // Pause status output when waiting for user input

// Global hotkey IDs
constexpr int HOTKEY_PAUSE = 1;
constexpr int HOTKEY_OVERLAY = 2;

void printHeader() {
    std::cout << "\n";
    std::cout << "  GAME-AWAY v1.0.0\n";
    std::cout << "  Input Mirroring for Windows\n";
    std::cout << "  ─────────────────────────────\n";
    std::cout << "\n";
}

void printStatus(bool isServer, bool paused, uint64_t events) {
    // Don't print status if waiting for user input
    if (g_waitingForInput.load()) {
        return;
    }
    
    std::cout << "\r";
    if (paused) {
        std::cout << "[PAUSED] ";
    } else {
        std::cout << "[ACTIVE] ";
    }
    std::cout << (isServer ? "Received: " : "Sent: ") << events << " events";
    std::cout << "                    " << std::flush;
}

void runServer() {
    std::string token = generateToken(TOKEN_LENGTH);
    
    std::cout << "SERVER MODE\n";
    std::cout << "───────────────────────\n";
    std::cout << "Connection Token: \033[32m" << token << "\033[0m\n";  // Green color
    std::cout << "Share this token with the client.\n";
    std::cout << "Waiting for connection on port " << DEFAULT_PORT << "...\n\n";
    
    Server server(DEFAULT_PORT);
    server.setToken(token);
    
    server.setApprovalCallback([](const std::string& pcName) {
        g_waitingForInput.store(true);
        
        std::cout << "\n\n[CONNECTION REQUEST]\n";
        std::cout << "PC Name: " << pcName << "\n";
        std::cout << "Accept connection? (y/n): " << std::flush;
        
        char response;
        std::cin >> response;
        
        g_waitingForInput.store(false);
        std::cout << "\n"; // New line after input
        
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
    std::cout << "CLIENT MODE\n";
    std::cout << "───────────────────────\n\n";
    
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
    
    // Flag to track if we should exit due to disconnection
    std::atomic<bool> shouldExit{false};
    
    client.setStatusCallback([&shouldExit](const std::string& status) {
        std::cout << "\n[STATUS] " << status << "\n";
        // If connection was closed or server closed, signal immediate exit
        if (status.find("closed") != std::string::npos || 
            status.find("Disconnected") != std::string::npos ||
            status.find("error") != std::string::npos) {
            shouldExit.store(true);
            g_running.store(false);
        }
    });
    
    std::cout << "\nConnecting to " << serverIp << ":" << DEFAULT_PORT << "...\n";
    
    if (!client.connect(serverIp, DEFAULT_PORT, token)) {
        std::cerr << "Failed to connect to server!\n";
        return;
    }
    
    std::cout << "\nConnected! Input mirroring active.\n";
    std::cout << "Press Ctrl+Shift+P to pause/resume.\n";
    std::cout << "Press Ctrl+Shift+L to toggle transparent overlay.\nPress Ctrl+C to exit.\n\n";
    
    // Register hotkeys
    RegisterHotKey(nullptr, HOTKEY_PAUSE, MOD_CONTROL | MOD_SHIFT, 'P');
    RegisterHotKey(nullptr, HOTKEY_OVERLAY, MOD_CONTROL | MOD_SHIFT, 'L');
    
    MSG msg;
    std::cerr << "[DEBUG] Entering main client loop" << std::endl;
    while (g_running.load() && client.isConnected() && !shouldExit.load()) {
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
        
        // Sync g_paused with client state (which may be updated by server)
        g_paused.store(client.isPaused());
        printStatus(false, client.isPaused(), client.getEventsSent());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cerr << "[DEBUG] Exited main client loop (g_running=" << g_running.load() 
              << ", isConnected=" << client.isConnected() 
              << ", shouldExit=" << shouldExit.load() << ")" << std::endl;
    
    std::cerr << "[DEBUG] Unregistering hotkeys..." << std::endl;
    UnregisterHotKey(nullptr, HOTKEY_PAUSE);
    UnregisterHotKey(nullptr, HOTKEY_OVERLAY);
    std::cerr << "[DEBUG] Hotkeys unregistered" << std::endl;
    
    // Ensure overlay is closed before exiting (prevents black screen on Ctrl+C)
    std::cerr << "[DEBUG] Hiding overlay..." << std::endl;
    overlay.hide();
    std::cerr << "[DEBUG] Overlay hidden" << std::endl;
    
    // Print exit reason
    if (!client.isConnected()) {
        std::cout << "\n\nConnection closed. Exiting...\n";
    }
    
    std::cerr << "[DEBUG] Calling client.disconnect()..." << std::endl;
    client.disconnect();
    std::cerr << "[DEBUG] client.disconnect() returned" << std::endl;
    return;
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
    
    // Enable ANSI escape codes for colored output on Windows
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode = 0;
    GetConsoleMode(hConsole, &consoleMode);
    SetConsoleMode(hConsole, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    
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