// The Smart Phonetic v2 port must reproduce the research repo's reference exactly. The golden file comes from
// akshara-phonetics/tools/build_golden.py; regenerate it when the reference changes. The word list comes from
// akshara-phonetics/tools/sync_word_data.py. Strings are compared code point by code point, as the reference does.

#include "SmartPhoneticService.h"
#include "SmartPhoneticV2.h"
#include "SoundLexicon.h"
#include "Utf.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
using akshara::SmartPhoneticOptions;
using akshara::SmartPhoneticService;
using akshara::SmartPhoneticV2;
using akshara::SoundLexicon;
using akshara::utf::fromUtf8;
using akshara::utf::toUtf8;

bool failed = false;

std::string readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::cerr << "FAIL cannot read " << path << '\n';
    std::exit(1);
  }
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

std::vector<std::vector<std::string>> readGolden(const std::string& path) {
  std::vector<std::vector<std::string>> rows;
  std::istringstream lines(readFile(path));
  for (std::string line; std::getline(lines, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (std::size_t tab; (tab = line.find('\t', start)) != std::string::npos; start = tab + 1) fields.push_back(line.substr(start, tab - start));
    fields.push_back(line.substr(start));
    rows.push_back(std::move(fields));
  }
  return rows;
}

SmartPhoneticOptions option(const std::string& name) {
  SmartPhoneticOptions options;
  if (name == "archaic") options.archaic = true;
  else if (name == "repaya_zwj") options.repayaZwj = true;
  else if (name == "classical") options.classical = true;
  else if (name == "rakaransaya_u") options.rakaransayaU = true;
  else if (name == "retroflex_d") options.retroflexD = true;
  else { std::cerr << "unknown option " << name << '\n'; std::exit(1); }
  return options;
}

std::string joined(const std::vector<std::u32string>& words) {
  std::string out;
  for (std::size_t i = 0; i < words.size(); ++i) out += (i ? "|" : "") + toUtf8(words[i]);
  return out;
}

void expect(const std::string& name, const std::string& actual, const std::string& wanted) {
  if (actual != wanted) {
    failed = true;
    std::cout << "FAIL " << name << ": expected " << wanted << ", got " << actual << '\n';
  }
}

void expect(const std::string& name, const std::u32string& actual, const std::string& wanted) {
  expect(name, toUtf8(actual), wanted);
}

std::string orNil(const std::optional<std::u32string>& word) { return word ? toUtf8(*word) : "nil"; }

using Row = std::vector<std::string>;

void check(const std::vector<Row>& golden, const std::string& kind, const std::string& name, std::size_t fields,
           const std::function<std::string(const Row&)>& actual) {
  std::size_t total = 0;
  std::vector<std::string> failures;
  for (const auto& row : golden) {
    if (row[0] != kind) continue;
    ++total;
    if (row.size() != fields) {
      failures.push_back("malformed row: " + row[1]);
      continue;
    }
    const auto got = actual(row);
    if (got != row.back()) {
      std::string input;
      for (std::size_t i = 1; i + 1 < row.size(); ++i) input += (i > 1 ? " " : "") + row[i];
      failures.push_back(input + ": expected " + row.back() + ", got " + got);
    }
  }
  if (total == 0) {
    failed = true;
    std::cout << "FAIL " << name << ": no " << kind << " rows\n";
  } else if (failures.empty()) {
    std::cout << "ok   " << name << ": " << total << " rows\n";
  } else {
    failed = true;
    std::cout << "FAIL " << name << ": " << failures.size() << "/" << total << " mismatches\n";
    for (std::size_t i = 0; i < failures.size() && i < 20; ++i) std::cout << "     " << failures[i] << '\n';
  }
}
}  // namespace

