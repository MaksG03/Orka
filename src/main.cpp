/**
 * ORKA — main entry point
 *
 * Initialises all components per §2 architecture:
 *   Selection Monitor → Language Engine → Text Injector
 *
 * Also registers Hotkey Mode (Ctrl+Shift+O) per §6, and
 * the Overlay UI (§5) with 300 ms intent detection.
 */

#include "core/language_engine.h"
#include "core/hangul_engine.h"
#include "core/hebrew_engine.h"

#ifdef __linux__
#include "platform/linux/selection_monitor.h"
#include "platform/linux/text_injector.h"
#include "ui/overlay.h"
#endif

#ifdef _WIN32
#include "platform/win/selection_monitor.h"
#include "platform/win/text_injector.h"
#endif

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <thread>
#include <locale>
#include <mutex>
#include <clocale>
#include <chrono>

#include "core/utf8_utils.h"

#ifdef __linux__
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

namespace {
    std::atomic<bool> g_running{true};

    // §5 overlay state — shared between selection monitor callback and overlay thread
    std::mutex              g_overlayMutex;
    std::atomic<bool>       g_selectionPending{false};
    std::wstring            g_pendingText;
    std::chrono::steady_clock::time_point g_selectionTime;
    int                     g_cursorX = 0;
    int                     g_cursorY = 0;

#ifdef __linux__
    orka::platform::SelectionMonitor* g_linuxMonitor = nullptr;
#endif

#ifdef _WIN32
    DWORD g_mainThreadId = 0;
#endif
}

void signalHandler(int) {
    g_running = false;
#ifdef __linux__
    if (g_linuxMonitor) g_linuxMonitor->stop();
#endif
#ifdef _WIN32
    if (g_mainThreadId != 0) {
        PostThreadMessage(g_mainThreadId, WM_QUIT, 0, 0);
    }
#endif
}

// ════════════════════════════════════════════════════════════════════
// UTF-8 conversion helper
// ════════════════════════════════════════════════════════════════════

namespace {
std::string wideToUtf8(const std::wstring& w) {
    try { return orka::util::wideToUtf8(w); }
    catch (...) { return "[encoding error]"; }
}
} // anonymous namespace


// ════════════════════════════════════════════════════════════════════
// §6  Hotkey Mode — Ctrl+Shift+O (Linux X11)
// ════════════════════════════════════════════════════════════════════

#ifdef __linux__
void hotkeyThread(orka::LanguageEngine& engine,
                  orka::platform::TextInjector& injector,
                  orka::LanguagePair activePair) {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "[ORKA] Hotkey: cannot open X display\n";
        return;
    }

    Window root = DefaultRootWindow(display);

    // §6: XGrabKey(display, keycode, ControlMask|ShiftMask, root, True, GrabModeAsync, GrabModeAsync)
    KeyCode keycode = XKeysymToKeycode(display, XStringToKeysym("o"));
    unsigned int modifiers = ControlMask | ShiftMask;

    XGrabKey(display, keycode, modifiers, root, True, GrabModeAsync, GrabModeAsync);
    // Also grab with NumLock / CapsLock variants
    XGrabKey(display, keycode, modifiers | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, modifiers | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, modifiers | Mod2Mask | LockMask, root, True, GrabModeAsync, GrabModeAsync);

    XSelectInput(display, root, KeyPressMask);
    std::cout << "[ORKA] Hotkey Ctrl+Shift+O registered\n";

    while (g_running) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);

            if (event.type == KeyPress) {
                std::cout << "[ORKA] Hotkey triggered!\n";

                // Get current PRIMARY selection
                orka::platform::SelectionMonitor mon;
                std::wstring selected = mon.getX11PrimarySelection();

                if (!selected.empty()) {
                    auto result = engine.convert(selected, activePair);
                    if (result.success) {
                        if (result.nikudStripped) {
                            std::cout << "[ORKA] Warning: Nikud diacritics were stripped (lossy)\n";
                        }
                        if (result.escapeRequired) {
                            std::cout << "[ORKA] Warning: IME unfinished syllable cancelled\n";
                        }
                        injector.inject(result.text);
                        std::cout << "[ORKA] Conversion complete\n";
                    } else {
                        std::cerr << "[ORKA] Conversion failed\n";
                    }
                } else {
                    std::cout << "[ORKA] No text selected\n";
                }
            }
        }
        usleep(50000); // 50ms polling for hotkey events
    }

    XUngrabKey(display, keycode, modifiers, root);
    XCloseDisplay(display);
    std::cout << "[ORKA] Hotkey thread stopped\n";
}


