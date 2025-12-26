#include "input_hook.hpp"
#include <Windows.h>
#include <chrono>
#include <iostream>

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
    std::cerr << "[DEBUG] InputHook::stop() called" << std::endl;
    if (!m_running.load()) {
        std::cerr << "[DEBUG] InputHook::stop() - not running, returning early" << std::endl;
        return;
    }
    
    m_running.store(false);
    std::cerr << "[DEBUG] InputHook::stop() - m_running set to false" << std::endl;
    
    // Post quit message to break message loop using stored thread ID
    DWORD threadId = m_messageThreadId.load();
    if (threadId != 0) {
        std::cerr << "[DEBUG] InputHook::stop() - posting WM_QUIT to thread " << threadId << "..." << std::endl;
        BOOL postResult = PostThreadMessage(threadId, WM_QUIT, 0, 0);
        std::cerr << "[DEBUG] InputHook::stop() - PostThreadMessage result: " << postResult << std::endl;
    } else {
        std::cerr << "[DEBUG] InputHook::stop() - no valid thread ID stored" << std::endl;
    }
    
    if (m_messageThread.joinable()) {
        std::cerr << "[DEBUG] InputHook::stop() - joining message thread..." << std::endl;
        m_messageThread.join();
        std::cerr << "[DEBUG] InputHook::stop() - message thread joined" << std::endl;
    } else {
        std::cerr << "[DEBUG] InputHook::stop() - message thread not joinable" << std::endl;
    }
    
    m_messageThreadId.store(0);
    std::cerr << "[DEBUG] InputHook::stop() complete" << std::endl;
}

void InputHook::pause() {
    m_paused.store(true);
}

void InputHook::resume() {
    m_paused.store(false);
}

void InputHook::messageLoop() {
    // Store this thread's ID immediately so stop() can post WM_QUIT to us
    m_messageThreadId.store(GetCurrentThreadId());
    std::cerr << "[DEBUG] InputHook::messageLoop() started (threadId: " << m_messageThreadId.load() << ")" << std::endl;
    
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
        std::cerr << "[DEBUG] InputHook::messageLoop() - failed to install hooks" << std::endl;
        m_running.store(false);
        return;
    }
    
    std::cerr << "[DEBUG] InputHook::messageLoop() - hooks installed, entering message loop" << std::endl;
    
    // Message loop
    MSG msg;
    while (m_running.load() && GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    std::cerr << "[DEBUG] InputHook::messageLoop() - exited message loop (m_running=" << m_running.load() << ")" << std::endl;
    
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
        
        InputEvent event{};
        event.x = mouse->pt.x;
        event.y = mouse->pt.y;
        event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        bool shouldSend = true;
        
        switch (wParam) {
            case WM_MOUSEMOVE:
                event.type = InputEventType::MouseMove;
                break;
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
