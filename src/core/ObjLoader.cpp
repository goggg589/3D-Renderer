#include "ObjLoader.h"
#include "Geometry.h"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void addFaceEdges(const std::vector<int> &f,
                         std::vector<std::pair<int, int>> &edges) {
  const int n = (int)f.size();
  if (n < 2)
    return;
  for (int i = 0; i < n; ++i) {
    int a = f[i];
    int b = f[(i + 1) % n];
    if (a == b)
      continue;
    edges.emplace_back(a, b);
  }
}

bool loadOBJ(const std::string &path, Mesh &out) {
  std::ifstream in(path);
  if (!in) {
    std::cerr << "Failed to open OBJ: " << path << "\n";
    return false;
  }
  std::string line;
  std::vector<std::pair<int, int>> edges;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream iss(line);
    std::string tag;
    iss >> tag;
    if (tag == "v") {
      Vec3f v{};
      iss >> v.x >> v.y >> v.z;
      out.vertices.push_back(v);
    } else if (tag == "f") {
      std::vector<int> f;
      std::string tok;
      while (iss >> tok) {
        // token like "3", or "3/2/1", or "3//1" or "3/"
        std::istringstream tss(tok);
        std::string a;
        std::getline(tss, a, '/');
        if (a.empty())
          continue;
        int idx = std::stoi(a);
        if (idx < 0)
          idx = (int)out.vertices.size() + idx + 1;
        f.push_back(idx - 1); // 0-based
      }
      if (f.size() >= 2)
        addFaceEdges(f, edges);
    }
  }
  // Deduplicate edges
  dedupEdges(edges);
  out.edges = std::move(edges);
  std::cerr << "Loaded \"" << path << "\" with " << out.vertices.size()
            << " vertices, " << out.edges.size() << " unique edges.\n";
  return true;
}
