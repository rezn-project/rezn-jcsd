/*
 *  Copyright 2006-2019 WebPKI.org (http://webpki.org)
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is a literal C++17 translation of the Go implementation in
 *  https://github.com/cyberphone/json-canonicalization and therefore follows
 *  the same algorithmic structure and error messages.
 */

#ifndef JSON_CANONICALIZER_HPP
#define JSON_CANONICALIZER_HPP

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <limits>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>

#if __cplusplus >= 202002L
#include <bit>
#endif

// ──────────────────────────────────────────────────────────────────────────────
//   Floating-point → canonical JSON (ES-6 “g” format)
// ──────────────────────────────────────────────────────────────────────────────

namespace jsoncanonicalizer
{

    // IEEE-754 invalid pattern (all exponent bits set, fraction ≠ 0)
    inline constexpr std::uint64_t kInvalidPattern = 0x7ff0'0000'0000'0000ULL;

    /*
     *  Converts an IEEE-754 binary64 into the canonical textual representation
     *  mandated by the JSON-Canonicalization-Scheme draft.
     *
     *  Throws std::invalid_argument on NaN/Infinity.
     */
    inline std::string
    NumberToJSON(double value)
    {
        //  1. Detect NaN or ±Inf ― return "null" and signal error
        std::uint64_t bits;
#if __cplusplus >= 202002L
        bits = std::bit_cast<std::uint64_t>(value);
#else
        static_assert(sizeof(double) == sizeof(std::uint64_t), "unexpected size");
        std::memcpy(&bits, &value, sizeof(bits));
#endif
        if ((bits & kInvalidPattern) == kInvalidPattern)
        {
            std::ostringstream oss;
            oss << std::hex << bits;
            throw std::invalid_argument("Invalid JSON number: " + oss.str());
        }

        //  2. Collapse –0 → 0
        if (value == 0.0)
        {
            return "0";
        }

        //  3. Handle sign separately
        std::string sign;
        if (value < 0.0)
        {
            sign = "-";
            value = -value;
        }

        //  4. Choose printf-style format specifier
        char format = (value < 1.0e21 && value >= 1.0e-6) ? 'f' : 'e';

        //  5. Produce ES6 “g” style string (unbounded precision, 64-bit fp)
        char buf[64];
        std::snprintf(buf, sizeof(buf), ("%.*" + std::string{1, format}).c_str(),
                      -1, value);
        std::string out(buf);

        //  6. Clean up exponent “1e+09” → “1e+9”
        auto ePos = out.find('e');
        if (ePos != std::string::npos)
        {
            if (ePos + 2 < out.size() && out[ePos + 2] == '0')
            {
                out.erase(ePos + 2, 1);
            }
        }
        return sign + out;
    }

    // ──────────────────────────────────────────────────────────────────────────────
    //   Canonical JSON transform
    // ──────────────────────────────────────────────────────────────────────────────

    namespace detail
    {

        struct NameValue
        {
            std::string name;
            std::vector<char16_t> sortKey; // UTF-16 code units
            std::string value;
        };

        //  JSON standard escapes
        inline constexpr char kAsciiEsc[] = {'\\', '"', 'b', 'f', 'n', 'r', 't'};
        inline constexpr char kBinaryEsc[] = {'\\', '"', '\b', '\f', '\n', '\r', '\t'};
        //  JSON literals
        inline constexpr std::string_view kLiterals[] = {"true", "false", "null"};

        //  ────────────────────────────── UTF-8 → UTF-16 ─────────────────────────────
        inline std::vector<char16_t> utf8_to_utf16(std::string_view src)
        {
            std::vector<char16_t> dst;
            dst.reserve(src.size()); // *worst* case 1→1
            std::uint32_t code = 0;
            int n = 0;

            auto push = [&](std::uint32_t cp)
            {
                if (cp < 0x10000)
                {
                    dst.push_back(static_cast<char16_t>(cp));
                }
                else
                {
                    cp -= 0x10000;
                    dst.push_back(static_cast<char16_t>((cp >> 10) + 0xd800));
                    dst.push_back(static_cast<char16_t>((cp & 0x3ff) + 0xdc00));
                }
            };

            for (unsigned char ch : src)
            {
                if (n == 0)
                { // leading byte
                    if (ch <= 0x7f)
                    { // plain ASCII
                        push(ch);
                    }
                    else if (ch >= 0xc2 && ch <= 0xdf)
                    {
                        code = ch & 0x1f;
                        n = 1;
                    }
                    else if (ch >= 0xe0 && ch <= 0xef)
                    {
                        code = ch & 0x0f;
                        n = 2;
                    }
                    else if (ch >= 0xf0 && ch <= 0xf4)
                    {
                        code = ch & 0x07;
                        n = 3;
                    }
                    else
                    {
                        throw std::invalid_argument("Invalid UTF-8");
                    }
                }
                else
                { // continuation byte
                    if ((ch & 0xc0) != 0x80)
                        throw std::invalid_argument("Invalid UTF-8 continuation");
                    code = (code << 6) | (ch & 0x3f);
                    if (--n == 0)
                        push(code);
                }
            }
            if (n != 0)
                throw std::invalid_argument("Truncated UTF-8 sequence");
            return dst;
        }

