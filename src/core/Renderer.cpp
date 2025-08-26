#include "Renderer.h"
#include <cmath>

bool Renderer::clipToNear(Vec3f &a, Vec3f &b, float nearZ) {
  // camera-space: z < 0 is in front of the camera
  float da = -a.z, db = -b.z;
  bool aIn = da >= nearZ, bIn = db >= nearZ;
  if (aIn && bIn)
    return true;
  if (!aIn && !bIn)
    return false;
  float t = (nearZ + a.z) / (a.z - b.z); // intersect z = -nearZ
  Vec3f i = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
             a.z + (b.z - a.z) * t};
  if (!aIn)
    a = i;
  else
    b = i;
  return true;
}

bool Renderer::projectToScreen(const Vec4f &clip, Vec2f &out) const {
  float w = clip.w;
  if (std::abs(w) < 1e-6f)
    return false;
  float ndcX = clip.x / w;
  float ndcY = clip.y / w;
  // clip check (optional; we already near-clip, but we can cull if far out of
  // screen)
  if (ndcX < -2.f || ndcX > 2.f || ndcY < -2.f || ndcY > 2.f) {
    // keep generous bounds to avoid disappearing during pan; renderer still
    // draws offscreen okay
  }
  out.x = (ndcX * 0.5f + 0.5f) * m_width;
  out.y = (1.0f - (ndcY * 0.5f + 0.5f)) * m_height;
  return std::isfinite(out.x) && std::isfinite(out.y);
}

std::vector<ScreenLine> Renderer::buildProjectedLines(const Mat4 &view,
                                                      const Mat4 &proj,
                                                      const Mesh &mesh,
                                                      float nearZ) const {
  std::vector<ScreenLine> out;
  out.reserve(mesh.edges.size());

  Mat4 vm = view * m_model;
  Mat4 pvm = proj * vm;

  for (const auto &e : mesh.edges) {
    Vec3f va = mesh.vertices[e.first];
    Vec3f vb = mesh.vertices[e.second];

    // camera-space (for clipping) -> multiply by view*model
    Vec4f a4 = mul(vm, {va.x, va.y, va.z, 1.f});
    Vec4f b4 = mul(vm, {vb.x, vb.y, vb.z, 1.f});
    Vec3f ac{a4.x, a4.y, a4.z};
    Vec3f bc{b4.x, b4.y, b4.z};

    if (!clipToNear(ac, bc, nearZ))
      continue;

    // project
    Vec4f ap = mul(proj, {ac.x, ac.y, ac.z, 1.f});
    Vec4f bp = mul(proj, {bc.x, bc.y, bc.z, 1.f});

    Vec2f sa, sb;
    if (projectToScreen(ap, sa) && projectToScreen(bp, sb)) {
      out.push_back({sa, sb});
    }
  }
  return out;
}
