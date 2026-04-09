#pragma once

/**
 * ORKA Text Injector — Windows
 *
 * §4 Text Injector priority matrix (Windows):
 *   1. SendInput(INPUT_KEYBOARD, KEYEVENTF_UNICODE)  — standard fields
 *   2. IUIAutomationValuePattern::SetValue()          — Rich text / Office
 *   3. OpenClipboard fallback                         — UAC / password fields
 *      (with clipboard owner save/restore per §Win11 fix)
 *
 * Method selection is automatic based on field type detection.
 */

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace orka {
namespace platform {

class TextInjector {
public:
    TextInjector();
    ~TextInjector();

    TextInjector(const TextInjector&) = delete;
    TextInjector& operator=(const TextInjector&) = delete;

    /// Inject converted text into the active field
    bool inject(const std::wstring& text);

    /// Always false on Windows
    bool isWayland() const { return false; }

private:
#ifdef _WIN32
    bool injectViaSendInput(const std::wstring& text);
    bool injectViaUIA(const std::wstring& text);
    bool injectViaClipboard(const std::wstring& text);
#endif
};

} // namespace platform
} // namespace orka
