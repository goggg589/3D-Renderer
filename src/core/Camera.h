#pragma once
#include "Math.h"

struct CameraOrbit {
  Vec3f target{0, 0, 0};
  float radius = 3.0f;
  float yaw = 0.8f;
  float pitch = 0.4f;
  bool perspective = true;
  float fovY = 60.0f * 3.14159265f / 180.0f;
  float znear = 0.05f;
  float zfar = 100.0f;
  float orthoScale = 1.0f; // half-height of the ortho volume

  Vec3f position() const {
    float cp = std::cos(pitch), sp = std::sin(pitch);
    float cy = std::cos(yaw), sy = std::sin(yaw);
    Vec3f offset{radius * cp * cy, radius * sp, radius * cp * sy};
    return target + offset;
  }

  Mat4 view() const { return Mat4::lookAt(position(), target, {0, 1, 0}); }

  Mat4 projection(float aspect) const {
    if (perspective) {
      return Mat4::perspective(fovY, aspect, znear, zfar);
    } else {
      float halfH = orthoScale;
      float halfW = halfH * aspect;
      return Mat4::orthographic(-halfW, halfW, -halfH, halfH, znear, zfar);
    }
  }
};
