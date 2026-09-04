#pragma once

#include "InputMode.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace akshara {

struct CompositionResult {
  std::u16string text;
  bool hasPendingInput{};
};

struct PhysicalKey {
  std::uint32_t virtualKey{};
  bool shift{};
  bool altGr{};
};

class AksharaEngine final {
 public:
  [[nodiscard]] CompositionResult compose(InputMode mode, std::u16string_view rawInput) const;
  [[nodiscard]] std::u16string mapWijesekaraKey(const PhysicalKey& key) const;
  [[nodiscard]] static bool isComposableWijesekaraUnit(std::u16string_view text);
};

class CompositionBuffer final {
 public:
  static constexpr std::size_t kMaxRawCodeUnits = 4096;
  explicit CompositionBuffer(InputMode mode) : mode_(mode) {}
  void setMode(InputMode mode) { clear(); mode_ = mode; }
  [[nodiscard]] InputMode mode() const { return mode_; }
  [[nodiscard]] bool empty() const { return raw_.empty(); }
  [[nodiscard]] const std::u16string& raw() const { return raw_; }
  void append(std::u16string_view input) {
    const auto available = kMaxRawCodeUnits - raw_.size();
    raw_.append(input.substr(0, available));
  }
  bool backspace();
  void clear() { raw_.clear(); }
  [[nodiscard]] CompositionResult render(const AksharaEngine& engine) const { return engine.compose(mode_, raw_); }
 private:
  InputMode mode_;
  std::u16string raw_;
};

}
