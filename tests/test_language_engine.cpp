/**
 * ORKA Unit Tests — §10 Obligatory Test Cases
 *
 * Tests all three language engines against the exact scenarios
 * defined in the technical specification v2.5, §10.1–10.3.
 */

#include "core/language_engine.h"
#include "core/hangul_engine.h"
#include "core/hebrew_engine.h"
#include "core/utf8_utils.h"

#include <iostream>
#include <string>
#include <vector>
#include <clocale>

// ── Simple test framework ─────────────────────────────────────────
static int g_passed = 0;
static int g_failed = 0;

std::string toUtf8(const std::wstring& w) {
    try { return orka::util::wideToUtf8(w); }
    catch (...) { return "[conversion error]"; }
}

void ASSERT_EQ(const std::wstring& expected, const std::wstring& actual, const char* testName) {
    if (expected == actual) {
        std::cout << "  ✅ " << testName << "\n";
        ++g_passed;
    } else {
        std::cout << "  ❌ " << testName << "\n"
                  << "     Expected: " << toUtf8(expected) << "\n"
                  << "     Actual:   " << toUtf8(actual) << "\n";
        ++g_failed;
    }
}

void ASSERT_TRUE(bool cond, const char* testName) {
    if (cond) {
        std::cout << "  ✅ " << testName << "\n";
        ++g_passed;
    } else {
        std::cout << "  ❌ " << testName << "\n";
        ++g_failed;
    }
}

void ASSERT_FALSE(bool cond, const char* testName) {
    ASSERT_TRUE(!cond, testName);
}


// ════════════════════════════════════════════════════════════════════
// §10.1  EN ↔ UK Test Cases
// ════════════════════════════════════════════════════════════════════

