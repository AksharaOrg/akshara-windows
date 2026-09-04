#include "AksharaEngine.h"
#include "MappingTrie.h"
#include "SinhalaUnicode.h"

#include <array>
#include <optional>

namespace akshara {
namespace {
using Entry = MappingTrie::Entry;

const MappingTrie& classicConsonants() {
  static const MappingTrie trie({
    {u"nndh",u"ඳ"},{u"nng",u"ඟ"},{u"nnj",u"ඦ"},{u"nnd",u"ඬ"},{u"nnq",u"ඳ"},{u"nnk",u"ඤ"},{u"nnh",u"ඥ"},
    {u"ng",u"ඞ"},{u"gn",u"ඥ"},{u"ny",u"ඤ"},{u"kh",u"ඛ"},{u"gh",u"ඝ"},{u"ch",u"ච"},{u"jh",u"ඣ"},
    {u"Th",u"ඨ"},{u"Dh",u"ඪ"},{u"th",u"ත"},{u"dh",u"ද"},{u"ph",u"ඵ"},{u"bh",u"භ"},{u"sh",u"ශ"},{u"Sh",u"ෂ"},
    {u"B",u"ඹ"},{u"k",u"ක"},{u"g",u"ග"},{u"c",u"ක"},{u"j",u"ජ"},{u"C",u"ඡ"},{u"T",u"ට"},{u"D",u"ඩ"},
    {u"N",u"ණ"},{u"t",u"ට"},{u"d",u"ඩ"},{u"n",u"න"},{u"p",u"ප"},{u"b",u"බ"},{u"m",u"ම"},{u"y",u"ය"},
    {u"r",u"ර"},{u"l",u"ල"},{u"L",u"ළ"},{u"v",u"ව"},{u"w",u"ව"},{u"s",u"ස"},{u"h",u"හ"},{u"f",u"ෆ"},{u"R",u"ර"},{u"Y",u"ය"}
  });
  return trie;
}

const MappingTrie& classicVowels() {
  static const MappingTrie trie({
    {u"aee",u"ඈ\tෑ"},{u"ae",u"ඇ\tැ"},{u"aa",u"ආ\tා"},{u"ii",u"ඊ\tී"},{u"uu",u"ඌ\tූ"},
    {u"ee",u"ඒ\tේ"},{u"ai",u"ඓ\tෛ"},{u"oo",u"ඕ\tෝ"},{u"au",u"ඖ\tෞ"},{u"A",u"ආ\tා"},
    {u"I",u"ඊ\tී"},{u"U",u"ඌ\tූ"},{u"E",u"ඒ\tේ"},{u"O",u"ඕ\tෝ"},{u"a",u"අ\t"},
    {u"i",u"ඉ\tි"},{u"u",u"උ\tු"},{u"e",u"එ\tෙ"},{u"o",u"ඔ\tො"}
  });
  return trie;
}

const MappingTrie& smartConsonants() {
  static const MappingTrie trie({
    {u"chh",u"ඡ"},{u"thh",u"ථ"},{u"dhh",u"ධ"},{u"zdh",u"ඳ"},{u"ch",u"ච"},{u"th",u"ත"},{u"dh",u"ද"},
    {u"sh",u"ශ"},{u"Sh",u"ෂ"},{u"kh",u"ඛ"},{u"gh",u"ඝ"},{u"ph",u"ඵ"},{u"bh",u"භ"},{u"zg",u"ඟ"},
    {u"zj",u"ඦ"},{u"zd",u"ඬ"},{u"zq",u"ඳ"},{u"zk",u"ඤ"},{u"zh",u"ඥ"},{u"k",u"ක"},{u"g",u"ග"},
    {u"c",u"ක"},{u"j",u"ජ"},{u"t",u"ට"},{u"d",u"ඩ"},{u"q",u"ද"},{u"n",u"න"},{u"N",u"ණ"},
    {u"p",u"ප"},{u"b",u"බ"},{u"m",u"ම"},{u"y",u"ය"},{u"r",u"ර"},{u"l",u"ල"},{u"L",u"ළ"},
    {u"w",u"ව"},{u"v",u"ව"},{u"s",u"ස"},{u"S",u"ෂ"},{u"h",u"හ"},{u"f",u"ෆ"},{u"T",u"ඨ"},
    {u"D",u"ඪ"},{u"B",u"ඹ"},{u"X",u"ඞ"},{u"K",u"ඛ"},{u"P",u"ඵ"},{u"W",u"ව"},{u"C",u"ඛ"},{u"V",u"ව"},{u"J",u"ඣ"},{u"G",u"ඝ"}
  });
  return trie;
}

const MappingTrie& smartVowels() {
  static const MappingTrie trie({
    {u"ruu",u"\tෲ"},{u"Aa",u"ඈ\tෑ"},{u"AA",u"ඈ\tෑ"},{u"aa",u"ආ\tා"},{u"ii",u"ඊ\tී"},
    {u"uu",u"ඌ\tූ"},{u"UU",u"ඌ\tූ"},{u"Uu",u"ඌ\tූ"},{u"ee",u"ඒ\tේ"},{u"ai",u"ඓ\tෛ"},
    {u"oo",u"ඕ\tෝ"},{u"OO",u"ඕ\tෝ"},{u"Oo",u"ඕ\tෝ"},{u"au",u"ඖ\tෞ"},{u"ou",u"ඖ\tෞ"},
    {u"Ru",u"ඎ\t\x01"},{u"ru",u"\tෘ"},{u"A",u"ඇ\tැ"},{u"I",u"ඊ\tී"},{u"U",u"උ\tු"},
    {u"E",u"ඓ\tෛ"},{u"O",u"ඔ\tො"},{u"a",u"අ\t"},{u"i",u"ඉ\tි"},{u"u",u"උ\tු"},
    {u"e",u"එ\tෙ"},{u"o",u"ඔ\tො"},{u"R",u"ඍ\t\x01"}
  });
  return trie;
}

struct VowelParts { std::u16string_view independent; std::optional<std::u16string_view> sign; };
VowelParts splitVowel(std::u16string_view packed) {
  const auto tab = packed.find(u'\t');
  const auto independent = packed.substr(0, tab);
  auto sign = packed.substr(tab + 1);
  if (sign.size() == 1 && sign.front() == u'\x01') return {independent, std::nullopt};
  return {independent, sign};
}

std::u16string transliterate(std::u16string_view input, bool smart) {
  const auto& consonants = smart ? smartConsonants() : classicConsonants();
  const auto& vowels = smart ? smartVowels() : classicVowels();
  std::u16string out;
  out.reserve(input.size() * 2);
  std::size_t i = 0;
  while (i < input.size()) {
    const auto ch = input[i];
    if (ch == u'M' || (smart && ch == u'x')) { out += u"ං"; ++i; continue; }
    if (smart && i + 2 <= input.size() && input.substr(i, 2) == u"zn") { out += u"ං"; i += 2; continue; }
    if (smart && ch == u'z') {
      const auto m = consonants.longest(input, i);
      if (!m || m->key.front() != u'z') { ++i; continue; }
    }
    if (ch == u'H') { out += u"ඃ"; ++i; continue; }
    if (const auto consonant = consonants.longest(input, i)) {
      out += consonant->value;
      i += consonant->key.size();
      const auto vowel = vowels.longest(input, i);
      if (vowel) {
        const auto parts = splitVowel(vowel->value);
        if (!smart || parts.sign.has_value()) {
          if (parts.sign) out += *parts.sign;
          i += vowel->key.size();
          continue;
        }
      }
      if (!smart && i < input.size() && input[i] == u'r') {
        out += u"්‍ර"; ++i;
        if (const auto after = vowels.longest(input, i)) {
          const auto parts = splitVowel(after->value);
          if (parts.sign) out += *parts.sign;
          i += after->key.size();
        }
      } else {
        const auto next = consonants.longest(input, i);
        const bool yansaya = next && next->key == u"y";
        const bool rakaransaya = next && next->key == u"r" && consonant->key != u"m" && consonant->key != u"n" && consonant->key != u"l";
        out += (yansaya || rakaransaya) ? u"්‍" : u"්";
      }
      continue;
    }
    if (const auto vowel = vowels.longest(input, i)) {
      const auto parts = splitVowel(vowel->value);
      if (!parts.independent.empty()) { out += parts.independent; i += vowel->key.size(); continue; }
    }
    out.push_back(ch); ++i;
  }
  return out;
}

std::optional<char16_t> usKey(std::uint32_t vk, bool shift) {
  if (vk >= 0x41 && vk <= 0x5A) return static_cast<char16_t>((shift ? u'A' : u'a') + (vk - 0x41));
  if (vk >= 0x30 && vk <= 0x39) {
    constexpr std::u16string_view shifted = u")!@#$%^&*(";
    return shift ? shifted[vk - 0x30] : static_cast<char16_t>(vk);
  }
  switch (vk) {
    case 0xC0: return shift ? u'~' : u'`'; case 0xBD: return shift ? u'_' : u'-';
    case 0xBB: return shift ? u'+' : u'='; case 0xDB: return shift ? u'{' : u'[';
    case 0xDD: return shift ? u'}' : u']'; case 0xDC: return shift ? u'|' : u'\\';
    case 0xBA: return shift ? u':' : u';'; case 0xDE: return shift ? u'"' : u'\'';
    case 0xBC: return shift ? u'<' : u','; case 0xBE: return shift ? u'>' : u'.';
    case 0xBF: return shift ? u'?' : u'/'; case 0x20: return u' ';
    default: return std::nullopt;
  }
}

std::u16string mapSlsChar(char16_t key, bool shift, bool altGr) {
  if (altGr) {
    switch (key >= u'A' && key <= u'Z' ? key + 32 : key) {
      case u'o': return u"ඳ"; case u'.': return u"ඟ"; case u'v': return u"ඬ"; case u'c': return u"ඦ";
      case u'x': return u"ඃ"; case u'\'': return u"෴"; case u',': return u"ඏ"; case u'a': return u"ෳ";
      case u'z': return std::u16string(1, unicode::kSanyakaya); case u'\\': return std::u16string(1, unicode::kTouch);
      case u' ': return u"‌"; default: break;
    }
  }
  if (!shift) {
    switch (key) {
      case u'`': return std::u16string(1, unicode::kRakaransaya); case u'q': return u"ු"; case u'w': return u"අ";
      case u'e': return u"ැ"; case u'r': return u"ර"; case u't': return u"එ"; case u'y': return u"හ"; case u'u': return u"ම";
      case u'i': return u"ස"; case u'o': return u"ද"; case u'p': return u"ච"; case u'[': return u"ඤ"; case u']': return u";";
      case u'\\': return std::u16string(1, unicode::kJoin); case u'a': return u"්"; case u's': return u"ි"; case u'd': return u"ා";
      case u'f': return u"ෙ"; case u'g': return u"ට"; case u'h': return u"ය"; case u'j': return u"ව"; case u'k': return u"න";
      case u'l': return u"ක"; case u';': return u"ත"; case u'\'': return u"."; case u'z': return u"'"; case u'x': return u"ං";
      case u'c': return u"ජ"; case u'v': return u"ඩ"; case u'b': return u"ඉ"; case u'n': return u"බ"; case u'm': return u"ප";
      case u',': return u"ල"; case u'.': return u"ග"; case u'/': return u"/"; default: return std::u16string(1, key);
    }
  }
  switch (key) {
    case u'~': return std::u16string(1, unicode::kRepaya); case u'Q': return u"ූ"; case u'W': return u"උ"; case u'E': return u"ෑ";
    case u'R': return u"ඍ"; case u'T': return u"ඔ"; case u'Y': return u"ශ"; case u'U': return u"ඹ"; case u'I': return u"ෂ";
    case u'O': return u"ධ"; case u'P': return u"ඡ"; case u'{': return u"ඥ"; case u'}': return u":";
    case u'|': return std::u16string(1, unicode::kTouch); case u'A': return u"ෟ"; case u'S': return u"ී"; case u'D': return u"ෘ";
    case u'F': return u"ෆ"; case u'G': return u"ඨ"; case u'H': return std::u16string(1, unicode::kYansaya); case u'J': return u"ළු";
    case u'K': return u"ණ"; case u'L': return u"ඛ"; case u':': return u"ථ"; case u'"': return u","; case u'Z': return u"\"";
    case u'X': return u"ඞ"; case u'C': return u"ඣ"; case u'V': return u"ඪ"; case u'B': return u"ඊ"; case u'N': return u"භ";
    case u'M': return u"ඵ"; case u'<': return u"ළ"; case u'>': return u"ඝ"; case u'?': return u"?"; default: return std::u16string(1, key);
  }
}
}

CompositionResult AksharaEngine::compose(InputMode mode, std::u16string_view rawInput) const {
  std::u16string text;
  switch (mode) {
    case InputMode::SmartPhonetic: text = transliterate(rawInput, true); break;
    case InputMode::Phonetic: text = transliterate(rawInput, false); break;
    case InputMode::Wijesekara: text = unicode::markedWijesekara(rawInput); break;
  }
  return {std::move(text), !rawInput.empty()};
}

std::u16string AksharaEngine::mapWijesekaraKey(const PhysicalKey& key) const {
  const auto physical = usKey(key.virtualKey, key.shift);
  return physical ? mapSlsChar(*physical, key.shift, key.altGr) : std::u16string{};
}

bool AksharaEngine::isComposableWijesekaraUnit(std::u16string_view text) {
  for (const auto ch : text)
    if (unicode::isSinhalaScalar(ch) || ch == u'\u200C' || ch == u'\u200D' || (ch >= u'\uE000' && ch <= u'\uE0FF')) return true;
  return false;
}

bool CompositionBuffer::backspace() {
  if (raw_.empty()) return false;
  const auto last = raw_.back();
  raw_.pop_back();
  if (last >= 0xDC00 && last <= 0xDFFF && !raw_.empty() && raw_.back() >= 0xD800 && raw_.back() <= 0xDBFF) raw_.pop_back();
  return true;
}
}
