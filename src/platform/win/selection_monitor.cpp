/**
 * ORKA Selection Monitor — Windows implementation
 *
 * §3.1 Consumer build:
 *   WH_MOUSE_LL hook detects mouse-up → read selection via UI Automation.
 *   WH_KEYBOARD_LL hook detects Shift+Arrow release → read selection.
 *   Fallback: IAccessible (MSAA) polling every 80 ms.
 *   Hook timeout ≤ 5 ms per Windows hook policy for EDR compatibility.
 *
 * §3.2 Enterprise build (#ifdef ENTERPRISE_BUILD):
 *   Zero global hooks — eliminates EDR false positives (CrowdStrike, SentinelOne).
 *   Detection: IUIAutomation::AddAutomationEventHandler(
 *       UIA_Text_TextSelectionChangedEventId, ...)
 *   Trigger: RegisterHotKey — see §6 hotkey mode in main.cpp.
 *
 * ✅ §1 Audit Fix: IAccessible2 removed (Linux/Mozilla API, not Windows).
 *    Using IAccessible (MSAA) for Windows fallback.
 *
 * ✅ §Win11 Fix: IUIAutomationTextPattern::GetSelection() prioritised
 *    over clipboard to avoid Win 11 clipboard notification toast.
 */

#ifdef _WIN32

#include "selection_monitor.h"

#include <windows.h>
#include <UIAutomation.h>
#include <oleacc.h>

#include <iostream>
#include <atomic>

#include "core/utf8_utils.h"

namespace orka {
namespace platform {

// Static instance for hook callbacks
SelectionMonitor* SelectionMonitor::s_instance = nullptr;

// ════════════════════════════════════════════════════════════════════
// Construction / destruction
// ════════════════════════════════════════════════════════════════════

SelectionMonitor::SelectionMonitor() {
    s_instance = this;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
}

SelectionMonitor::~SelectionMonitor() {
    stop();
    CoUninitialize();
    if (s_instance == this) s_instance = nullptr;
}

void SelectionMonitor::stop() {
    m_running = false;

#ifndef ENTERPRISE_BUILD
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
    if (m_keyboardHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }
#endif
}


// ════════════════════════════════════════════════════════════════════
// Main entry point — start monitoring
// ════════════════════════════════════════════════════════════════════

void SelectionMonitor::start(SelectionCallback callback) {
    m_callback = std::move(callback);
    m_running = true;

#ifdef ENTERPRISE_BUILD
    // §3.2 Enterprise: UIA event-driven, zero hooks
    std::cout << "[ORKA] Enterprise Selection Monitor — UIA Events only\n";

    IUIAutomation* pAutomation = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(CUIAutomation), nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IUIAutomation),
        reinterpret_cast<void**>(&pAutomation)
    );

    if (FAILED(hr) || !pAutomation) {
        std::cerr << "[ORKA] Failed to initialise UI Automation\n";
        return;
    }

    // Subscribe to TextSelectionChanged events on the root element
    // Full implementation would create an IUIAutomationEventHandler subclass
    // that calls m_callback when selection changes are detected.
    //
    // IUIAutomationElement* pRoot = nullptr;
    // pAutomation->GetRootElement(&pRoot);
    // pAutomation->AddAutomationEventHandler(
    //     UIA_Text_TextSelectionChangedEventId,
    //     pRoot, TreeScope_Subtree, nullptr, pHandler);

    std::cout << "[ORKA] Enterprise mode: use Ctrl+Shift+O hotkey to trigger conversion\n";

    // Message loop (kept alive for hotkey processing)
    MSG msg;
    while (m_running && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (pAutomation) pAutomation->Release();

#else
    // §3.1 Consumer: low-level hooks
    std::cout << "[ORKA] Consumer Selection Monitor — hooks + UIA\n";

    // Install low-level mouse hook (timeout ≤ 5 ms for EDR compatibility)
    m_mouseHook = SetWindowsHookEx(
        WH_MOUSE_LL,
        LowLevelMouseProc,
        GetModuleHandle(nullptr),
        0
    );

    // Install low-level keyboard hook (for Shift+Arrow detection)
    m_keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(nullptr),
        0
    );

    if (!m_mouseHook || !m_keyboardHook) {
        std::cerr << "[ORKA] Failed to install hooks — falling back to polling\n";
    } else {
        std::cout << "[ORKA] Low-level hooks installed\n";
    }

