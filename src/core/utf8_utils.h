#pragma once

/**
 * ORKA UTF-8 Utilities
 *
 * Manual UTF-8 ↔ wstring conversion to replace deprecated std::codecvt_utf8.
 * Compliant with RFC 3629 (UTF-8) and handles full BMP + supplementary planes.
 */

#include <string>
#include <cstdint>
#include <stdexcept>

namespace orka {
namespace util {

/// Convert UTF-8 std::string → std::wstring
inline std::wstring utf8ToWide(const std::string& utf8) {
    std::wstring result;
    result.reserve(utf8.size());

    size_t i = 0;
    while (i < utf8.size()) {
        uint32_t cp = 0;
        unsigned char ch = static_cast<unsigned char>(utf8[i]);

        if (ch < 0x80) {
            // ASCII: 0xxxxxxx
            cp = ch;
            ++i;
        } else if ((ch & 0xE0) == 0xC0) {
            // 2-byte: 110xxxxx 10xxxxxx
            if (i + 1 >= utf8.size()) break;
            cp = (ch & 0x1F) << 6;
            cp |= (static_cast<unsigned char>(utf8[i + 1]) & 0x3F);
            i += 2;
        } else if ((ch & 0xF0) == 0xE0) {
            // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
            if (i + 2 >= utf8.size()) break;
            cp  = (ch & 0x0F) << 12;
            cp |= (static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 6;
            cp |= (static_cast<unsigned char>(utf8[i + 2]) & 0x3F);
            i += 3;
        } else if ((ch & 0xF8) == 0xF0) {
            // 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            if (i + 3 >= utf8.size()) break;
            cp  = (ch & 0x07) << 18;
            cp |= (static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 12;
            cp |= (static_cast<unsigned char>(utf8[i + 2]) & 0x3F) << 6;
            cp |= (static_cast<unsigned char>(utf8[i + 3]) & 0x3F);
            i += 4;
        } else {
            // Invalid leading byte — skip
            ++i;
            continue;
        }

        // Emit as wchar_t (on Linux wchar_t is 32-bit, on Windows 16-bit)
        if constexpr (sizeof(wchar_t) == 4) {
            result.push_back(static_cast<wchar_t>(cp));
        } else {
            // UTF-16 surrogate pair for code points > 0xFFFF
            if (cp <= 0xFFFF) {
                result.push_back(static_cast<wchar_t>(cp));
            } else {
                cp -= 0x10000;
                result.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
                result.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
            }
        }
    }

    return result;
}

/// Convert std::wstring → UTF-8 std::string
inline std::string wideToUtf8(const std::wstring& wide) {
    std::string result;
    result.reserve(wide.size() * 3);  // worst case for BMP

    size_t i = 0;
    while (i < wide.size()) {
        uint32_t cp = 0;

        if constexpr (sizeof(wchar_t) == 4) {
            cp = static_cast<uint32_t>(wide[i]);
            ++i;
        } else {
            // UTF-16: handle surrogate pairs
            wchar_t w = wide[i];
            if (w >= 0xD800 && w <= 0xDBFF && i + 1 < wide.size()) {
                wchar_t w2 = wide[i + 1];
                if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
                    cp = 0x10000 + ((static_cast<uint32_t>(w) - 0xD800) << 10)
                                 + (static_cast<uint32_t>(w2) - 0xDC00);
                    i += 2;
                } else {
                    cp = static_cast<uint32_t>(w);
                    ++i;
                }
            } else {
                cp = static_cast<uint32_t>(w);
                ++i;
            }
        }

        // Encode code point as UTF-8
        if (cp < 0x80) {
            result.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x110000) {
            result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    return result;
}

} // namespace util
} // namespace orka
