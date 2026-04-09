/**
 * ORKA Hebrew Engine — §7.3: EN ↔ HE (Hebrew + BiDi)
 *
 * Standard QWERTY → Hebrew keyboard mapping.
 * Per §7.3.1:
 *   - EN → HE: Nikud not generated (not on QWERTY)
 *   - HE → EN (no Nikud): standard reverse mapping
 *   - HE → EN (with Nikud): strip Nikud, set nikudStripped=true
 *     so UI can show mandatory warning before action
 *
 * BiDi handling: Raw Bidi markers (U+202B/U+202C) are NOT inserted
 * into fields without Bidi support (per §10.3 test case).
 */

#include "hebrew_engine.h"
#include <unordered_map>
#include <cwctype>

namespace orka {

namespace {

// ── Standard QWERTY → Hebrew mapping ──────────────────────────────
const std::unordered_map<wchar_t, wchar_t> kEnToHe = {
    {L't', L'\u05D0'}, // א (alef)
    {L'c', L'\u05D1'}, // ב (bet)
    {L'd', L'\u05D2'}, // ג (gimel)
    {L's', L'\u05D3'}, // ד (dalet)
    {L'v', L'\u05D4'}, // ה (he)
    {L'u', L'\u05D5'}, // ו (vav)
    {L'z', L'\u05D6'}, // ז (zayin)
    {L'j', L'\u05D7'}, // ח (het)
    {L'y', L'\u05D8'}, // ט (tet)
    {L'h', L'\u05D9'}, // י (yod)
    {L'l', L'\u05DA'}, // ך (final kaf)
    {L'f', L'\u05DB'}, // כ (kaf)
    {L'k', L'\u05DC'}, // ל (lamed)
    {L'o', L'\u05DD'}, // ם (final mem)
    {L'n', L'\u05DE'}, // מ (mem)
    {L'i', L'\u05DF'}, // ן (final nun)
    {L'b', L'\u05E0'}, // נ (nun)
    {L'x', L'\u05E1'}, // ס (samekh)
    {L'g', L'\u05E2'}, // ע (ayin)
    {L';', L'\u05E3'}, // ף (final pe)
    {L'p', L'\u05E4'}, // פ (pe)
    {L'.', L'\u05E5'}, // ץ (final tsadi)
    {L'm', L'\u05E6'}, // צ (tsadi)
    {L'e', L'\u05E7'}, // ק (qof)
    {L'r', L'\u05E8'}, // ר (resh)
    {L'a', L'\u05E9'}, // ש (shin)
    {L',', L'\u05EA'}, // ת (tav)
    // Punctuation pass-through handled separately
    {L'/', L'.'}, 
    {L'w', L','},
    {L'\'',L','},
};

// Build reverse map HE → EN
std::unordered_map<wchar_t, wchar_t> buildReverse() {
    std::unordered_map<wchar_t, wchar_t> rev;
    for (auto& [en, he] : kEnToHe) {
        // Only map Hebrew letters (avoid overwriting with punctuation mappings)
        if (he >= 0x05D0 && he <= 0x05EA) {
            rev[he] = en;
        }
    }
    return rev;
}
const std::unordered_map<wchar_t, wchar_t> kHeToEn = buildReverse();

bool isHebrewLetter(wchar_t ch) {
    return ch >= 0x05D0 && ch <= 0x05EA;
}

bool isNikud(wchar_t ch) {
    return ch >= 0x05B0 && ch <= 0x05C7;
}

bool isLatinLetter(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
}

} // anonymous namespace


// ════════════════════════════════════════════════════════════════════

Direction HebrewEngine::detectDirection(const std::wstring& input) {
    int latin = 0, hebrew = 0;
    for (wchar_t ch : input) {
        if (isLatinLetter(ch))  ++latin;
        if (isHebrewLetter(ch)) ++hebrew;
    }
    if (latin > hebrew)  return Direction::FORWARD;
    if (hebrew > latin)  return Direction::REVERSE;
    return Direction::UNKNOWN;
}

bool HebrewEngine::containsNikud(const std::wstring& input) {
    for (wchar_t ch : input) {
        if (isNikud(ch)) return true;
    }
    return false;
}

std::wstring HebrewEngine::stripNikud(const std::wstring& input) {
    std::wstring result;
    result.reserve(input.size());
    for (wchar_t ch : input) {
        if (!isNikud(ch)) result.push_back(ch);
    }
    return result;
}

ConversionResult HebrewEngine::convert(const std::wstring& input) {
    ConversionResult result;
    result.nikudStripped = false;
    result.escapeRequired = false;

    Direction dir = detectDirection(input);

    if (dir == Direction::FORWARD || dir == Direction::UNKNOWN) {
        // EN → HE
        std::wstring out;
        out.reserve(input.size());
        for (wchar_t ch : input) {
            wchar_t lower = std::towlower(ch);
            auto it = kEnToHe.find(lower);
            if (it != kEnToHe.end()) {
                out.push_back(it->second);
            } else {
                // Pass-through: digits, spaces, punctuation not in map
                out.push_back(ch);
            }
        }
        result.text = out;
        result.success = true;
    } else {
        // HE → EN
        // §7.3.1: Check for Nikud — lossy conversion warning
        if (containsNikud(input)) {
            result.nikudStripped = true;
            // Strip Nikud before conversion
        }

        std::wstring cleaned = result.nikudStripped ? stripNikud(input) : input;
        std::wstring out;
        out.reserve(cleaned.size());

        for (wchar_t ch : cleaned) {
            auto it = kHeToEn.find(ch);
            if (it != kHeToEn.end()) {
                out.push_back(it->second);
            } else {
                out.push_back(ch);
            }
        }
        result.text = out;
        result.success = true;
    }

    return result;
}

} // namespace orka
