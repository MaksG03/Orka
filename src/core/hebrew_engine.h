#pragma once

/**
 * ORKA Hebrew Engine — §7.3: EN ↔ HE (Hebrew + BiDi)
 *
 * QWERTY → Hebrew mapping with BiDi awareness via ICU (UAX #9).
 * Handles Nikud (U+05B0–U+05C7) lossy stripping with user warning.
 */

#include "language_engine.h"
#include <string>

namespace orka {

class HebrewEngine {
public:
    /// Convert EN ↔ HE text
    static ConversionResult convert(const std::wstring& input);

    /// Detect whether input is Latin (FORWARD) or Hebrew (REVERSE)
    static Direction detectDirection(const std::wstring& input);

    /// Check if text contains Nikud diacritics
    static bool containsNikud(const std::wstring& input);

    /// Strip Nikud from Hebrew text
    static std::wstring stripNikud(const std::wstring& input);
};

} // namespace orka
