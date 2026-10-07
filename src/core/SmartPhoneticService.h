#pragma once

#include "SmartPhoneticV2.h"
#include "SoundLexicon.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace akshara {

// Smart Phonetic v2 for the text service: the converter with the user's spelling options, and the word list that
// picks the dictionary spelling of what was typed (හොඳ for "honda"). A port of akshara-mac's
// SmartPhoneticService.swift, which follows Android's PredictionRepository.phoneticChoice / phoneticCandidates,
// ranked by word frequency only: there is no next-word context or learning on Windows yet.
//
// Until a word list is set, the converter's spelling is used as is.
class SmartPhoneticService final {
 public:
  // How many whole words and completions to consider before ranking (Android's PHONETIC_POOL).
  static constexpr std::size_t kPool = 12;

  SmartPhoneticOptions options;

  void setLexicon(std::shared_ptr<const SoundLexicon> lexicon) { lexicon_ = std::move(lexicon); }
  [[nodiscard]] bool isLoaded() const { return lexicon_ != nullptr; }

  // The converter's spelling of a romanized word with the current options.
  [[nodiscard]] std::u32string transliterate(std::u32string_view roman) const;
  // The word Space commits for a romanized word, or nullopt to keep the converter's spelling.
  [[nodiscard]] std::optional<std::u32string> choice(std::u32string_view roman) const;
  // Whole words that sound like roman, then completions of it, most frequent first.
  [[nodiscard]] std::vector<std::u32string> candidates(std::u32string_view roman, std::size_t limit) const;

 private:
  [[nodiscard]] std::vector<std::u32string> words(const SoundLexicon& lexicon, std::u32string_view roman) const;
  [[nodiscard]] std::vector<std::u32string> byFrequency(const SoundLexicon& lexicon, std::vector<std::u32string> words) const;
  [[nodiscard]] std::int64_t countOf(const SoundLexicon& lexicon, const std::u32string& word) const;

  std::shared_ptr<const SoundLexicon> lexicon_;
};

}  // namespace akshara
