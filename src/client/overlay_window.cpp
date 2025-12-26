#include "overlay_window.hpp"

namespace GameAway {

OverlayWindow::OverlayWindow() {
    registerWindowClass();
}

OverlayWindow::~OverlayWindow() {
    hide();
}

bool OverlayWindow::registerWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    // RegisterClassExW returns 0 if the class already exists or on error
    // We ignore the error if it's because the class already exists
    ATOM result = RegisterClassExW(&wc);
    if (result == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    
    return true;
}

bool OverlayWindow::createWindow() {
    if (m_hwnd != nullptr) {
        return true; // Already created
    }
    
    // Get the virtual screen dimensions (covers all monitors)
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    
    // Create layered, topmost window
    // WS_EX_LAYERED: Allows transparency
    // WS_EX_TOPMOST: Always on top
    // WS_EX_TOOLWINDOW: Doesn't show in taskbar
    // WS_EX_NOACTIVATE: Doesn't become the active window when shown
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    DWORD style = WS_POPUP;
    
    m_hwnd = CreateWindowExW(
        exStyle,
        WINDOW_CLASS_NAME,
        L"GameAway Overlay",
        style,
        x, y, width, height,
        nullptr,    // No parent
        nullptr,    // No menu
        GetModuleHandle(nullptr),
        this        // Pass this pointer to WM_CREATE
    );
    
    if (m_hwnd == nullptr) {
        return false;
    }
    
    // Set window to be almost fully transparent (alpha = 1 to still capture input)
    // Alpha of 0 would make it completely click-through, which we don't want
    // Alpha of 1 makes it nearly invisible but still captures all input
    SetLayeredWindowAttributes(m_hwnd, 0, 1, LWA_ALPHA);
    
    return true;
}

bool OverlayWindow::show() {
    if (m_visible.load()) {
        return true; // Already visible
    }
    
    if (!createWindow()) {
        return false;
    }
    
    // Show the window without activating it
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hwnd);
    
    m_visible.store(true);
    return true;
}

void OverlayWindow::hide() {
    if (!m_visible.load()) {
        return; // Already hidden
    }
    
    if (m_hwnd != nullptr) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    
    m_visible.store(false);
}

void OverlayWindow::toggle() {
    if (m_visible.load()) {
        hide();
    } else {
        show();
    }
}

LRESULT CALLBACK OverlayWindow::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            // Store the this pointer
            {
                CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            }
            return 0;
            
        case WM_DESTROY:
            return 0;
            
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                // Don't paint anything - window is transparent
                EndPaint(hwnd, &ps);
            }
            return 0;
            
        // Block all input by handling these messages and doing nothing
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
            // Consume the input - do nothing
            return 0;
            
        case WM_SETCURSOR:
            // Set cursor to normal arrow
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
            
        case WM_ERASEBKGND:
            // Don't erase background
            return 1;
            
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace GameAway
