#pragma once
#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

inline uint64_t edgeKey(int a, int b) {
  if (a > b)
    std::swap(a, b);
  return (uint64_t(a) << 32) | uint32_t(b);
}

template <typename EdgeArray> inline void dedupEdges(EdgeArray &edges) {
  std::unordered_set<uint64_t> seen;
  std::vector<std::pair<int, int>> unique;
  unique.reserve(edges.size());
  for (const auto &e : edges) {
    auto k = edgeKey(e.first, e.second);
    if (seen.insert(k).second)
      unique.push_back(e);
  }
  edges.assign(unique.begin(), unique.end());
}
