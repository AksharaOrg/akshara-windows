#include "SoundLexicon.h"

#include "Utf.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace akshara {
namespace {

constexpr char32_t kHal = SmartPhoneticV2::kHal;
constexpr char32_t kZwj = SmartPhoneticV2::kZwj;
constexpr char32_t kYa = U'ය';
constexpr char32_t kRa = U'ර';

// [ක-ෆ]: a consonant letter.
bool isConsonant(char32_t c) { return c >= U'ක' && c <= U'ෆ'; }

// C ් ය / C ් ර takes ZWJ, except after ර (G-HC-14, R-09) and C ් ර after ම න ල (R-07).
bool joins(char32_t c, char32_t next) {
  return c != kRa && !(next == kRa && (c == U'ම' || c == U'න' || c == U'ල'));
}

std::vector<std::u32string> unique(std::vector<std::u32string> words) {
  std::unordered_set<std::u32string> seen;
  std::vector<std::u32string> out;
  out.reserve(words.size());
  for (auto& word : words) {
    if (seen.insert(word).second) out.push_back(std::move(word));
  }
  return out;
}

struct Fold { std::u32string_view from; std::u32string_view to; };

constexpr Fold kFoldSequences[] = {
    // composite vowels first
    {U"ෛ", U"යි"}, {U"ඓ", U"අයි"}, {U"ෞ", U"වු"}, {U"ඖ", U"අවු"},
    {U"ෘ", U"්රු"}, {U"ෲ", U"්රු"}, {U"ඍ", U"රු"}, {U"ඎ", U"රු"},
    {U"ඥ", U"ග්න"},                                                          // G-NS-12: gn
    // sanyaka → nasal + stop (R-03)
    {U"ඟ", U"න්ග"}, {U"ඦ", U"න්ජ"}, {U"ඬ", U"න්ද"}, {U"ඳ", U"න්ද"}, {U"ඹ", U"ම්බ"},
    {U"ං", U"න්"}, {U"ඞ්", U"න්"},                                            // R-11
};

constexpr Fold kFoldChars[] = {
    // aspirates → plain (G-SP-01, G-SP-06)
    {U"ඛ", U"ක"}, {U"ඝ", U"ග"}, {U"ඡ", U"ච"}, {U"ඣ", U"ජ"}, {U"ඨ", U"ට"}, {U"ඪ", U"ද"}, {U"ථ", U"ත"}, {U"ධ", U"ද"},
    {U"ඵ", U"ප"}, {U"භ", U"බ"},
    {U"ඩ", U"ද"},                                                             // R-01: d is written for both
    {U"ණ", U"න"}, {U"ළ", U"ල"}, {U"ශ", U"ස"}, {U"ෂ", U"ස"}, {U"ඤ", U"න"},    // G-SP-02…05, G-NS-13
    // vowel length and ae/e (G-SP-07, G-TY-04, G-TY-07)
    {U"ආ", U"අ"}, {U"ඊ", U"ඉ"}, {U"ඌ", U"උ"}, {U"ඒ", U"එ"}, {U"ඕ", U"ඔ"}, {U"ඇ", U"එ"}, {U"ඈ", U"එ"},
    {U"ා", U""}, {U"ී", U"ි"}, {U"ූ", U"ු"}, {U"ේ", U"ෙ"}, {U"ෝ", U"ො"}, {U"ැ", U"ෙ"}, {U"ෑ", U"ෙ"},
    {U"‍", U""},
};

// Replaces every non-overlapping `from`, left to right, like Python's str.replace.
std::u32string replaceAll(const std::u32string& s, std::u32string_view from, std::u32string_view to) {
  if (s.find(from) == std::u32string::npos) return s;
  std::u32string out;
  out.reserve(s.size() + 4);
  std::size_t i = 0;
  while (i < s.size()) {
    if (std::u32string_view(s).substr(i, from.size()) == from) {
      out += to;
      i += from.size();
    } else {
      out.push_back(s[i++]);
    }
  }
  return out;
}

bool startsWith(std::u32string_view text, std::u32string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}

}  // namespace

SoundLexicon::SoundLexicon(const std::vector<Row>& rows) {
  bool repair = true;
  for (const auto& [word, n] : rows) {
    if (word.find(kZwj) != std::u32string::npos) { repair = false; break; }
  }
  for (const auto& [word, n] : rows) {
    if (word.empty()) continue;
    count_[repair ? normalize(word) : word] += n;
  }
  for (const auto& [word, n] : count_) byKey_[soundKey(word)].push_back(word);
  keys_.reserve(byKey_.size());
  for (const auto& [key, words] : byKey_) keys_.push_back(key);
  std::sort(keys_.begin(), keys_.end());
}

