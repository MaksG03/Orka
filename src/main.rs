// ═══════════════════════════════════════════════════════
// Orka — Keyboard Layout Converter
// Perepelytsia Orka Technologies · 2025
//
// Схема роботи:
//   1. Запуск → іконка в системному треї
//   2. Клік на іконку → відкривається вікно налаштувань
//   3. Користувач виділяє текст у будь-якому додатку
//   4. Alt+Z → Orka читає виділення, конвертує, вставляє
//
// Мовні пари:
//   v1.0 → 🇺🇦 Українська ↔ 🇬🇧 Англійська
//   v1.5 → 🇰🇷 Корейська (Dubeolsik)
//   v2.0 → 🇮🇱 Іврит (Hebrew + Nikud)
// ═══════════════════════════════════════════════════════

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
// ^ Приховує консольне вікно у Release збірці на Windows

mod conversion_engine;
mod hangul_engine;
mod hebrew_engine;
mod selection_monitor;
mod settings;
mod settings_ui;
mod tray;

use std::sync::{Arc, Mutex};
use conversion_engine::ConversionEngine;
use settings::Settings;
use settings_ui::SettingsWindow;

fn main() {
    // ── Ініціалізація ───────────────────────────────────
    eprintln!("╔══════════════════════════════════════════╗");
    eprintln!("║  ORKA — Intelligent Keyboard Converter   ║");
    eprintln!("║  v1.0  ·  Perepelytsia Orka Technologies ║");
    eprintln!("╚══════════════════════════════════════════╝");

    let engine   = Arc::new(ConversionEngine::new());
    let cfg      = Arc::new(Mutex::new(Settings::load()));
    let show_ui  = Arc::new(Mutex::new(false));

    // ── Tray іконка ─────────────────────────────────────
    let _tray = tray::OrkaTray::new(show_ui.clone());

    // ── Hotkey listener у фоновому потоці ───────────────
    {
        let eng = engine.clone();
        let settings = cfg.clone();
        std::thread::spawn(move || {
            hotkey_loop(&eng, &settings);
        });
    }

    // ── Settings UI watcher у фоновому потоці ──────────
    {
        let show_ui_clone = show_ui.clone();
        let cfg_clone = cfg.clone();
        std::thread::spawn(move || {
            loop {
                std::thread::sleep(std::time::Duration::from_millis(200));
                let should_show = {
                    let mut s = show_ui_clone.lock().unwrap();
                    if *s {
                        *s = false;
                        true
                    } else {
                        false
                    }
                };
                if should_show {
                    SettingsWindow::show(cfg_clone.clone());
                }
            }
        });
    }

    // ── Основний цикл (тримаємо програму живою) ────────
    eprintln!("[ORKA] Запущено. Alt+Z для конвертації виділеного тексту.");
    let lang = cfg.lock().unwrap().language.clone();
    match lang.as_str() {
        "ko" => eprintln!("[ORKA] Мовна пара: EN ↔ KO (Korean)"),
        "he" => eprintln!("[ORKA] Мовна пара: EN ↔ HE (Hebrew)"),
        _    => eprintln!("[ORKA] Мовна пара: EN ↔ UK (Ukrainian)"),
    }

    loop {
        std::thread::sleep(std::time::Duration::from_secs(1));
    }
}

// ───────────────────────────────────────────────────────
// Hotkey Loop
// ───────────────────────────────────────────────────────

fn hotkey_loop(engine: &ConversionEngine, settings: &Arc<Mutex<Settings>>) {
    #[cfg(target_os = "windows")]
    windows_loop(engine, settings);

    #[cfg(target_os = "linux")]
    linux_loop(engine, settings);
}

