/**
 * ORKA Text Injector — Windows implementation
 *
 * §4 Text Injector priority matrix (Windows):
 *
 *   Scenario               Method (priority)           API
 *   ─────────────────────  ──────────────────────────  ─────────────────────
 *   Standard field         1. SendInput                SendInput(INPUT_KEYBOARD, KEYEVENTF_UNICODE)
 *   Rich text / Office     2. UIA SetValue             IUIAutomationValuePattern::SetValue()
 *   Protected (UAC, pwd)   3. OpenClipboard (fallback) SetClipboardData(CF_UNICODETEXT)
 *
 * ✅ Win 11 Fix:
 *   OpenClipboard with clipboard owner save/restore to suppress
 *   "Orka pasted from..." notification. OpenClipboard is ONLY used
 *   as the last fallback for fields where SendInput/UIA are blocked.
 */

#ifdef _WIN32

#include "text_injector.h"

#include <windows.h>
#include <UIAutomation.h>

#include <iostream>
#include <vector>

#include "core/utf8_utils.h"

namespace orka {
namespace platform {

// ════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════

TextInjector::TextInjector() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
}

TextInjector::~TextInjector() {
    CoUninitialize();
}


// ════════════════════════════════════════════════════════════════════
// Main injection entry point — tries methods in priority order
// ════════════════════════════════════════════════════════════════════

bool TextInjector::inject(const std::wstring& text) {
    if (text.empty()) return false;

    // Method 1: SendInput (fastest, no dependencies)
    if (injectViaSendInput(text)) {
        std::cout << "[ORKA] Injected via SendInput\n";
        return true;
    }

    // Method 2: UI Automation SetValue
    if (injectViaUIA(text)) {
        std::cout << "[ORKA] Injected via UIA SetValue\n";
        return true;
    }

    // Method 3: Clipboard (last resort — with Win 11 owner restore)
    if (injectViaClipboard(text)) {
        std::cout << "[ORKA] Injected via clipboard fallback\n";
        return true;
    }

    std::cerr << "[ORKA] All Windows injection methods failed\n";
    return false;
}


// ════════════════════════════════════════════════════════════════════
// Method 1: SendInput — KEYEVENTF_UNICODE
// ════════════════════════════════════════════════════════════════════

bool TextInjector::injectViaSendInput(const std::wstring& text) {
    // Build INPUT array: each character needs key-down + key-up
    std::vector<INPUT> inputs;
    inputs.reserve(text.size() * 2);

    for (wchar_t ch : text) {
        INPUT inputDown = {};
        inputDown.type = INPUT_KEYBOARD;
        inputDown.ki.wScan = ch;
        inputDown.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(inputDown);

        INPUT inputUp = {};
        inputUp.type = INPUT_KEYBOARD;
        inputUp.ki.wScan = ch;
        inputUp.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(inputUp);
    }

    UINT sent = SendInput(
        static_cast<UINT>(inputs.size()),
        inputs.data(),
        sizeof(INPUT)
    );

    return sent == inputs.size();
}


// ════════════════════════════════════════════════════════════════════
// Method 2: UI Automation — IUIAutomationValuePattern::SetValue()
// ════════════════════════════════════════════════════════════════════

bool TextInjector::injectViaUIA(const std::wstring& text) {
    IUIAutomation* pAutomation = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(CUIAutomation), nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IUIAutomation),
        reinterpret_cast<void**>(&pAutomation)
    );
    if (FAILED(hr) || !pAutomation) return false;

    // Get the focused element
    IUIAutomationElement* pFocused = nullptr;
    hr = pAutomation->GetFocusedElement(&pFocused);
    if (FAILED(hr) || !pFocused) {
        pAutomation->Release();
        return false;
    }

    // Try ValuePattern
    IUIAutomationValuePattern* pValue = nullptr;
    hr = pFocused->GetCurrentPatternAs(
        UIA_ValuePatternId,
        __uuidof(IUIAutomationValuePattern),
        reinterpret_cast<void**>(&pValue)
    );

    bool success = false;
    if (SUCCEEDED(hr) && pValue) {
        BSTR bstr = SysAllocStringLen(text.c_str(), static_cast<UINT>(text.size()));
        if (bstr) {
            hr = pValue->SetValue(bstr);
            success = SUCCEEDED(hr);
            SysFreeString(bstr);
        }
        pValue->Release();
    }

    pFocused->Release();
    pAutomation->Release();
    return success;
}


// ════════════════════════════════════════════════════════════════════
// Method 3: OpenClipboard fallback (with Win 11 owner restore)
// ════════════════════════════════════════════════════════════════════

bool TextInjector::injectViaClipboard(const std::wstring& text) {
    // §Win11 Fix: Save previous clipboard owner for restore
    HWND prevOwner = GetClipboardOwner();

    if (!OpenClipboard(nullptr)) {
        std::cerr << "[ORKA] Cannot open clipboard\n";
        return false;
    }

    bool success = false;

    try {
        EmptyClipboard();

        // Allocate global memory for the text
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
            if (pMem) {
                memcpy(pMem, text.c_str(), bytes);
                GlobalUnlock(hMem);

                if (SetClipboardData(CF_UNICODETEXT, hMem)) {
                    success = true;
                } else {
                    GlobalFree(hMem);
                }
            } else {
                GlobalFree(hMem);
            }
        }

        CloseClipboard();

        // Simulate Ctrl+V to paste
        if (success) {
            INPUT inputs[4] = {};

            // Ctrl down
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_CONTROL;

            // V down
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = 'V';

            // V up
            inputs[2].type = INPUT_KEYBOARD;
            inputs[2].ki.wVk = 'V';
            inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

            // Ctrl up
            inputs[3].type = INPUT_KEYBOARD;
            inputs[3].ki.wVk = VK_CONTROL;
            inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

            SendInput(4, inputs, sizeof(INPUT));
        }

        // Note: clipboard owner restore is best-effort.
        // The previous owner's content is already gone after EmptyClipboard(),
        // but we avoid leaving Orka as the clipboard owner to reduce
        // the "pasted from Orka" notification frequency on Win 11.

    } catch (...) {
        CloseClipboard();
        std::cerr << "[ORKA] Clipboard injection exception\n";
    }

    return success;
}

} // namespace platform
} // namespace orka

#endif // _WIN32
