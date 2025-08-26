#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#include "core/Camera.h"
#include "core/Math.h"
#include "core/ObjLoader.h"

static void frameCameraToMesh(CameraOrbit &cam, const Mesh &mesh) {
  if (mesh.vertices.empty())
    return;
  Vec3f mn{std::numeric_limits<float>::infinity(),
           std::numeric_limits<float>::infinity(),
           std::numeric_limits<float>::infinity()};
  Vec3f mx{-mn.x, -mn.y, -mn.z};
  for (const auto &v : mesh.vertices) {
    mn.x = std::min(mn.x, v.x);
    mn.y = std::min(mn.y, v.y);
    mn.z = std::min(mn.z, v.z);
    mx.x = std::max(mx.x, v.x);
    mx.y = std::max(mx.y, v.y);
    mx.z = std::max(mx.z, v.z);
  }
  Vec3f center{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f,
               (mn.z + mx.z) * 0.5f};
  float ex = mx.x - mn.x, ey = mx.y - mn.y, ez = mx.z - mn.z;
  float r = std::max({ex, ey, ez}) * 0.5f;
  if (r < 1e-4f)
    r = 1.0f;
  cam.target = center;
  cam.radius = std::max(3.0f * r, 0.5f);
  cam.orthoScale = r * 1.2f;
  cam.znear = 0.01f;
  cam.zfar = 20000.0f;
}

static inline bool projectToScreen(const Vec3f &c, const Mat4 &P, int W, int H,
                                   Vec2f &s) {
  Vec4f clip = mul(P, {c.x, c.y, c.z, 1.f});
  if (std::abs(clip.w) < 1e-6f)
    return false;
  float ndcX = clip.x / clip.w, ndcY = clip.y / clip.w;
  if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
    return false;
  s.x = (ndcX * 0.5f + 0.5f) * W;
  s.y = (1.f - (ndcY * 0.5f + 0.5f)) * H;
  return true;
}