// ════════════════════════════════════════════════════════════════════
// §5  Overlay Thread — intent detection + overlay button
// ════════════════════════════════════════════════════════════════════

void overlayThread(orka::LanguageEngine& engine,
                   orka::platform::TextInjector& injector,
                   orka::LanguagePair activePair) {
    orka::ui::OverlayButton overlay;
    if (!overlay.init()) {
        std::cerr << "[ORKA] Overlay init failed — running hotkey-only\n";
        return;
    }

    // Set the click callback: when user clicks overlay → convert + inject
    overlay.setClickCallback([&]() {
        std::lock_guard<std::mutex> lock(g_overlayMutex);
        if (!g_pendingText.empty()) {
            auto result = engine.convert(g_pendingText, activePair);
            if (result.success && !result.text.empty()) {
                if (result.nikudStripped) {
                    std::cout << "[ORKA] ⚠ Огласовки (Nikud) будуть втрачені\n";
                }
                if (result.escapeRequired) {
                    std::cout << "[ORKA] ⚠ Незавершений склад скасовано\n";
                }
                injector.inject(result.text);
                std::cout << "[ORKA] Overlay conversion: " << wideToUtf8(result.text) << "\n";
            }
            g_pendingText.clear();
            g_selectionPending = false;
        }
    });

    std::cout << "[ORKA] Overlay thread started\n";

    while (g_running) {
        // Check if a selection is pending and 300ms has elapsed (§5 intent detection)
        if (g_selectionPending) {
            bool shouldShow = false;
            {
                std::lock_guard<std::mutex> lock(g_overlayMutex);
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - g_selectionTime).count();
                if (elapsed >= orka::ui::kIntentDelayMs && !overlay.isVisible()) {
                    shouldShow = true;
                }
            }

            if (shouldShow) {
                // Intent confirmed — show the overlay near the cursor
                int cx = g_cursorX, cy = g_cursorY;
                overlay.getCursorPosition(cx, cy);  // get fresh position
                overlay.show(cx, cy);
                std::cout << "[ORKA] Overlay shown at (" << cx << ", " << cy << ")\n";
            }
        }

        // Process overlay window events (clicks, expose, etc.)
        overlay.pumpEvents();

        // Auto-hide overlay after 5 seconds of no interaction
        if (overlay.isVisible()) {
            bool shouldHide = false;
            {
                std::lock_guard<std::mutex> lock(g_overlayMutex);
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - g_selectionTime).count();
                if (elapsed > 5000) {
                    shouldHide = true;
                }
            }
            if (shouldHide) {
                overlay.hide();
                g_selectionPending = false;
                std::cout << "[ORKA] Overlay auto-hidden (timeout)\n";
            }
        }

        usleep(20000); // 20ms — smooth overlay responsiveness
    }

    overlay.shutdown();
    std::cout << "[ORKA] Overlay thread stopped\n";
}
#endif


// ════════════════════════════════════════════════════════════════════
// Main
// ════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    std::setlocale(LC_ALL, "");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║  ORKA — Intelligent Keyboard Converter   ║\n";
    std::cout << "║  v1.0  ·  Perepelytsia Orka Technologies ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n\n";

    // Parse command-line arguments
    orka::LanguagePair activePair = orka::LanguagePair::EN_UK;
    bool hotkeyOnly = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--lang=ko" || arg == "--lang=KO") activePair = orka::LanguagePair::EN_KO;
        if (arg == "--lang=he" || arg == "--lang=HE") activePair = orka::LanguagePair::EN_HE;
        if (arg == "--lang=uk" || arg == "--lang=UK") activePair = orka::LanguagePair::EN_UK;
        if (arg == "--hotkey-only") hotkeyOnly = true;
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: orka [options]\n"
                      << "  --lang=uk|ko|he   Set language pair (default: uk)\n"
                      << "  --hotkey-only      Disable overlay, use Ctrl+Shift+O only\n"
                      << "  --help             Show this help\n";
            return 0;
        }
    }

    switch (activePair) {
        case orka::LanguagePair::EN_UK:
            std::cout << "[ORKA] Language pair: EN ↔ UK (Ukrainian)\n"; break;
        case orka::LanguagePair::EN_KO:
            std::cout << "[ORKA] Language pair: EN ↔ KO (Korean)\n"; break;
        case orka::LanguagePair::EN_HE:
            std::cout << "[ORKA] Language pair: EN ↔ HE (Hebrew)\n"; break;
    }

    orka::LanguageEngine engine;

