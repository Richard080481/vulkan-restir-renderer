#include "platform/Win32Window.hpp"
#include "vulkan/VulkanContext.hpp"

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

std::optional<std::uint64_t> frameLimitFromArguments(int argumentCount, char** arguments) {
    constexpr std::string_view prefix = "--frames=";
    for (int index = 1; index < argumentCount; ++index) {
        const std::string_view argument(arguments[index]);
        if (argument.starts_with(prefix)) {
            return std::stoull(std::string(argument.substr(prefix.size())));
        }
    }
    return std::nullopt;
}

} // namespace

int main(int argumentCount, char** arguments) {
    try {
        const auto frameLimit = frameLimitFromArguments(argumentCount, arguments);
        Win32Window window(1600, 900, L"Vulkan ReSTIR Renderer - Milestone 0");
        VulkanContext renderer(window);
        std::uint64_t framesRendered = 0;

        while (window.pollEvents()) {
            if (window.width() == 0 || window.height() == 0) {
                window.waitForEvent();
                continue;
            }
            renderer.drawFrame();
            ++framesRendered;
            if (frameLimit.has_value() && framesRendered >= *frameLimit) {
                break;
            }
        }

        renderer.waitIdle();
        std::cout << "Rendered " << framesRendered << " frames\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
