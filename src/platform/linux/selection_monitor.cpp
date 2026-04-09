/**
 * ORKA Selection Monitor — Linux implementation
 *
 * §3.3 X11:
 *   XFixesSelectSelectionInput(display, root, XA_PRIMARY,
 *       XFixesSetSelectionOwnerNotifyMask)
 *   On XFixesSelectionNotifyEvent → XConvertSelection(XA_PRIMARY, XA_UTF8_STRING)
 *
 * §3.4 Wayland:
 *   wlr-data-control-v1 or xdg-desktop-portal for GNOME
 *
 * Design: zero polling, pure event-driven.
 */

#include "selection_monitor.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "core/utf8_utils.h"

namespace orka {
namespace platform {

// ════════════════════════════════════════════════════════════════════
// Construction / destruction
// ════════════════════════════════════════════════════════════════════

SelectionMonitor::SelectionMonitor() {
    // Detect display server
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const char* xdgSession = std::getenv("XDG_SESSION_TYPE");
    const char* desktopEnv = std::getenv("XDG_CURRENT_DESKTOP");

    m_isWayland = (waylandDisplay != nullptr)
               || (xdgSession && std::strcmp(xdgSession, "wayland") == 0);

    m_isGnome = (desktopEnv != nullptr)
             && (std::strstr(desktopEnv, "GNOME") != nullptr
              || std::strstr(desktopEnv, "gnome") != nullptr);
}

SelectionMonitor::~SelectionMonitor() {
    stop();
}

bool SelectionMonitor::isWayland() const { return m_isWayland; }
bool SelectionMonitor::isGnomeWayland() const { return m_isWayland && m_isGnome; }

void SelectionMonitor::stop() {
    m_running = false;
    if (m_waylandPid > 0) {
        ::kill(m_waylandPid, SIGTERM);
        int status;
        ::waitpid(m_waylandPid, &status, WNOHANG);
        m_waylandPid = -1;
    }
}


// ════════════════════════════════════════════════════════════════════
// Main entry point
// ════════════════════════════════════════════════════════════════════

void SelectionMonitor::start(SelectionCallback callback) {
    m_callback = std::move(callback);
    m_running = true;

    if (m_isWayland && !m_isGnome) {
        runWaylandEventLoop();
    } else {
        // X11 or XWayland (GNOME Wayland falls back to hotkey mode)
        runX11EventLoop();
    }
}


// ════════════════════════════════════════════════════════════════════
// §3.3 X11 — XFixes PRIMARY selection monitoring
// ════════════════════════════════════════════════════════════════════

void SelectionMonitor::runX11EventLoop() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "[ORKA] Cannot open X11 display\n";
        return;
    }

    Window root = DefaultRootWindow(display);

    // Check XFixes extension
    int xfixesEventBase, xfixesErrorBase;
    if (!XFixesQueryExtension(display, &xfixesEventBase, &xfixesErrorBase)) {
        std::cerr << "[ORKA] XFixes extension not available\n";
        XCloseDisplay(display);
        return;
    }

    // Subscribe to PRIMARY selection changes
    Atom primaryAtom = XA_PRIMARY;
    XFixesSelectSelectionInput(display, root, primaryAtom,
        XFixesSetSelectionOwnerNotifyMask);

    Atom utf8String = XInternAtom(display, "UTF8_STRING", False);
    Atom orkaTarget = XInternAtom(display, "ORKA_SELECTION", False);

    std::cout << "[ORKA] X11 Selection Monitor started\n";