void testEnUk() {
    std::cout << "\n═══ §10.1: EN ↔ UK ═══\n";
    orka::LanguageEngine engine;

    // ── Keyboard layout mapping (QWERTY position → Ukrainian) ─────
    // These are exact expected values based on §7.1 mapping table:
    // q→й, w→ц, e→у, r→к, t→е, y→н, u→г, i→ш, o→щ, p→з
    // a→ф, s→і, d→в, f→а, g→п, h→р, j→о, k→л, l→д
    // z→я, x→ч, c→с, v→м, b→и, n→т, m→ь

    // "Hello" → H=Р, e=у, l=д, l=д, o=щ → "Руддщ"
    {
        auto res = engine.convert(L"Hello", orka::LanguagePair::EN_UK);
        ASSERT_TRUE(res.success, "EN→UK: Hello conversion succeeds");
        ASSERT_EQ(L"\u0420\u0443\u0434\u0434\u0449", res.text, "EN→UK: Hello → Руддщ");
    }

    // "test" → t=е, e=у, s=і, t=е → "уну" wait: t=е, e=у, s=і, t=е → "еуіе"
    {
        auto res = engine.convert(L"test", orka::LanguagePair::EN_UK);
        ASSERT_EQ(L"\u0435\u0443\u0456\u0435", res.text, "EN→UK: test → еуіе");
    }

    // Roundtrip: EN→UK→EN must return original
    {
        auto fwd = engine.convert(L"test", orka::LanguagePair::EN_UK);
        auto rev = engine.convert(fwd.text, orka::LanguagePair::EN_UK);
        ASSERT_EQ(L"test", rev.text, "EN→UK→EN roundtrip: test");
    }

    // ── Direction detection ───────────────────────────────────────
    {
        auto dir = engine.detectDirection(L"Hello", orka::LanguagePair::EN_UK);
        ASSERT_TRUE(dir == orka::Direction::FORWARD, "Direction: Latin → FORWARD");
    }
    {
        // Привіт (Ukr.)
        auto dir = engine.detectDirection(L"\u041F\u0440\u0438\u0432\u0456\u0442", orka::LanguagePair::EN_UK);
        ASSERT_TRUE(dir == orka::Direction::REVERSE, "Direction: Cyrillic → REVERSE");
    }

    // ── §10.1: HELLO_WORLD — ALL_CAPS + underscore passthrough ───
    {
        auto res = engine.convert(L"HELLO_WORLD", orka::LanguagePair::EN_UK);
        ASSERT_TRUE(res.success, "ALL_CAPS conversion succeeds");
        ASSERT_TRUE(res.text.find(L'_') != std::wstring::npos,
                    "Underscore preserved in ALL_CAPS");
        // H=Р, E=У, L=Д, L=Д, O=Щ, _=_, W=Ц, O=Щ, R=К, L=Д, D=В
        ASSERT_EQ(L"\u0420\u0423\u0414\u0414\u0429_\u0426\u0429\u041A\u0414\u0412",
                  res.text, "EN→UK: HELLO_WORLD → РУДДЩ_ЦЩКДВ");
    }

    // ── §10.1: Hello, World! — punctuation preservation ──────────
    {
        auto res = engine.convert(L"Hello, World!", orka::LanguagePair::EN_UK);
        ASSERT_TRUE(res.success, "Punctuation conversion succeeds");
        // comma maps to б in UK layout, but ',' in EN maps to 'б'
        // No — per mapping table: {L',', L'\u0431'} — comma → б
        // '!' is not in the mapping table → pass-through
        ASSERT_TRUE(res.text.find(L'!') != std::wstring::npos,
                    "Exclamation mark preserved");
    }

    // ── §10.1: Typographic quotes ────────────────────────────────
    {
        // \u201C = ", \u201D = ", should map to « »
        std::wstring input = L"\u201CHello\u201D";
        auto res = engine.convert(input, orka::LanguagePair::EN_UK);
        ASSERT_TRUE(res.success, "Typographic quotes conversion succeeds");
        ASSERT_TRUE(res.text.front() == L'\u00AB', "Left quote \" → «");
        ASSERT_TRUE(res.text.back() == L'\u00BB', "Right quote \" → »");
    }

    // ── Reverse: « » → " " ──────────────────────────────────────
    {
        std::wstring input = L"\u00AB\u041F\u0440\u0438\u0432\u0456\u0442\u00BB";
        auto res = engine.convert(input, orka::LanguagePair::EN_UK);
        ASSERT_TRUE(res.success, "Reverse typographic quotes conversion");
        ASSERT_TRUE(res.text.front() == L'\u201C', "« → left \"");
        ASSERT_TRUE(res.text.back() == L'\u201D', "» → right \"");
    }

    // ── §10.1 Edge cases ─────────────────────────────────────────
    {
        auto res = engine.convert(L"", orka::LanguagePair::EN_UK);
        ASSERT_FALSE(res.success, "Empty string → no conversion (panel hidden)");
    }
    {
        auto res = engine.convert(L"   \t\n", orka::LanguagePair::EN_UK);
        ASSERT_FALSE(res.success, "Whitespace-only → no conversion (panel hidden)");
    }
    {
        auto res = engine.convert(L"12345", orka::LanguagePair::EN_UK);
        ASSERT_FALSE(res.success, "Digits-only → no conversion (panel hidden)");
    }

    // ── Digits pass-through within mixed text ────────────────────
    {
        auto res = engine.convert(L"test123", orka::LanguagePair::EN_UK);
        ASSERT_TRUE(res.text.find(L'1') != std::wstring::npos, "Digits pass-through in mixed text");
        ASSERT_TRUE(res.text.find(L'2') != std::wstring::npos, "Digits pass-through (2)");
        ASSERT_TRUE(res.text.find(L'3') != std::wstring::npos, "Digits pass-through (3)");
    }

    // ── Special keys mapping ─────────────────────────────────────
    {
        // Backtick → ґ
        auto res = engine.convert(L"`", orka::LanguagePair::EN_UK);
        ASSERT_EQ(L"\u0491", res.text, "Backtick → ґ");
    }
    {
        // Square brackets → х, ї
        auto res = engine.convert(L"[]", orka::LanguagePair::EN_UK);
        ASSERT_EQ(L"\u0445\u0457", res.text, "[] → хї");
    }
}


// ════════════════════════════════════════════════════════════════════
// §10.2  EN ↔ KO Test Cases
// ════════════════════════════════════════════════════════════════════

