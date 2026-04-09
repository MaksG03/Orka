#ifdef _WIN32
#include "sys_tray.h"
#include <windows.h>
#include <shellapi.h>
#include <iostream>

namespace orka {
namespace ui {

class SysTrayWin : public SysTray {
public:
    SysTrayWin() {}

    bool init() override {
        // Minimum viable implementation for later
        return true;
    }

    void runLoop() override {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    void quit() override {
        PostQuitMessage(0);
    }
};

SysTray* createSysTray() {
    return new SysTrayWin();
}

} // namespace ui
} // namespace orka
#endif