    while (m_running) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);

            // Check if it's the PRIMARY selection change
            if (event.type == xfixesEventBase + XFixesSelectionNotify) {
                XFixesSelectionNotifyEvent* s_event =
                    reinterpret_cast<XFixesSelectionNotifyEvent*>(&event);

                if (s_event->selection == primaryAtom) {
                    XConvertSelection(display, primaryAtom, utf8String, orkaTarget,
                        root, CurrentTime);
                }
            }
            else if (event.type == SelectionNotify) {
                if (event.xselection.selection == primaryAtom &&
                    event.xselection.property != None) {
                    // Selection data is ready
                    Atom actualType;
                    int actualFormat;
                    unsigned long nitems, bytesAfter;
                    unsigned char* data = nullptr;

                    XGetWindowProperty(display, root, orkaTarget,
                        0, 65536, False, utf8String,
                        &actualType, &actualFormat, &nitems, &bytesAfter, &data);

                    if (data && nitems > 0) {
                        std::string utf8Text(reinterpret_cast<char*>(data), nitems);
                        XFree(data);

                        // Convert UTF-8 → wstring
                        try {
                            std::wstring wide = orka::util::utf8ToWide(utf8Text);
                            if (m_callback && !wide.empty()) {
                                m_callback(wide);
                            }
                        } catch (...) {}
                    } else {
                        if (data) XFree(data);
                    }
                    XDeleteProperty(display, root, orkaTarget);
                }
            }
        }
        
        if (!m_running) break;
        
        // Efficient wait for X11 events instead of spinning CPU
        fd_set in_fds;
        FD_ZERO(&in_fds);
        int x11_fd = ConnectionNumber(display);
        FD_SET(x11_fd, &in_fds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout
        
        select(x11_fd + 1, &in_fds, nullptr, nullptr, &tv);
    }

    XCloseDisplay(display);
    std::cout << "[ORKA] X11 Selection Monitor stopped\n";
}


std::wstring SelectionMonitor::getX11PrimarySelection() {
    // One-shot selection read (used by hotkey mode)
    Display* display = XOpenDisplay(nullptr);
    if (!display) return L"";

    Window root = DefaultRootWindow(display);
    Atom utf8String = XInternAtom(display, "UTF8_STRING", False);
    Atom orkaTarget = XInternAtom(display, "ORKA_SEL_HOTKEY", False);

    XConvertSelection(display, XA_PRIMARY, utf8String, orkaTarget, root, CurrentTime);
    XFlush(display);

    // Wait for SelectionNotify event (with timeout)
    XEvent event;
    std::wstring result;
    
    // Simple polling with short timeout
    for (int i = 0; i < 50; ++i) { // up to 500ms
        if (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == SelectionNotify) {
                Atom actualType;
                int actualFormat;
                unsigned long nitems, bytesAfter;
                unsigned char* data = nullptr;

                XGetWindowProperty(display, root, orkaTarget,
                    0, 65536, False, utf8String,
                    &actualType, &actualFormat, &nitems, &bytesAfter, &data);

                if (data && nitems > 0) {
                    std::string utf8(reinterpret_cast<char*>(data), nitems);
                    XFree(data);
                    try { result = orka::util::utf8ToWide(utf8); } catch (...) {}
                } else {
                    if (data) XFree(data);
                }
                XDeleteProperty(display, root, orkaTarget);
                break;
            }
        }
        usleep(10000); // 10ms
    }

    XCloseDisplay(display);
    return result;
}


// ════════════════════════════════════════════════════════════════════
// §3.4 Wayland — wlr-data-control (non-GNOME compositors)
// ════════════════════════════════════════════════════════════════════

void SelectionMonitor::runWaylandEventLoop() {
    std::cout << "[ORKA] Wayland Selection Monitor started (wl-paste --watch)\n";

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        std::cerr << "[ORKA] Cannot create pipe for Wayland watcher\n";
        return;
    }

    m_waylandPid = fork();
    if (m_waylandPid == -1) {
        std::cerr << "[ORKA] Cannot fork Wayland watcher\n";
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (m_waylandPid == 0) {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(pipefd[1]);
        execlp("wl-paste", "wl-paste", "--watch", "cat", "--primary", nullptr);
        _exit(1);
    }

    // Parent
    close(pipefd[1]);
    FILE* proc = fdopen(pipefd[0], "r");
    if (!proc) {
        close(pipefd[0]);
        return;
    }

    char buf[65536];
    while (m_running && fgets(buf, sizeof(buf), proc)) {
        std::string utf8(buf);
        // Trim trailing newline
        while (!utf8.empty() && (utf8.back() == '\n' || utf8.back() == '\r'))
            utf8.pop_back();

        if (!utf8.empty()) {
            try {
                std::wstring wide = orka::util::utf8ToWide(utf8);
                if (m_callback) m_callback(wide);
            } catch (...) {}
        }
    }

    fclose(proc);
    
    // Ensure child exits and is reaped
    if (m_waylandPid > 0) {
        ::kill(m_waylandPid, SIGTERM);
        int status;
        ::waitpid(m_waylandPid, &status, WNOHANG);
        m_waylandPid = -1;
    }

    std::cout << "[ORKA] Wayland Selection Monitor stopped\n";
}

} // namespace platform
} // namespace orka
