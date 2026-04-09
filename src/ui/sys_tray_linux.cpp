#ifdef __linux__
#include "sys_tray.h"
#include <iostream>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

extern "C" {
#include <libappindicator/app-indicator.h>
#include <gtk/gtk.h>
}

namespace orka {
namespace ui {

class SysTrayLinux : public SysTray {
public:
    SysTrayLinux() : m_indicator(nullptr) {}

    bool init() override {
        // We require GTK inside this thread to be initialized
        if (!gtk_init_check(0, nullptr)) {
            std::cerr << "[ORKA] Failed to initialize GTK for System Tray\n";
            return false;
        }

        m_indicator = app_indicator_new("orka-tray",
                                        "input-keyboard", // A standard fallback icon
                                        APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
        
        if (!m_indicator) {
            std::cerr << "[ORKA] Failed to create AppIndicator\n";
            return false;
        }

        app_indicator_set_status(m_indicator, APP_INDICATOR_STATUS_ACTIVE);

        GtkWidget *menu = gtk_menu_new();

        GtkWidget *item_settings = gtk_menu_item_new_with_label("Налаштування мови");
        g_signal_connect(item_settings, "activate", G_CALLBACK(on_settings_clicked), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_settings);

        GtkWidget *item_quit = gtk_menu_item_new_with_label("Вийти з Orka");
        g_signal_connect(item_quit, "activate", G_CALLBACK(on_quit_clicked), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_quit);

        gtk_widget_show_all(menu);
        app_indicator_set_menu(m_indicator, GTK_MENU(menu));

        return true;
    }

    void runLoop() override {
        gtk_main();
    }

    void quit() override {
        gtk_main_quit();
    }

private:
    AppIndicator *m_indicator;

    static void on_settings_clicked(GtkWidget* widget, gpointer data) {
        auto* self = static_cast<SysTrayLinux*>(data);
        if (self->m_onSettingsClicked) {
            self->m_onSettingsClicked();
        }
    }

    static void on_quit_clicked(GtkWidget* widget, gpointer data) {
        auto* self = static_cast<SysTrayLinux*>(data);
        if (self->m_onExitClicked) {
            self->m_onExitClicked();
        }
        self->quit();
    }
};

SysTray* createSysTray() {
    return new SysTrayLinux();
}

} // namespace ui
} // namespace orka
#endif
