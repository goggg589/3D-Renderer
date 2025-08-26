#include "core/Camera.h"
#include "core/Math.h"
#include "core/ObjLoader.h"
#include "core/Renderer.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void usage(const char* exe) {
  std::cerr << "Usage:\n  " << exe
            << " input.obj output.ppm [--eye x y z] [--target x y z] [--fov deg]"
               " [--size W H] [--ortho scale]\n";
}

// simple RGB image
struct Image {
  int w = 0, h = 0;
  std::vector<uint8_t> data; // size = w*h*3
  Image(int W, int H, uint8_t r, uint8_t g, uint8_t b) : w(W), h(H), data(W*H*3) {
    for (int i = 0; i < W*H; ++i) { data[3*i+0]=r; data[3*i+1]=g; data[3*i+2]=b; }
  }
  inline void put(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    int idx = (y*w + x) * 3;
    data[idx+0]=r; data[idx+1]=g; data[idx+2]=b;
  }
};

static bool savePPM(const std::string& path, const Image& img) {
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f << "P6\n" << img.w << " " << img.h << "\n255\n";
  f.write(reinterpret_cast<const char*>(img.data.data()), img.data.size());
  return f.good();
}

// integer Bresenham
static void drawLine(Image& im, int x0, int y0, int x1, int y1,
                     uint8_t r, uint8_t g, uint8_t b) {
  auto plot = [&](int x, int y){ im.put(x, y, r, g, b); };

  bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
  if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
  if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }

  int dx = x1 - x0;
  int dy = std::abs(y1 - y0);
  int err = dx / 2;
  int ystep = (y0 < y1) ? 1 : -1;
  int y = y0;

  for (int x = x0; x <= x1; ++x) {
    if (steep) plot(y, x); else plot(x, y);
    err -= dy;
    if (err < 0) { y += ystep; err += dx; }
  }
}

int main(int argc, char** argv) {
  if (argc < 3) { usage(argv[0]); return 1; }
  std::string inPath = argv[1];
  std::string outPath = argv[2];

  CameraOrbit cam{};
  int W = 1000, H = 800;

  cam.target = {0,0,0};
  cam.perspective = true;
  cam.radius = 3.5f;
  cam.yaw = 0.8f;
  cam.pitch = 0.4f;

  for (int i = 3; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](int n){ if (i + n >= argc) { usage(argv[0]); std::exit(2);} return true; };
    if (a == "--eye" && need(3)) {
      float x = std::stof(argv[++i]), y = std::stof(argv[++i]), z = std::stof(argv[++i]);
      Vec3f e{x,y,z}; Vec3f d = e - cam.target;
      cam.radius = length(d);
      cam.pitch = std::asin(d.y / std::max(1e-6f, cam.radius));
      cam.yaw   = std::atan2(d.z, d.x);
    } else if (a == "--target" && need(3)) {
      cam.target = {std::stof(argv[++i]), std::stof(argv[++i]), std::stof(argv[++i])};
    } else if (a == "--fov" && need(1)) {
      cam.fovY = std::stof(argv[++i]) * 3.14159265f / 180.f;
    } else if (a == "--size" && need(2)) {
      W = std::stoi(argv[++i]); H = std::stoi(argv[++i]);
    } else if (a == "--ortho" && need(1)) {
      cam.perspective = false; cam.orthoScale = std::stof(argv[++i]);
    } else {
      std::cerr << "Unknown arg: " << a << "\n"; usage(argv[0]); return 2;
    }
  }

  Mesh mesh;
  if (!loadOBJ(inPath, mesh)) return 3;

  Renderer renderer(W, H);
  Mat4 view = cam.view();
  Mat4 proj = cam.projection(float(W) / float(H));

  auto lines = renderer.buildProjectedLines(view, proj, mesh, cam.znear);

  // draw
  Image img(W, H, /bg/18, 18, 20);
  const uint8_t R = 230, G = 230, B = 240;
  for (const auto& ln : lines) {
    int x0 = static_cast<int>(std::lround(ln.a.x));
    int y0 = static_cast<int>(std::lround(ln.a.y));
    int x1 = static_cast<int>(std::lround(ln.b.x));
    int y1 = static_cast<int>(std::lround(ln.b.y));
    drawLine(img, x0, y0, x1, y1, R, G, B);
  }

  if (!savePPM(outPath, img)) {
    std::cerr << "Failed to save " << outPath << "\n"; return 5;
  }
  std::cout << "Wrote " << outPath << " (" << W << "x" << H << ")\n";
  return 0;
}