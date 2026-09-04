#include "MappingTrie.h"
#include <algorithm>

namespace akshara {
MappingTrie::MappingTrie(std::initializer_list<Entry> entries) : entries_(entries) {
  std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
    return a.first.size() > b.first.size();
  });
}

std::optional<MappingTrie::Match> MappingTrie::longest(std::u16string_view input, std::size_t offset) const {
  for (const auto& [key, value] : entries_) {
    if (offset <= input.size() && key.size() <= input.size() - offset && input.substr(offset, key.size()) == key) {
      return Match{key, value};
    }
  }
  return std::nullopt;
}
}