        //  ────────────────────────────── Helpers ─────────────────────────────
        inline bool isWhiteSpace(char c)
        {
            return c == 0x20 || c == 0x0a || c == 0x0d || c == 0x09;
        }

        // Decorate raw UTF-8 string with minimal JSON escapes
        inline std::string decorateString(std::string_view raw)
        {
            std::string out;
            out.reserve(raw.size() + 2);
            out.push_back('"');

            for (unsigned char c : raw)
            {
                bool escaped = false;
                // Standard escape?
                for (std::size_t i = 0; i < sizeof(kBinaryEsc); ++i)
                {
                    if (kBinaryEsc[i] == static_cast<char>(c))
                    {
                        out.push_back('\\');
                        out.push_back(kAsciiEsc[i]);
                        escaped = true;
                        break;
                    }
                }
                if (escaped)
                    continue;

                if (c < 0x20)
                {
                    char tmp[7];
                    std::snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                    out.append(tmp);
                }
                else
                {
                    out.push_back(static_cast<char>(c));
                }
            }
            out.push_back('"');
            return out;
        }

        // ───────────────────────────────────────────────────────────────────────────

        class Parser
        {
        public:
            explicit Parser(std::string_view json)
                : data_(json), len_(json.size()) {}

            std::string transform()
            {
                std::string out;
                if (peekNonWs() == '[')
                {
                    next(); // consume '['
                    out = parseArray();
                }
                else
                {
                    scanFor('{');
                    out = parseObject();
                }
                //  Trailing garbage check
                while (idx_ < len_)
                {
                    if (!isWhiteSpace(data_[idx_]))
                        error("Improperly terminated JSON object");
                    ++idx_;
                }
                return out;
            }

        private:
            //  Core recursive-descent helpers ────────────────────────────────────────
            char peek() const
            {
                if (idx_ < len_)
                    return data_[idx_];
                error("Unexpected EOF reached");
                return '\0';
            }
            char next()
            {
                char c = peek();
                if (static_cast<unsigned char>(c) > 0x7f)
                    error("Unexpected non-ASCII character");
                ++idx_;
                return c;
            }
            char scan()
            {
                for (;;)
                {
                    char c = next();
                    if (!isWhiteSpace(c))
                        return c;
                }
            }
            char peekNonWs()
            {
                std::size_t save = idx_;
                char c = scan();
                idx_ = save;
                return c;
            }
            void scanFor(char expected)
            {
                char c = scan();
                if (c != expected)
                    error(std::string("Expected '") + expected + "' but got '" + c + "'");
            }

            //  \uXXXX handling
            char32_t getUEscape()
            {
                std::size_t start = idx_;
                for (int i = 0; i < 4; ++i)
                    next(); // consume 4 hex bytes
                std::uint32_t v = 0;
                std::from_chars(&data_[start], &data_[start + 4], v, 16);
                return v;
            }

            //  "..." string literal (returns raw UTF-8, *not* quoted/escaped)
            std::string parseQuotedString()
            {
                std::string raw;
                for (;;)
                {
                    if (idx_ >= len_)
                        error("Unterminated string literal");
                    char c = next();
                    if (c == '"')
                        break;
                    if (static_cast<unsigned char>(c) < 0x20)
                        error("Unterminated string literal");
                    if (c == '\\')
                    { // escape sequence
                        c = next();
                        if (c == 'u')
                        {
                            char32_t first = getUEscape();
                            if (first >= 0xd800 && first <= 0xdbff)
                            { // leading surrogate
                                if (next() != '\\' || next() != 'u')
                                    error("Missing surrogate");
                                char32_t second = getUEscape();
                                char32_t cp =
                                    0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00);
                                appendUtf8(raw, cp);
                            }
                            else
                            {
                                appendUtf8(raw, first);
                            }
                        }
                        else if (c == '/')
                        {
                            raw.push_back('/');
                        }
                        else
                        {
                            bool match = false;
                            for (std::size_t i = 0; i < sizeof(kAsciiEsc); ++i)
                            {
                                if (kAsciiEsc[i] == c)
                                {
                                    raw.push_back(kBinaryEsc[i]);
                                    match = true;
                                    break;
                                }
                            }
                            if (!match)
                                error(std::string("Unexpected escape: \\") + c);
                        }
                    }
                    else
                    {
                        raw.push_back(c);
                    }
                }
                return raw;
            }

