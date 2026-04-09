/**
 * ORKA Language Engine — EN ↔ UK / KO / HE conversion core
 *
 * Implements §7.1 (EN↔UK), delegates §7.2 (KO) and §7.3 (HE) to
 * dedicated sub-engines.  All conversion is O(1) per character via
 * hash-map lookup with case / punctuation preservation.
 */

#include "language_engine.h"
#include "hangul_engine.h"
#include "hebrew_engine.h"

#include <unordered_map>
#include <cwctype>

namespace orka {

// ════════════════════════════════════════════════════════════════════
// §7.1  EN ↔ UK  Unicode code-point mapping
// ════════════════════════════════════════════════════════════════════

namespace {

// Full QWERTY → Ukrainian mapping (lower-case)
// Covers standard keyboard layout positions
const std::unordered_map<wchar_t, wchar_t> kEnToUk = {
    // top row
    {L'q', L'\u0439'}, // й
    {L'w', L'\u0446'}, // ц
    {L'e', L'\u0443'}, // у
    {L'r', L'\u043A'}, // к
    {L't', L'\u0435'}, // е
    {L'y', L'\u043D'}, // н
    {L'u', L'\u0433'}, // г
    {L'i', L'\u0448'}, // ш
    {L'o', L'\u0449'}, // щ
    {L'p', L'\u0437'}, // з
    {L'[', L'\u0445'}, // х
    {L']', L'\u0457'}, // ї
    // home row
    {L'a', L'\u0444'}, // ф
    {L's', L'\u0456'}, // і
    {L'd', L'\u0432'}, // в
    {L'f', L'\u0430'}, // а
    {L'g', L'\u043F'}, // п
    {L'h', L'\u0440'}, // р
    {L'j', L'\u043E'}, // о
    {L'k', L'\u043B'}, // л
    {L'l', L'\u0434'}, // д
    {L';', L'\u0436'}, // ж
    {L'\'',L'\u0454'}, // є
    // bottom row
    {L'z', L'\u044F'}, // я
    {L'x', L'\u0447'}, // ч
    {L'c', L'\u0441'}, // с
    {L'v', L'\u043C'}, // м
    {L'b', L'\u0438'}, // и
    {L'n', L'\u0442'}, // т
    {L'm', L'\u044C'}, // ь
    {L',', L'\u0431'}, // б
    {L'.', L'\u044E'}, // ю
    {L'/', L'.'},
    // special: backtick / tilde area
    {L'`', L'\u0491'}, // ґ
};

// Upper-case overrides where EN shift-symbols differ
const std::unordered_map<wchar_t, wchar_t> kEnToUkUpper = {
    {L'~', L'\u0490'}, // Ґ (uppercase)
    {L'{', L'\u0425'}, // Х (uppercase)
    {L'}', L'\u0407'}, // Ї (uppercase)
    {L':', L'\u0416'}, // Ж (uppercase)
    {L'"', L'\u0404'}, // Є (uppercase)
    {L'<', L'\u0411'}, // Б (uppercase)
    {L'>', L'\u042E'}, // Ю (uppercase)
};

// Typographic quotes mapping (§7.1 special rules)
const std::unordered_map<wchar_t, wchar_t> kTypographicQuotes = {
    {L'\u201C', L'\u00AB'}, // " → «
    {L'\u201D', L'\u00BB'}, // " → »
    {L'\u2018', L'\u2018'}, // ' pass-through
    {L'\u2019', L'\u2019'}, // ' pass-through
    {L'\u00AB', L'\u201C'}, // « → "
    {L'\u00BB', L'\u201D'}, // » → "
};

// Build reverse map UK → EN at static init
std::unordered_map<wchar_t, wchar_t> buildReverse() {
    std::unordered_map<wchar_t, wchar_t> rev;
    for (auto& [en, uk] : kEnToUk)  rev[uk] = en;
    return rev;
}
const std::unordered_map<wchar_t, wchar_t> kUkToEn = buildReverse();

std::unordered_map<wchar_t, wchar_t> buildReverseUpper() {
    std::unordered_map<wchar_t, wchar_t> rev;
    for (auto& [en, uk] : kEnToUkUpper)  rev[uk] = en;
    return rev;
}
const std::unordered_map<wchar_t, wchar_t> kUkToEnUpper = buildReverseUpper();

// ── Helpers ────────────────────────────────────────────────────────
bool isUkrainianLetter(wchar_t ch) {
    wchar_t lower = std::towlower(ch);
    // Ukrainian letters: basic Cyrillic + special (і, ї, є, ґ)
    return (lower >= 0x0430 && lower <= 0x044F)  // а-я
        || lower == L'\u0456'   // і
        || lower == L'\u0457'   // ї
        || lower == L'\u0454'   // є
        || lower == L'\u0491'   // ґ
        || lower == L'\u0436'   // ж (already in range but explicit)
        ;
}

bool isLatinLetter(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
}

} // anonymous namespace


// ════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════

LanguageEngine::LanguageEngine()  = default;
LanguageEngine::~LanguageEngine() = default;

void LanguageEngine::setHangulImeActive(bool active) {
    m_hangulImeActive = active;
}

Direction LanguageEngine::detectDirection(const std::wstring& input, LanguagePair pair) {
    switch (pair) {
    case LanguagePair::EN_UK: {
        int latin = 0, cyrillic = 0;
        for (wchar_t ch : input) {
            if (isLatinLetter(ch))     ++latin;
            if (isUkrainianLetter(ch)) ++cyrillic;
        }
        if (latin > 0 && cyrillic == 0)  return Direction::FORWARD;   // EN → UK
        if (cyrillic > 0 && latin == 0)  return Direction::REVERSE;   // UK → EN
        if (latin > cyrillic)            return Direction::FORWARD;
        if (cyrillic > latin)            return Direction::REVERSE;
        return Direction::UNKNOWN;
    }
    case LanguagePair::EN_KO:
        return HangulEngine::detectDirection(input);
    case LanguagePair::EN_HE:
        return HebrewEngine::detectDirection(input);
    default:
        return Direction::UNKNOWN;
    }
}


ConversionResult LanguageEngine::convert(const std::wstring& input, LanguagePair pair) {
    // ── Edge cases per §10.1: empty / whitespace-only / digits-only ──
    if (input.empty()) {
        return { L"", false, false, false, L"Empty selection" };
    }

    bool hasConvertible = false;
    for (wchar_t ch : input) {
        if (!std::iswspace(ch) && !std::iswdigit(ch)) {
            hasConvertible = true;
            break;
        }
    }
    if (!hasConvertible) {
        return { L"", false, false, false, L"Whitespace/digits only" };
    }

    switch (pair) {
    // ── v1.0: EN ↔ UK ──────────────────────────────────────────────
    case LanguagePair::EN_UK: {
        Direction dir = detectDirection(input, pair);
        std::wstring result;
        result.reserve(input.size());

        for (wchar_t ch : input) {
            // Typographic quotes special handling
            auto tq = kTypographicQuotes.find(ch);
            if (tq != kTypographicQuotes.end()) {
                result.push_back(tq->second);
                continue;
            }

            if (dir == Direction::FORWARD || dir == Direction::UNKNOWN) {
                // EN → UK
                bool isUp = std::iswupper(ch);
                wchar_t low = std::towlower(ch);

                // Check upper-case special symbols first
                if (isUp || !std::iswalpha(ch)) {
                    auto itU = kEnToUkUpper.find(ch);
                    if (itU != kEnToUkUpper.end()) {
                        result.push_back(itU->second);
                        continue;
                    }
                }

                auto it = kEnToUk.find(low);
                if (it != kEnToUk.end()) {
                    result.push_back(isUp ? std::towupper(it->second) : it->second);
                } else {
                    result.push_back(ch); // pass-through
                }
            } else {
                // UK → EN
                bool isUp = std::iswupper(ch);
                wchar_t low = std::towlower(ch);

                // Check upper special first
                auto itU = kUkToEnUpper.find(ch);
                if (itU != kUkToEnUpper.end()) {
                    result.push_back(itU->second);
                    continue;
                }

                auto it = kUkToEn.find(low);
                if (it != kUkToEn.end()) {
                    result.push_back(isUp ? std::towupper(it->second) : it->second);
                } else {
                    result.push_back(ch); // pass-through
                }
            }
        }
        return { result, false, false, true, L"" };
    }

    // ── v1.5: EN ↔ KO (Hangul) ────────────────────────────────────
    case LanguagePair::EN_KO: {
        return HangulEngine::convert(input, m_hangulImeActive);
    }

    // ── v2.0: EN ↔ HE (Hebrew + Bidi) ─────────────────────────────
    case LanguagePair::EN_HE: {
        return HebrewEngine::convert(input);
    }

    default:
        return { input, false, false, false, L"Unknown language pair" };
    }
}

} // namespace orka
