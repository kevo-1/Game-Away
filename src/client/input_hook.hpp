#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <functional>
#include <atomic>
#include <thread>
#include <cstdint>

namespace GameAway {

// Input event types
enum class InputEventType {
    KeyDown,
    KeyUp,
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    MouseWheel
};

// Input event data
struct InputEvent {
    InputEventType type;
    int vkCode;      // Virtual key code for keyboard
    int scanCode;    // Scan code for keyboard
    int x;           // Mouse X position (absolute or delta)
    int y;           // Mouse Y position (absolute or delta)
    int button;      // Mouse button (0=left, 1=right, 2=middle)
    int wheelDelta;  // Mouse wheel delta
    uint64_t timestamp;
};

// Callback type for input events
using InputCallback = std::function<void(const InputEvent&)>;

class InputHook {
public:
    InputHook();
    ~InputHook();
    
    // Start capturing input events
    bool start(InputCallback callback);
    
    // Stop capturing
    void stop();
    
    // Pause/resume without stopping
    void pause();
    void resume();
    bool isPaused() const { return m_paused.load(); }
    
    // Check if running
    bool isRunning() const { return m_running.load(); }

private:
    InputCallback m_callback;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::thread m_messageThread;
    std::atomic<DWORD> m_messageThreadId{0};  // Store thread ID for reliable WM_QUIT posting
    
    void messageLoop();
    
    // Static hook procedures (Windows requires static callbacks)
    static InputHook* s_instance;
    static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam);
};

} // namespace GameAway