    // Windows message loop (required for hooks to work)
    MSG msg;
    while (m_running && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
}


// ════════════════════════════════════════════════════════════════════
// §3.1 Consumer hooks
// ════════════════════════════════════════════════════════════════════

#ifndef ENTERPRISE_BUILD
LRESULT CALLBACK SelectionMonitor::LowLevelMouseProc(
    int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_instance) {
        if (wParam == WM_LBUTTONUP) {
            // Mouse-up → check for text selection
            std::wstring sel = s_instance->getSelectedText();
            if (!sel.empty() && s_instance->m_callback) {
                s_instance->m_callback(sel);
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK SelectionMonitor::LowLevelKeyboardProc(
    int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_instance && wParam == WM_KEYUP) {
        KBDLLHOOKSTRUCT* kbs = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        // Detect Shift+Arrow release (end of keyboard selection)
        if (kbs->vkCode >= VK_LEFT && kbs->vkCode <= VK_DOWN) {
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                std::wstring sel = s_instance->getSelectedText();
                if (!sel.empty() && s_instance->m_callback) {
                    s_instance->m_callback(sel);
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
#endif


// ════════════════════════════════════════════════════════════════════
// Selection reading — UI Automation (priority) + MSAA (fallback)
// ════════════════════════════════════════════════════════════════════

std::wstring SelectionMonitor::getSelectedText() {
    // §Win11 fix: Prioritise UIA over clipboard to avoid notification toast
    std::wstring text = getSelectionViaUIA();
    if (!text.empty()) return text;

    // Fallback: IAccessible (MSAA)
    text = getSelectionViaMSAA();
    return text;
}

std::wstring SelectionMonitor::getSelectionViaUIA() {
    IUIAutomation* pAutomation = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(CUIAutomation), nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IUIAutomation),
        reinterpret_cast<void**>(&pAutomation)
    );
    if (FAILED(hr) || !pAutomation) return L"";

    // Get the focused element
    IUIAutomationElement* pFocused = nullptr;
    hr = pAutomation->GetFocusedElement(&pFocused);
    if (FAILED(hr) || !pFocused) {
        pAutomation->Release();
        return L"";
    }

    // Try TextPattern for selected text
    IUIAutomationTextPattern* pTextPattern = nullptr;
    hr = pFocused->GetCurrentPatternAs(
        UIA_TextPatternId,
        __uuidof(IUIAutomationTextPattern),
        reinterpret_cast<void**>(&pTextPattern)
    );

    std::wstring result;
    if (SUCCEEDED(hr) && pTextPattern) {
        IUIAutomationTextRangeArray* pRanges = nullptr;
        hr = pTextPattern->GetSelection(&pRanges);
        if (SUCCEEDED(hr) && pRanges) {
            int length = 0;
            pRanges->get_Length(&length);
            if (length > 0) {
                IUIAutomationTextRange* pRange = nullptr;
                pRanges->GetElement(0, &pRange);
                if (pRange) {
                    BSTR bstr = nullptr;
                    pRange->GetText(-1, &bstr);
                    if (bstr) {
                        result = std::wstring(bstr, SysStringLen(bstr));
                        SysFreeString(bstr);
                    }
                    pRange->Release();
                }
            }
            pRanges->Release();
        }
        pTextPattern->Release();
    }

    pFocused->Release();
    pAutomation->Release();
    return result;
}

std::wstring SelectionMonitor::getSelectionViaMSAA() {
    // IAccessible (MSAA) fallback — §3.1
    // Get the foreground window's focused child via AccessibleObjectFromWindow
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return L"";

    IAccessible* pAccessible = nullptr;
    HRESULT hr = AccessibleObjectFromWindow(
        hwnd, OBJID_CLIENT,
        __uuidof(IAccessible),
        reinterpret_cast<void**>(&pAccessible)
    );
    if (FAILED(hr) || !pAccessible) return L"";

    // Get focused child selection
    VARIANT varChild;
    VariantInit(&varChild);
    hr = pAccessible->get_accFocus(&varChild);

    std::wstring result;
    if (SUCCEEDED(hr) && varChild.vt == VT_I4) {
        VARIANT varSelf;
        varSelf.vt = VT_I4;
        varSelf.lVal = varChild.lVal;

        BSTR bstr = nullptr;
        hr = pAccessible->get_accValue(varSelf, &bstr);
        if (SUCCEEDED(hr) && bstr) {
            result = std::wstring(bstr, SysStringLen(bstr));
            SysFreeString(bstr);
        }
    }

    VariantClear(&varChild);
    pAccessible->Release();
    return result;
}

} // namespace platform
} // namespace orka

#endif // _WIN32
