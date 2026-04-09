// ═══════════════════════════════════════════════════════
// Orka — ConversionEngine
// Підтримувані розкладки:
//   v1.0 → 🇺🇦 Українська ↔ 🇬🇧 Англійська
//   v1.5 → 🇰🇷 Корейська (Dubeolsik)
//   v2.0 → 🇮🇱 Іврит (Hebrew + Nikud)
// ═══════════════════════════════════════════════════════

use std::collections::HashMap;
use crate::hangul_engine::HangulEngine;
use crate::hebrew_engine::HebrewEngine;

// ── Мовні пари ─────────────────────────────────────────
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum LanguagePair {
    EnUk,  // v1.0
    EnKo,  // v1.5
    EnHe,  // v2.0
}

// ── Результат конвертації ──────────────────────────────
#[derive(Debug, Clone)]
pub struct ConversionResult {
    pub text: String,
    pub success: bool,
    pub nikud_stripped: bool,   // HE: lossy conversion warning
    pub escape_required: bool, // KO: IME needs VK_ESCAPE first
    pub changed_count: usize,
}

pub struct ConversionEngine {
    ua_to_en: HashMap<char, char>,
    en_to_ua: HashMap<char, char>,
    // Upper-case special symbols (where shift changes the symbol)
    en_to_ua_upper: HashMap<char, char>,
    ua_to_en_upper: HashMap<char, char>,
    // Typographic quotes
    typo_quotes: HashMap<char, char>,
}

impl ConversionEngine {
    pub fn new() -> Self {
        // Позиційна карта: UA QWERTY → EN QWERTY (lowercase)
        let pairs: &[(char, char)] = &[
            // Верхній ряд
            ('й','q'),('ц','w'),('у','e'),('к','r'),('е','t'),
            ('н','y'),('г','u'),('ш','i'),('щ','o'),('з','p'),
            ('х','['),('ї',']'),
            // Середній ряд
            ('ф','a'),('і','s'),('в','d'),('а','f'),('п','g'),
            ('р','h'),('о','j'),('л','k'),('д','l'),('ж',';'),
            ('є','\''),
            // Нижній ряд
            ('я','z'),('ч','x'),('с','c'),('м','v'),('и','b'),
            ('т','n'),('ь','m'),('б',','),('ю','.'),('ґ','`'),
        ];

        let mut ua_to_en = HashMap::new();
        let mut en_to_ua = HashMap::new();

        for &(ua, en) in pairs {
            // Lowercase
            ua_to_en.insert(ua, en);
            en_to_ua.insert(en, ua);

            // Uppercase — автоматично
            let ua_up: Vec<char> = ua.to_uppercase().collect();
            let en_up: Vec<char> = en.to_uppercase().collect();
            if let (Some(&u), Some(&e)) = (ua_up.first(), en_up.first()) {
                if u != ua {
                    ua_to_en.insert(u, e);
                    en_to_ua.insert(e, u);
                }
            }
        }

        // Upper-case special overrides (shift symbols differ)
        let upper_pairs: &[(char, char)] = &[
            ('~', '\u{0490}'), // Ґ (uppercase)
            ('{', '\u{0425}'), // Х (uppercase)
            ('}', '\u{0407}'), // Ї (uppercase)
            (':', '\u{0416}'), // Ж (uppercase)
            ('"', '\u{0404}'), // Є (uppercase)
            ('<', '\u{0411}'), // Б (uppercase)
            ('>', '\u{042E}'), // Ю (uppercase)
        ];

        let mut en_to_ua_upper = HashMap::new();
        let mut ua_to_en_upper = HashMap::new();
        for &(en, ua) in upper_pairs {
            en_to_ua_upper.insert(en, ua);
            ua_to_en_upper.insert(ua, en);
        }

        // Typographic quotes «» ↔ ""
        let typo_quotes = HashMap::from([
            ('\u{201C}', '\u{00AB}'), // " → «
            ('\u{201D}', '\u{00BB}'), // " → »
            ('\u{00AB}', '\u{201C}'), // « → "
            ('\u{00BB}', '\u{201D}'), // » → "
        ]);

        Self { ua_to_en, en_to_ua, en_to_ua_upper, ua_to_en_upper, typo_quotes }
    }

