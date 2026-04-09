#ifdef _WIN32
#include "sys_tray.h"
#include <windows.h>
#include <shellapi.h>
#include <iostream>

namespace orka {
namespace ui {

class SysTrayWin : public SysTray {
    HWND m_hwnd = nullptr;
    NOTIFYICONDATAW m_nid = {};

    static constexpr UINT WM_TRAYICON = WM_USER + 1;
    static constexpr UINT ID_SETTINGS = 2001;
    static constexpr UINT ID_EXIT = 2002;

public:
    SysTrayWin() {}

    bool init() override {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = s_WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"OrkaTrayClass";
        RegisterClassW(&wc);

        m_hwnd = CreateWindowExW(0, L"OrkaTrayClass", L"Orka System Tray", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, nullptr, wc.hInstance, this);
        if (!m_hwnd) return false;

        m_nid.cbSize = sizeof(NOTIFYICONDATAW);
        m_nid.hWnd = m_hwnd;
        m_nid.uID = 1;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        
        // Use generic application icon for now
        m_nid.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION); 
        wcscpy_s(m_nid.szTip, L"Orka Keyboard Converter");

        Shell_NotifyIconW(NIM_ADD, &m_nid);
        return true;
    }

    void runLoop() override {
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void quit() override {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        PostQuitMessage(0);
    }

private:
    static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        SysTrayWin* tray = nullptr;
        if (msg == WM_NCCREATE) {
            CREATESTRUCT* cs = (CREATESTRUCT*)lparam;
            tray = (SysTrayWin*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)tray);
        } else {
            tray = (SysTrayWin*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        }

        if (tray) {
            return tray->wndProc(hwnd, msg, wparam, lparam);
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        if (msg == WM_TRAYICON) {
            if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_SETTINGS, L"Settings");
                InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
                InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_EXIT, L"Exit");

                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(hMenu);

                if (cmd == ID_SETTINGS && m_onSettingsClicked) {
                    m_onSettingsClicked();
                } else if (cmd == ID_EXIT && m_onExitClicked) {
                    m_onExitClicked();
                }
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
};

SysTray* createSysTray() {
    return new SysTrayWin();
}

} // namespace ui
} // namespace orka
#endif