int main(int argc, char** argv) {
  const std::string root = argc > 1 ? argv[1] : AKSHARA_SOURCE_DIR;
  const auto golden = readGolden(root + "/tests/fixtures/smart_phonetic_v2_golden.tsv");
  const auto started = std::chrono::steady_clock::now();
  const auto lexicon = std::make_shared<const SoundLexicon>(SoundLexicon::parse(readFile(root + "/data/sinhala_frequency_model.tsv")));
  const auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
  std::cout << "lexicon: " << lexicon->size() << " words in " << loadMs << " ms\n";

  const auto v2 = [](const std::string& roman, const SmartPhoneticOptions& options = {}) {
    return toUtf8(SmartPhoneticV2::transliterate(fromUtf8(roman), options));
  };
  check(golden, "T", "transliteration", 3, [&](const Row& r) { return v2(r[1]); });
  check(golden, "O", "options", 4, [&](const Row& r) { return v2(r[2], option(r[1])); });
  check(golden, "R", "restyle", 4, [](const Row& r) { return toUtf8(SoundLexicon::restyle(fromUtf8(r[2]), option(r[1]))); });
  check(golden, "K", "sound key", 3, [](const Row& r) { return toUtf8(SoundLexicon::soundKey(fromUtf8(r[1]))); });
  check(golden, "N", "normalize", 3, [](const Row& r) { return toUtf8(SoundLexicon::normalize(fromUtf8(r[1]))); });
  check(golden, "C", "candidates", 4, [&](const Row& r) { return joined(lexicon->candidates(fromUtf8(r[1]), 5, r[2] == "1")); });
  check(golden, "D", "candidates with options", 5,
        [&](const Row& r) { return joined(lexicon->candidates(fromUtf8(r[2]), 5, r[3] == "1", option(r[1]))); });

  // Everyday words and R-07, as in Android's SmartPhoneticV2Test.
  const std::string z = "‍";
  const std::map<std::string, std::string> words = {
      {"lankaava", "ලංකාව"}, {"kruura", "කෲර"}, {"lait", "ලයිට්"}, {"kramaya", "ක්" + z + "රමය"}, {"d", "ද්"},
      {"D", "ඩ්"}, {"ee", "ඒ"}, {"ai", "අයි"}, {"Au", "ඖ"}, {"kaaryaya", "කාර්යය"}, {"dumriya", "දුම්රිය"},
      {"henri", "හෙන්රි"}, {"dilrukshi", "දිල්රුක්ශි"}, {"mrudu", "මෘදු"}, {"samruddhi", "සමෘද්ධි"},
  };
  for (const auto& [roman, wanted] : words) expect(roman, v2(roman), wanted);
  SmartPhoneticOptions classical;
  classical.classical = true;
  expect("thaamra (classical)", v2("thaamra", classical), "තාම්" + z + "ර");
  SmartPhoneticOptions styled;
  styled.rakaransayaU = true;
  styled.repayaZwj = true;
  expect("kruura (rakaransaya_u)", v2("kruura", styled), "ක්" + z + "රූර");
  expect("karma (repaya_zwj)", v2("karma", styled), "කර්" + z + "ම");
  SmartPhoneticOptions singlish;
  singlish.retroflexD = true;
  expect("bada (retroflex_d)", v2("bada", singlish), "බඩ");
  expect("kohomadha (retroflex_d)", v2("kohomadha", singlish), "කොහොමද");
  expect("Dharmaya (retroflex_d)", v2("Dharmaya", singlish), "ධර්මය");
  expect("da (default)", v2("da"), "ද");
  const auto first = [&](const std::string& roman) {
    const auto all = lexicon->candidates(fromUtf8(roman));
    return all.empty() ? std::string() : toUtf8(all.front());
  };
  expect("honda", first("honda"), "හොඳ");
  expect("kramaya candidate", first("kramaya"), "ක්" + z + "රමය");
  expect("dumriya candidate", first("dumriya"), "දුම්රිය");
  expect("normalize දුම්රිය", SoundLexicon::normalize(fromUtf8("දුම්රිය")), "දුම්රිය");
  expect("hasPrefix", lexicon->hasPrefix(SoundLexicon::soundKey(fromUtf8("ලංක"))) ? "yes" : "no", "yes");

  // The text service's choices, as in Android's SmartPhoneticV2IntegrationTest and the macOS service tests.
  SmartPhoneticService service;
  expect("no word list yet", orNil(service.choice(U"honda")), "nil");
  service.setLexicon(lexicon);
  expect("honda: rules", service.transliterate(U"honda"), "හොන්ද");
  expect("honda: Space choice", orNil(service.choice(U"honda")), "හොඳ");
  const auto firstCandidate = [&](const std::u32string& roman) {
    const auto all = service.candidates(roman, 5);
    return all.empty() ? std::string() : toUtf8(all.front());
  };
  expect("honda: first candidate", firstCandidate(U"honda"), "හොඳ");
  expect("hond: completion", firstCandidate(U"hond"), "හොඳ");
  expect("kazda: explicit spelling kept", orNil(service.choice(U"kazda")), "කඳ");
  expect("kramaya: not in the list, rules kept", orNil(service.choice(U"kramaya")), "nil");
  // A lone vowel letter typed with a marker stays; unmarked vowels are matched by sound.
  const std::pair<std::u32string, std::string> vowels[] = {{U"A", "ඇ"}, {U"Aa", "ඈ"}, {U"R", "ඍ"}, {U"E", "ඓ"},
                                                           {U"Au", "ඖ"}, {U"O", "ඔ"}, {U"e", "ඒ"}, {U"o", "ඕ"}};
  for (const auto& [roman, letter] : vowels) expect("lone vowel " + toUtf8(roman), orNil(service.choice(roman)), letter);
  // The dictionary examples in the macOS typing guide.
  const std::pair<std::u32string, std::string> guide[] = {{U"honda", "හොඳ"}, {U"sinhala", "සිංහල"}, {U"bada", "බඩ"},
                                                          {U"lamaya", "ළමයා"}, {U"pilithura", "පිළිතුර"}, {U"kalu", "කළු"},
                                                          {U"keema", "කෑම"}, {U"amma", "අම්මා"}};
  for (const auto& [roman, word] : guide) expect("guide: " + toUtf8(roman), orNil(service.choice(roman)), word);
  service.options.rakaransayaU = true;
  service.options.repayaZwj = true;
  expect("kruura: style kept", orNil(service.choice(U"kruura")), "ක්" + z + "රූර");
  expect("karma: style kept", orNil(service.choice(U"karma")), "කර්" + z + "ම");

  if (failed) return 1;
  std::cout << "Smart Phonetic v2 tests passed\n";
  return 0;
}
