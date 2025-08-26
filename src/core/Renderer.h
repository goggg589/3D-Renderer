#pragma once
#include "Math.h"
#include "Mesh.h"
#include <utility>
#include <vector>

struct ScreenLine {
  Vec2f a, b;
};

class Renderer {
public:
  Renderer(int w = 1000, int h = 800) : m_width(w), m_height(h) {}
  void setViewport(int w, int h) {
    m_width = w;
    m_height = h;
  }
  void setModel(const Mat4 &m) { m_model = m; }

  // Returns 2D line segments in pixel coordinates after transform+clip+project
  std::vector<ScreenLine> buildProjectedLines(const Mat4 &view,
                                              const Mat4 &proj,
                                              const Mesh &mesh,
                                              float nearZ) const;

private:
  int m_width, m_height;
  Mat4 m_model = Mat4::identity();

  static bool clipToNear(Vec3f &a, Vec3f &b, float nearZ);
  bool projectToScreen(const Vec4f &clip, Vec2f &out) const;
};
