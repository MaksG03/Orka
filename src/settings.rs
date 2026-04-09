// ═══════════════════════════════════════════════════════
// Orka — Налаштування (зберігаються у JSON)
// Шлях:
//   Windows: %APPDATA%\Orka\settings.json
//   Linux:   ~/.config/orka/settings.json
// ═══════════════════════════════════════════════════════

use serde::{Deserialize, Serialize};
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Settings {
    /// Хоткей для конвертації (за замовчуванням Alt+Z)
    pub hotkey: String,
    /// Запускати разом з системою
    pub start_with_os: bool,
    /// Беззвучний режим (без сповіщень)
    pub silent: bool,
    /// Поточна мовна пара (uk, ko, he)
    pub language: String,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            hotkey: "Alt+Z".to_string(),
            start_with_os: false,
            silent: false,
            language: "uk".to_string(),
        }
    }
}

impl Settings {
    pub fn load() -> Self {
        let path = settings_path();
        if !path.exists() {
            return Self::default();
        }
        std::fs::read_to_string(&path)
            .ok()
            .and_then(|s| serde_json::from_str(&s).ok())
            .unwrap_or_default()
    }

    pub fn save(&self) {
        let path = settings_path();
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent).ok();
        }
        if let Ok(json) = serde_json::to_string_pretty(self) {
            std::fs::write(path, json).ok();
        }
    }

    /// Get LanguagePair from settings string
    pub fn language_pair(&self) -> crate::conversion_engine::LanguagePair {
        match self.language.as_str() {
            "ko" => crate::conversion_engine::LanguagePair::EnKo,
            "he" => crate::conversion_engine::LanguagePair::EnHe,
            _    => crate::conversion_engine::LanguagePair::EnUk,
        }
    }
}

fn settings_path() -> PathBuf {
    #[cfg(target_os = "windows")]
    {
        let appdata = std::env::var("APPDATA")
            .unwrap_or_else(|_| ".".to_string());
        PathBuf::from(appdata).join("Orka").join("settings.json")
    }
    #[cfg(target_os = "linux")]
    {
        let home = std::env::var("HOME")
            .unwrap_or_else(|_| ".".to_string());
        PathBuf::from(home).join(".config").join("orka").join("settings.json")
    }
}
