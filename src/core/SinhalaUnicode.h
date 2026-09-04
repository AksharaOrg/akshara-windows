#pragma once
#include <string>
#include <string_view>

namespace akshara::unicode {
inline constexpr char16_t kJoin = u'\uE000';
inline constexpr char16_t kTouch = u'\uE001';
inline constexpr char16_t kRepaya = u'\uE002';
inline constexpr char16_t kSanyakaya = u'\uE003';
inline constexpr char16_t kRakaransaya = u'\uE004';
inline constexpr char16_t kYansaya = u'\uE005';

[[nodiscard]] bool isSinhalaScalar(char16_t ch);
[[nodiscard]] bool isConsonant(char16_t ch);
[[nodiscard]] bool isIndependentVowel(char16_t ch);
[[nodiscard]] bool isDependentVowelSign(char16_t ch);
[[nodiscard]] std::u16string normalizeWijesekara(std::u16string_view input);
[[nodiscard]] std::u16string markedWijesekara(std::u16string_view input);
}