void testEnKo() {
    std::cout << "\n═══ §10.2: EN ↔ KO (Hangul) ═══\n";

    // §10.2: "r" → ㄱ (single jamo, consonant without vowel)
    {
        auto res = orka::HangulEngine::convert(L"r", false);
        ASSERT_EQ(L"\u3131", res.text, "r → ㄱ (single consonant)");
    }

    // §10.2: "rk" → 가 (leading ㄱ + vowel ㅏ = complete syllable)
    {
        auto res = orka::HangulEngine::convert(L"rk", false);
        ASSERT_EQ(L"\uAC00", res.text, "rk → 가 (leading+vowel)");
    }

    // §10.2: "rtk" — r=ㄱ, t=ㅅ(consonant), k=ㅏ(vowel)
    // ㄱ flushes standalone, then ㅅ+ㅏ = 사
    {
        auto res = orka::HangulEngine::convert(L"rtk", false);
        ASSERT_TRUE(res.success, "rtk conversion succeeds");
        // Expected: ㄱ사 (ㄱ standalone + 사 syllable)
        std::wstring expected = L"\u3131\uC0AC";
        ASSERT_EQ(expected, res.text, "rtk → ㄱ사");
    }

    // Closed syllable: 각 = ㄱ+ㅏ+ㄱ → input "rkr"
    // r=ㄱ(lead), k=ㅏ(vowel), r=ㄱ(trail) → 각
    {
        auto res = orka::HangulEngine::convert(L"rkr", false);
        // 각 = 0xAC00 + (0*21+0)*28+1 = 0xAC01
        ASSERT_EQ(L"\uAC01", res.text, "rkr → 각 (closed syllable)");
    }

    // Two consonants without vowel → two standalone jamo
    {
        auto res = orka::HangulEngine::convert(L"rr", false);
        ASSERT_EQ(L"\u3131\u3131", res.text, "rr → ㄱㄱ (two standalone)");
    }

    // Mixed EN+KO: non-Dubeolsik chars pass-through
    {
        auto res = orka::HangulEngine::convert(L"Hello ", false);
        ASSERT_TRUE(res.success, "Mixed EN+KO starts processing");
        // H/e/l/l/o are NOT in Dubeolsik → pass-through
        ASSERT_TRUE(res.text.find(L'H') != std::wstring::npos,
                    "Non-Dubeolsik chars pass-through (H)");
    }

    // §10.2: IME active → escapeRequired=true
    {
        auto res = orka::HangulEngine::convert(L"rk", true);
        ASSERT_TRUE(res.escapeRequired, "IME active → escapeRequired=true");
        ASSERT_TRUE(res.success, "IME active: conversion still succeeds");
    }

    // Reverse: 가 → "rk" (KO → EN)
    {
        auto res = orka::HangulEngine::convert(L"\uAC00", false);
        ASSERT_EQ(L"rk", res.text, "가 → rk (KO→EN reverse)");
    }

    // Reverse: 각 → "rkr"
    {
        auto res = orka::HangulEngine::convert(L"\uAC01", false);
        ASSERT_EQ(L"rkr", res.text, "각 → rkr (closed syllable reverse)");
    }

    // Roundtrip: rk → 가 → rk
    {
        auto fwd = orka::HangulEngine::convert(L"rk", false);
        auto rev = orka::HangulEngine::convert(fwd.text, false);
        ASSERT_EQ(L"rk", rev.text, "EN→KO→EN roundtrip: rk");
    }
}


// ════════════════════════════════════════════════════════════════════
// §10.3  EN ↔ HE Test Cases
// ════════════════════════════════════════════════════════════════════

