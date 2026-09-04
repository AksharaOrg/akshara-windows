#include "AksharaEngine.h"
#include "SinhalaUnicode.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

namespace {
std::u16string fromUtf8(std::string_view input) {
  std::u16string out;
  for (std::size_t i = 0; i < input.size();) {
    const auto lead = static_cast<unsigned char>(input[i++]);
    char32_t cp = lead;
    int continuation = 0;
    if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F; continuation = 1; }
    else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F; continuation = 2; }
    else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07; continuation = 3; }
    else if (lead >= 0x80) { out.push_back(u'\uFFFD'); continue; }
    bool valid = i + static_cast<std::size_t>(continuation) <= input.size();
    for (int n = 0; valid && n < continuation; ++n) {
      const auto next = static_cast<unsigned char>(input[i++]);
      if ((next & 0xC0) != 0x80) { valid = false; break; }
      cp = (cp << 6) | (next & 0x3F);
    }
    if (!valid || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { out.push_back(u'\uFFFD'); continue; }
    if (cp <= 0xFFFF) out.push_back(static_cast<char16_t>(cp));
    else { cp -= 0x10000; out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10))); out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF))); }
  }
  return out;
}

std::string toUtf8(std::u16string_view input) {
  std::string out;
  for (std::size_t i = 0; i < input.size(); ++i) {
    char32_t cp = input[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < input.size() && input[i + 1] >= 0xDC00 && input[i + 1] <= 0xDFFF)
      cp = 0x10000 + ((cp - 0xD800) << 10) + (input[++i] - 0xDC00);
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
  }
  return out;
}

int failures = 0;
void expect(std::string_view name, std::u16string_view actual, std::u16string_view wanted) {
  if (actual != wanted) {
    std::cerr << "FAIL " << name << ": expected " << toUtf8(wanted) << ", got " << toUtf8(actual) << '\n';
    ++failures;
  }
}

void expectMode(const akshara::AksharaEngine& engine, akshara::InputMode mode, std::string_view input, std::string_view wanted) {
  expect(input, engine.compose(mode, fromUtf8(input)).text, fromUtf8(wanted));
}
}

int main() {
  using akshara::InputMode;
  akshara::AksharaEngine engine;
  expectMode(engine, InputMode::Phonetic, "amma", "අම්ම");
  expectMode(engine, InputMode::Phonetic, "mama", "මම");
  expectMode(engine, InputMode::Phonetic, "siMhala", "සිංහල");
  expectMode(engine, InputMode::Phonetic, "kramaya", "ක්‍රමය");
  expectMode(engine, InputMode::Phonetic, "priya", "ප්‍රිය");
  expectMode(engine, InputMode::SmartPhonetic, "Aa", "ඈ");
  expectMode(engine, InputMode::SmartPhonetic, "x", "ං");
  expectMode(engine, InputMode::SmartPhonetic, "N", "ණ්");
  expectMode(engine, InputMode::SmartPhonetic, "kru", "කෘ");
  expectMode(engine, InputMode::SmartPhonetic, "kruu", "කෲ");
  expect("kombuva", akshara::unicode::normalizeWijesekara(u"ෙකා"), u"කො");
  expect("kombuva-long", akshara::unicode::normalizeWijesekara(u"ෙකා්"), u"කෝ");
  expect("double-kombuva", akshara::unicode::normalizeWijesekara(u"ෙෙක"), u"කෛ");
  expect("independent-aa", akshara::unicode::normalizeWijesekara(u"අා"), u"ආ");
  expect("gayanukitta", akshara::unicode::normalizeWijesekara(u"ගෘෘ"), u"ගෲ");
  expect("rakaransaya", akshara::unicode::normalizeWijesekara(u"ක\uE004"), u"ක්‍ර");
  expect("yansaya", akshara::unicode::normalizeWijesekara(u"ක\uE005"), u"ක්‍ය");
  expect("repaya", akshara::unicode::normalizeWijesekara(u"ක\uE002"), u"ර්‍ක");
  expect("join", akshara::unicode::normalizeWijesekara(u"ක\uE000ෂ"), u"ක්‍ෂ");
  expect("standalone-kombuva", akshara::unicode::normalizeWijesekara(u"ෙ"), u"‌ෙ");
  expect("marked-kombuva", akshara::unicode::markedWijesekara(u"ෙ"), u"");

  expect("key-base", engine.mapWijesekaraKey({0x4C, false, false}), u"ක");
  expect("key-shift", engine.mapWijesekaraKey({0x4C, true, false}), u"ඛ");
  expect("key-altgr", engine.mapWijesekaraKey({0x4F, false, true}), u"ඳ");
  expect("key-oem", engine.mapWijesekaraKey({0xDE, false, false}), u".");
  expect("key-zwnj", engine.mapWijesekaraKey({0x20, false, true}), u"‌");
  expect("key-shift-bracket", engine.mapWijesekaraKey({0xDB, true, false}), u"ඥ");

  akshara::CompositionBuffer buffer(InputMode::Phonetic);
  for (char ch : std::string("amma")) buffer.append(std::u16string(1, static_cast<char16_t>(ch)));
  expect("session", buffer.render(engine).text, u"අම්ම");
  buffer.backspace(); expect("session-backspace", buffer.render(engine).text, u"අම්ම්");
  buffer.clear(); buffer.append(std::u16string(5000, u'a'));
  if (buffer.raw().size() != akshara::CompositionBuffer::kMaxRawCodeUnits) { std::cerr << "FAIL buffer limit\n"; ++failures; }
  expect("malformed-utf16", engine.compose(InputMode::Phonetic, std::u16string_view(u"\xD800", 1)).text, std::u16string_view(u"\xD800", 1));

  std::ifstream fixture(std::string(AKSHARA_FIXTURE_DIR) + "/SLSLexiconStress.tsv", std::ios::binary);
  if (!fixture) { std::cerr << "FAIL cannot open fixture\n"; ++failures; }
  std::string line; std::size_t cases = 0;
  while (std::getline(fixture, line)) {
    if (line.empty() || line.front() == '#') continue;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto tab = line.find('\t');
    if (tab == std::string::npos) { ++failures; continue; }
    ++cases;
    const auto raw = fromUtf8(std::string_view(line).substr(0, tab));
    const auto wanted = fromUtf8(std::string_view(line).substr(tab + 1));
    const auto actual = akshara::unicode::normalizeWijesekara(raw);
    if (actual != wanted) {
      std::cerr << "FAIL fixture " << cases << ": expected " << toUtf8(wanted) << ", got " << toUtf8(actual) << '\n';
      ++failures;
    }
  }
  if (cases != 500) { std::cerr << "FAIL expected 500 fixtures, got " << cases << '\n'; ++failures; }

  std::u16string longInput;
  for (int i = 0; i < 1000; ++i) longInput += u"siMhala";
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100; ++i) (void)engine.compose(InputMode::Phonetic, longInput);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
  if (elapsed > std::chrono::seconds(5)) { std::cerr << "FAIL performance guard: " << elapsed.count() << "ms\n"; ++failures; }

  if (failures == 0) std::cout << "PASS: core mappings, composition, physical keys, 500 SLS fixtures, and performance guard\n";
  return failures == 0 ? 0 : 1;
}
