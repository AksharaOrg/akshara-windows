#pragma once

#include <string>
#include <string_view>

// UTF conversions for the Smart Phonetic v2 port, which works on code points like the reference's
// Python strings. Invalid input becomes U+FFFD.
namespace akshara::utf {

inline std::u32string fromUtf8(std::string_view input) {
  std::u32string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size();) {
    const auto lead = static_cast<unsigned char>(input[i++]);
    char32_t cp = lead;
    int continuation = 0;
    if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F; continuation = 1; }
    else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F; continuation = 2; }
    else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07; continuation = 3; }
    else if (lead >= 0x80) { out.push_back(U'�'); continue; }
    bool valid = i + static_cast<std::size_t>(continuation) <= input.size();
    for (int n = 0; valid && n < continuation; ++n) {
      const auto next = static_cast<unsigned char>(input[i++]);
      if ((next & 0xC0) != 0x80) { valid = false; break; }
      cp = (cp << 6) | (next & 0x3F);
    }
    if (!valid || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { out.push_back(U'�'); continue; }
    out.push_back(cp);
  }
  return out;
}

inline std::string toUtf8(std::u32string_view input) {
  std::string out;
  out.reserve(input.size() * 3);
  for (const char32_t cp : input) {
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

inline std::u32string fromUtf16(std::u16string_view input) {
  std::u32string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    char32_t cp = input[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < input.size() && input[i + 1] >= 0xDC00 && input[i + 1] <= 0xDFFF) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (input[++i] - 0xDC00);
    } else if (cp >= 0xD800 && cp <= 0xDFFF) {
      cp = U'�';
    }
    out.push_back(cp);
  }
  return out;
}

inline std::u16string toUtf16(std::u32string_view input) {
  std::u16string out;
  out.reserve(input.size());
  for (char32_t cp : input) {
    if (cp <= 0xFFFF) {
      out.push_back(static_cast<char16_t>(cp));
    } else {
      cp -= 0x10000;
      out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
      out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
    }
  }
  return out;
}

}  // namespace akshara::utf
