#pragma once

#include <functional>

namespace orka {
namespace ui {

class SysTray {
public:
    virtual ~SysTray() = default;

    // Initializes the tray icon (usually needs to be called in main thread)
    virtual bool init() = 0;

    // Blocking loop for the GUI/Tray. 
    // Returns when the tray is closed or application is exiting.
    virtual void runLoop() = 0;

    // Signals the loop to exit
    virtual void quit() = 0;

    // Set callback to run when "Settings" is clicked in the tray menu
    void setOnSettingsClicked(std::function<void()> callback) {
        m_onSettingsClicked = callback;
    }

    // Set callback to run when "Exit" is clicked
    void setOnExitClicked(std::function<void()> callback) {
        m_onExitClicked = callback;
    }

protected:
    std::function<void()> m_onSettingsClicked;
    std::function<void()> m_onExitClicked;
};

// Factory function
SysTray* createSysTray();

} // namespace ui
} // namespace orka