            //  number / true / false / null
            std::string parseSimpleType()
            {
                std::size_t start = idx_ - 1;
                while (idx_ < len_)
                {
                    char c = data_[idx_];
                    if (c == ',' || c == ']' || c == '}' || isWhiteSpace(c))
                        break;
                    ++idx_;
                }
                if (start == idx_)
                    error("Missing argument");
                std::string token = std::string(data_.substr(start, idx_ - start));

                for (auto lit : kLiterals)
                    if (token == lit)
                        return std::string(lit);

                double v;
                {
                    auto [p, ec] = std::from_chars(token.data(),
                                                   token.data() + token.size(), v);
                    if (ec != std::errc())
                        error("Malformed number");
                }
                return NumberToJSON(v); // may throw
            }

            std::string parseElement()
            {
                switch (scan())
                {
                case '{':
                    return parseObject();
                case '[':
                    return parseArray();
                case '"':
                    return decorateString(parseQuotedString());
                default:
                    return parseSimpleType();
                }
            }

            std::string parseArray()
            {
                std::string out;
                out.push_back('[');
                bool nextElem = false;
                while (peekNonWs() != ']')
                {
                    if (nextElem)
                    {
                        scanFor(',');
                        out.push_back(',');
                    }
                    else
                        nextElem = true;
                    out += parseElement();
                }
                next(); // consume ']'
                out.push_back(']');
                return out;
            }

            bool lexicographicallyPrecedes(const std::vector<char16_t> &key,
                                           const std::list<NameValue>::iterator &it)
            {
                const auto &other = it->sortKey;
                std::size_t minLen = std::min(key.size(), other.size());
                for (std::size_t i = 0; i < minLen; ++i)
                {
                    int diff = int(key[i]) - int(other[i]);
                    if (diff < 0)
                        return true;
                    if (diff > 0)
                        return false;
                }
                if (key.size() < other.size())
                    return true;
                if (key.size() == other.size())
                    error("Duplicate key: " + it->name);
                return false;
            }

            std::string parseObject()
            {
                std::list<NameValue> items;
                bool nextPair = false;

                while (peekNonWs() != '}')
                {
                    if (nextPair)
                        scanFor(',');
                    nextPair = true;

                    scanFor('"');
                    std::string rawName = parseQuotedString();
                    scanFor(':');

                    auto sortKey = utf8_to_utf16(rawName);
                    NameValue nv{rawName, sortKey, parseElement()};

                    auto it = items.begin();
                    for (; it != items.end(); ++it)
                    {
                        if (lexicographicallyPrecedes(sortKey, it))
                            break;
                    }
                    items.insert(it, std::move(nv));
                }
                next(); // consume '}'

                //  Serialize in the sorted order
                std::string out;
                out.push_back('{');
                bool first = true;
                for (const auto &nv : items)
                {
                    if (!first)
                        out.push_back(',');
                    first = false;
                    out += decorateString(nv.name);
                    out.push_back(':');
                    out += nv.value;
                }
                out.push_back('}');
                return out;
            }

            //  UTF-32 → UTF-8 helper for \uXXXX escapes
            static void appendUtf8(std::string &dst, char32_t cp)
            {
                if (cp <= 0x7f)
                {
                    dst.push_back(static_cast<char>(cp));
                }
                else if (cp <= 0x7ff)
                {
                    dst.push_back(static_cast<char>(0xc0 | (cp >> 6)));
                    dst.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                }
                else if (cp <= 0xffff)
                {
                    dst.push_back(static_cast<char>(0xe0 | (cp >> 12)));
                    dst.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                    dst.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                }
                else
                {
                    dst.push_back(static_cast<char>(0xf0 | (cp >> 18)));
                    dst.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
                    dst.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                    dst.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                }
            }

            [[noreturn]] void error(const std::string &msg) const
            {
                throw std::runtime_error(msg);
            }

        private:
            std::string_view data_;
            std::size_t len_;
            std::size_t idx_ = 0;
        };

    } // namespace detail

    /*
     *  The public entry‐point corresponding to Go’s Transform(…).
     *
     *  Input must be UTF-8.  On syntax or encoding errors, throws std::runtime_error.
     */
    inline std::string Transform(std::string_view jsonUtf8)
    {
        detail::Parser p(jsonUtf8);
        return p.transform();
    }

} // namespace jsoncanonicalizer

#endif // JSON_CANONICALIZER_HPP
