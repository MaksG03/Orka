#pragma once

/**
 * ORKA Text Injector — Linux (X11 / Wayland)
 *
 * §4 Text Injector priority matrix:
 *   Linux X11:     1. xdotool type  2. xdotool clipboard fallback
 *   Linux Wayland: 1. wl-copy + Ctrl+V simulation
 */

#include <string>

namespace orka {
namespace platform {

class TextInjector {
public:
    TextInjector();
    ~TextInjector();

    /// Inject converted text into the active field
    bool inject(const std::wstring& text);

    /// Check if running under Wayland
    bool isWayland() const { return m_isWayland; }

private:
    bool m_isWayland;

    bool injectX11(const std::wstring& text);
    bool injectWayland(const std::wstring& text);
};

} // namespace platform
} // namespace orka
