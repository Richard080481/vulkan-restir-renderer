#pragma once

#include <cstdint>
#include <windows.h>

class Win32Window {
public:
    Win32Window(std::uint32_t width, std::uint32_t height, const wchar_t* title);
    ~Win32Window();

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    [[nodiscard]] bool pollEvents();
    void waitForEvent() const;
    [[nodiscard]] HWND handle() const { return window_; }
    [[nodiscard]] HINSTANCE instance() const { return instance_; }
    [[nodiscard]] std::uint32_t width() const;
    [[nodiscard]] std::uint32_t height() const;

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    bool running_ = true;
    const wchar_t* className_ = L"VulkanReSTIRRendererWindow";
};
