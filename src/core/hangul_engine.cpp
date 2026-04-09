/**
 * ORKA Hangul Engine — §7.2: EN ↔ KO (Dubeolsik 두벌식)
 *
 * Implements the standard Korean 2-set (Dubeolsik) keyboard layout.
 * QWERTY keys map to Hangul Jamo, which are then composed into
 * syllable blocks using the Unicode Hangul Syllables algorithm:
 *
 *   Syllable = (Lead × 21 + Vowel) × 28 + Trail + 0xAC00
 *
 * Per §7.2.1 IME State Machine:
 *   - If IME inactive  → convert immediately
 *   - If IME active + unfinished syllable → escapeRequired=true
 *   - If IME active + finished syllable   → convert immediately
 */

#include "hangul_engine.h"
#include <unordered_map>
#include <cwctype>

namespace orka {

namespace {

// ── Dubeolsik QWERTY → Jamo tables ────────────────────────────────

// Leading consonants (초성 choseong) — index in Unicode block
// ㄱ ㄲ ㄴ ㄷ ㄸ ㄹ ㅁ ㅂ ㅃ ㅅ ㅆ ㅇ ㅈ ㅉ ㅊ ㅋ ㅌ ㅍ ㅎ
const wchar_t CHOSEONG_BASE = 0x1100;
const wchar_t JUNGSEONG_BASE = 0x1161;
const wchar_t JONGSEONG_BASE = 0x11A8;

// QWERTY key → Compatibility Jamo (for display of standalone consonants)
const std::unordered_map<wchar_t, wchar_t> kQwertyToCompat = {
    // Consonants
    {L'r', L'\u3131'}, // ㄱ
    {L'R', L'\u3132'}, // ㄲ
    {L's', L'\u3134'}, // ㄴ
    {L'e', L'\u3137'}, // ㄷ
    {L'E', L'\u3138'}, // ㄸ
    {L'f', L'\u3139'}, // ㄹ
    {L'a', L'\u3141'}, // ㅁ
    {L'q', L'\u3142'}, // ㅂ
    {L'Q', L'\u3143'}, // ㅃ
    {L't', L'\u3145'}, // ㅅ
    {L'T', L'\u3146'}, // ㅆ
    {L'd', L'\u3147'}, // ㅇ
    {L'w', L'\u3148'}, // ㅈ
    {L'W', L'\u3149'}, // ㅉ
    {L'c', L'\u314A'}, // ㅊ
    {L'z', L'\u314B'}, // ㅋ
    {L'x', L'\u314C'}, // ㅌ
    {L'v', L'\u314D'}, // ㅍ
    {L'g', L'\u314E'}, // ㅎ
    // Vowels
    {L'k', L'\u314F'}, // ㅏ
    {L'o', L'\u3150'}, // ㅐ
    {L'i', L'\u3151'}, // ㅑ
    {L'O', L'\u3152'}, // ㅒ
    {L'j', L'\u3153'}, // ㅓ
    {L'p', L'\u3154'}, // ㅔ
    {L'u', L'\u3155'}, // ㅕ
    {L'P', L'\u3156'}, // ㅖ
    {L'h', L'\u3157'}, // ㅗ
    {L'y', L'\u315B'}, // ㅛ
    {L'n', L'\u315C'}, // ㅜ
    {L'b', L'\u3160'}, // ㅠ
    {L'l', L'\u3161'}, // ㅡ
    {L'm', L'\u3163'}, // ㅣ
};

// Choseong index from compatibility Jamo
const std::unordered_map<wchar_t, int> kCompatToChoseong = {
    {L'\u3131', 0},  // ㄱ
    {L'\u3132', 1},  // ㄲ
    {L'\u3134', 2},  // ㄴ
    {L'\u3137', 3},  // ㄷ
    {L'\u3138', 4},  // ㄸ
    {L'\u3139', 5},  // ㄹ
    {L'\u3141', 6},  // ㅁ
    {L'\u3142', 7},  // ㅂ
    {L'\u3143', 8},  // ㅃ
    {L'\u3145', 9},  // ㅅ
    {L'\u3146', 10}, // ㅆ
    {L'\u3147', 11}, // ㅇ
    {L'\u3148', 12}, // ㅈ
    {L'\u3149', 13}, // ㅉ
    {L'\u314A', 14}, // ㅊ
    {L'\u314B', 15}, // ㅋ
    {L'\u314C', 16}, // ㅌ
    {L'\u314D', 17}, // ㅍ
    {L'\u314E', 18}, // ㅎ
};

// Jungseong index from compatibility Jamo
const std::unordered_map<wchar_t, int> kCompatToJungseong = {
    {L'\u314F', 0},  // ㅏ
    {L'\u3150', 1},  // ㅐ
    {L'\u3151', 2},  // ㅑ
    {L'\u3152', 3},  // ㅒ
    {L'\u3153', 4},  // ㅓ
    {L'\u3154', 5},  // ㅔ
    {L'\u3155', 6},  // ㅕ
    {L'\u3156', 7},  // ㅖ
    {L'\u3157', 8},  // ㅗ
    {L'\u3158', 9},  // ㅘ
    {L'\u3159', 10}, // ㅙ
    {L'\u315A', 11}, // ㅚ
    {L'\u315B', 12}, // ㅛ
    {L'\u315C', 13}, // ㅜ
    {L'\u315D', 14}, // ㅝ
    {L'\u315E', 15}, // ㅞ
    {L'\u315F', 16}, // ㅟ
    {L'\u3160', 17}, // ㅠ
    {L'\u3161', 18}, // ㅡ
    {L'\u3162', 19}, // ㅢ
    {L'\u3163', 20}, // ㅣ
};

// Jongseong index from compatibility Jamo (0 = no trailing)
const std::unordered_map<wchar_t, int> kCompatToJongseong = {
    {L'\u3131', 1},  // ㄱ
    {L'\u3132', 2},  // ㄲ
    {L'\u3134', 4},  // ㄴ
    {L'\u3137', 7},  // ㄷ
    {L'\u3139', 8},  // ㄹ
    {L'\u3141', 16}, // ㅁ
    {L'\u3142', 17}, // ㅂ
    {L'\u3145', 19}, // ㅅ
    {L'\u3146', 20}, // ㅆ
    {L'\u3147', 21}, // ㅇ
    {L'\u3148', 22}, // ㅈ
    {L'\u314A', 23}, // ㅊ
    {L'\u314B', 24}, // ㅋ
    {L'\u314C', 25}, // ㅌ
    {L'\u314D', 26}, // ㅍ
    {L'\u314E', 27}, // ㅎ
};

// Reverse Jongseong index → compatibility Jamo
const std::unordered_map<int, wchar_t> kJongseongToCompat = []() {
    std::unordered_map<int, wchar_t> rev;
    for (auto& [k, v] : kCompatToJongseong) rev[v] = k;
    return rev;
}();

bool isHangulSyllable(wchar_t ch) {
    return ch >= 0xAC00 && ch <= 0xD7A3;
}

bool isHangulJamo(wchar_t ch) {
    return (ch >= 0x3131 && ch <= 0x3163);
}

bool isConsonant(wchar_t compat) {
    return kCompatToChoseong.count(compat) > 0;
}

bool isVowel(wchar_t compat) {
    return kCompatToJungseong.count(compat) > 0;
}

// QWERTY key → Jamo reverse map (for decomposing Hangul → QWERTY)
std::unordered_map<wchar_t, wchar_t> buildJamoToQwerty() {
    std::unordered_map<wchar_t, wchar_t> rev;
    for (auto& [q, j] : kQwertyToCompat) rev[j] = q;
    return rev;
}
const std::unordered_map<wchar_t, wchar_t> kJamoToQwerty = buildJamoToQwerty();

// Choseong index → compat Jamo
const std::unordered_map<int, wchar_t> kChoseongToCompat = []() {
    std::unordered_map<int, wchar_t> rev;
    for (auto& [k, v] : kCompatToChoseong) rev[v] = k;
    return rev;
}();

// Jungseong index → compat Jamo
const std::unordered_map<int, wchar_t> kJungseongToCompat = []() {
    std::unordered_map<int, wchar_t> rev;
    for (auto& [k, v] : kCompatToJungseong) rev[v] = k;
    return rev;
}();

} // anonymous namespace


// ════════════════════════════════════════════════════════════════════
// Direction detection
// ════════════════════════════════════════════════════════════════════

Direction HangulEngine::detectDirection(const std::wstring& input) {
    int latin = 0, hangul = 0;
    for (wchar_t ch : input) {
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z')) ++latin;
        if (isHangulSyllable(ch) || isHangulJamo(ch)) ++hangul;
    }
    if (latin > hangul) return Direction::FORWARD;
    if (hangul > latin) return Direction::REVERSE;
    return Direction::UNKNOWN;
}


// ════════════════════════════════════════════════════════════════════
// EN → KO conversion (QWERTY → Hangul syllable composition)
// ════════════════════════════════════════════════════════════════════

ConversionResult HangulEngine::convert(const std::wstring& input, bool imeActive) {
    ConversionResult result;
    result.escapeRequired = false;

    Direction dir = detectDirection(input);

    if (dir == Direction::REVERSE) {
        // KO → EN: decompose Hangul syllables back to QWERTY
        std::wstring out;
        for (wchar_t ch : input) {
            if (isHangulSyllable(ch)) {
                out += decomposeSyllableToQwerty(ch);
            } else if (isHangulJamo(ch)) {
                auto it = kJamoToQwerty.find(ch);
                if (it != kJamoToQwerty.end())
                    out.push_back(it->second);
                else
                    out.push_back(ch);
            } else {
                out.push_back(ch);
            }
        }
        result.text = out;
        result.success = true;
        return result;
    }

    // EN → KO: compose QWERTY keys into Hangul
    if (imeActive) {
        // §7.2.1: IME active + unfinished syllable → need VK_ESCAPE first
        result.escapeRequired = true;
    }

    std::wstring out;
    enum State { EMPTY, LEAD, LEAD_VOWEL, LEAD_VOWEL_TRAIL };
    State state = EMPTY;
    wchar_t curLead = 0, curVowel = 0, curTrail = 0;

    auto flushSyllable = [&]() {
        if (state == LEAD) {
            // Just a consonant, no vowel → output as standalone Jamo
            out.push_back(curLead);
        } else if (state == LEAD_VOWEL) {
            auto li = kCompatToChoseong.find(curLead);
            auto vi = kCompatToJungseong.find(curVowel);
            if (li != kCompatToChoseong.end() && vi != kCompatToJungseong.end()) {
                wchar_t syl = 0xAC00 + (li->second * 21 + vi->second) * 28;
                out.push_back(syl);
            } else {
                out.push_back(curLead);
                out.push_back(curVowel);
            }
        } else if (state == LEAD_VOWEL_TRAIL) {
            auto li = kCompatToChoseong.find(curLead);
            auto vi = kCompatToJungseong.find(curVowel);
            auto ti = kCompatToJongseong.find(curTrail);
            if (li != kCompatToChoseong.end() && vi != kCompatToJungseong.end()
                && ti != kCompatToJongseong.end()) {
                wchar_t syl = 0xAC00 + (li->second * 21 + vi->second) * 28 + ti->second;
                out.push_back(syl);
            } else {
                out.push_back(curLead);
                out.push_back(curVowel);
                out.push_back(curTrail);
            }
        }
        state = EMPTY;
        curLead = curVowel = curTrail = 0;
    };

    for (wchar_t ch : input) {
        auto it = kQwertyToCompat.find(ch);
        if (it == kQwertyToCompat.end()) {
            // Not a Dubeolsik key → flush and pass-through
            flushSyllable();
            out.push_back(ch);
            continue;
        }

        wchar_t jamo = it->second;

        switch (state) {
        case EMPTY:
            if (isConsonant(jamo)) {
                curLead = jamo;
                state = LEAD;
            } else if (isVowel(jamo)) {
                // Standalone vowel
                out.push_back(jamo);
            }
            break;

        case LEAD:
            if (isVowel(jamo)) {
                curVowel = jamo;
                state = LEAD_VOWEL;
            } else {
                // Two consonants without vowel → flush first, start new
                flushSyllable();
                curLead = jamo;
                state = LEAD;
            }
            break;

        case LEAD_VOWEL:
            if (isConsonant(jamo)) {
                // Could be trailing consonant of this syllable
                curTrail = jamo;
                state = LEAD_VOWEL_TRAIL;
            } else if (isVowel(jamo)) {
                // New vowel → flush current syllable, output standalone vowel
                flushSyllable();
                out.push_back(jamo);
            }
            break;

        case LEAD_VOWEL_TRAIL:
            if (isVowel(jamo)) {
                // The trailing consonant was actually the leading of next syllable
                // Re-emit current syllable without trailing, start new with trail as lead
                wchar_t savedTrail = curTrail;
                curTrail = 0;
                state = LEAD_VOWEL;
                flushSyllable();
                curLead = savedTrail;
                curVowel = jamo;
                state = LEAD_VOWEL;
            } else if (isConsonant(jamo)) {
                // Flush complete syllable, start new with this consonant
                flushSyllable();
                curLead = jamo;
                state = LEAD;
            }
            break;
        }
    }
    flushSyllable();

    result.text = out;
    result.success = true;
    return result;
}


// ════════════════════════════════════════════════════════════════════
// KO → EN decomposition
// ════════════════════════════════════════════════════════════════════

std::wstring HangulEngine::decomposeSyllableToQwerty(wchar_t syllable) {
    if (!isHangulSyllable(syllable)) return { syllable };

    int code = syllable - 0xAC00;
    int trailIdx = code % 28;
    code /= 28;
    int vowelIdx = code % 21;
    int leadIdx = code / 21;

    std::wstring result;

    // Lead → compat → QWERTY
    auto li = kChoseongToCompat.find(leadIdx);
    if (li != kChoseongToCompat.end()) {
        auto qi = kJamoToQwerty.find(li->second);
        if (qi != kJamoToQwerty.end()) result.push_back(qi->second);
    }

    // Vowel → compat → QWERTY
    auto vi = kJungseongToCompat.find(vowelIdx);
    if (vi != kJungseongToCompat.end()) {
        auto qi = kJamoToQwerty.find(vi->second);
        if (qi != kJamoToQwerty.end()) result.push_back(qi->second);
    }

    // Trail → compat → QWERTY (only if trail ≠ 0)
    if (trailIdx > 0) {
        auto ti = kJongseongToCompat.find(trailIdx);
        if (ti != kJongseongToCompat.end()) {
            auto qi = kJamoToQwerty.find(ti->second);
            if (qi != kJamoToQwerty.end()) result.push_back(qi->second);
        }
    }

    return result;
}

// Unused helper stubs for interface compliance
wchar_t HangulEngine::qwertyToJamo(wchar_t ch) {
    auto it = kQwertyToCompat.find(ch);
    return (it != kQwertyToCompat.end()) ? it->second : ch;
}

bool HangulEngine::isLeadingJamo(wchar_t ch) { return isConsonant(ch); }
bool HangulEngine::isVowelJamo(wchar_t ch) { return isVowel(ch); }
bool HangulEngine::isTrailingJamo(wchar_t ch) { return kCompatToJongseong.count(ch) > 0; }

wchar_t HangulEngine::composeHangulSyllable(wchar_t lead, wchar_t vowel, wchar_t trail) {
    auto li = kCompatToChoseong.find(lead);
    auto vi = kCompatToJungseong.find(vowel);
    if (li == kCompatToChoseong.end() || vi == kCompatToJungseong.end()) return 0;
    int t = 0;
    if (trail) {
        auto ti = kCompatToJongseong.find(trail);
        if (ti != kCompatToJongseong.end()) t = ti->second;
    }
    return 0xAC00 + (li->second * 21 + vi->second) * 28 + t;
}

} // namespace orka
