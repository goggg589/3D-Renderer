#pragma once
#include <algorithm>
#include <cmath>

struct Vec2f {
  float x{}, y{};
};

struct Vec3f {
  float x{}, y{}, z{};
  Vec3f() = default;
  Vec3f(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
  Vec3f operator+(const Vec3f &o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3f operator-(const Vec3f &o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3f operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3f operator/(float s) const { return {x / s, y / s, z / s}; }
  Vec3f &operator+=(const Vec3f &o) {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }
};

struct Vec4f {
  float x{}, y{}, z{}, w{};
  Vec4f() = default;
  Vec4f(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
};

inline float dot(const Vec3f &a, const Vec3f &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3f cross(const Vec3f &a, const Vec3f &b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3f &v) { return std::sqrt(dot(v, v)); }
inline Vec3f normalize(const Vec3f &v) {
  float L = length(v);
  return (L > 0.f) ? (v / L) : v;
}

struct Mat4 {
  float m[4][4];
  static Mat4 identity() {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        r.m[i][j] = (i == j) ? 1.f : 0.f;
    return r;
  }
  static Mat4 translation(const Vec3f &t) {
    Mat4 r = identity();
    r.m[0][3] = t.x;
    r.m[1][3] = t.y;
    r.m[2][3] = t.z;
    return r;
  }
  static Mat4 scale(const Vec3f &s) {
    Mat4 r = identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    r.m[2][2] = s.z;
    return r;
  }
  static Mat4 rotationX(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[1][1] = c;
    r.m[1][2] = -s;
    r.m[2][1] = s;
    r.m[2][2] = c;
    return r;
  }
  static Mat4 rotationY(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0][0] = c;
    r.m[0][2] = s;
    r.m[2][0] = -s;
    r.m[2][2] = c;
    return r;
  }
  static Mat4 rotationZ(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0][0] = c;
    r.m[0][1] = -s;
    r.m[1][0] = s;
    r.m[1][1] = c;
    return r;
  }
  static Mat4 lookAt(const Vec3f &eye, const Vec3f &target, const Vec3f &up) {
    Vec3f fwd = normalize(target - eye);
    Vec3f right = normalize(cross(fwd, up));
    Vec3f upv = cross(right, fwd);
    Mat4 r = identity();
    r.m[0][0] = right.x;
    r.m[0][1] = right.y;
    r.m[0][2] = right.z;
    r.m[0][3] = -dot(right, eye);
    r.m[1][0] = upv.x;
    r.m[1][1] = upv.y;
    r.m[1][2] = upv.z;
    r.m[1][3] = -dot(upv, eye);
    r.m[2][0] = -fwd.x;
    r.m[2][1] = -fwd.y;
    r.m[2][2] = -fwd.z;
    r.m[2][3] = dot(fwd, eye);
    r.m[3][3] = 1.f;
    return r;
  }
  // Right-handed perspective matrix (OpenGL-style NDC z in [-1,1])
  static Mat4 perspective(float fovYRadians, float aspect, float znear,
                          float zfar) {
    float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        r.m[i][j] = 0.f;
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = (zfar + znear) / (znear - zfar);
    r.m[2][3] = (2.f * zfar * znear) / (znear - zfar);
    r.m[3][2] = -1.f;
    return r;
  }
  static Mat4 orthographic(float left, float right, float bottom, float top,
                           float znear, float zfar) {
    Mat4 r = identity();
    r.m[0][0] = 2.f / (right - left);
    r.m[1][1] = 2.f / (top - bottom);
    r.m[2][2] = -2.f / (zfar - znear);
    r.m[0][3] = -(right + left) / (right - left);
    r.m[1][3] = -(top + bottom) / (top - bottom);
    r.m[2][3] = -(zfar + znear) / (zfar - znear);
    return r;
  }
};

inline Mat4 operator*(const Mat4 &a, const Mat4 &b) {
  Mat4 r{};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      r.m[i][j] = 0.f;
      for (int k = 0; k < 4; ++k)
        r.m[i][j] += a.m[i][k] * b.m[k][j];
    }
  }
  return r;
}

inline Vec4f mul(const Mat4 &a, const Vec4f &v) {
  Vec4f r{};
  r.x = a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z + a.m[0][3] * v.w;
  r.y = a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z + a.m[1][3] * v.w;
  r.z = a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z + a.m[2][3] * v.w;
  r.w = a.m[3][0] * v.x + a.m[3][1] * v.y + a.m[3][2] * v.z + a.m[3][3] * v.w;
  return r;
}
