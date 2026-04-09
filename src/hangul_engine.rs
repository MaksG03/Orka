// ═══════════════════════════════════════════════════════
// Orka — Hangul Engine (EN ↔ KO, Dubeolsik 두벌식)
//
// QWERTY keys → Hangul Jamo → syllable composition
// Syllable = (Lead × 21 + Vowel) × 28 + Trail + 0xAC00
//
// Ported from C++ hangul_engine.cpp
// ═══════════════════════════════════════════════════════

use std::collections::HashMap;
use crate::conversion_engine::ConversionResult;

pub struct HangulEngine;

impl HangulEngine {
    /// Detect direction: Latin → FORWARD, Hangul → REVERSE
    pub fn detect_direction(input: &str) -> Direction {
        let mut latin = 0usize;
        let mut hangul = 0usize;
        for ch in input.chars() {
            if ch.is_ascii_alphabetic() { latin += 1; }
            if is_hangul_syllable(ch) || is_hangul_jamo(ch) { hangul += 1; }
        }
        if latin > hangul { Direction::Forward }
        else if hangul > latin { Direction::Reverse }
        else { Direction::Unknown }
    }

    /// Convert EN ↔ KO text
    pub fn convert(input: &str, ime_active: bool) -> ConversionResult {
        let dir = Self::detect_direction(input);

        if dir == Direction::Reverse {
            // KO → EN: decompose Hangul syllables back to QWERTY
            let jamo_to_qwerty = build_jamo_to_qwerty();
            let mut out = String::new();
            for ch in input.chars() {
                if is_hangul_syllable(ch) {
                    out.push_str(&decompose_syllable_to_qwerty(ch, &jamo_to_qwerty));
                } else if is_hangul_jamo(ch) {
                    if let Some(&q) = jamo_to_qwerty.get(&ch) {
                        out.push(q);
                    } else {
                        out.push(ch);
                    }
                } else {
                    out.push(ch);
                }
            }
            return ConversionResult {
                text: out, success: true,
                nikud_stripped: false, escape_required: false, changed_count: 0,
            };
        }

        // EN → KO: compose QWERTY keys into Hangul
        let escape_required = ime_active;
        let qwerty_to_compat = build_qwerty_to_compat();
        let compat_to_choseong = build_compat_to_choseong();
        let compat_to_jungseong = build_compat_to_jungseong();
        let compat_to_jongseong = build_compat_to_jongseong();

        let mut out = String::new();

        #[derive(PartialEq)]
        enum State { Empty, Lead, LeadVowel, LeadVowelTrail }
        let mut state = State::Empty;
        let mut cur_lead: char = '\0';
        let mut cur_vowel: char = '\0';
        let mut cur_trail: char = '\0';

        let is_consonant = |c: char| compat_to_choseong.contains_key(&c);
        let is_vowel = |c: char| compat_to_jungseong.contains_key(&c);

        let flush_syllable = |state: &mut State, out: &mut String,
                                   lead: &mut char, vowel: &mut char, trail: &mut char| {
            match state {
                State::Lead => {
                    out.push(*lead);
                }
                State::LeadVowel => {
                    if let (Some(&li), Some(&vi)) = (compat_to_choseong.get(lead), compat_to_jungseong.get(vowel)) {
                        let syl = 0xAC00u32 + (li as u32 * 21 + vi as u32) * 28;
                        if let Some(c) = char::from_u32(syl) { out.push(c); }
                    } else {
                        out.push(*lead);
                        out.push(*vowel);
                    }
                }
                State::LeadVowelTrail => {
                    if let (Some(&li), Some(&vi), Some(&ti)) = (
                        compat_to_choseong.get(lead),
                        compat_to_jungseong.get(vowel),
                        compat_to_jongseong.get(trail)
                    ) {
                        let syl = 0xAC00u32 + (li as u32 * 21 + vi as u32) * 28 + ti as u32;
                        if let Some(c) = char::from_u32(syl) { out.push(c); }
                    } else {
                        out.push(*lead);
                        out.push(*vowel);
                        out.push(*trail);
                    }
                }
                _ => {}
            }
            *state = State::Empty;
            *lead = '\0';
            *vowel = '\0';
            *trail = '\0';
        };

        for ch in input.chars() {
            let jamo = match qwerty_to_compat.get(&ch) {
                Some(&j) => j,
                None => {
                    flush_syllable(&mut state, &mut out, &mut cur_lead, &mut cur_vowel, &mut cur_trail);
                    out.push(ch);
                    continue;
                }
            };

            match state {
                State::Empty => {
                    if is_consonant(jamo) {
                        cur_lead = jamo;
                        state = State::Lead;
                    } else if is_vowel(jamo) {
                        out.push(jamo);
                    }
                }
                State::Lead => {
                    if is_vowel(jamo) {
                        cur_vowel = jamo;
                        state = State::LeadVowel;
                    } else {
                        flush_syllable(&mut state, &mut out, &mut cur_lead, &mut cur_vowel, &mut cur_trail);
                        cur_lead = jamo;
                        state = State::Lead;
                    }
                }
                State::LeadVowel => {
                    if is_consonant(jamo) {
                        cur_trail = jamo;
                        state = State::LeadVowelTrail;
                    } else if is_vowel(jamo) {
                        flush_syllable(&mut state, &mut out, &mut cur_lead, &mut cur_vowel, &mut cur_trail);
                        out.push(jamo);
                    }
                }
                State::LeadVowelTrail => {
                    if is_vowel(jamo) {
                        // Trail was actually the lead of the next syllable
                        let saved_trail = cur_trail;
                        cur_trail = '\0';
                        state = State::LeadVowel;
                        flush_syllable(&mut state, &mut out, &mut cur_lead, &mut cur_vowel, &mut cur_trail);
                        cur_lead = saved_trail;
                        cur_vowel = jamo;
                        state = State::LeadVowel;
                    } else if is_consonant(jamo) {
                        flush_syllable(&mut state, &mut out, &mut cur_lead, &mut cur_vowel, &mut cur_trail);
                        cur_lead = jamo;
                        state = State::Lead;
                    }
                }
            }
        }
        flush_syllable(&mut state, &mut out, &mut cur_lead, &mut cur_vowel, &mut cur_trail);

        ConversionResult {
            text: out, success: true,
            nikud_stripped: false, escape_required,
            changed_count: 0,
        }
    }
}

