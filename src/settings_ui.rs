// ═══════════════════════════════════════════════════════
// Orka — Settings UI (wry webview)
// Відкривається при кліку на іконку в треї
// Завантажує React webapp з src/ui/webapp/dist/
// ═══════════════════════════════════════════════════════

use std::sync::{Arc, Mutex};
use crate::settings::Settings;

pub struct SettingsWindow;

impl SettingsWindow {
    /// Show the settings webview window
    /// Loads the React webapp from the built dist/index.html
    pub fn show(settings: Arc<Mutex<Settings>>) {
        // Find the webapp dist path
        let ui_path = find_ui_path();

        match ui_path {
            Some(path) => {
                eprintln!("Orka: відкриваю налаштування — {}", path);

                // Use wry to create a webview window
                use tao::event_loop::EventLoopBuilder;
                use tao::window::WindowBuilder;
                use tao::event::{Event, WindowEvent};
                use wry::WebViewBuilder;

                let event_loop = EventLoopBuilder::new().build();
                let window = WindowBuilder::new()
                    .with_title("Orka — Налаштування")
                    .with_inner_size(tao::dpi::LogicalSize::new(400.0, 350.0))
                    .with_resizable(false)
                    .build(&event_loop)
                    .expect("Не вдалося створити вікно налаштувань");

                let url = format!("file://{}", path);

                let settings_clone = settings.clone();

                let _webview = WebViewBuilder::new(&window)
                    .with_url(&url)
                    .with_ipc_handler(move |msg| {
                        // Handle messages from JS: orkaSetLanguage('uk')
                        let body = msg.body();
                        if body.starts_with("lang:") {
                            let lang = body.trim_start_matches("lang:").trim();
                            if let Ok(mut s) = settings_clone.lock() {
                                s.language = lang.to_string();
                                s.save();
                                eprintln!("Orka: мову змінено на {}", lang);
                            }
                        }
                    })
                    .build()
                    .expect("Не вдалося створити webview");

                event_loop.run(move |event, _, control_flow| {
                    *control_flow = tao::event_loop::ControlFlow::Wait;
                    if let Event::WindowEvent { event: WindowEvent::CloseRequested, .. } = event {
                        *control_flow = tao::event_loop::ControlFlow::Exit;
                    }
                });
            }
            None => {
                eprintln!("Orka: webapp не знайдено. Перебудуйте: cd src/ui/webapp && npm run build");
            }
        }
    }
}

/// Find the webapp dist/index.html path
fn find_ui_path() -> Option<String> {
    // Try relative paths first (dev mode)
    let candidates = [
        "src/ui/webapp/dist/index.html",
        "./src/ui/webapp/dist/index.html",
    ];

    for c in &candidates {
        if std::path::Path::new(c).exists() {
            return Some(std::fs::canonicalize(c).ok()?.to_string_lossy().to_string());
        }
    }

    // Try from executable directory
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let p = dir.join("ui").join("index.html");
            if p.exists() {
                return Some(p.to_string_lossy().to_string());
            }
        }
    }

    // Linux installed path
    #[cfg(target_os = "linux")]
    {
        let system_path = "/usr/share/orka/ui/index.html";
        if std::path::Path::new(system_path).exists() {
            return Some(system_path.to_string());
        }
    }

    // Windows installed path
    #[cfg(target_os = "windows")]
    {
        let system_path = "C:\\Program Files\\Orka\\index.html";
        if std::path::Path::new(system_path).exists() {
            return Some(system_path.to_string());
        }
    }

    None
}