    /// Головна точка конвертації з вибором мовної пари
    pub fn convert(&self, text: &str, pair: LanguagePair) -> ConversionResult {
        // Edge cases: empty / whitespace-only / digits-only
        if text.is_empty() {
            return ConversionResult {
                text: String::new(), success: false,
                nikud_stripped: false, escape_required: false, changed_count: 0,
            };
        }

        let has_convertible = text.chars().any(|c| !c.is_whitespace() && !c.is_ascii_digit());
        if !has_convertible {
            return ConversionResult {
                text: String::new(), success: false,
                nikud_stripped: false, escape_required: false, changed_count: 0,
            };
        }

        match pair {
            LanguagePair::EnUk => self.convert_en_uk(text),
            LanguagePair::EnKo => HangulEngine::convert(text, false),
            LanguagePair::EnHe => HebrewEngine::convert(text),
        }
    }

    /// EN ↔ UK авто-конвертація
    fn convert_en_uk(&self, text: &str) -> ConversionResult {
        let cyr = text.chars()
            .filter(|c| matches!(*c, '\u{0400}'..='\u{04FF}'))
            .count();
        let lat = text.chars()
            .filter(|c| c.is_ascii_alphabetic())
            .count();

        let result = if cyr == 0 && lat == 0 {
            text.to_string() // цифри/пунктуація — без змін
        } else if cyr >= lat {
            self.convert_ua_to_en(text)
        } else {
            self.convert_en_to_ua(text)
        };

        let changed = self.count_changed(text, &result);
        ConversionResult {
            text: result, success: true,
            nikud_stripped: false, escape_required: false, changed_count: changed,
        }
    }

    pub fn convert_ua_to_en(&self, text: &str) -> String {
        text.chars().map(|c| {
            // Typographic quotes
            if let Some(&mapped) = self.typo_quotes.get(&c) {
                return mapped;
            }
            // Upper special symbols
            if let Some(&mapped) = self.ua_to_en_upper.get(&c) {
                return mapped;
            }
            *self.ua_to_en.get(&c).unwrap_or(&c)
        }).collect()
    }

    pub fn convert_en_to_ua(&self, text: &str) -> String {
        text.chars().map(|c| {
            // Typographic quotes
            if let Some(&mapped) = self.typo_quotes.get(&c) {
                return mapped;
            }
            // Upper special symbols
            if let Some(&mapped) = self.en_to_ua_upper.get(&c) {
                return mapped;
            }
            // Check lowercase lookup, preserving case
            let is_upper = c.is_uppercase();
            let lower = c.to_lowercase().next().unwrap_or(c);
            if let Some(&mapped) = self.en_to_ua.get(&lower) {
                if is_upper {
                    mapped.to_uppercase().next().unwrap_or(mapped)
                } else {
                    mapped
                }
            } else {
                c
            }
        }).collect()
    }

    /// Кількість змінених символів
    pub fn count_changed(&self, original: &str, result: &str) -> usize {
        original.chars()
            .zip(result.chars())
            .filter(|(a, b)| a != b)
            .count()
    }
}

