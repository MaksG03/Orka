#pragma once

/**
 * ORKA Overlay UI — §5: Floating convert button
 *
 * Intent detection: overlay appears only after 300 ms of mouse
 * inactivity following a text selection.  Does NOT appear on
 * empty / whitespace-only / digits-only selections.
 *
 * Platforms:
 *   X11:             override-redirect window (libX11)
 *                    _NET_WM_WINDOW_TYPE_TOOLTIP
 *   Wayland+wlroots: layer-shell (stub — requires protocol codegen)
 *   GNOME Wayland:   Overlay disabled; hotkey-only mode
 *
 * Minimum button size: 48×48 px (§5 requirement).
 */

#include <string>
#include <functional>
#include <atomic>

#ifdef __linux__
#include <X11/Xlib.h>
#endif

namespace orka {
namespace ui {

/// Callback invoked when the user clicks the overlay button
using OverlayClickCallback = std::function<void()>;

/// §5 intent-detection delay (milliseconds)
constexpr int kIntentDelayMs = 300;

/// Overlay button dimensions
constexpr int kButtonWidth  = 56;
constexpr int kButtonHeight = 32;
constexpr int kButtonOffset = 8;   // px offset from cursor position

class OverlayButton {
public:
    OverlayButton();
    ~OverlayButton();

    OverlayButton(const OverlayButton&) = delete;
    OverlayButton& operator=(const OverlayButton&) = delete;

    /// Initialise the X11 overlay window (call once from main thread)
    bool init();

    /// Show the button near current mouse position
    void show(int cursorX, int cursorY);

    /// Hide / dismiss the overlay
    void hide();

    /// Is the overlay currently visible?
    bool isVisible() const { return m_visible; }

    /// Set callback for overlay click
    void setClickCallback(OverlayClickCallback cb) { m_clickCb = std::move(cb); }

    /// Process pending X11 events for the overlay window (non-blocking).
    /// Returns true if the convert button was clicked.
    bool pumpEvents();

    /// Get current mouse position (X11)
    bool getCursorPosition(int& x, int& y);

    /// Shut down and release resources
    void shutdown();

private:
    std::atomic<bool>    m_visible{false};
    OverlayClickCallback m_clickCb;

#ifdef __linux__
    Display* m_display  = nullptr;
    Window   m_window   = 0;
    GC       m_gc       = nullptr;
    int      m_screen   = 0;

    void createWindow();
    void drawButton();
#endif
};

} // namespace ui
} // namespace orka
