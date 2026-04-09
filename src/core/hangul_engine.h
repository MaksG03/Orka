#pragma once

/**
 * ORKA Hangul Engine — §7.2: EN ↔ KO (Dubeolsik 두벌식)
 *
 * Built-in syllable composer that maps QWERTY keys to Hangul Jamo
 * and composes them into syllable blocks per Unicode Hangul algorithm.
 *
 * If libhangul is available at build time (ORKA_HAS_HANGUL), it is
 * used for composition; otherwise the built-in table is used.
 */

#include "language_engine.h"
#include <string>

namespace orka {

class HangulEngine {
public:
    /// Convert EN ↔ KO text
    static ConversionResult convert(const std::wstring& input, bool imeActive);

    /// Detect whether input is Latin (FORWARD) or Hangul (REVERSE)
    static Direction detectDirection(const std::wstring& input);

private:
    // Dubeolsik QWERTY → Jamo mapping
    static wchar_t qwertyToJamo(wchar_t ch);
    
    // Unicode Hangul composition
    static bool isLeadingJamo(wchar_t ch);
    static bool isVowelJamo(wchar_t ch);
    static bool isTrailingJamo(wchar_t ch);
    static wchar_t composeHangulSyllable(wchar_t lead, wchar_t vowel, wchar_t trail = 0);
    
    // Reverse: Hangul syllable → QWERTY
    static std::wstring decomposeSyllableToQwerty(wchar_t syllable);
};

} // namespace orka
