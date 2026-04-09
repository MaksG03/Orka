#pragma once

#include <string>
#include <memory>
#include <functional>

namespace orka {
namespace ui {

class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    // Show the window representing the Figma UI
    void show(const std::string& indexPath);

    // Callbacks to interact with core logic
    void setOnLanguageChanged(std::function<void(const std::string&)> callback) {
        m_onLangChanged = callback;
    }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::function<void(const std::string&)> m_onLangChanged;
};

} // namespace ui
} // namespace orka
