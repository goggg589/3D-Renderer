#include "core/Camera.h"
#include "core/Math.h"
#include "core/ObjLoader.h"
#include "core/Renderer.h"
#include <SFML/Graphics.hpp>
#include <SFML/Config.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void usage(const char *exe) {
  std::cerr << "Usage:\n  " << exe
            << " input.obj output.png [--eye x y z] [--target x y z] [--fov "
               "deg] [--size W H] [--ortho scale]\n";
}

int main(int argc, char **argv) {
  if (argc < 3) {
    usage(argv[0]);
    return 1;
  }
  std::string inPath = argv[1];
  std::string outPath = argv[2];

  CameraOrbit cam{};
  int W = 1000, H = 800;

  // defaults
  cam.target = {0, 0, 0};
  cam.perspective = true;
  cam.radius = 3.5f;
  cam.yaw = 0.8f;
  cam.pitch = 0.4f;

  // parse args
  for (int i = 3; i < argc; i++) {
    std::string a = argv[i];
    auto need = [&](int n) -> bool {
      if (i + n >= argc) {
        usage(argv[0]);
        std::exit(2);
      }
      return true;
    };
    if (a == "--eye" && need(3)) {
      float x = std::stof(argv[++i]);
      float y = std::stof(argv[++i]);
      float z = std::stof(argv[++i]);
      // derive yaw/pitch/radius from eye
      Vec3f e{x, y, z};
      Vec3f d = e - cam.target;
      cam.radius = length(d);
      cam.pitch = std::asin(d.y / std::max(1e-6f, cam.radius));
      cam.yaw = std::atan2(d.z, d.x);
    } else if (a == "--target" && need(3)) {
      cam.target = {std::stof(argv[++i]), std::stof(argv[++i]),
                    std::stof(argv[++i])};
    } else if (a == "--fov" && need(1)) {
      cam.fovY = std::stof(argv[++i]) * 3.14159265f / 180.f;
    } else if (a == "--size" && need(2)) {
      W = std::stoi(argv[++i]);
      H = std::stoi(argv[++i]);
    } else if (a == "--ortho" && need(1)) {
      cam.perspective = false;
      cam.orthoScale = std::stof(argv[++i]);
    } else {
      std::cerr << "Unknown arg: " << a << "\n";
      usage(argv[0]);
      return 2;
    }
  }

  Mesh mesh;
  if (!loadOBJ(inPath, mesh))
    return 3;

  sf::RenderTexture rt;
  if (!rt.create(W, H)) {
    std::cerr << "Failed to create render texture " << W << "x" << H << "\n";
    return 4;
  }
  rt.clear(sf::Color(18, 18, 20));

  Renderer renderer(W, H);
  Mat4 view = cam.view();
  Mat4 proj = cam.projection(float(W) / float(H));

  auto lines = renderer.buildProjectedLines(view, proj, mesh, cam.znear);
  sf::VertexArray varr(sf::Lines);
  varr.resize(lines.size() * 2);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    varr[i * 2 + 0] = sf::Vertex(sf::Vector2f(lines[i].a.x, lines[i].a.y),
                                 sf::Color(230, 230, 240));
    varr[i * 2 + 1] = sf::Vertex(sf::Vector2f(lines[i].b.x, lines[i].b.y),
                                 sf::Color(230, 230, 240));
  }
  rt.draw(varr);
  rt.display();

  auto img = rt.getTexture().copyToImage();
  if (!img.saveToFile(outPath)) {
    std::cerr << "Failed to save " << outPath << "\n";
    return 5;
  }
  std::cout << "Wrote " << outPath << " (" << W << "x" << H << ")\n";
  return 0;
}
