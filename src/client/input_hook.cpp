#include "input_hook.hpp"
#include <Windows.h>
#include <chrono>

namespace GameAway {

// Static instance for Windows callbacks
InputHook* InputHook::s_instance = nullptr;
static HHOOK s_keyboardHook = nullptr;
static HHOOK s_mouseHook = nullptr;

InputHook::InputHook() {
    s_instance = this;
}

InputHook::~InputHook() {
    stop();
    s_instance = nullptr;
}

bool InputHook::start(InputCallback callback) {
    if (m_running.load()) return false;
    
    m_callback = std::move(callback);
    m_running.store(true);
    m_paused.store(false);
    
    // Start message loop in separate thread
    m_messageThread = std::thread(&InputHook::messageLoop, this);
    
    return true;
}

void InputHook::stop() {
    if (!m_running.load()) return;
    
    m_running.store(false);
    
    // Post quit message to break message loop
    auto handle = reinterpret_cast<HANDLE>(m_messageThread.native_handle());
    PostThreadMessage(GetThreadId(handle), WM_QUIT, 0, 0);
    
    if (m_messageThread.joinable()) {
        m_messageThread.join();
    }
}

void InputHook::pause() {
    m_paused.store(true);
}

void InputHook::resume() {
    m_paused.store(false);
}

void InputHook::messageLoop() {
    // Install hooks (must be done from the thread that will run the message loop)
    s_keyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        keyboardProc,
        GetModuleHandle(nullptr),
        0
    );
    
    s_mouseHook = SetWindowsHookExW(
        WH_MOUSE_LL,
        mouseProc,
        GetModuleHandle(nullptr),
        0
    );
    
    if (!s_keyboardHook || !s_mouseHook) {
        m_running.store(false);
        return;
    }
    
    // Message loop
    MSG msg;
    while (m_running.load() && GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup hooks
    if (s_keyboardHook) {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = nullptr;
    }
    if (s_mouseHook) {
        UnhookWindowsHookEx(s_mouseHook);
        s_mouseHook = nullptr;
    }
}

LRESULT CALLBACK InputHook::keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance && !s_instance->m_paused.load()) {
        KBDLLHOOKSTRUCT* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        
        InputEvent event{};
        event.vkCode = static_cast<int>(kbd->vkCode);
        event.scanCode = static_cast<int>(kbd->scanCode);
        event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        switch (wParam) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                event.type = InputEventType::KeyDown;
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                event.type = InputEventType::KeyUp;
                break;
            default:
                return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
        }
        
        if (s_instance->m_callback) {
            s_instance->m_callback(event);
        }
    }
    
    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK InputHook::mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance && !s_instance->m_paused.load()) {
        MSLLHOOKSTRUCT* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        
        // Throttle mouse move events (max ~60 updates/second)
        static auto lastMoveTime = std::chrono::steady_clock::now();
        static constexpr auto MOUSE_THROTTLE_MS = std::chrono::milliseconds(16);
        
        InputEvent event{};
        event.x = mouse->pt.x;
        event.y = mouse->pt.y;
        event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        bool shouldSend = true;
        
        switch (wParam) {
            case WM_MOUSEMOVE:
            {
                auto now = std::chrono::steady_clock::now();
                if (now - lastMoveTime < MOUSE_THROTTLE_MS) {
                    shouldSend = false;  // Throttle - skip this event
                } else {
                    lastMoveTime = now;
                    event.type = InputEventType::MouseMove;
                }
                break;
            }
            case WM_LBUTTONDOWN:
                event.type = InputEventType::MouseButtonDown;
                event.button = 0;
                break;
            case WM_LBUTTONUP:
                event.type = InputEventType::MouseButtonUp;
                event.button = 0;
                break;
            case WM_RBUTTONDOWN:
                event.type = InputEventType::MouseButtonDown;
                event.button = 1;
                break;
            case WM_RBUTTONUP:
                event.type = InputEventType::MouseButtonUp;
                event.button = 1;
                break;
            case WM_MBUTTONDOWN:
                event.type = InputEventType::MouseButtonDown;
                event.button = 2;
                break;
            case WM_MBUTTONUP:
                event.type = InputEventType::MouseButtonUp;
                event.button = 2;
                break;
            case WM_MOUSEWHEEL:
                event.type = InputEventType::MouseWheel;
                event.wheelDelta = GET_WHEEL_DELTA_WPARAM(mouse->mouseData);
                break;
            default:
                shouldSend = false;
                break;
        }
        
        if (shouldSend && s_instance->m_callback) {
            s_instance->m_callback(event);
        }
    }
    
    return CallNextHookEx(s_mouseHook, nCode, wParam, lParam);
}

} // namespace GameAway
