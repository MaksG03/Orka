#pragma once

/**
 * ORKA Selection Monitor — Linux (X11 / Wayland)
 *
 * §3.3 X11:  XFixesSelectSelectionInput for PRIMARY selection
 * §3.4 Wayland: wlr-data-control / zwp-primary-selection
 *
 * Architecture: event-driven, no polling. Callback fires only
 * when user finishes selecting text (mouse-up / Shift+Arrow).
 */

#include <string>
#include <functional>

#ifdef __linux__
#include <sys/types.h>
#endif

namespace orka {
namespace platform {

using SelectionCallback = std::function<void(const std::wstring& selectedText)>;

class SelectionMonitor {
public:
    SelectionMonitor();
    ~SelectionMonitor();

    SelectionMonitor(const SelectionMonitor&) = delete;
    SelectionMonitor& operator=(const SelectionMonitor&) = delete;

    /// Start monitoring text selections (blocking event loop)
    void start(SelectionCallback callback);

    /// Stop the event loop
    void stop();

    /// Check if running under Wayland (affects overlay availability)
    bool isWayland() const;

    /// Check if running under GNOME Wayland (overlay disabled, hotkey-only)
    bool isGnomeWayland() const;

    /// One-shot PRIMARY selection read (used by hotkey mode, §6)
    std::wstring getX11PrimarySelection();

private:
    bool        m_running = false;
    bool        m_isWayland = false;
    bool        m_isGnome = false;
    SelectionCallback m_callback;

#ifdef __linux__
    pid_t       m_waylandPid = -1;
#endif

    // X11 internals
    void runX11EventLoop();

    // Wayland internals
    void runWaylandEventLoop();
};

} // namespace platform
} // namespace orka
