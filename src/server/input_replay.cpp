#include "input_replay.hpp"
#include <Windows.h>

namespace GameAway {

InputReplay::InputReplay() {
    // Get virtual screen dimensions (handles multi-monitor)
    m_screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    m_screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    m_screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    
    // Fallback to primary monitor if virtual screen fails
    if (m_screenWidth == 0) m_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    if (m_screenHeight == 0) m_screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

void InputReplay::setScreenSize(int width, int height) {
    m_screenWidth = width;
    m_screenHeight = height;
}

bool InputReplay::replay(const InputEvent& event) {
    INPUT input{};
    
    switch (event.type) {
        case InputEventType::KeyDown:
        case InputEventType::KeyUp:
        {
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = static_cast<WORD>(event.vkCode);
            input.ki.wScan = static_cast<WORD>(event.scanCode);
            input.ki.dwFlags = KEYEVENTF_SCANCODE;
            
            if (event.type == InputEventType::KeyUp) {
                input.ki.dwFlags |= KEYEVENTF_KEYUP;
            }
            
            // Extended key check (arrows, ins, del, home, end, page up/down, numpad enter)
            if (event.scanCode & 0xE000) {
                input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            }
            
            break;
        }
        
        case InputEventType::MouseMove:
        {
            input.type = INPUT_MOUSE;
            // Convert to absolute coordinates (0-65535 range) with proper rounding
            // Account for virtual screen offset (multi-monitor)
            int adjustedX = event.x - m_screenLeft;
            int adjustedY = event.y - m_screenTop;
            
            input.mi.dx = static_cast<LONG>(((adjustedX * 65536) / m_screenWidth) + 1);
            input.mi.dy = static_cast<LONG>(((adjustedY * 65536) / m_screenHeight) + 1);
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
            break;
        }
        
        case InputEventType::MouseButtonDown:
        case InputEventType::MouseButtonUp:
        {
            input.type = INPUT_MOUSE;
            
            switch (event.button) {
                case 0: // Left
                    input.mi.dwFlags = (event.type == InputEventType::MouseButtonDown) 
                        ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
                    break;
                case 1: // Right
                    input.mi.dwFlags = (event.type == InputEventType::MouseButtonDown) 
                        ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
                    break;
                case 2: // Middle
                    input.mi.dwFlags = (event.type == InputEventType::MouseButtonDown) 
                        ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
                    break;
            }
            break;
        }
        
        case InputEventType::MouseWheel:
        {
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_WHEEL;
            input.mi.mouseData = static_cast<DWORD>(event.wheelDelta);
            break;
        }
    }
    
    UINT result = SendInput(1, &input, sizeof(INPUT));
    return result == 1;
}

} // namespace GameAway
