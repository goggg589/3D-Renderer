#pragma once
#include "Math.h"
#include <cstdint>
#include <utility>
#include <vector>

struct Mesh {
  std::vector<Vec3f> vertices;            // positions
  std::vector<std::pair<int, int>> edges; // pairs of vertex indices (0-based)
};