void testEnHe() {
    std::cout << "\n═══ §10.3: EN ↔ HE (Hebrew) ═══\n";

    // Direction detection
    {
        auto dir = orka::HebrewEngine::detectDirection(L"hello");
        ASSERT_TRUE(dir == orka::Direction::FORWARD, "Direction: Latin → FORWARD");
    }
    {
        // שלום
        auto dir = orka::HebrewEngine::detectDirection(L"\u05E9\u05DC\u05D5\u05DD");
        ASSERT_TRUE(dir == orka::Direction::REVERSE, "Direction: Hebrew → REVERSE");
    }

    // EN → HE: basic mapping
    // a→ש, k→ל, u→ו, n→מ
    {
        auto res = orka::HebrewEngine::convert(L"a");
        ASSERT_TRUE(res.success, "EN→HE: single letter succeeds");
        ASSERT_EQ(L"\u05E9", res.text, "a → ש (shin)");
    }
    {
        auto res = orka::HebrewEngine::convert(L"t");
        ASSERT_EQ(L"\u05D0", res.text, "t → א (alef)");
    }

    // §10.3: "akln" — maps per QWERTY Hebrew layout
    // a→ש, k→ל, l→ך, n→מ → שלךמ
    {
        auto res = orka::HebrewEngine::convert(L"akln");
        ASSERT_TRUE(res.success, "EN→HE: akln conversion succeeds");
        ASSERT_EQ(L"\u05E9\u05DC\u05DA\u05DE", res.text, "akln → שלךמ");
    }

    // HE → EN: standard reverse mapping (no Nikud)
    {
        auto res = orka::HebrewEngine::convert(L"\u05E9");  // ש
        ASSERT_TRUE(res.success, "HE→EN: single letter succeeds");
        ASSERT_FALSE(res.nikudStripped, "No Nikud → nikudStripped=false");
        ASSERT_EQ(L"a", res.text, "ש → a");
    }

    // §10.3: HE → EN with Nikud — lossy, nikudStripped=true
    {
        // שָׁלוֹם (shalom with nikud marks)
        std::wstring withNikud = L"\u05E9\u05B8\u05C1\u05DC\u05D5\u05B9\u05DD";
        auto res = orka::HebrewEngine::convert(withNikud);
        ASSERT_TRUE(res.nikudStripped, "Nikud present → nikudStripped=true (lossy warning)");
        ASSERT_TRUE(res.success, "Nikud stripping + conversion succeeds");
    }

    // Nikud detection utility
    {
        ASSERT_TRUE(orka::HebrewEngine::containsNikud(L"\u05E9\u05B8"),
                    "containsNikud: true when nikud present");
        ASSERT_FALSE(orka::HebrewEngine::containsNikud(L"\u05E9\u05DC"),
                     "containsNikud: false when no nikud");
    }

    // Nikud stripping utility
    {
        std::wstring stripped = orka::HebrewEngine::stripNikud(L"\u05E9\u05B8\u05DC");
        ASSERT_EQ(L"\u05E9\u05DC", stripped, "stripNikud removes diacritics");
    }

    // Roundtrip EN→HE→EN
    {
        auto fwd = orka::HebrewEngine::convert(L"test");
        auto rev = orka::HebrewEngine::convert(fwd.text);
        ASSERT_EQ(L"test", rev.text, "EN→HE→EN roundtrip: test");
    }

    // Case insensitivity: uppercase maps same as lowercase
    {
        auto lower = orka::HebrewEngine::convert(L"a");
        auto upper = orka::HebrewEngine::convert(L"A");
        ASSERT_EQ(lower.text, upper.text, "Case insensitive: a == A → same Hebrew");
    }
}


// ════════════════════════════════════════════════════════════════════
// UTF-8 Utility Tests
// ════════════════════════════════════════════════════════════════════

void testUtf8Utils() {
    std::cout << "\n═══ UTF-8 Utility Tests ═══\n";

    // ASCII roundtrip
    {
        std::string utf8 = "Hello";
        std::wstring wide = orka::util::utf8ToWide(utf8);
        std::string back = orka::util::wideToUtf8(wide);
        ASSERT_TRUE(back == utf8, "UTF-8 ASCII roundtrip");
    }

    // Cyrillic roundtrip
    {
        std::string utf8 = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD1\x96\xD1\x82"; // Привіт
        std::wstring wide = orka::util::utf8ToWide(utf8);
        std::string back = orka::util::wideToUtf8(wide);
        ASSERT_TRUE(back == utf8, "UTF-8 Cyrillic roundtrip");
    }

    // Korean roundtrip
    {
        std::wstring hangul = L"\uAC00"; // 가
        std::string utf8 = orka::util::wideToUtf8(hangul);
        std::wstring back = orka::util::utf8ToWide(utf8);
        ASSERT_EQ(hangul, back, "UTF-8 Korean roundtrip");
    }

    // Empty string
    {
        ASSERT_TRUE(orka::util::utf8ToWide("").empty(), "UTF-8 empty → empty wstring");
        ASSERT_TRUE(orka::util::wideToUtf8(L"").empty(), "wstring empty → empty UTF-8");
    }
}


// ════════════════════════════════════════════════════════════════════
// Main test runner
// ════════════════════════════════════════════════════════════════════

int main() {
    std::setlocale(LC_ALL, "");

    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║  ORKA Unit Tests — §10 Test Cases        ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    testEnUk();
    testEnKo();
    testEnHe();
    testUtf8Utils();

    std::cout << "\n══════════════════════════════════════════\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  ✅ Passed: " << g_passed
              << "  ❌ Failed: " << g_failed << "\n";
    std::cout << "══════════════════════════════════════════\n";

    return (g_failed > 0) ? 1 : 0;
}
