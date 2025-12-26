#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <atomic>
#include <thread>

namespace GameAway {

/**
 * Transparent full-screen overlay window that blocks all input.
 * When visible, the overlay covers the entire screen but is fully transparent,
 * allowing the user to see the desktop/apps behind while preventing any
 * mouse/keyboard input from reaching them.
 * 
 * Uses a low-level keyboard hook to also block system keys like Windows key.
 */
class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();
    
    // Show the overlay window (creates it if not already created)
    bool show();
    
    // Hide and destroy the overlay window
    void hide();
    
    // Toggle visibility
    void toggle();
    
    // Check if overlay is currently visible
    bool isVisible() const { return m_visible.load(); }

private:
    HWND m_hwnd = nullptr;
    std::atomic<bool> m_visible{false};
    
    // Keyboard hook for blocking system keys
    HHOOK m_keyboardHook = nullptr;
    std::thread m_hookThread;
    std::atomic<bool> m_hookRunning{false};
    
    // Window class name
    static constexpr const wchar_t* WINDOW_CLASS_NAME = L"GameAwayOverlay";
    
    // Static instance for hook callback
    static OverlayWindow* s_instance;
    
    // Register window class (called once)
    bool registerWindowClass();
    
    // Create the overlay window
    bool createWindow();
    
    // Install/uninstall keyboard hook
    void installKeyboardHook();
    void uninstallKeyboardHook();
    void hookThreadProc();
    
    // Static callbacks
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
};

} // namespace GameAway
