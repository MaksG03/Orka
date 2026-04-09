#pragma once

/**
 * ORKA Selection Monitor — Windows (Consumer + Enterprise)
 *
 * §3.1 Consumer build:
 *   WH_MOUSE_LL + WH_KEYBOARD_LL in low-priority thread (timeout ≤ 5 ms)
 *   On mouse-up / Shift+Arrow → IUIAutomationTextPattern::GetSelection()
 *   Fallback: IAccessible (MSAA) polling every 80 ms
 *
 * §3.2 Enterprise build (#ifdef ENTERPRISE_BUILD):
 *   Zero global hooks. Detection via:
 *   IUIAutomation::AddAutomationEventHandler(UIA_Text_TextSelectionChangedEventId)
 *   Conversion trigger: RegisterHotKey only.
 *
 * Architecture: event-driven. Callback fires only when user finishes
 * selecting text (mouse-up or Shift+Arrow key release).
 */

#include <string>
#include <functional>

#ifdef _WIN32
#include <windows.h>
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

    /// Start monitoring text selections (blocking message loop)
    void start(SelectionCallback callback);

    /// Stop the message loop
    void stop();

    /// One-shot read of currently selected text (used by hotkey mode §6)
    std::wstring getSelectedText();

    /// Always false on Windows (Wayland is Linux-only)
    bool isWayland() const { return false; }
    bool isGnomeWayland() const { return false; }

private:
    bool              m_running = false;
    SelectionCallback m_callback;

#ifdef _WIN32
#ifndef ENTERPRISE_BUILD
    // Consumer: low-level hooks
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    HHOOK m_mouseHook    = nullptr;
    HHOOK m_keyboardHook = nullptr;
#endif

    // UI Automation — selection reading
    bool initUIAutomation();
    std::wstring getSelectionViaUIA();
    std::wstring getSelectionViaMSAA();  // IAccessible fallback

    // Static instance for hook callbacks
    static SelectionMonitor* s_instance;
#endif
};

} // namespace platform
} // namespace orka
