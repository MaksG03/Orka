// ═══════════════════════════════════════════════════════
// Orka — Tray Icon
// Іконка в системному треї з контекстним меню
// Використовує іконки з logo/ каталогу проекту
// ═══════════════════════════════════════════════════════

use std::sync::{Arc, Mutex};
use tray_icon::{
    TrayIconBuilder,
    menu::{Menu, MenuItem, MenuEvent, PredefinedMenuItem},
};

pub struct OrkaTray {
    // Тримаємо живим — при drop іконка зникає
    _icon: tray_icon::TrayIcon,
}

impl OrkaTray {
    pub fn new(show_settings: Arc<Mutex<bool>>) -> Self {
        let icon = load_icon();

        // Контекстне меню
        let menu = Menu::new();

        let item_settings = MenuItem::new(
            "⚙  Налаштування", true, None
        );
        let item_separator = PredefinedMenuItem::separator();
        let item_quit = MenuItem::new(
            "✕  Вийти з Orka", true, None
        );

        menu.append(&item_settings).ok();
        menu.append(&item_separator).ok();
        menu.append(&item_quit).ok();

        let id_settings = item_settings.id().clone();
        let id_quit     = item_quit.id().clone();

        // Слухаємо події меню у фоновому потоці
        std::thread::spawn(move || {
            let receiver = MenuEvent::receiver();
            loop {
                if let Ok(event) = receiver.recv() {
                    if event.id == id_settings {
                        *show_settings.lock().unwrap() = true;
                    }
                    if event.id == id_quit {
                        std::process::exit(0);
                    }
                }
            }
        });

        let icon_obj = TrayIconBuilder::new()
            .with_menu(Box::new(menu))
            .with_tooltip("Orka  ·  Alt+Z → конвертувати виділення")
            .with_icon(icon)
            .build()
            .expect("Не вдалося створити tray icon");

        Self { _icon: icon_obj }
    }
}

fn load_icon() -> tray_icon::Icon {
    // Спробувати завантажити іконку з logo/ каталогу проекту
    let icon_paths = [
        "logo/Square44x44Logo.png",
        "logo/Square150x150Logo.png",
        "logo/StoreLogo50х50.png",
    ];

    for path in &icon_paths {
        if let Ok(data) = std::fs::read(path) {
            if let Ok(img) = image::load_from_memory(&data) {
                let rgba = img.to_rgba8();
                let (w, h) = rgba.dimensions();
                if let Ok(icon) = tray_icon::Icon::from_rgba(rgba.into_raw(), w, h) {
                    return icon;
                }
            }
        }
    }

    // Якщо файли logo/ не знайдені — спробувати поряд з бінарником
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            for name in &["orka-icon.png", "Square44x44Logo.png"] {
                let p = dir.join(name);
                if let Ok(data) = std::fs::read(&p) {
                    if let Ok(img) = image::load_from_memory(&data) {
                        let rgba = img.to_rgba8();
                        let (w, h) = rgba.dimensions();
                        if let Ok(icon) = tray_icon::Icon::from_rgba(rgba.into_raw(), w, h) {
                            return icon;
                        }
                    }
                }
            }
        }
    }

    // Linux: спробувати системний шлях
    #[cfg(target_os = "linux")]
    {
        let system_icon = "/usr/share/icons/hicolor/256x256/apps/orka.png";
        if let Ok(data) = std::fs::read(system_icon) {
            if let Ok(img) = image::load_from_memory(&data) {
                let rgba = img.to_rgba8();
                let (w, h) = rgba.dimensions();
                if let Ok(icon) = tray_icon::Icon::from_rgba(rgba.into_raw(), w, h) {
                    return icon;
                }
            }
        }
    }

    // Fallback: маленька 2×2 синя іконка
    eprintln!("Orka: іконку не знайдено, використовую fallback");
    let rgba = vec![
        0x33, 0x99, 0xFF, 0xFF,  0x33, 0x99, 0xFF, 0xFF,
        0x33, 0x99, 0xFF, 0xFF,  0x33, 0x99, 0xFF, 0xFF,
    ];
    tray_icon::Icon::from_rgba(rgba, 2, 2)
        .expect("Не вдалося створити fallback Icon")
}