// ── WINDOWS ─────────────────────────────────────────────
#[cfg(target_os = "windows")]
fn windows_loop(engine: &ConversionEngine, settings: &Arc<Mutex<Settings>>) {
    use ::windows::Win32::UI::Input::KeyboardAndMouse::*;
    use ::windows::Win32::UI::WindowsAndMessaging::*;
    use ::windows::Win32::Foundation::*;

    const ID_HOTKEY: i32 = 1;

    unsafe {
        let ok = RegisterHotKey(
            HWND(0),
            ID_HOTKEY,
            MOD_ALT | MOD_NOREPEAT,
            'Z' as u32,
        );

        if ok.is_err() {
            eprintln!(
                "Orka: не вдалося зареєструвати Alt+Z. \
                 Можливо, зайнято іншою програмою."
            );
            return;
        }

        eprintln!("Orka: Alt+Z зареєстровано ✓");

        let mut msg = MSG::default();
        while GetMessageW(&mut msg, HWND(0), 0, 0).as_bool() {
            if msg.message == WM_HOTKEY && msg.wParam.0 as i32 == ID_HOTKEY {
                do_convert(engine, settings);
            }
        }

        UnregisterHotKey(HWND(0), ID_HOTKEY).ok();
    }
}

// ── LINUX ───────────────────────────────────────────────
#[cfg(target_os = "linux")]
fn linux_loop(engine: &ConversionEngine, settings: &Arc<Mutex<Settings>>) {
    use x11rb::connection::Connection;
    use x11rb::protocol::xproto::*;

    let (conn, screen_num) = match x11rb::connect(None) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Orka: X11 помилка: {}", e);
            return;
        }
    };

    let root = conn.setup().roots[screen_num].root;

    // Keycode 'Z' = 52 на більшості клавіатур
    // Alt = ModMask::M1
    conn.grab_key(
        true, root,
        ModMask::M1,
        52u8, // Z
        GrabMode::ASYNC,
        GrabMode::ASYNC,
    ).ok();

    // Also grab with NumLock / CapsLock variants
    conn.grab_key(true, root, ModMask::M1 | ModMask::M2, 52u8, GrabMode::ASYNC, GrabMode::ASYNC).ok();
    conn.grab_key(true, root, ModMask::M1 | ModMask::LOCK, 52u8, GrabMode::ASYNC, GrabMode::ASYNC).ok();
    conn.grab_key(true, root, ModMask::M1 | ModMask::M2 | ModMask::LOCK, 52u8, GrabMode::ASYNC, GrabMode::ASYNC).ok();

    conn.flush().ok();
    eprintln!("Orka: Alt+Z зареєстровано ✓");

    loop {
        match conn.wait_for_event() {
            Ok(event) => {
                use x11rb::protocol::Event::*;
                if let KeyPress(_) = event {
                    do_convert(engine, settings);
                }
            }
            Err(e) => {
                eprintln!("Orka: помилка event loop: {}", e);
                break;
            }
        }
    }
}

// ── Конвертація ─────────────────────────────────────────
fn do_convert(engine: &ConversionEngine, settings: &Arc<Mutex<Settings>>) {
    #[cfg(target_os = "windows")]
    let get_fn    = selection_monitor::windows::get_selected_text;
    #[cfg(target_os = "windows")]
    let inject_fn = selection_monitor::windows::inject_text;

    #[cfg(target_os = "linux")]
    let get_fn    = selection_monitor::linux::get_selected_text;
    #[cfg(target_os = "linux")]
    let inject_fn = selection_monitor::linux::inject_text;

    let pair = settings.lock().unwrap().language_pair();

    match get_fn() {
        None => {
            eprintln!("Orka: нічого не виділено");
        }
        Some(text) if text.trim().is_empty() => {
            eprintln!("Orka: виділено лише пробіли");
        }
        Some(text) => {
            let result = engine.convert(&text, pair);

            if !result.success {
                eprintln!("Orka: конвертація не потрібна");
                return;
            }

            if result.changed_count == 0 && result.text == text {
                eprintln!("Orka: символи не змінились");
                return;
            }

            if result.nikud_stripped {
                eprintln!("Orka: ⚠ Огласовки (Nikud) будуть втрачені");
            }
            if result.escape_required {
                eprintln!("Orka: ⚠ Незавершений склад IME скасовано");
            }

            inject_fn(&result.text);
            eprintln!("Orka: ✓ конвертовано");
        }
    }
}