std::vector<SoundLexicon::Row> SoundLexicon::parse(std::string_view utf8) {
  std::vector<Row> rows;
  std::size_t start = 0;
  while (start < utf8.size()) {
    std::size_t end = utf8.find_first_of("\r\n", start);
    if (end == std::string_view::npos) end = utf8.size();
    const auto line = utf8.substr(start, end - start);
    start = end + (utf8.substr(end, 2) == "\r\n" ? 2 : 1);

    const auto tab = line.find('\t');
    if (tab == std::string_view::npos || tab == 0) continue;
    const auto digits = line.substr(tab + 1);
    if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) continue;
    std::int64_t value = 0;
    for (const char c : digits) {
      if (value > (std::numeric_limits<std::int64_t>::max() - 9) / 10) break;
      value = value * 10 + (c - '0');
    }
    rows.emplace_back(utf::fromUtf8(line.substr(0, tab)), value);
  }
  return rows;
}

std::int64_t SoundLexicon::countOf(const std::u32string& word) const {
  const auto found = count_.find(word);
  return found == count_.end() ? 0 : found->second;
}

const std::vector<std::u32string>& SoundLexicon::exact(const std::u32string& key) const {
  static const std::vector<std::u32string> none;
  const auto found = byKey_.find(key);
  return found == byKey_.end() ? none : found->second;
}

std::size_t SoundLexicon::firstKeyAtOrAfter(std::u32string_view key) const {
  return static_cast<std::size_t>(
      std::lower_bound(keys_.begin(), keys_.end(), key, [](const std::u32string& a, std::u32string_view b) { return a < b; }) -
      keys_.begin());
}

std::vector<std::u32string> SoundLexicon::prefix(std::u32string_view key, std::size_t limit) const {
  std::vector<std::u32string> out;
  for (auto i = firstKeyAtOrAfter(key); i < keys_.size() && startsWith(keys_[i], key) && out.size() < limit; ++i) {
    const auto& words = byKey_.at(keys_[i]);
    out.insert(out.end(), words.begin(), words.end());
  }
  return out;
}

bool SoundLexicon::hasPrefix(std::u32string_view key) const {
  const auto i = firstKeyAtOrAfter(key);
  return i < keys_.size() && startsWith(keys_[i], key);
}

std::vector<std::u32string> SoundLexicon::byFrequency(std::vector<std::u32string> words) const {
  words = unique(std::move(words));
  std::sort(words.begin(), words.end(), [this](const std::u32string& a, const std::u32string& b) {
    const auto ca = countOf(a), cb = countOf(b);
    return ca != cb ? ca > cb : a < b;
  });
  return words;
}

std::vector<std::u32string> SoundLexicon::candidates(std::u32string_view roman, std::size_t limit, bool partial,
                                                     const SmartPhoneticOptions& options) const {
  const auto spelled = SmartPhoneticV2::transliterate(roman, options);
  auto key = soundKey(spelled);
  const auto restyled = [&](const std::vector<std::u32string>& words) {
    std::vector<std::u32string> out;
    out.reserve(words.size());
    for (const auto& word : words) out.push_back(restyle(word, options));
    return unique(std::move(out));
  };
  if (partial) {
    // An incomplete word: its last consonant may still take a vowel, so drop a trailing hal.
    if (!key.empty() && key.back() == kHal) key.pop_back();
    auto ranked = restyled(byFrequency(prefix(key)));
    if (ranked.size() > limit) ranked.resize(limit);
    return ranked;
  }
  auto ranked = restyled(byFrequency(exact(key)));
  const bool known = std::find(ranked.begin(), ranked.end(), spelled) != ranked.end();
  if (isExplicit(roman) && (known || isLoneVowel(spelled))) {
    std::erase(ranked, spelled);
    ranked.insert(ranked.begin(), spelled);              // explicit markers beat frequency
  } else if (!known) {
    ranked.push_back(spelled);                           // the rule spelling is always included
  }
  if (ranked.size() > limit) ranked.resize(limit);
  return ranked;
}

