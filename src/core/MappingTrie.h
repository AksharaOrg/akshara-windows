#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace akshara {

class MappingTrie final {
 public:
  using Entry = std::pair<std::u16string_view, std::u16string_view>;
  explicit MappingTrie(std::initializer_list<Entry> entries);
  struct Match { std::u16string_view key; std::u16string_view value; };
  [[nodiscard]] std::optional<Match> longest(std::u16string_view input, std::size_t offset) const;
 private:
  std::vector<Entry> entries_;
};

}

