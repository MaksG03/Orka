#pragma once

#include <string>
#include <functional>

namespace orka {

// ── Language pairs supported by ORKA ───────────────────────────────
enum class LanguagePair {
    EN_UK,  // v1.0
    EN_KO,  // v1.5
    EN_HE   // v2.0
};

// ── Conversion result ──────────────────────────────────────────────
struct ConversionResult {
    std::wstring text;
    bool         nikudStripped  = false;  // HE: lossy conversion warning
    bool         escapeRequired = false;  // KO: IME needs VK_ESCAPE first
    bool         success        = true;
    std::wstring errorMessage;
};

// ── Conversion direction (auto-detected) ───────────────────────────
enum class Direction {
    FORWARD,   // EN → target
    REVERSE,   // target → EN
    UNKNOWN
};

// ── Main engine interface ──────────────────────────────────────────
class LanguageEngine {
public:
    LanguageEngine();
    ~LanguageEngine();

    LanguageEngine(const LanguageEngine&) = delete;
    LanguageEngine& operator=(const LanguageEngine&) = delete;

    /// Core conversion entry point
    ConversionResult convert(const std::wstring& input, LanguagePair pair);

    /// Auto-detect direction based on input characters
    Direction detectDirection(const std::wstring& input, LanguagePair pair);

    /// IME state (Korean)
    void setHangulImeActive(bool active);
    bool isHangulImeActive() const { return m_hangulImeActive; }

private:
    bool m_hangulImeActive = false;
};

} // namespace orka
