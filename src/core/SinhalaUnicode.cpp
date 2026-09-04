#include "SinhalaUnicode.h"

namespace akshara::unicode {
namespace {
bool isTrailing(char16_t ch) {
  return (ch >= u'\u0DCF' && ch <= u'\u0DD4') || ch == u'\u0DD6' || ch == u'\u0DD8' ||
         ch == u'\u0DDF' || ch == u'\u0DF2' || ch == u'\u0DF3';
}
bool isSemi(char16_t ch) { return ch == u'\u0D82' || ch == u'\u0D83'; }
void appendStandalone(char16_t sign, std::u16string& out) { out.push_back(u'\u200C'); out.push_back(sign); }

std::u16string sanyaka(char16_t base) {
  switch (base) {
    case u'\u0D9C': return u"ඟ";
    case u'\u0DA2': return u"ඦ";
    case u'\u0DA9': return u"ඬ";
    case u'\u0DAF': return u"ඳ";
    default: return {};
  }
}

std::u16string independent(char16_t vowel, std::u16string_view in, std::size_t& i) {
  if (i < in.size()) {
    const auto next = in[i];
    if (vowel == u'\u0D85') {
      if (next == u'\u0DCF') { ++i; return u"ආ"; }
      if (next == u'\u0DD0') { ++i; return u"ඇ"; }
      if (next == u'\u0DD1') { ++i; return u"ඈ"; }
    }
    if (vowel == u'\u0D91' && next == u'\u0DCA') { ++i; return u"ඒ"; }
    if (vowel == u'\u0D89' && next == u'\u0DD3') { ++i; return u"ඊ"; }
    if (vowel == u'\u0D94' && next == u'\u0DCA') { ++i; return u"ඕ"; }
    if (vowel == u'\u0D94' && (next == u'\u0DDF' || next == u'\u0DD6')) { ++i; return u"ඖ"; }
    if (vowel == u'\u0D8B' && (next == u'\u0DDF' || next == u'\u0DD6')) { ++i; return u"ඌ"; }
    if (vowel == u'\u0D8D' && next == u'\u0DD8') { ++i; return u"ඎ"; }
  }
  return std::u16string(1, vowel);
}

std::u16string cluster(char16_t base, std::u16string_view in, std::size_t& i) {
  std::u16string out(1, base);
  while (i < in.size()) {
    const auto marker = in[i];
    if ((marker == kJoin || marker == kTouch) && i + 1 < in.size() && isConsonant(in[i + 1])) {
      out += u"්‍"; out.push_back(in[i + 1]); i += 2; continue;
    }
    if (marker == kRakaransaya) { out += u"්‍ර"; ++i; continue; }
    if (marker == kYansaya) { out += u"්‍ය"; ++i; continue; }
    if (marker == kRepaya) { out.insert(0, u"ර්‍"); ++i; continue; }
    break;
  }
  return out;
}

std::u16string vowelSign(std::size_t prefixes, std::u16string_view in, std::size_t& i) {
  if (prefixes >= 2) return u"ෛ";
  if (prefixes == 1) {
    if (i < in.size()) {
      const auto next = in[i];
      if (next == u'\u0DCA') { ++i; return u"ේ"; }
      if (next == u'\u0DCF') {
        ++i;
        if (i < in.size() && in[i] == u'\u0DCA') { ++i; return u"ෝ"; }
        return u"ො";
      }
      if (next == u'\u0DDF') { ++i; return u"ෞ"; }
    }
    return u"ෙ";
  }
  if (i < in.size() && (isTrailing(in[i]) || in[i] == u'\u0DCA')) {
    const auto next = in[i++];
    if (next == u'\u0DD8' && i < in.size() && in[i] == u'\u0DD8') { ++i; return u"ෲ"; }
    return std::u16string(1, next);
  }
  return {};
}

void appendSemi(std::u16string_view in, std::size_t& i, std::u16string& out) {
  while (i < in.size() && isSemi(in[i])) out.push_back(in[i++]);
}
}

bool isSinhalaScalar(char16_t ch) { return ch >= u'\u0D80' && ch <= u'\u0DFF'; }
bool isConsonant(char16_t ch) { return ch >= u'\u0D9A' && ch <= u'\u0DC6'; }
bool isIndependentVowel(char16_t ch) { return ch >= u'\u0D85' && ch <= u'\u0D96'; }
bool isDependentVowelSign(char16_t ch) {
  return (ch >= u'\u0DCF' && ch <= u'\u0DD4') || ch == u'\u0DD6' ||
         (ch >= u'\u0DD8' && ch <= u'\u0DDF') || ch == u'\u0DF2' || ch == u'\u0DF3';
}

std::u16string normalizeWijesekara(std::u16string_view input) {
  std::u16string out;
  out.reserve(input.size() + 8);
  std::size_t i = 0;
  while (i < input.size()) {
    const auto ch = input[i];
    if (ch == u'\u0DD9') {
      std::size_t prefixes = 0;
      while (i < input.size() && input[i] == u'\u0DD9') { ++prefixes; ++i; }
      if (i < input.size() && isConsonant(input[i])) {
        const auto base = input[i++];
        std::u16string syllable;
        if (i < input.size() && input[i] == kSanyakaya) {
          syllable = sanyaka(base);
          if (!syllable.empty()) ++i;
        }
        if (syllable.empty()) syllable = cluster(base, input, i);
        syllable += vowelSign(prefixes, input, i);
        appendSemi(input, i, syllable);
        out += syllable;
        continue;
      }
      if (prefixes == 1 && i < input.size() && input[i] == u'\u0D91') { out += u"ඓ"; ++i; continue; }
      while (prefixes--) appendStandalone(u'\u0DD9', out);
      continue;
    }
    if (isIndependentVowel(ch)) {
      ++i; auto vowel = independent(ch, input, i); appendSemi(input, i, vowel); out += vowel; continue;
    }
    if (isConsonant(ch)) {
      ++i; std::u16string syllable;
      if (i < input.size() && input[i] == kSanyakaya) {
        syllable = sanyaka(ch); if (!syllable.empty()) ++i;
      }
      if (syllable.empty()) syllable = cluster(ch, input, i);
      syllable += vowelSign(0, input, i); appendSemi(input, i, syllable); out += syllable; continue;
    }
    if (isDependentVowelSign(ch) || ch == u'\u0DCA' || isSemi(ch)) { appendStandalone(ch, out); ++i; continue; }
    if (ch == kRakaransaya) { appendStandalone(u'\u0DCA', out); out += u"‍ර"; ++i; continue; }
    if (ch == kYansaya) { appendStandalone(u'\u0DCA', out); out += u"‍ය"; ++i; continue; }
    if (ch == kRepaya) { out += u"ර්‍"; ++i; continue; }
    if (ch == kSanyakaya) { ++i; continue; }
    if (ch == kJoin || ch == kTouch) {
      ++i;
      if (i < input.size() && isConsonant(input[i])) { out += u"්‍"; out.push_back(input[i++]); }
      continue;
    }
    out.push_back(ch); ++i;
  }
  return out;
}

std::u16string markedWijesekara(std::u16string_view input) {
  auto result = normalizeWijesekara(input);
  constexpr std::u16string_view pending = u"‌ෙ";
  while (result.size() >= pending.size() && std::u16string_view(result).substr(result.size() - pending.size()) == pending)
    result.resize(result.size() - pending.size());
  return result;
}
}

