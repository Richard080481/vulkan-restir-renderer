#include "platform/Win32Window.hpp"

#include <stdexcept>

Win32Window::Win32Window(std::uint32_t width, std::uint32_t height, const wchar_t* title)
    : instance_(GetModuleHandleW(nullptr)) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className_;

    if (RegisterClassExW(&windowClass) == 0) {
        throw std::runtime_error("Failed to register the Win32 window class");
    }

    RECT rectangle{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);

    window_ = CreateWindowExW(
        0,
        className_,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top,
        nullptr,
        nullptr,
        instance_,
        this);

    if (window_ == nullptr) {
        UnregisterClassW(className_, instance_);
        throw std::runtime_error("Failed to create the Win32 window");
    }

    ShowWindow(window_, SW_SHOW);
}

Win32Window::~Win32Window() {
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
    if (instance_ != nullptr) {
        UnregisterClassW(className_, instance_);
    }
}

bool Win32Window::pollEvents() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            running_ = false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return running_;
}

void Win32Window::waitForEvent() const {
    WaitMessage();
}

std::uint32_t Win32Window::width() const {
    RECT rectangle{};
    GetClientRect(window_, &rectangle);
    return static_cast<std::uint32_t>(rectangle.right - rectangle.left);
}

std::uint32_t Win32Window::height() const {
    RECT rectangle{};
    GetClientRect(window_, &rectangle);
    return static_cast<std::uint32_t>(rectangle.bottom - rectangle.top);
}

LRESULT CALLBACK Win32Window::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
    }

    auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CLOSE:
        if (self != nullptr) {
            self->running_ = false;
        }
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (self != nullptr) {
            self->window_ = nullptr;
            self->running_ = false;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