static inline bool clipNear(Vec3f &a, Vec3f &b, float znear) {
  float da = -a.z, db = -b.z;
  bool aIn = da >= znear, bIn = db >= znear;
  if (aIn && bIn)
    return true;
  if (!aIn && !bIn)
    return false;
  float t = (-znear - a.z) / (b.z - a.z);
  Vec3f i{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
  if (!aIn)
    a = i;
  else
    b = i;
  return true;
}

class Viewer : public QWidget {
public:
  explicit Viewer(const QString &objPath, QWidget *parent = nullptr)
      : QWidget(parent) {
    setWindowTitle("3D Renderer - Qt Viewer (FPS HUD)");
    resize(1280, 800);
    if (!loadOBJ(objPath.toStdString(), mesh)) {
      std::cerr << "Failed to load OBJ " << objPath.toStdString() << "\n";
    } else {
      std::cerr << "Loaded OBJ with " << mesh.vertices.size() << " verts, "
                << mesh.edges.size() << " edges\n";
    }
    frameCameraToMesh(cam, mesh);
    setMouseTracking(true);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, QOverload<>::of(&Viewer::update));
    timer->start(0); // repaint ASAP; we adapt to hit target fps
    clock.start();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    const int W = width(), H = height();
    Mat4 V = cam.view();
    Mat4 P = cam.projection(float(W) / float(H));

    qint64 t0 = clock.nsecsElapsed();

    // 1) world -> camera
    size_t N = mesh.vertices.size();
    camVerts.resize(N);
    for (size_t i = 0; i < N; ++i) {
      const auto &v = mesh.vertices[i];
      Vec4f c4 = mul(V, {v.x, v.y, v.z, 1.f});
      camVerts[i] = {c4.x, c4.y, c4.z};
    }

    // 2) pre-project & mark valid
    screens.resize(N);
    valid.resize(N);
    for (size_t i = 0; i < N; ++i) {
      const Vec3f c = camVerts[i];
      if (-c.z >= cam.znear) {
        Vec2f s;
        valid[i] = projectToScreen(c, P, W, H, s) ? 1 : 0;
        if (valid[i])
          screens[i] = s;
      } else
        valid[i] = 0;
    }

    // 3) build line batch with LOD + hard cap
    lines.clear();
    lines.reserve(int(mesh.edges.size()));
    const float lod2 = lodPx * lodPx;
    for (const auto &e : mesh.edges) {
      size_t ia = (size_t)e.first, ib = (size_t)e.second;
      Vec2f sa, sb;

      if (valid[ia] && valid[ib]) {
        sa = screens[ia];
        sb = screens[ib];
      } else if (!aggressiveSkip) {
        Vec3f a = camVerts[ia], b = camVerts[ib];
        if (!clipNear(a, b, cam.znear))
          continue;
        if (!projectToScreen(a, P, W, H, sa))
          continue;
        if (!projectToScreen(b, P, W, H, sb))
          continue;
      } else
        continue;

      float dx = sa.x - sb.x, dy = sa.y - sb.y;
      if (fastMode && (dx * dx + dy * dy) < lod2)
        continue;

      lines.push_back(QLineF(sa.x, sa.y, sb.x, sb.y));
      if ((int)lines.size() >= maxLinesCap)
        break; // enforce cap
    }

    // 4) draw
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 20));
    p.setRenderHint(QPainter::Antialiasing, antialias);

    // axes
    auto drawAxis = [&](const Vec3f &a0, const Vec3f &b0, const QColor &col) {
      Vec4f a4 = mul(V, {a0.x, a0.y, a0.z, 1.f});
      Vec4f b4 = mul(V, {b0.x, b0.y, b0.z, 1.f});
      Vec3f ac{a4.x, a4.y, a4.z}, bc{b4.x, b4.y, b4.z};
      if (!clipNear(ac, bc, 0.01f))
        return;
      Vec2f sa2, sb2;
      if (projectToScreen(ac, P, W, H, sa2) &&
          projectToScreen(bc, P, W, H, sb2)) {
        QPen ax(col);
        ax.setCosmetic(true);
        ax.setWidth(2);
        p.setPen(ax);
        p.drawLine(QPointF(sa2.x, sa2.y), QPointF(sb2.x, sb2.y));
      }
    };
    drawAxis({0, 0, 0}, {1, 0, 0}, QColor(240, 60, 60));
    drawAxis({0, 0, 0}, {0, 1, 0}, QColor(60, 240, 60));
    drawAxis({0, 0, 0}, {0, 0, 1}, QColor(60, 140, 240));

    QPen pen(QColor(220, 220, 235));
    pen.setCosmetic(true);
    p.setPen(pen);
    if (!lines.empty())
      p.drawLines(lines.constData(), lines.size());

    // 5) FPS + HUD
    qint64 t1 = clock.nsecsElapsed();
    double ms = (t1 - t0) / 1e6;
    smoothedMs = 0.85 * smoothedMs + 0.15 * ms;
    lastFPS = 1000.0 / std::max(1e-3, smoothedMs);

    // auto-tune LOD to target FPS
    double goal = 1000.0 / targetFps;
    if (smoothedMs > goal * 1.05 && lodPx < 5.0f)
      lodPx *= 1.10f;
    else if (smoothedMs < goal * 0.80 && lodPx > 0.25f)
      lodPx *= 0.90f;

    p.setPen(QColor(180, 180, 200));
    QString hud =
        QString("%1 | FPS=%2 | radius=%3 | fov=%4 | edges=%5 | drawn=%6 | "
                "AA=%7 | FAST=%8 | LOD=%9px | cap=%10 | target=%11fps")
            .arg(cam.perspective ? "Perspective" : "Orthographic")
            .arg(lastFPS, 0, 'f', 1)
            .arg(cam.radius, 0, 'f', 2)
            .arg(cam.fovY * 180.0 / 3.14159265, 0, 'f', 1)
            .arg((qulonglong)mesh.edges.size())
            .arg((qulonglong)lines.size())
            .arg(antialias ? "on" : "off")
            .arg(fastMode ? "on" : "off")
            .arg(lodPx, 0, 'f', 2)
            .arg(maxLinesCap)
            .arg(targetFps);
    p.drawText(10, 20, hud);
  }

  void wheelEvent(QWheelEvent *e) override {
    if (cam.perspective) {
      cam.radius *= (e->angleDelta().y() > 0 ? 0.9f : 1.1f);
      cam.radius = std::max(0.2f, cam.radius);
    } else {
      cam.orthoScale *= (e->angleDelta().y() > 0 ? 0.9f : 1.1f);
      cam.orthoScale = std::max(0.02f, cam.orthoScale);
    }
    update();
  }
  void mousePressEvent(QMouseEvent *e) override {
    last = e->pos();
    if (e->button() == Qt::LeftButton)
      L = true;
    if (e->button() == Qt::RightButton)
      R = true;
  }
  void mouseReleaseEvent(QMouseEvent *e) override {
    if (e->button() == Qt::LeftButton)
      L = false;
    if (e->button() == Qt::RightButton)
      R = false;
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    QPoint d = e->pos() - last;
    last = e->pos();
    if (L) {
      cam.yaw -= d.x() * 0.01f;
      cam.pitch -= d.y() * 0.01f;
      cam.pitch = std::max(-1.55f, std::min(1.55f, cam.pitch));
      update();
    }
    if (R) {
      Vec3f eye = cam.position();
      Vec3f fwd = normalize(cam.target - eye);
      Vec3f right = normalize(cross(fwd, {0, 1, 0}));
      Vec3f up = cross(right, fwd);
      float k = 0.002f * cam.radius;
      cam.target = cam.target + right * (-d.x() * k) + up * (d.y() * k);
      update();
    }
  }
  void keyPressEvent(QKeyEvent *e) override {
    if (e->key() == Qt::Key_Escape)
      close();
    if (e->key() == Qt::Key_O) {
      cam.perspective = !cam.perspective;
      update();
    }
    if (e->key() == Qt::Key_R) {
      cam = CameraOrbit{};
      frameCameraToMesh(cam, mesh);
      update();
    }
    if (e->key() == Qt::Key_A) {
      antialias = !antialias;
      update();
    } // toggle AA
    if (e->key() == Qt::Key_F) {
      fastMode = !fastMode;
      update();
    } // toggle LOD
    if (e->key() == Qt::Key_T) {
      targetFps = (targetFps == 30 ? 60 : 30);
      update();
    } // 30/60
    QWidget::keyPressEvent(e);
  }

private:
  Mesh mesh;
  CameraOrbit cam;

  std::vector<Vec3f> camVerts;
  std::vector<Vec2f> screens;
  std::vector<uint8_t> valid;
  QVector<QLineF> lines;

  bool L = false, R = false;
  QPoint last;
  QTimer *timer = nullptr;

  // perf / HUD
  bool antialias = false;
  bool fastMode = true;
  bool aggressiveSkip = true;
  float lodPx = 1.5f;
  int maxLinesCap = 180000;
  int targetFps = 30;

  QElapsedTimer clock;
  double smoothedMs = 33.0;
  double lastFPS = 0.0; // <--- FPS shown in HUD
};

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " path/to/model.obj\n";
    return 1;
  }
  QApplication app(argc, argv);
  Viewer w(QString::fromLocal8Bit(argv[1]));
  w.show();
  return app.exec();
}
