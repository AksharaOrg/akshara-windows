#include "SmartPhoneticService.h"

#include <algorithm>
#include <iterator>
#include <unordered_set>

namespace akshara {

std::u32string SmartPhoneticService::transliterate(std::u32string_view roman) const {
  return SmartPhoneticV2::transliterate(roman, options);
}

std::optional<std::u32string> SmartPhoneticService::choice(std::u32string_view roman) const {
  if (!lexicon_ || roman.empty()) return std::nullopt;
  auto ranked = words(*lexicon_, roman);
  if (ranked.empty()) return std::nullopt;
  return std::move(ranked.front());
}

std::vector<std::u32string> SmartPhoneticService::candidates(std::u32string_view roman, std::size_t limit) const {
  if (!lexicon_ || roman.empty() || limit == 0) return {};
  auto all = words(*lexicon_, roman);
  const auto completions = byFrequency(*lexicon_, lexicon_->candidates(roman, kPool, true, options));
  all.insert(all.end(), completions.begin(), completions.end());
  std::unordered_set<std::u32string> seen;
  std::vector<std::u32string> out;
  for (auto& word : all) {
    if (out.size() == limit) break;
    if (seen.insert(word).second) out.push_back(std::move(word));
  }
  return out;
}

// Whole words that sound like roman. An explicit spelling (kazda, aa …) that is a word stays first.
std::vector<std::u32string> SmartPhoneticService::words(const SoundLexicon& lexicon, std::u32string_view roman) const {
  const auto all = lexicon.candidates(roman, kPool, false, options);
  const auto spelled = SmartPhoneticV2::transliterate(roman, options);
  // The reference puts an explicit spelling first when it is a word or a lone vowel letter (R ඍ).
  std::optional<std::u32string> pinned;
  if (!all.empty() && all.front() == spelled && SoundLexicon::isExplicit(roman)) pinned = all.front();
  // Candidates come back in the style of the options; keep those whose dictionary spelling is a word.
  std::vector<std::u32string> exact;
  std::copy_if(all.begin(), all.end(), std::back_inserter(exact), [&](const auto& word) { return countOf(lexicon, word) > 0; });
  std::vector<std::u32string> out;
  if (pinned) out.push_back(*pinned);
  for (auto& word : byFrequency(lexicon, std::move(exact))) {
    if (!pinned || word != *pinned) out.push_back(std::move(word));
  }
  return out;
}

// Most frequent first; equal counts keep their order.
std::vector<std::u32string> SmartPhoneticService::byFrequency(const SoundLexicon& lexicon, std::vector<std::u32string> words) const {
  std::vector<std::pair<std::int64_t, std::u32string>> counted;
  counted.reserve(words.size());
  for (auto& word : words) counted.emplace_back(countOf(lexicon, word), std::move(word));
  std::stable_sort(counted.begin(), counted.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  std::vector<std::u32string> out;
  out.reserve(counted.size());
  for (auto& [n, word] : counted) out.push_back(std::move(word));
  return out;
}

// Frequency of a word as shown in the options' style: the most frequent dictionary spelling that restyles to it.
std::int64_t SmartPhoneticService::countOf(const SoundLexicon& lexicon, const std::u32string& word) const {
  if (const auto n = lexicon.countOf(word)) return n;
  std::int64_t best = 0;
  for (const auto& spelling : lexicon.exact(SoundLexicon::soundKey(word))) {
    if (SoundLexicon::restyle(spelling, options) == word) best = std::max(best, lexicon.countOf(spelling));
  }
  return best;
}

}  // namespace akshara
