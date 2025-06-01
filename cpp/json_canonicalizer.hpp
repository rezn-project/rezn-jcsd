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
#include <bitset>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace jsoncanonicalizer
{

    // ─────────────────────────────  Double → JCS string  ─────────────────────────

    inline constexpr std::uint64_t kInvalidPattern = 0x7ff0'0000'0000'0000ULL;

    inline std::string number_to_json(double v)
    {
        if (std::isnan(v) || std::isinf(v))
            throw std::invalid_argument("Invalid JSON number");

        if (v == 0.0)
            return "0";

        std::string sign;
        if (v < 0.0)
        {
            sign = "-";
            v = -v;
        }

        char buf[64];
        std::string out;

        if (v >= 1e-6 && v < 1e21)
        {
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed);
            if (ec != std::errc())
                throw std::runtime_error("to_chars fixed failed");
            out = std::string(buf, ptr);
            // Trim trailing zeros and possibly trailing dot
            if (out.find('.') != std::string::npos)
            {
                while (out.back() == '0')
                    out.pop_back();
                if (out.back() == '.')
                    out.pop_back();
            }
        }
        else
        {
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::scientific);
            if (ec != std::errc())
                throw std::runtime_error("to_chars scientific failed");
            out = std::string(buf, ptr);
            // Canonicalize exponent: e+09 → e+9
            auto e = out.find('e');
            if (e != std::string::npos && e + 2 < out.size() && out[e + 2] == '0')
            {
                out.erase(e + 2, 1);
            }
        }

        return sign + out;
    }

    // ─────────────────────────────  UTF-8 → UTF-16 helper  ───────────────────────

    inline std::vector<char16_t> utf8_to_utf16(std::string_view s)
    {
        std::vector<char16_t> out;
        out.reserve(s.size());

        std::uint32_t cp = 0;
        int n = 0;
        auto push = [&](std::uint32_t u)
        {
            if (u < 0x10000)
                out.push_back(char16_t(u));
            else
            {
                u -= 0x10000;
                out.push_back(char16_t((u >> 10) + 0xd800));
                out.push_back(char16_t((u & 0x3ff) + 0xdc00));
            }
        };

        for (unsigned char ch : s)
        {
            if (n == 0)
            { // lead byte
                if (ch <= 0x7f)
                    push(ch);
                else if (ch >= 0xc2 && ch <= 0xdf)
                {
                    cp = ch & 0x1f;
                    n = 1;
                }
                else if (ch >= 0xe0 && ch <= 0xef)
                {
                    cp = ch & 0x0f;
                    n = 2;
                }
                else if (ch >= 0xf0 && ch <= 0xf4)
                {
                    cp = ch & 0x07;
                    n = 3;
                }
                else
                    throw std::invalid_argument("Invalid UTF-8");
            }
            else
            { // continuation
                if ((ch & 0xc0) != 0x80)
                    throw std::invalid_argument("Bad UTF-8");
                cp = (cp << 6) | (ch & 0x3f);
                if (--n == 0)
                    push(cp);
            }
        }
        if (n)
            throw std::invalid_argument("Truncated UTF-8");
        return out;
    }

    // ─────────────────────────────  String escaping  ─────────────────────────────

    inline std::string escape_string(std::string_view raw)
    {
        static constexpr char kAsciiEsc[] = {'\\', '"', 'b', 'f', 'n', 'r', 't'};
        static constexpr char kBinaryEsc[] = {'\\', '"', '\b', '\f', '\n', '\r', '\t'};

        std::string out;
        out.reserve(raw.size() + 2);
        out.push_back('"');

        for (unsigned char c : raw)
        {
            bool escaped = false;
            // standard ASCII escapes?
            for (unsigned i = 0; i < sizeof(kBinaryEsc); ++i)
                if (kBinaryEsc[i] == char(c))
                {
                    out.push_back('\\');
                    out.push_back(kAsciiEsc[i]);
                    escaped = true;
                    break;
                }

            if (escaped)
                continue;

            if (c < 0x20)
            { // other C0 controls → \u00XX
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out.append(buf);
            }
            else
            {
                out.push_back(char(c));
            }
        }
        out.push_back('"');
        return out;
    }

    // ─────────────────────────────  Core canonicalizer  ──────────────────────────

    namespace detail
    {

        using json = nlohmann::json;

        struct KeyOrder
        {
            bool operator()(std::string_view a, std::string_view b) const
            {
                auto ua = utf8_to_utf16(a), ub = utf8_to_utf16(b);
                return std::lexicographical_compare(ua.begin(), ua.end(),
                                                    ub.begin(), ub.end());
            }
        };

        inline std::string canonicalize(const json &j);

        inline std::string canonicalize_object(const json &obj)
        {
            // 1)  collect keys, then sort by UTF-16 code units
            std::vector<std::string_view> keys;
            keys.reserve(obj.size());
            for (auto it = obj.cbegin(); it != obj.cend(); ++it)
                keys.emplace_back(it.key());

            std::sort(keys.begin(), keys.end(), KeyOrder{});

            // 2)  emit in order
            std::string out;
            out.push_back('{');
            bool first = true;
            for (auto k : keys)
            {
                if (!first)
                    out.push_back(',');
                first = false;
                out += escape_string(k);
                out.push_back(':');
                out += canonicalize(obj.at(std::string(k)));
            }
            out.push_back('}');
            return out;
        }

        inline std::string canonicalize_array(const json &arr)
        {
            std::string out;
            out.push_back('[');
            for (size_t i = 0; i < arr.size(); ++i)
            {
                if (i)
                    out.push_back(',');
                out += canonicalize(arr[i]);
            }
            out.push_back(']');
            return out;
        }

        inline std::string canonicalize(const json &j)
        {
            switch (j.type())
            {
            case json::value_t::object:
                return canonicalize_object(j);
            case json::value_t::array:
                return canonicalize_array(j);
            case json::value_t::string:
                return escape_string(j.get_ref<const std::string &>());
            case json::value_t::boolean:
                return j.get<bool>() ? "true" : "false";
            case json::value_t::number_integer:
            case json::value_t::number_unsigned:
            case json::value_t::number_float:
            {
                //  The JCS spec mandates **binary64** semantics even for integers.
                return number_to_json(j.template get<double>());
            }
            case json::value_t::null:
                return "null";
            default:
                throw std::runtime_error("Unsupported JSON value");
            }
        }

    } // namespace detail

    // ─────────────────────────────  Public API  ──────────────────────────────────

    /** Canonicalizes a UTF-8 JSON text according to RFC 8785 / JCS draft.
     *
     *  @param json_utf8   Source text (may contain insignificant spaces, etc.)
     *  @return           Byte-perfect canonical JSON
     *  @throws           std::exception on malformed input or disallowed numbers
     */
    inline std::string canonicalize(std::string_view json_utf8)
    {
        auto dom = detail::json::parse(json_utf8, /*cb=*/nullptr,
                                       /*allow_exceptions=*/true,
                                       /*ignore_comments=*/false);
        return detail::canonicalize(dom);
    }

} // namespace jsoncanonicalizer

#endif /* JSON_CANONICALIZER_HPP */
