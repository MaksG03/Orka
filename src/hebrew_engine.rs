// ═══════════════════════════════════════════════════════
// Orka — Hebrew Engine (EN ↔ HE)
//
// Standard QWERTY → Hebrew keyboard mapping.
// Handles Nikud (U+05B0–U+05C7) lossy stripping with warning.
//
// Ported from C++ hebrew_engine.cpp
// ═══════════════════════════════════════════════════════

use std::collections::HashMap;
use crate::conversion_engine::ConversionResult;
use crate::hangul_engine::Direction;

pub struct HebrewEngine;

impl HebrewEngine {
    /// Detect direction: Latin → Forward, Hebrew → Reverse
    pub fn detect_direction(input: &str) -> Direction {
        let mut latin = 0usize;
        let mut hebrew = 0usize;
        for ch in input.chars() {
            if ch.is_ascii_alphabetic() { latin += 1; }
            if is_hebrew_letter(ch) { hebrew += 1; }
        }
        if latin > hebrew { Direction::Forward }
        else if hebrew > latin { Direction::Reverse }
        else { Direction::Unknown }
    }

    /// Check if text contains Nikud diacritics
    pub fn contains_nikud(input: &str) -> bool {
        input.chars().any(is_nikud)
    }

    /// Strip Nikud from Hebrew text
    pub fn strip_nikud(input: &str) -> String {
        input.chars().filter(|c| !is_nikud(*c)).collect()
    }

    /// Convert EN ↔ HE text
    pub fn convert(input: &str) -> ConversionResult {
        let en_to_he = build_en_to_he();
        let he_to_en = build_he_to_en(&en_to_he);
        let dir = Self::detect_direction(input);

        if dir == Direction::Forward || dir == Direction::Unknown {
            // EN → HE
            let out: String = input.chars().map(|ch| {
                let lower = ch.to_lowercase().next().unwrap_or(ch);
                *en_to_he.get(&lower).unwrap_or(&ch)
            }).collect();

            ConversionResult {
                text: out, success: true,
                nikud_stripped: false, escape_required: false, changed_count: 0,
            }
        } else {
            // HE → EN
            let nikud_stripped = Self::contains_nikud(input);
            let cleaned = if nikud_stripped { Self::strip_nikud(input) } else { input.to_string() };

            let out: String = cleaned.chars().map(|ch| {
                *he_to_en.get(&ch).unwrap_or(&ch)
            }).collect();

            ConversionResult {
                text: out, success: true,
                nikud_stripped, escape_required: false, changed_count: 0,
            }
        }
    }
}

// ── Helpers ────────────────────────────────────────────

fn is_hebrew_letter(ch: char) -> bool {
    let cp = ch as u32;
    cp >= 0x05D0 && cp <= 0x05EA
}

fn is_nikud(ch: char) -> bool {
    let cp = ch as u32;
    cp >= 0x05B0 && cp <= 0x05C7
}

// ── Standard QWERTY → Hebrew keyboard mapping ────────
fn build_en_to_he() -> HashMap<char, char> {
    HashMap::from([
        ('t', '\u{05D0}'), // א (alef)
        ('c', '\u{05D1}'), // ב (bet)
        ('d', '\u{05D2}'), // ג (gimel)
        ('s', '\u{05D3}'), // ד (dalet)
        ('v', '\u{05D4}'), // ה (he)
        ('u', '\u{05D5}'), // ו (vav)
        ('z', '\u{05D6}'), // ז (zayin)
        ('j', '\u{05D7}'), // ח (het)
        ('y', '\u{05D8}'), // ט (tet)
        ('h', '\u{05D9}'), // י (yod)
        ('l', '\u{05DA}'), // ך (final kaf)
        ('f', '\u{05DB}'), // כ (kaf)
        ('k', '\u{05DC}'), // ל (lamed)
        ('o', '\u{05DD}'), // ם (final mem)
        ('n', '\u{05DE}'), // מ (mem)
        ('i', '\u{05DF}'), // ן (final nun)
        ('b', '\u{05E0}'), // נ (nun)
        ('x', '\u{05E1}'), // ס (samekh)
        ('g', '\u{05E2}'), // ע (ayin)
        (';', '\u{05E3}'), // ף (final pe)
        ('p', '\u{05E4}'), // פ (pe)
        ('.', '\u{05E5}'), // ץ (final tsadi)
        ('m', '\u{05E6}'), // צ (tsadi)
        ('e', '\u{05E7}'), // ק (qof)
        ('r', '\u{05E8}'), // ר (resh)
        ('a', '\u{05E9}'), // ש (shin)
        (',', '\u{05EA}'), // ת (tav)
        ('/', '.'),
        ('w', ','),
    ])
}

fn build_he_to_en(en_to_he: &HashMap<char, char>) -> HashMap<char, char> {
    let mut rev = HashMap::new();
    for (&en, &he) in en_to_he {
        // Only map Hebrew letters (avoid overwriting with punctuation mappings)
        let cp = he as u32;
        if cp >= 0x05D0 && cp <= 0x05EA {
            rev.insert(he, en);
        }
    }
    rev
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_direction_latin() {
        assert_eq!(HebrewEngine::detect_direction("hello"), Direction::Forward);
    }

    #[test]
    fn test_direction_hebrew() {
        assert_eq!(HebrewEngine::detect_direction("\u{05E9}\u{05DC}\u{05D5}\u{05DD}"), Direction::Reverse);
    }

    #[test]
    fn test_en_to_he_single() {
        let res = HebrewEngine::convert("a");
        assert!(res.success);
        assert_eq!(res.text, "\u{05E9}"); // ש
    }

    #[test]
    fn test_en_to_he_alef() {
        let res = HebrewEngine::convert("t");
        assert_eq!(res.text, "\u{05D0}"); // א
    }

    #[test]
    fn test_en_to_he_word() {
        let res = HebrewEngine::convert("akln");
        assert!(res.success);
        assert_eq!(res.text, "\u{05E9}\u{05DC}\u{05DA}\u{05DE}"); // שלךמ
    }

    #[test]
    fn test_he_to_en_single() {
        let res = HebrewEngine::convert("\u{05E9}");
        assert!(res.success);
        assert!(!res.nikud_stripped);
        assert_eq!(res.text, "a");
    }

    #[test]
    fn test_nikud_detection() {
        assert!(HebrewEngine::contains_nikud("\u{05E9}\u{05B8}"));
        assert!(!HebrewEngine::contains_nikud("\u{05E9}\u{05DC}"));
    }

    #[test]
    fn test_nikud_stripping() {
        let stripped = HebrewEngine::strip_nikud("\u{05E9}\u{05B8}\u{05DC}");
        assert_eq!(stripped, "\u{05E9}\u{05DC}");
    }

    #[test]
    fn test_nikud_lossy_warning() {
        let with_nikud = "\u{05E9}\u{05B8}\u{05C1}\u{05DC}\u{05D5}\u{05B9}\u{05DD}";
        let res = HebrewEngine::convert(with_nikud);
        assert!(res.nikud_stripped);
        assert!(res.success);
    }

    #[test]
    fn test_roundtrip() {
        let fwd = HebrewEngine::convert("test");
        let rev = HebrewEngine::convert(&fwd.text);
        assert_eq!(rev.text, "test");
    }

    #[test]
    fn test_case_insensitive() {
        let lower = HebrewEngine::convert("a");
        let upper = HebrewEngine::convert("A");
        assert_eq!(lower.text, upper.text);
    }
}