// ── Direction enum (shared) ────────────────────────────
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Direction {
    Forward,
    Reverse,
    Unknown,
}

// ── Helpers ────────────────────────────────────────────

fn is_hangul_syllable(ch: char) -> bool {
    let cp = ch as u32;
    cp >= 0xAC00 && cp <= 0xD7A3
}

fn is_hangul_jamo(ch: char) -> bool {
    let cp = ch as u32;
    cp >= 0x3131 && cp <= 0x3163
}

// ── QWERTY → Compatibility Jamo mapping ───────────────
fn build_qwerty_to_compat() -> HashMap<char, char> {
    HashMap::from([
        // Consonants
        ('r', '\u{3131}'), // ㄱ
        ('R', '\u{3132}'), // ㄲ
        ('s', '\u{3134}'), // ㄴ
        ('e', '\u{3137}'), // ㄷ
        ('E', '\u{3138}'), // ㄸ
        ('f', '\u{3139}'), // ㄹ
        ('a', '\u{3141}'), // ㅁ
        ('q', '\u{3142}'), // ㅂ
        ('Q', '\u{3143}'), // ㅃ
        ('t', '\u{3145}'), // ㅅ
        ('T', '\u{3146}'), // ㅆ
        ('d', '\u{3147}'), // ㅇ
        ('w', '\u{3148}'), // ㅈ
        ('W', '\u{3149}'), // ㅉ
        ('c', '\u{314A}'), // ㅊ
        ('z', '\u{314B}'), // ㅋ
        ('x', '\u{314C}'), // ㅌ
        ('v', '\u{314D}'), // ㅍ
        ('g', '\u{314E}'), // ㅎ
        // Vowels
        ('k', '\u{314F}'), // ㅏ
        ('o', '\u{3150}'), // ㅐ
        ('i', '\u{3151}'), // ㅑ
        ('O', '\u{3152}'), // ㅒ
        ('j', '\u{3153}'), // ㅓ
        ('p', '\u{3154}'), // ㅔ
        ('u', '\u{3155}'), // ㅕ
        ('P', '\u{3156}'), // ㅖ
        ('h', '\u{3157}'), // ㅗ
        ('y', '\u{315B}'), // ㅛ
        ('n', '\u{315C}'), // ㅜ
        ('b', '\u{3160}'), // ㅠ
        ('l', '\u{3161}'), // ㅡ
        ('m', '\u{3163}'), // ㅣ
    ])
}

fn build_jamo_to_qwerty() -> HashMap<char, char> {
    let fwd = build_qwerty_to_compat();
    let mut rev = HashMap::new();
    for (&q, &j) in &fwd { rev.insert(j, q); }
    rev
}

fn build_compat_to_choseong() -> HashMap<char, u32> {
    HashMap::from([
        ('\u{3131}', 0),  ('\u{3132}', 1),  ('\u{3134}', 2),
        ('\u{3137}', 3),  ('\u{3138}', 4),  ('\u{3139}', 5),
        ('\u{3141}', 6),  ('\u{3142}', 7),  ('\u{3143}', 8),
        ('\u{3145}', 9),  ('\u{3146}', 10), ('\u{3147}', 11),
        ('\u{3148}', 12), ('\u{3149}', 13), ('\u{314A}', 14),
        ('\u{314B}', 15), ('\u{314C}', 16), ('\u{314D}', 17),
        ('\u{314E}', 18),
    ])
}