// ═══════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_all_33_lowercase_ua_to_en() {
        let e = ConversionEngine::new();
        let ua = "йцукенгшщзхїфівапрлджєячсмитьбюґ";
        let en = "qwertyuiop[]asdfghjkl;'zxcvbnm,.`";
        assert_eq!(e.convert_ua_to_en(ua), en);
    }

    #[test]
    fn test_all_33_lowercase_en_to_ua() {
        let e = ConversionEngine::new();
        let en = "qwertyuiop[]asdfghjkl;'zxcvbnm,.`";
        let ua = "йцукенгшщзхїфівапрлджєячсмитьбюґ";
        assert_eq!(e.convert_en_to_ua(en), ua);
    }

    #[test]
    fn test_uppercase_preserved() {
        let e = ConversionEngine::new();
        assert_eq!(e.convert_ua_to_en("Привіт"), "Ghbdim");
        assert_eq!(e.convert_en_to_ua("Hello"), "Руддщ");
    }

    #[test]
    fn test_digits_passthrough() {
        let e = ConversionEngine::new();
        assert_eq!(e.convert_ua_to_en("привіт 123!"), "ghbdim 123!");
    }

    #[test]
    fn test_auto_detect_cyrillic_dominant() {
        let e = ConversionEngine::new();
        let result = e.convert("привіт", LanguagePair::EnUk);
        assert!(result.success);
        assert!(result.text.chars().all(|c| c.is_ascii() || c == ' ' || c == '!'));
    }

    #[test]
    fn test_auto_detect_latin_dominant() {
        let e = ConversionEngine::new();
        let result = e.convert("ghbdim", LanguagePair::EnUk);
        assert!(result.success);
        assert!(result.text.chars().any(|c| matches!(c, '\u{0400}'..='\u{04FF}')));
    }

    #[test]
    fn test_empty_string() {
        let e = ConversionEngine::new();
        let result = e.convert("", LanguagePair::EnUk);
        assert!(!result.success);
    }

    #[test]
    fn test_digits_only_no_change() {
        let e = ConversionEngine::new();
        let result = e.convert("12345", LanguagePair::EnUk);
        assert!(!result.success);
    }

    #[test]
    fn test_special_chars_ua() {
        let e = ConversionEngine::new();
        assert_eq!(e.convert_ua_to_en("їєіґ"), "]';`");
    }

    #[test]
    fn test_count_changed() {
        let e = ConversionEngine::new();
        let original = "привіт";
        let result = e.convert_ua_to_en(original);
        assert_eq!(e.count_changed(original, &result), 6);
    }

    #[test]
    fn test_typographic_quotes() {
        let e = ConversionEngine::new();
        // " → «, " → »
        let input = "\u{201C}Hello\u{201D}";
        let result = e.convert_en_to_ua(input);
        assert!(result.starts_with('\u{00AB}'));
        assert!(result.ends_with('\u{00BB}'));
    }

    #[test]
    fn test_typographic_quotes_reverse() {
        let e = ConversionEngine::new();
        // « → ", » → "
        let input = "\u{00AB}Привіт\u{00BB}";
        let result = e.convert_ua_to_en(input);
        assert!(result.starts_with('\u{201C}'));
        assert!(result.ends_with('\u{201D}'));
    }

    #[test]
    fn test_roundtrip_en_uk() {
        let e = ConversionEngine::new();
        let fwd = e.convert("test", LanguagePair::EnUk);
        let rev = e.convert(&fwd.text, LanguagePair::EnUk);
        assert_eq!(rev.text, "test");
    }

    #[test]
    fn test_upper_special_symbols() {
        let e = ConversionEngine::new();
        // ~ → Ґ, { → Х, } → Ї, : → Ж, " → Є
        assert_eq!(e.convert_en_to_ua("~"), "\u{0490}");
        assert_eq!(e.convert_en_to_ua("{"), "\u{0425}");
        assert_eq!(e.convert_en_to_ua("}"), "\u{0407}");
    }

    // ── Korean tests ─────────────────────────────────────
    #[test]
    fn test_korean_basic() {
        let e = ConversionEngine::new();
        let result = e.convert("rk", LanguagePair::EnKo);
        assert!(result.success);
        assert_eq!(result.text, "\u{AC00}"); // 가
    }

    #[test]
    fn test_korean_roundtrip() {
        let e = ConversionEngine::new();
        let fwd = e.convert("rk", LanguagePair::EnKo);
        let rev = e.convert(&fwd.text, LanguagePair::EnKo);
        assert_eq!(rev.text, "rk");
    }

    // ── Hebrew tests ─────────────────────────────────────
    #[test]
    fn test_hebrew_basic() {
        let e = ConversionEngine::new();
        let result = e.convert("a", LanguagePair::EnHe);
        assert!(result.success);
        assert_eq!(result.text, "\u{05E9}"); // ש
    }

    #[test]
    fn test_hebrew_roundtrip() {
        let e = ConversionEngine::new();
        let fwd = e.convert("test", LanguagePair::EnHe);
        let rev = e.convert(&fwd.text, LanguagePair::EnHe);
        assert_eq!(rev.text, "test");
    }
}