std::u32string SoundLexicon::normalize(std::u32string_view s) {
  std::u32string out;
  out.reserve(s.size() + 2);
  for (std::size_t i = 0; i < s.size(); ++i) {
    out.push_back(s[i]);
    // ([ක-ෆ])්(?!ZWJ)(?=([යර]))
    if (s[i] == kHal && i > 0 && isConsonant(s[i - 1]) && i + 1 < s.size() && (s[i + 1] == kYa || s[i + 1] == kRa) &&
        joins(s[i - 1], s[i + 1])) {
      out.push_back(kZwj);
    }
  }
  return out;
}

std::u32string SoundLexicon::soundKey(std::u32string_view text) {
  std::u32string folded(text);
  for (const auto& fold : kFoldSequences) folded = replaceAll(folded, fold.from, fold.to);
  std::u32string out;
  out.reserve(folded.size());
  for (const char32_t c : folded) {
    const auto* fold = std::find_if(std::begin(kFoldChars), std::end(kFoldChars), [c](const Fold& f) { return f.from.front() == c; });
    if (fold != std::end(kFoldChars)) out += fold->to; else out.push_back(c);
  }
  return out;
}

std::u32string SoundLexicon::restyle(std::u32string_view word, const SmartPhoneticOptions& options) {
  std::u32string s(word);
  if (!options.rakaransayaU && !options.repayaZwj && !options.classical) return s;
  if (options.rakaransayaU) {   // ([ක-ෆ])([ෘෲ]) → C ් ZWJ ර ු/ූ, except after ර
    std::u32string out;
    for (std::size_t i = 0; i < s.size();) {
      if (isConsonant(s[i]) && i + 1 < s.size() && (s[i + 1] == U'ෘ' || s[i + 1] == U'ෲ')) {
        out.push_back(s[i]);
        if (s[i] == kRa) {
          out.push_back(s[i + 1]);
        } else {
          out += {kHal, kZwj, kRa, s[i + 1] == U'ෘ' ? U'ු' : U'ූ'};
        }
        i += 2;
      } else {
        out.push_back(s[i++]);
      }
    }
    s = std::move(out);
  }
  if (options.repayaZwj) {   // ර්(?!ZWJ)(?=[ක-ෆ]) → ර් ZWJ
    std::u32string out;
    for (std::size_t i = 0; i < s.size();) {
      if (s[i] == kRa && i + 2 < s.size() && s[i + 1] == kHal && isConsonant(s[i + 2])) {
        out += {kRa, kHal, kZwj};
        i += 2;
      } else {
        out.push_back(s[i++]);
      }
    }
    s = std::move(out);
  }
  if (options.classical) {   // ([ක-ෆ])්(?!ZWJ)(?=([ක-ෆ])) → C ් (ZWJ for the bandi pairs)
    std::u32string out;
    for (std::size_t i = 0; i < s.size();) {
      if (isConsonant(s[i]) && i + 2 < s.size() && s[i + 1] == kHal && isConsonant(s[i + 2])) {
        out += {s[i], kHal};
        if (SmartPhoneticV2::isBandiPair(s[i], s[i + 2])) out.push_back(kZwj);
        i += 2;
      } else {
        out.push_back(s[i++]);
      }
    }
    s = std::move(out);
  }
  return s;
}

bool SoundLexicon::isExplicit(std::u32string_view roman) {
  // [KCGJTDNLPBSWVUIEOAXRMH]|z[a-zA-Z]|aa|ii|uu|ee|oo|ae|thh|dh|kh|gh|chh|jh|ph|bh|x
  constexpr std::u32string_view capitals = U"KCGJTDNLPBSWVUIEOAXRMH";
  constexpr std::u32string_view pairs[] = {U"aa", U"ii", U"uu", U"ee", U"oo", U"ae", U"thh", U"dh",
                                           U"kh", U"gh", U"chh", U"jh", U"ph", U"bh"};
  const auto isLatin = [](char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); };
  for (std::size_t i = 0; i < roman.size(); ++i) {
    const char32_t c = roman[i];
    if (c == U'x' || capitals.find(c) != std::u32string_view::npos) return true;
    if (c == U'z' && i + 1 < roman.size() && isLatin(roman[i + 1])) return true;
    for (const auto pair : pairs) {
      if (startsWith(roman.substr(i), pair)) return true;
    }
  }
  return false;
}

bool SoundLexicon::isLoneVowel(std::u32string_view spelling) {
  return spelling.size() == 1 && spelling[0] >= U'අ' && spelling[0] <= U'ඖ';
}

}  // namespace akshara
