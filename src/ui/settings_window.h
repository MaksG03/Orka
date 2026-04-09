#pragma once

#include <string>
#include <memory>
#include <functional>

namespace webview {
    class webview;
}

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
    std::unique_ptr<webview::webview> m_webview;
    std::function<void(const std::string&)> m_onLangChanged;
};

} // namespace ui
} // namespace orka