#ifdef __linux__
    orka::platform::SelectionMonitor monitor;
    g_linuxMonitor = &monitor;
    
    orka::platform::TextInjector injector;

    // §3.4: Warn if GNOME Wayland (overlay unavailable)
    if (monitor.isGnomeWayland()) {
        std::cout << "\n[ORKA] ⚠ На вашому робочому столі (GNOME Wayland) плаваюча кнопка\n"
                  << "       недоступна. Orka працює через Ctrl+Shift+O.\n"
                  << "       Це можна змінити у Налаштуваннях.\n\n";
        hotkeyOnly = true;
    }

    // Start hotkey thread (§6 — always active)
    std::thread hotkeyTh(hotkeyThread, std::ref(engine), std::ref(injector), activePair);

    // Start overlay thread (§5 — only in overlay mode)
    std::thread overlayTh;
    if (!hotkeyOnly) {
        overlayTh = std::thread(overlayThread, std::ref(engine), std::ref(injector), activePair);
    }

    if (!hotkeyOnly) {
        // Start selection monitor (overlay mode)
        std::cout << "[ORKA] Selection Monitor active (overlay mode)\n";
        std::cout << "[ORKA] Press Ctrl+C to exit\n\n";

        monitor.start([&](const std::wstring& selectedText) {
            // §5 Intent detection: record the selection and timestamp.
            // The overlay thread checks if 300ms have elapsed before showing.
            std::lock_guard<std::mutex> lock(g_overlayMutex);
            g_pendingText = selectedText;
            g_selectionTime = std::chrono::steady_clock::now();
            g_selectionPending = true;

            std::cout << "[ORKA] Selection detected: " << wideToUtf8(selectedText) << "\n";
        });
    } else {
        std::cout << "[ORKA] Hotkey-only mode active (Ctrl+Shift+O)\n";
        std::cout << "[ORKA] Press Ctrl+C to exit\n\n";
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    g_running = false;
    if (hotkeyTh.joinable()) hotkeyTh.join();
    if (overlayTh.joinable()) overlayTh.join();
#endif

#ifdef _WIN32
    g_mainThreadId = GetCurrentThreadId();
    
    orka::platform::SelectionMonitor monitor;
    orka::platform::TextInjector injector;

    // §6: Register global hotkey Ctrl+Shift+O
    const int HOTKEY_ID = 1;
    if (!RegisterHotKey(nullptr, HOTKEY_ID,
                        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'O')) {
        std::cerr << "[ORKA] Failed to register hotkey Ctrl+Shift+O\n";
    } else {
        std::cout << "[ORKA] Hotkey Ctrl+Shift+O registered\n";
    }

#ifdef ENTERPRISE_BUILD
    std::cout << "[ORKA] Enterprise mode — hotkey-only (zero hooks)\n";
#else
    std::cout << "[ORKA] Consumer mode — hooks + UIA\n";
#endif

    // Start selection monitor in a separate thread (it runs its own message loop)
    std::thread monitorThread;
    if (!hotkeyOnly) {
        monitorThread = std::thread([&]() {
            monitor.start([&](const std::wstring& selectedText) {
                std::lock_guard<std::mutex> lock(g_overlayMutex);
                g_pendingText = selectedText;
                g_selectionTime = std::chrono::steady_clock::now();
                g_selectionPending = true;
                std::cout << "[ORKA] Selection detected: "
                          << wideToUtf8(selectedText) << "\n";
            });
        });
    }

    // Windows message loop — processes hotkey messages
    std::cout << "[ORKA] Press Ctrl+Shift+O to convert selected text\n";
    std::cout << "[ORKA] Close window or Ctrl+C to exit\n\n";

    MSG msg;
    while (g_running && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_ID) {
            std::cout << "[ORKA] Hotkey triggered!\n";

            std::wstring selected = monitor.getSelectedText();
            if (!selected.empty()) {
                auto result = engine.convert(selected, activePair);
                if (result.success) {
                    if (result.nikudStripped) {
                        std::cout << "[ORKA] Warning: Nikud diacritics were stripped (lossy)\n";
                    }
                    if (result.escapeRequired) {
                        std::cout << "[ORKA] Warning: IME unfinished syllable cancelled\n";
                    }
                    injector.inject(result.text);
                    std::cout << "[ORKA] Conversion complete\n";
                } else {
                    std::cerr << "[ORKA] Conversion failed\n";
                }
            } else {
                std::cout << "[ORKA] No text selected\n";
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(nullptr, HOTKEY_ID);
    g_running = false;
    monitor.stop();
    if (monitorThread.joinable()) monitorThread.join();
#endif

    std::cout << "\n[ORKA] Shutdown complete.\n";
    return 0;
}
