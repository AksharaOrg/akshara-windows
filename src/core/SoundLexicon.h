#pragma once

#include "SmartPhoneticV2.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace akshara {

// Sound-alike disambiguation for Smart Phonetic v2: a port of lexicon.py from the Sinhala-Phonetic-Orthography
// research repo. Words are indexed by a sound key that erases the distinctions speakers don't hear or don't
// write in Latin script (aspiration, ණ/න, ළ/ල, ශ/ෂ/ස, ද/ඩ, vowel length, sanyaka vs cluster …), so "honda"
// finds හොඳ although the rules spell හොන්ද. Words are returned in the style of the converter options
// (restyle), so a word list in the usual style (කෲර, කර්ම) doesn't undo the user's spelling options (ක්‍රූර, කර්‍ම).
// A list with no ZWJ at all is repaired with normalize; one that has ZWJ is used as written, since normalize
// would also join across word boundaries (බවත්ය).
//
// Ordering and prefixes work on code points, like the reference's Python strings. Checked against the
// reference by tests/core/SmartPhoneticV2Tests.cpp.
class SoundLexicon final {
 public:
  using Row = std::pair<std::u32string, std::int64_t>;

  explicit SoundLexicon(const std::vector<Row>& rows);

  // Parses "word<TAB>count" lines (UTF-8) like the reference: rows whose count isn't a number are skipped.
  [[nodiscard]] static std::vector<Row> parse(std::string_view utf8);

  [[nodiscard]] std::size_t size() const { return count_.size(); }
  // The word's frequency, or 0 when it isn't in the list.
  [[nodiscard]] std::int64_t countOf(const std::u32string& word) const;
  [[nodiscard]] const std::vector<std::u32string>& exact(const std::u32string& key) const;
  [[nodiscard]] std::vector<std::u32string> prefix(std::u32string_view key, std::size_t limit = 200) const;
  // True when some word's sound key starts with key.
  [[nodiscard]] bool hasPrefix(std::u32string_view key) const;

  // Ranked Sinhala spellings for a romanized word, or for a word prefix with partial.
  [[nodiscard]] std::vector<std::u32string> candidates(std::u32string_view roman, std::size_t limit = 5, bool partial = false,
                                                       const SmartPhoneticOptions& options = {}) const;

  // Restores the mandatory ZWJ in yansaya and rakaransaya, for word lists that dropped it.
  [[nodiscard]] static std::u32string normalize(std::u32string_view word);
  [[nodiscard]] static std::u32string soundKey(std::u32string_view text);
  // Writes a word in the style the converter options choose, as SmartPhoneticV2 would (restyle() in the
  // reference): rakaransaya + u for C + ෘ/ෲ (R-06), ZWJ repaya (R-08), classical bandi akuru (R-10).
  // With no options the word is unchanged.
  [[nodiscard]] static std::u32string restyle(std::u32string_view word, const SmartPhoneticOptions& options);
  // The romanization has a marker that pins down a distinction the sound key erases (a capital, a z- prefix,
  // a doubled vowel …).
  [[nodiscard]] static bool isExplicit(std::u32string_view roman);
  // One independent vowel letter (අ … ඖ). A list has no such words, yet a letter typed on its own is meant as
  // that letter: frequency would turn ඍ into රු and ඓ into අයි.
  [[nodiscard]] static bool isLoneVowel(std::u32string_view spelling);

 private:
  [[nodiscard]] std::size_t firstKeyAtOrAfter(std::u32string_view key) const;
  // Most frequent first, then in code point order.
  [[nodiscard]] std::vector<std::u32string> byFrequency(std::vector<std::u32string> words) const;

  std::unordered_map<std::u32string, std::int64_t> count_;
  std::unordered_map<std::u32string, std::vector<std::u32string>> byKey_;
  std::vector<std::u32string> keys_;
};

}  // namespace akshara