fn build_compat_to_jungseong() -> HashMap<char, u32> {
    HashMap::from([
        ('\u{314F}', 0),  ('\u{3150}', 1),  ('\u{3151}', 2),
        ('\u{3152}', 3),  ('\u{3153}', 4),  ('\u{3154}', 5),
        ('\u{3155}', 6),  ('\u{3156}', 7),  ('\u{3157}', 8),
        ('\u{3158}', 9),  ('\u{3159}', 10), ('\u{315A}', 11),
        ('\u{315B}', 12), ('\u{315C}', 13), ('\u{315D}', 14),
        ('\u{315E}', 15), ('\u{315F}', 16), ('\u{3160}', 17),
        ('\u{3161}', 18), ('\u{3162}', 19), ('\u{3163}', 20),
    ])
}

fn build_compat_to_jongseong() -> HashMap<char, u32> {
    HashMap::from([
        ('\u{3131}', 1),  ('\u{3132}', 2),  ('\u{3134}', 4),
        ('\u{3137}', 7),  ('\u{3139}', 8),  ('\u{3141}', 16),
        ('\u{3142}', 17), ('\u{3145}', 19), ('\u{3146}', 20),
        ('\u{3147}', 21), ('\u{3148}', 22), ('\u{314A}', 23),
        ('\u{314B}', 24), ('\u{314C}', 25), ('\u{314D}', 26),
        ('\u{314E}', 27),
    ])
}

fn build_choseong_to_compat() -> HashMap<u32, char> {
    let fwd = build_compat_to_choseong();
    fwd.into_iter().map(|(k, v)| (v, k)).collect()
}

fn build_jungseong_to_compat() -> HashMap<u32, char> {
    let fwd = build_compat_to_jungseong();
    fwd.into_iter().map(|(k, v)| (v, k)).collect()
}

fn build_jongseong_to_compat() -> HashMap<u32, char> {
    let fwd = build_compat_to_jongseong();
    fwd.into_iter().map(|(k, v)| (v, k)).collect()
}

fn decompose_syllable_to_qwerty(syllable: char, jamo_to_qwerty: &HashMap<char, char>) -> String {
    if !is_hangul_syllable(syllable) {
        return syllable.to_string();
    }

    let code = syllable as u32 - 0xAC00;
    let trail_idx = code % 28;
    let rem = code / 28;
    let vowel_idx = rem % 21;
    let lead_idx = rem / 21;

    let choseong_to_compat = build_choseong_to_compat();
    let jungseong_to_compat = build_jungseong_to_compat();
    let jongseong_to_compat = build_jongseong_to_compat();

    let mut result = String::new();

    // Lead → compat → QWERTY
    if let Some(&compat) = choseong_to_compat.get(&lead_idx) {
        if let Some(&q) = jamo_to_qwerty.get(&compat) { result.push(q); }
    }

    // Vowel → compat → QWERTY
    if let Some(&compat) = jungseong_to_compat.get(&vowel_idx) {
        if let Some(&q) = jamo_to_qwerty.get(&compat) { result.push(q); }
    }

    // Trail → compat → QWERTY
    if trail_idx > 0 {
        if let Some(&compat) = jongseong_to_compat.get(&trail_idx) {
            if let Some(&q) = jamo_to_qwerty.get(&compat) { result.push(q); }
        }
    }

    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_single_consonant() {
        let res = HangulEngine::convert("r", false);
        assert_eq!(res.text, "\u{3131}"); // ㄱ
    }

    #[test]
    fn test_simple_syllable() {
        let res = HangulEngine::convert("rk", false);
        assert_eq!(res.text, "\u{AC00}"); // 가
    }

    #[test]
    fn test_closed_syllable() {
        let res = HangulEngine::convert("rkr", false);
        assert_eq!(res.text, "\u{AC01}"); // 각
    }

    #[test]
    fn test_two_standalone_consonants() {
        let res = HangulEngine::convert("rr", false);
        assert_eq!(res.text, "\u{3131}\u{3131}"); // ㄱㄱ
    }

    #[test]
    fn test_reverse_ga() {
        let res = HangulEngine::convert("\u{AC00}", false);
        assert_eq!(res.text, "rk");
    }

    #[test]
    fn test_reverse_gak() {
        let res = HangulEngine::convert("\u{AC01}", false);
        assert_eq!(res.text, "rkr");
    }

    #[test]
    fn test_roundtrip() {
        let fwd = HangulEngine::convert("rk", false);
        let rev = HangulEngine::convert(&fwd.text, false);
        assert_eq!(rev.text, "rk");
    }

    #[test]
    fn test_ime_active() {
        let res = HangulEngine::convert("rk", true);
        assert!(res.escape_required);
        assert!(res.success);
    }
}
