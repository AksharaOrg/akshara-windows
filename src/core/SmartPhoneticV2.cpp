#include "SmartPhoneticV2.h"

#include "Utf.h"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>

namespace akshara {
namespace {

constexpr char32_t kHal = SmartPhoneticV2::kHal;
constexpr char32_t kZwj = SmartPhoneticV2::kZwj;
constexpr char32_t kAnusvara = U'ං';
constexpr char32_t kNga = U'ඞ';

bool in(std::u32string_view set, char32_t c) { return set.find(c) != std::u32string_view::npos; }

constexpr std::u32string_view kSanyaka = U"ඟඦඬඳඹ";
constexpr std::u32string_view kNoHal = U"ඟඦඬඳඹළ";           // G-HC-06, G-HC-07
constexpr std::u32string_view kPlainBeforeRa = U"මනල";       // R-07: දුම්රිය, හෙන්රි
constexpr std::u32string_view kVelars = U"කඛගඝ";            // R-11

// R-01 retroflexD: d ඩ · dh ද · D ඪ · Dh ධ · zd ඬ (q, dhh, zdh, zq and zD keep their letters). 0: unchanged.
char32_t retroflexD(std::u32string_view seq) {
  if (seq == U"d") return U'ඩ';
  if (seq == U"dh") return U'ද';
  if (seq == U"D") return U'ඪ';
  if (seq == U"Dh") return U'ධ';
  if (seq == U"zd") return U'ඬ';
  return 0;
}
// C-13: ෘ / ෲ only after the consonants where the form is attested (validity.json: valid, loan or rare).
constexpr std::u32string_view kGaettaAfterU = U"කගඝජටඩතදධනපබභමවශසහෆ";
constexpr std::u32string_view kGaettaAfterUu = U"කගටඩතදපබම";

// G-PH-01: no sanyaka word-initially.
std::optional<char32_t> plain(char32_t letter) {
  switch (letter) {
    case U'ඟ': return U'ග';
    case U'ඦ': return U'ජ';
    case U'ඬ': return U'ඩ';
    case U'ඳ': return U'ද';
    case U'ඹ': return U'බ';
    default: return std::nullopt;
  }
}

bool isFront(std::string_view id) {
  return id == "i" || id == "ii" || id == "e" || id == "ee" || id == "ae" || id == "aee" || id == "ai";
}
bool isBack(std::string_view id) { return id == "u" || id == "uu" || id == "o" || id == "oo" || id == "au"; }

enum class Kind { Consonant, Vowel, Mark, Touch, Literal };

struct Token {
  Kind kind{};
  char32_t letter{};          // consonant letter, independent vowel, mark output or literal
  char32_t sign{};            // vowel sign; 0 for the inherent a
  std::string_view id;        // vowel id
  std::u32string_view seq;    // the romanization the token was read from
};

struct ConsonantRow { std::u32string_view seq; char32_t letter; bool archaic; };
struct VowelRow { std::u32string_view seq; std::string_view id; char32_t independent; char32_t sign; bool archaic; };
struct MarkRow { std::u32string_view seq; char32_t output; bool archaic; };

constexpr ConsonantRow kConsonants[] = {
    {U"k", U'ක', false}, {U"c", U'ක', false}, {U"kh", U'ඛ', false}, {U"K", U'ඛ', false}, {U"C", U'ඛ', false},
    {U"g", U'ග', false}, {U"gh", U'ඝ', false}, {U"G", U'ඝ', false}, {U"X", U'ඞ', false}, {U"zg", U'ඟ', false},
    {U"ch", U'ච', false}, {U"chh", U'ඡ', false}, {U"j", U'ජ', false}, {U"jh", U'ඣ', false}, {U"J", U'ඣ', false},
    {U"zk", U'ඤ', false}, {U"zh", U'ඥ', false}, {U"t", U'ට', false}, {U"T", U'ඨ', false}, {U"D", U'ඩ', false},
    {U"Dh", U'ඪ', false}, {U"N", U'ණ', false}, {U"zD", U'ඬ', false}, {U"th", U'ත', false}, {U"thh", U'ථ', false},
    {U"d", U'ද', false}, {U"q", U'ද', false}, {U"dh", U'ධ', false}, {U"dhh", U'ධ', false}, {U"n", U'න', false},
    {U"zd", U'ඳ', false}, {U"zdh", U'ඳ', false}, {U"zq", U'ඳ', false}, {U"p", U'ප', false}, {U"ph", U'ඵ', false},
    {U"P", U'ඵ', false}, {U"b", U'බ', false}, {U"bh", U'භ', false}, {U"m", U'ම', false}, {U"B", U'ඹ', false},
    {U"y", U'ය', false}, {U"r", U'ර', false}, {U"l", U'ල', false}, {U"w", U'ව', false}, {U"v", U'ව', false},
    {U"W", U'ව', false}, {U"V", U'ව', false}, {U"sh", U'ශ', false}, {U"Sh", U'ෂ', false}, {U"S", U'ෂ', false},
    {U"s", U'ස', false}, {U"h", U'හ', false}, {U"L", U'ළ', false}, {U"f", U'ෆ', false},
    {U"zj", U'ඦ', true},                                                                    // R-14
};

constexpr VowelRow kVowels[] = {
    {U"a", "a", U'අ', 0, false}, {U"aa", "aa", U'ආ', U'ා', false}, {U"A", "ae", U'ඇ', U'ැ', false},
    {U"ae", "ae", U'ඇ', U'ැ', false}, {U"Aa", "aee", U'ඈ', U'ෑ', false}, {U"AA", "aee", U'ඈ', U'ෑ', false},
    {U"aee", "aee", U'ඈ', U'ෑ', false}, {U"i", "i", U'ඉ', U'ි', false}, {U"ii", "ii", U'ඊ', U'ී', false},
    {U"I", "ii", U'ඊ', U'ී', false}, {U"u", "u", U'උ', U'ු', false}, {U"U", "u", U'උ', U'ු', false},
    {U"uu", "uu", U'ඌ', U'ූ', false}, {U"UU", "uu", U'ඌ', U'ූ', false}, {U"Uu", "uu", U'ඌ', U'ූ', false},
    {U"R", "ru", U'ඍ', U'ෘ', false}, {U"RR", "ruu", U'ඎ', U'ෲ', false}, {U"e", "e", U'එ', U'ෙ', false},
    {U"ee", "ee", U'ඒ', U'ේ', false}, {U"E", "ai", U'ඓ', U'ෛ', false}, {U"o", "o", U'ඔ', U'ො', false},
    {U"O", "o", U'ඔ', U'ො', false}, {U"oo", "oo", U'ඕ', U'ෝ', false}, {U"OO", "oo", U'ඕ', U'ෝ', false},
    {U"Oo", "oo", U'ඕ', U'ෝ', false}, {U"Au", "au", U'ඖ', U'ෞ', false}, {U"AU", "au", U'ඖ', U'ෞ', false},
    {U"~l", "ilu", U'ඏ', U'ෟ', true}, {U"~ll", "iluu", U'ඐ', U'ෳ', true},                    // R-14
};

constexpr MarkRow kMarks[] = {
    {U"x", kAnusvara, false}, {U"zn", kAnusvara, false}, {U"M", kAnusvara, false},
    {U"H", U'ඃ', false}, {U"~n", U'ඁ', true},                                               // R-14, G-NS-06
};

using SequenceTable = std::unordered_map<char32_t, std::vector<Token>>;

// All sequences for a mode, longest first; ties keep table order (consonants, vowels, marks).
// Grouped by first code point for lookup.
SequenceTable buildSequences(bool archaic) {
  std::vector<Token> all;
  for (const auto& row : kConsonants)
    if (archaic || !row.archaic) all.push_back({Kind::Consonant, row.letter, 0, {}, row.seq});
  for (const auto& row : kVowels)
    if (archaic || !row.archaic) all.push_back({Kind::Vowel, row.independent, row.sign, row.id, row.seq});
  for (const auto& row : kMarks)
    if (archaic || !row.archaic) all.push_back({Kind::Mark, row.output, 0, {}, row.seq});
  if (archaic) all.push_back({Kind::Touch, U'+', 0, {}, U"+"});                              // R-14, G-HC-17
  std::stable_sort(all.begin(), all.end(), [](const Token& a, const Token& b) { return a.seq.size() > b.seq.size(); });
  SequenceTable table;
  for (const auto& token : all) table[token.seq.front()].push_back(token);
  return table;
}

const SequenceTable& sequences(bool archaic) {
  static const SequenceTable normal = buildSequences(false);
  static const SequenceTable withArchaic = buildSequences(true);
  return archaic ? withArchaic : normal;
}

std::vector<Token> tokenize(std::u32string_view source, bool archaic) {
  const auto& table = sequences(archaic);
  std::vector<Token> tokens;
  tokens.reserve(source.size());
  std::size_t i = 0;
  while (i < source.size()) {
    const Token* match = nullptr;
    if (const auto found = table.find(source[i]); found != table.end()) {
      for (const auto& token : found->second) {
        if (source.substr(i, token.seq.size()) == token.seq) { match = &token; break; }
      }
    }
    if (match) {
      tokens.push_back(*match);
      i += match->seq.size();
    } else {
      if (source[i] != U'z') tokens.push_back({Kind::Literal, source[i], 0, {}, source.substr(i, 1)});   // an unknown z-combination is dropped
      ++i;
    }
  }
  return tokens;
}

// G-VS-06: ය after front vowels, ව after back; after a/aa, decided by the next vowel.
char32_t glide(std::string_view previous, std::string_view vowel) {
  if (isFront(previous)) return U'ය';
  if (isBack(previous)) return U'ව';
  return isFront(vowel) ? U'ය' : U'ව';
}

enum class State { WordStart, Vowel, Anusvara, Hal };

}  // namespace

bool SmartPhoneticV2::isBandiPair(char32_t first, char32_t second) {
  static constexpr std::u32string_view pairs[] = {U"කෂ", U"කව", U"ගධ", U"ටඨ", U"තථ", U"තව", U"දධ",
                                                  U"දව", U"නථ", U"නද", U"නධ", U"නව", U"ඤච"};
  const char32_t pair[] = {first, second};
  return std::find(std::begin(pairs), std::end(pairs), std::u32string_view(pair, 2)) != std::end(pairs);
}

std::u32string SmartPhoneticV2::transliterate(std::u32string_view source, const SmartPhoneticOptions& options) {
  auto tokens = tokenize(source, options.archaic);
  if (options.retroflexD)
    for (auto& token : tokens)
      if (token.kind == Kind::Consonant)
        if (const auto letter = retroflexD(token.seq)) token.letter = letter;
  std::u32string out;
  out.reserve(tokens.size() * 2);
  auto state = State::WordStart;
  std::string_view previousVowel;
  std::size_t j = 0;
  while (j < tokens.size()) {
    const Token& token = tokens[j];
    const Token* next = j + 1 < tokens.size() ? &tokens[j + 1] : nullptr;
    const Token* after = j + 2 < tokens.size() ? &tokens[j + 2] : nullptr;
    const auto nextIs = [&](Kind kind) { return next && next->kind == kind; };

    switch (token.kind) {
      case Kind::Consonant: {
        char32_t letter = token.letter;
        if (state == State::WordStart) {
          if (const auto plainLetter = plain(letter)) letter = *plainLetter;
        }
        if (letter == kNga && nextIs(Kind::Vowel)) {
          if (state == State::WordStart) { out += token.seq; ++j; continue; }   // G-PH-01: ඞ never starts a word
          letter = U'ඟ';                                                          // G-HC-08: /ŋ/ + vowel is ඟ
        }
        if (letter == kNga && state == State::WordStart) { out += token.seq; ++j; continue; }
        // R-11: n + velar after a vowel → ං; word-final "ng" → ං
        if (letter == U'න' && token.seq == U"n" && state == State::Vowel && nextIs(Kind::Consonant) &&
            in(kVelars, next->letter)) {
          out.push_back(kAnusvara);
          state = State::Anusvara;
          const bool wordFinal = !after || after->kind == Kind::Literal;
          j += next->seq == U"g" && wordFinal ? 2 : 1;
          continue;
        }
        out.push_back(letter);
        if (nextIs(Kind::Vowel)) {
          if (next->sign) out.push_back(next->sign);
          state = State::Vowel;
          previousVowel = next->id;
          j += 2;
        } else if (nextIs(Kind::Mark)) {
          state = State::Vowel;                                  // ං/ඃ/ඁ need a vowel base: keep inherent a
          previousVowel = "a";
          ++j;
        } else if (nextIs(Kind::Consonant)) {
          const char32_t nextLetter = next->letter;
          if (in(kNoHal, letter) || in(kSanyaka, nextLetter)) {   // no hal here: keep inherent a
            state = State::Vowel;
            previousVowel = "a";
            ++j;
            continue;
          }
          if (letter == kNga) {
            out.push_back(kHal);
          } else if (nextLetter == U'ය') {
            out.push_back(kHal);                                   // G-HC-11, G-HC-14, R-09
            if (letter != U'ර' || options.repayaZwj) out.push_back(kZwj);
          } else if (nextLetter == U'ර') {
            const std::string_view vowel = after && after->kind == Kind::Vowel ? after->id : std::string_view{};
            const bool ru = vowel == "u" || vowel == "uu";
            const bool attested = ru && in(vowel == "u" ? kGaettaAfterU : kGaettaAfterUu, letter);   // C-13: මෘ, not ලෘ
            if (attested && !options.rakaransayaU) {
              out.push_back(vowel == "u" ? U'ෘ' : U'ෲ');            // G-VS-15, R-06
              state = State::Vowel;
              previousVowel = vowel;
              j += 3;
              continue;
            }
            // R-07: plain hal after ම න ල (දුම්රිය, දිල්රුක්ෂි), except a rakaransaya that stands for an
            // attested ෘ (rakaransayaU: ම්‍රුදු) or, without u, under classical (තාම්‍ර)
            const bool plainHal = letter == U'ර' ||
                                  (in(kPlainBeforeRa, letter) && !attested && !(options.classical && !ru));
            out.push_back(kHal);                                   // G-HC-12, R-07
            if (!plainHal) out.push_back(kZwj);
          } else if (letter == U'ර') {
            out.push_back(kHal);                                   // R-08
            if (options.repayaZwj) out.push_back(kZwj);
          } else if (options.classical && isBandiPair(letter, nextLetter)) {
            out.push_back(kHal);                                   // R-10
            out.push_back(kZwj);
          } else {
            out.push_back(kHal);
          }
          state = State::Hal;
          ++j;
        } else if (nextIs(Kind::Touch) && after && after->kind == Kind::Consonant && !in(kNoHal, letter)) {
          out.push_back(kZwj);                                     // R-14: touching letters
          out.push_back(kHal);
          state = State::Hal;
          j += 2;
        } else {                                                   // end of word
          if (in(kNoHal, letter)) {
            state = State::Vowel;
            previousVowel = "a";
          } else {
            out.push_back(kHal);
            state = State::Hal;
          }
          ++j;
        }
        break;
      }
      case Kind::Vowel:
        if (state == State::Vowel) {
          out.push_back(glide(previousVowel, token.id));
          if (token.sign) out.push_back(token.sign);
        } else if (state == State::Anusvara) {
          out.back() = U'ම';                                       // G-NS-04: ං never before a vowel
          if (token.sign) out.push_back(token.sign);
        } else {
          out.push_back(token.id == "ruu" && !options.archaic ? U'ඍ' : token.letter);   // G-VS-08: ඎ is archaic
        }
        state = State::Vowel;
        previousVowel = token.id;
        ++j;
        break;
      case Kind::Mark:
        if (token.letter == kAnusvara && nextIs(Kind::Consonant) && in(kSanyaka, next->letter)) {
          ++j;                                                     // G-NS-09: the sanyaka carries the nasal
          break;
        }
        if (state == State::Vowel) {
          out.push_back(token.letter);
          state = token.letter == kAnusvara ? State::Anusvara : State::Vowel;
        } else {
          out += token.seq;                                        // no base: leave the romanization as written
          state = State::WordStart;
        }
        ++j;
        break;
      case Kind::Touch:
        out.push_back(U'+');
        state = State::WordStart;
        ++j;
        break;
      case Kind::Literal:
        out.push_back(token.letter);
        state = State::WordStart;
        previousVowel = {};
        ++j;
        break;
    }
  }
  return out;
}

std::u16string SmartPhoneticV2::transliterate(std::u16string_view source, const SmartPhoneticOptions& options) {
  return utf::toUtf16(transliterate(utf::fromUtf16(source), options));
}

}  // namespace akshara
