/**
 * ORKA Text Injector — Linux implementation
 *
 * §4 Priority matrix:
 *   X11:     xdotool type --clearmodifiers --delay 0
 *   X11 fallback: xdotool key ctrl+v (after setting PRIMARY)
 *   Wayland: wl-copy + wlr-virtual-keyboard Ctrl+V simulation
 */

#include "text_injector.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "core/utf8_utils.h"
#include <array>

namespace orka {
namespace platform {

namespace {

std::string wideToUtf8(const std::wstring& wide) {
    return orka::util::wideToUtf8(wide);
}

// Shell-escape a string for safe use in system() calls
std::string shellEscape(const std::string& s) {
    std::string escaped = "'";
    for (char c : s) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

} // anonymous namespace


TextInjector::TextInjector() {
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const char* xdgSession = std::getenv("XDG_SESSION_TYPE");
    m_isWayland = (waylandDisplay != nullptr)
               || (xdgSession && std::strcmp(xdgSession, "wayland") == 0);
}

TextInjector::~TextInjector() = default;

bool TextInjector::inject(const std::wstring& text) {
    if (text.empty()) return false;

    if (m_isWayland) {
        return injectWayland(text);
    } else {
        return injectX11(text);
    }
}


// ════════════════════════════════════════════════════════════════════
// §4: X11 — xdotool type (primary method)
// ════════════════════════════════════════════════════════════════════

bool TextInjector::injectX11(const std::wstring& text) {
    std::string utf8 = wideToUtf8(text);
    std::string escaped = shellEscape(utf8);

    // Method 1: xdotool type (fastest, no clipboard dependency)
    std::string cmd = "xdotool type --clearmodifiers --delay 0 " + escaped + " 2>/dev/null";
    int ret = std::system(cmd.c_str());

    if (ret == 0) {
        std::cout << "[ORKA] Injected via xdotool type\n";
        return true;
    }

    // Method 2: Clipboard fallback — set PRIMARY then paste
    std::cerr << "[ORKA] xdotool type failed, falling back to clipboard\n";
    std::string setClip = "echo -n " + escaped + " | xclip -selection primary 2>/dev/null";
    if (std::system(setClip.c_str()) != 0) {
        std::cerr << "[ORKA] Failed to set PRIMARY selection for fallback.\n";
    }

    std::string paste = "xdotool key --clearmodifiers ctrl+v 2>/dev/null";
    ret = std::system(paste.c_str());

    if (ret == 0) {
        std::cout << "[ORKA] Injected via xdotool clipboard fallback\n";
        return true;
    }

    std::cerr << "[ORKA] All X11 injection methods failed\n";
    return false;
}


// ════════════════════════════════════════════════════════════════════
// §4: Wayland — wl-copy + Ctrl+V
// ════════════════════════════════════════════════════════════════════

bool TextInjector::injectWayland(const std::wstring& text) {
    std::string utf8 = wideToUtf8(text);
    std::string escaped = shellEscape(utf8);

    // wl-copy sets the clipboard, then we simulate Ctrl+V
    std::string cmd = "echo -n " + escaped + " | wl-copy 2>/dev/null";
    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        std::cerr << "[ORKA] wl-copy failed. Install wl-clipboard.\n";
        return false;
    }

    // Simulate Ctrl+V via wtype (wlroots) or ydotool
    std::string paste = "wtype -M ctrl -k v 2>/dev/null || "
                        "ydotool key ctrl+v 2>/dev/null";
    ret = std::system(paste.c_str());

    if (ret == 0) {
        std::cout << "[ORKA] Injected via Wayland wl-copy + paste\n";
        return true;
    }

    std::cerr << "[ORKA] Wayland injection failed\n";
    return false;
}

} // namespace platform
} // namespace orka
