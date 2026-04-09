#include "settings_window.h"
#include <webview.h>

namespace orka {
namespace ui {

struct SettingsWindow::Impl {
    std::unique_ptr<webview::webview> wv;
};

SettingsWindow::SettingsWindow() : m_impl(std::make_unique<Impl>()) {}

SettingsWindow::~SettingsWindow() = default;

void SettingsWindow::show(const std::string& indexPath) {
    if (m_impl->wv) {
        // If already open, just focus it
        m_impl->wv->eval("window.focus()");
        return;
    }

    // Create webview
    // 0 = false (not debug mode)
    // 1 = true (frameless/transparent on supported OS? Actually just use regular constructor)
    m_impl->wv = std::make_unique<webview::webview>(false, nullptr);
    
    // Figma designs are usually small, let's set fixed modern size
    m_impl->wv->set_title("Orka Settings");
    m_impl->wv->set_size(400, 300, WEBVIEW_HINT_FIXED);

    // Bind JavaScript function 'orkaSetLanguage' to C++
    m_impl->wv->bind("orkaSetLanguage", [this](const std::string& seq, const std::string& req, void* arg) {
        // req will contain something like '["uk"]'
        if (m_onLangChanged && req.length() > 4) {
            // Very naive JSON string unescape: e.g. ["uk"] -> uk
            std::string lang = req.substr(2, req.length() - 4);
            m_onLangChanged(lang);
        }
    }, nullptr);

    m_impl->wv->navigate("file://" + indexPath);
    
    // Run the inner window loop
    m_impl->wv->run();

    // After window closed
    m_impl->wv.reset();
}

} // namespace ui
} // namespace orka
