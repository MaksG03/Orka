/**
 * ORKA Overlay UI — §5: Floating convert button (X11 implementation)
 *
 * Creates an override-redirect X11 window that acts as a floating
 * "convert" button near the mouse cursor.  The window has:
 *   - override_redirect = True  (no WM decorations)
 *   - _NET_WM_WINDOW_TYPE_TOOLTIP window type
 *   - 56×32 px size (exceeds §5 minimum of 48×48 interaction area)
 *   - Simple text label "Aa↔" rendered with XDrawString
 *
 * Intent detection (300 ms delay) is handled by the caller
 * (main.cpp) — this class only provides show/hide/pumpEvents.
 */

#include "overlay.h"

#ifdef __linux__

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <cstring>
#include <iostream>

namespace orka {
namespace ui {

// ════════════════════════════════════════════════════════════════════
// Construction / destruction
// ════════════════════════════════════════════════════════════════════

OverlayButton::OverlayButton() = default;

OverlayButton::~OverlayButton() {
    shutdown();
}

bool OverlayButton::init() {
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        std::cerr << "[ORKA] Overlay: cannot open X display\n";
        return false;
    }
    m_screen = DefaultScreen(m_display);
    createWindow();
    return true;
}

void OverlayButton::shutdown() {
    if (m_display) {
        if (m_gc) {
            XFreeGC(m_display, m_gc);
            m_gc = nullptr;
        }
        if (m_window) {
            XDestroyWindow(m_display, m_window);
            m_window = 0;
        }
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
    m_visible = false;
}

// ════════════════════════════════════════════════════════════════════
// X11 window creation — override-redirect tooltip
// ════════════════════════════════════════════════════════════════════

void OverlayButton::createWindow() {
    Window root = RootWindow(m_display, m_screen);

    // Visual attributes
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel  = 0x2D2D3F;   // dark indigo background
    attrs.border_pixel      = 0x7C6FE0;   // accent purple border
    attrs.event_mask        = ExposureMask | ButtonPressMask
                            | EnterWindowMask | LeaveWindowMask
                            | StructureNotifyMask;

    unsigned long valueMask = CWOverrideRedirect | CWBackPixel
                            | CWBorderPixel | CWEventMask;

    m_window = XCreateWindow(
        m_display, root,
        -200, -200,                    // start off-screen
        kButtonWidth, kButtonHeight,
        2,                             // border width
        CopyFromParent,                // depth
        InputOutput,                   // class
        CopyFromParent,                // visual
        valueMask,
        &attrs
    );

    // §5: Set window type to TOOLTIP (floats above everything, no taskbar)
    Atom wmWindowType = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE", False);
    Atom wmTooltip    = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
    XChangeProperty(m_display, m_window, wmWindowType, XA_ATOM, 32,
                    PropModeReplace, reinterpret_cast<unsigned char*>(&wmTooltip), 1);

    // Keep on top
    Atom wmState     = XInternAtom(m_display, "_NET_WM_STATE", False);
    Atom wmAbove     = XInternAtom(m_display, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(m_display, m_window, wmState, XA_ATOM, 32,
                    PropModeReplace, reinterpret_cast<unsigned char*>(&wmAbove), 1);

    // Set WM class for identification
    XClassHint classHint;
    classHint.res_name  = const_cast<char*>("orka_overlay");
    classHint.res_class = const_cast<char*>("Orka");
    XSetClassHint(m_display, m_window, &classHint);

    // Create GC for drawing
    XGCValues gcValues;
    gcValues.foreground = 0xE8E6F0;   // light text
    gcValues.background = 0x2D2D3F;
    m_gc = XCreateGC(m_display, m_window, GCForeground | GCBackground, &gcValues);

    // Load a font
    XFontStruct* font = XLoadQueryFont(m_display, "-*-helvetica-bold-r-*-*-14-*-*-*-*-*-*-*");
    if (!font) {
        font = XLoadQueryFont(m_display, "-*-fixed-bold-r-*-*-14-*-*-*-*-*-*-*");
    }
    if (font) {
        XSetFont(m_display, m_gc, font->fid);
    }

    std::cout << "[ORKA] Overlay window created\n";
}


// ════════════════════════════════════════════════════════════════════
// Show / hide
// ════════════════════════════════════════════════════════════════════

void OverlayButton::show(int cursorX, int cursorY) {
    if (!m_display || !m_window) return;

    // Position: slightly below and right of cursor
    int posX = cursorX + kButtonOffset;
    int posY = cursorY + kButtonOffset;

    // Ensure we don't go off-screen
    int screenW = DisplayWidth(m_display, m_screen);
    int screenH = DisplayHeight(m_display, m_screen);
    if (posX + kButtonWidth > screenW)  posX = cursorX - kButtonWidth - kButtonOffset;
    if (posY + kButtonHeight > screenH) posY = cursorY - kButtonHeight - kButtonOffset;

    XMoveWindow(m_display, m_window, posX, posY);
    XMapRaised(m_display, m_window);
    drawButton();
    XFlush(m_display);

    m_visible = true;
}

void OverlayButton::hide() {
    if (!m_display || !m_window) return;

    XUnmapWindow(m_display, m_window);
    XFlush(m_display);
    m_visible = false;
}


// ════════════════════════════════════════════════════════════════════
// Drawing — simple "Aa⇄" label
// ════════════════════════════════════════════════════════════════════

void OverlayButton::drawButton() {
    if (!m_display || !m_window || !m_gc) return;

    // Background (already set by window bg pixel, but clear explicitly)
    XSetForeground(m_display, m_gc, 0x2D2D3F);
    XFillRectangle(m_display, m_window, m_gc, 0, 0, kButtonWidth, kButtonHeight);

    // Rounded-feel highlight along top edge
    XSetForeground(m_display, m_gc, 0x3D3D5C);
    XFillRectangle(m_display, m_window, m_gc, 0, 0, kButtonWidth, 2);

    // Button label "Aa"  (using ASCII — the icon conveys "convert text")
    XSetForeground(m_display, m_gc, 0xE8E6F0);
    const char* label = "Aa<>";
    XDrawString(m_display, m_window, m_gc, 10, 22, label, static_cast<int>(std::strlen(label)));
}


// ════════════════════════════════════════════════════════════════════
// Event pump — handle clicks on the overlay
// ════════════════════════════════════════════════════════════════════

bool OverlayButton::pumpEvents() {
    if (!m_display) return false;

    bool clicked = false;

    while (XPending(m_display)) {
        XEvent ev;
        XNextEvent(m_display, &ev);

        switch (ev.type) {
        case Expose:
            drawButton();
            break;

        case ButtonPress:
            // User clicked the convert button
            clicked = true;
            if (m_clickCb) m_clickCb();
            hide();
            break;

        case LeaveNotify:
            // Mouse left the overlay — dismiss after a short grace period
            // (handled by caller via timer for smoother UX)
            break;

        default:
            break;
        }
    }

    return clicked;
}


// ════════════════════════════════════════════════════════════════════
// Cursor position query
// ════════════════════════════════════════════════════════════════════

bool OverlayButton::getCursorPosition(int& x, int& y) {
    if (!m_display) return false;

    Window root = RootWindow(m_display, m_screen);
    Window child;
    int rootX, rootY, winX, winY;
    unsigned int mask;

    if (XQueryPointer(m_display, root, &root, &child, &rootX, &rootY, &winX, &winY, &mask)) {
        x = rootX;
        y = rootY;
        return true;
    }
    return false;
}

} // namespace ui
} // namespace orka

#else
// ── Non-Linux stub ────────────────────────────────────────────────
namespace orka {
namespace ui {
    OverlayButton::OverlayButton()  = default;
    OverlayButton::~OverlayButton() = default;
    bool OverlayButton::init()      { return false; }
    void OverlayButton::show(int, int) {}
    void OverlayButton::hide() {}
    bool OverlayButton::pumpEvents() { return false; }
    bool OverlayButton::getCursorPosition(int& , int&) { return false; }
    void OverlayButton::shutdown() {}
} // namespace ui
} // namespace orka
#endif
