#pragma once

#include "client/input_hook.hpp"

namespace GameAway {

class InputReplay {
public:
    InputReplay();
    ~InputReplay() = default;
    
    // Replay an input event on this machine
    bool replay(const InputEvent& event);
    
    // Set screen resolution for coordinate scaling
    void setScreenSize(int width, int height);
    
private:
    int m_screenWidth = 1920;
    int m_screenHeight = 1080;
    int m_screenLeft = 0;   // Virtual screen left offset (multi-monitor)
    int m_screenTop = 0;    // Virtual screen top offset (multi-monitor)
};

} // namespace GameAway
