#pragma once

#include <string>
#include <string_view>

namespace akshara {

// Smart Phonetic v2: a port of to_sinhala() from the Sinhala-Phonetic-Orthography research repo,
// src/sinhala_orthography/romanization.py, with all of its options. The tables mirror the JSON tables in
// its data folder; rule ids refer to its docs/00-rules.md and docs/07-phonetic-romanization.md.
//
// Don't change behaviour here first: change the research repo, then port. tests/core/SmartPhoneticV2Tests.cpp
// checks this port against tests/fixtures/smart_phonetic_v2_golden.tsv, generated from the reference by
// akshara-phonetics/tools/build_golden.py. The same golden file holds the Android and macOS ports.
//
// Text is handled as code points (std::u32string), like the reference's Python strings.
struct SmartPhoneticOptions {
  // Allow ඏ ඐ ෟ ෳ ඎ ඁ ඦ and touching letters (R-14).
  bool archaic{};
  // Write repaya as ර්‍ + C instead of plain ර් + C (R-08).
  bool repayaZwj{};
  // ZWJ conjuncts for the classical bandi akuru pairs (R-10), and rakaransaya after ම න ල (R-07).
  bool classical{};
  // Write C + r + u/uu as rakaransaya + ු/ූ (ක්‍රූර) instead of the usual ෘ/ෲ (කෲර) (R-06).
  bool rakaransayaU{};
  // Write d as ඩ and dh as ද, the older keyboard convention (R-01).
  bool retroflexD{};

  friend bool operator==(const SmartPhoneticOptions&, const SmartPhoneticOptions&) = default;
};

class SmartPhoneticV2 final {
 public:
  static constexpr char32_t kHal = U'්';
  static constexpr char32_t kZwj = U'‍';

  // The converter's spelling of a romanized word or text.
  [[nodiscard]] static std::u32string transliterate(std::u32string_view source, const SmartPhoneticOptions& options = {});
  [[nodiscard]] static std::u16string transliterate(std::u16string_view source, const SmartPhoneticOptions& options = {});

  // G-HC-15, R-10: the classical bandi akuru pairs.
  [[nodiscard]] static bool isBandiPair(char32_t first, char32_t second);
};

}  // namespace akshara
