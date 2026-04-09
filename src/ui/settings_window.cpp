#include "settings_window.h"
#include <webview/webview.h>

namespace orka {
namespace ui {

SettingsWindow::SettingsWindow() {}

SettingsWindow::~SettingsWindow() {}

void SettingsWindow::show(const std::string& indexPath) {
    if (m_webview) {
        // If already open, just focus it
        m_webview->eval("window.focus()");
        return;
    }

    // Create webview
    // 0 = false (not debug mode)
    // 1 = true (frameless/transparent on supported OS? Actually just use regular constructor)
    m_webview = std::make_unique<webview::webview>(false, nullptr);
    
    // Figma designs are usually small, let's set fixed modern size
    m_webview->set_title("Orka Settings");
    m_webview->set_size(400, 300, WEBVIEW_HINT_FIXED);

    // Bind JavaScript function 'orkaSetLanguage' to C++
    m_webview->bind("orkaSetLanguage", [this](const std::string& seq, const std::string& req, void* arg) {
        // req will contain something like '["uk"]'
        if (m_onLangChanged && req.length() > 4) {
            // Very naive JSON string unescape: e.g. ["uk"] -> uk
            std::string lang = req.substr(2, req.length() - 4);
            m_onLangChanged(lang);
        }
    }, nullptr);

    m_webview->navigate("file://" + indexPath);
    
    // Run the inner window loop
    m_webview->run();

    // After window closed
    m_webview.reset();
}

} // namespace ui
} // namespace orka
