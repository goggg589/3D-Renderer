#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QTimer>
#include <QElapsedTimer>
#include <QPoint>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QVector>
#include <QLineF>
#include <QString>

#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <vector>
#include <iostream>

#include "core/Math.h"
#include "core/Camera.h"
#include "core/ObjLoader.h"

// --- Helpers ---------------------------------------------------------------

static void frameCameraToMesh(CameraOrbit& cam, const Mesh& mesh) {
    if (mesh.vertices.empty()) return;

    Vec3f mn{ std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity() };
    Vec3f mx{ -mn.x, -mn.y, -mn.z };

    for (const auto& v : mesh.vertices) {
        mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
        mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
    }

    Vec3f center{ (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
    float ex = mx.x - mn.x, ey = mx.y - mn.y, ez = mx.z - mn.z;
    float r = std::max({ ex, ey, ez }) * 0.5f; if (r < 1e-4f) r = 1.0f;

    cam.target     = center;
    cam.radius     = std::max(3.0f * r, 0.5f);
    cam.orthoScale = r * 1.2f;
    cam.fovY       = 60.0f * 3.14159265f / 180.0f;

    // Start with a tiny near plane; we’ll also adapt this per-frame
    cam.znear      = std::max(0.0005f * cam.radius, 0.001f);
    cam.zfar       = 20000.0f;
}

static inline bool projectToScreen(const Vec3f& c, const Mat4& P, int W, int H, Vec2f& s) {
    Vec4f clip = mul(P, { c.x, c.y, c.z, 1.f });
    if (std::abs(clip.w) < 1e-6f) return false;
    float ndcX = clip.x / clip.w, ndcY = clip.y / clip.w;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) return false;
    s.x = (ndcX * 0.5f + 0.5f) * float(W);
    s.y = (1.f - (ndcY * 0.5f + 0.5f)) * float(H);
    return true;
}

// Clip segment against the near plane z = -znear in camera space (z<0 is in front).
static inline bool clipNear(Vec3f& a, Vec3f& b, float znear) {
    float da = -a.z;
    float db = -b.z;
    bool aIn = da >= znear;
    bool bIn = db >= znear;
    if (aIn && bIn) return true;
    if (!aIn && !bIn) return false;

    float t = (-znear - a.z) / (b.z - a.z);
    Vec3f i{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
    if (!aIn) a = i; else b = i;
    return true;
}

// --- Viewer ---------------------------------------------------------------

class Viewer : public QWidget {
public:
    explicit Viewer(const QString& objPath, QWidget* parent=nullptr) : QWidget(parent) {
        setWindowTitle("3D Renderer - Qt Viewer (near-clip fixed, adaptive LOD)");
        resize(1280, 800);

        if (!loadOBJ(objPath.toStdString(), mesh)) {
            std::cerr << "Failed to load OBJ " << objPath.toStdString() << "\n";
        } else {
            std::cerr << "Loaded OBJ with " << mesh.vertices.size()
                      << " verts, " << mesh.edges.size() << " edges\n";
        }

        frameCameraToMesh(cam, mesh);
        setMouseTracking(true);

        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &Viewer::update);
        timer->start(0); // ASAP; we adapt to target fps in paintEvent()
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const int W = width(), H = height();

        // Keep near plane tiny and proportional to zoom to avoid popping edges.
        cam.znear = std::max(0.0005f * cam.radius, 0.001f);

        Mat4 V = cam.view();
        Mat4 P = cam.projection(float(W) / float(H));

        if (!clock.isValid()) clock.start();
        qint64 t0 = clock.nsecsElapsed();

        // 1) world -> camera space for all verts
        const size_t N = mesh.vertices.size();
        camVerts.resize(N);
        for (size_t i = 0; i < N; ++i) {
            const auto& v = mesh.vertices[i];
            Vec4f c4 = mul(V, { v.x, v.y, v.z, 1.f });
            camVerts[i] = { c4.x, c4.y, c4.z };
        }

        // 2) Project verts that are in front of near plane
        screens.resize(N);
        valid.assign(N, 0);
        for (size_t i = 0; i < N; ++i) {
            const Vec3f c = camVerts[i];
            if (-c.z >= cam.znear) {
                Vec2f s;
                if (projectToScreen(c, P, W, H, s)) { screens[i] = s; valid[i] = 1; }
            }
        }

        // 3) Build line batch: ALWAYS clip to near plane, then project.
        lines.clear();
        lines.reserve(int(mesh.edges.size()));

        const float lod2 = lodPx * lodPx;
        const int   cap  = maxLinesCap;

        for (const auto& e : mesh.edges) {
            const size_t ia = (size_t)e.first;
            const size_t ib = (size_t)e.second;

            Vec2f sa, sb;

            if (valid[ia] && valid[ib]) {
                // Both pre-projected
                sa = screens[ia]; sb = screens[ib];
            } else {
                // Try clipping against near plane, then project
                Vec3f a = camVerts[ia], b = camVerts[ib];
                if (!clipNear(a, b, cam.znear)) continue;
                if (!projectToScreen(a, P, W, H, sa)) continue;
                if (!projectToScreen(b, P, W, H, sb)) continue;
            }

            float dx = sa.x - sb.x, dy = sa.y - sb.y;
            if (fastMode && (dx*dx + dy*dy) < lod2) continue; // pixel-length LOD

            lines.push_back(QLineF(sa.x, sa.y, sb.x, sb.y));
            if ((int)lines.size() >= cap) break;
        }

        // 4) Draw
        QPainter p(this);
        p.fillRect(rect(), QColor(18, 18, 20));
        p.setRenderHint(QPainter::Antialiasing, antialias);

        // Axis gizmo (clipped to near)
        auto drawAxis = [&](const Vec3f& a0, const Vec3f& b0, const QColor& col) {
            Vec4f a4 = mul(V, { a0.x, a0.y, a0.z, 1.f });
            Vec4f b4 = mul(V, { b0.x, b0.y, b0.z, 1.f });
            Vec3f ac{ a4.x, a4.y, a4.z }, bc{ b4.x, b4.y, b4.z };
            if (!clipNear(ac, bc, 0.01f)) return;
            Vec2f sa2, sb2;
            if (projectToScreen(ac, P, W, H, sa2) && projectToScreen(bc, P, W, H, sb2)) {
                QPen ax(col); ax.setCosmetic(true); ax.setWidth(2);
                p.setPen(ax);
                p.drawLine(QPointF(sa2.x, sa2.y), QPointF(sb2.x, sb2.y));
            }
        };
        drawAxis({0,0,0},{1,0,0}, QColor(240, 60, 60));
        drawAxis({0,0,0},{0,1,0}, QColor( 60,240, 60));
        drawAxis({0,0,0},{0,0,1}, QColor( 60,140,240));

        QPen pen(QColor(220, 220, 235));
        pen.setCosmetic(true);
        p.setPen(pen);
        if (!lines.empty()) p.drawLines(lines);

        // HUD
        qint64 t1 = clock.nsecsElapsed();
        double ms = (t1 - t0) / 1e6;
        smoothedMs = 0.85 * smoothedMs + 0.15 * ms;

        std::ostringstream hud;
        hud.setf(std::ios::fixed); hud.precision(1);
        hud << (cam.perspective ? "Perspective" : "Orthographic")
            << " | FPS=" << (1000.0 / std::max(0.001, smoothedMs))
            << " | radius=" << cam.radius
            << " | fov=" << (cam.fovY * 180.0 / 3.14159265)
            << " | edges=" << mesh.edges.size()
            << " | drawn=" << lines.size()
            << " | AA=" << (antialias ? "on" : "off")
            << " | FAST=" << (fastMode ? "on" : "off")
            << " | LOD=" << lodPx << "px"
            << " | cap=" << maxLinesCap
            << " | target=" << targetFps << "fps";

        p.setPen(QColor(180, 180, 200));
        p.drawText(10, 20, QString::fromStdString(hud.str()));

        // 5) Adapt LOD to hold target FPS
        const double goal = 1000.0 / double(targetFps);
        if (smoothedMs > goal * 1.05 && lodPx < 5.0f)      lodPx *= 1.10f; // slower -> increase LOD
        else if (smoothedMs < goal * 0.80 && lodPx > 0.25f) lodPx *= 0.90f; // faster -> decrease LOD
    }

    void wheelEvent(QWheelEvent* e) override {
        if (cam.perspective) {
            cam.radius *= (e->angleDelta().y() > 0 ? 0.9f : 1.1f);
            cam.radius = std::max(0.2f, cam.radius);
        } else {
            cam.orthoScale *= (e->angleDelta().y() > 0 ? 0.9f : 1.1f);
            cam.orthoScale = std::max(0.02f, cam.orthoScale);
        }
        update();
    }

    void mousePressEvent(QMouseEvent* e) override {
        last = e->pos();
        if (e->button() == Qt::LeftButton)  L = true;
        if (e->button() == Qt::RightButton) R = true;
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton)  L = false;
        if (e->button() == Qt::RightButton) R = false;
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        QPoint d = e->pos() - last; last = e->pos();
        if (L) {
            cam.yaw   -= d.x() * 0.01f;
            cam.pitch -= d.y() * 0.01f;
            cam.pitch  = std::max(-1.55f, std::min(1.55f, cam.pitch));
            update();
        }
        if (R) {
            Vec3f eye   = cam.position();
            Vec3f fwd   = normalize(cam.target - eye);
            Vec3f right = normalize(cross(fwd, {0,1,0}));
            Vec3f up    = cross(right, fwd);
            float k     = 0.002f * cam.radius;
            cam.target  = cam.target + right * (-d.x() * k) + up * (d.y() * k);
            update();
        }
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape) close();
        if (e->key() == Qt::Key_O) { cam.perspective = !cam.perspective; update(); }
        if (e->key() == Qt::Key_R) { cam = CameraOrbit{}; frameCameraToMesh(cam, mesh); update(); }
        if (e->key() == Qt::Key_A) { antialias = !antialias; update(); }
        if (e->key() == Qt::Key_F) { fastMode  = !fastMode;  update(); }
        if (e->key() == Qt::Key_T) { targetFps = (targetFps == 30 ? 60 : 30); update(); }
        QWidget::keyPressEvent(e);
    }

private:
    Mesh        mesh;
    CameraOrbit cam;

    std::vector<Vec3f>   camVerts;
    std::vector<Vec2f>   screens;
    std::vector<uint8_t> valid;
    QVector<QLineF>      lines;

    bool    L=false, R=false;
    QPoint  last;
    QTimer* timer=nullptr;

    // Perf knobs / targets
    int   targetFps   = 60;   // toggle 30/60 with 'T'
    bool  antialias   = false;
    bool  fastMode    = true; // pixel-length LOD on/off
    float lodPx       = 1.5f; // LOD threshold in pixels
    int   maxLinesCap = 180000; // hard ceiling for safety

    QElapsedTimer clock;
    double        smoothedMs = 33.0;
};

// --- main ------------------------------------------------------------------

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/model.obj\n";
        return 1;
    }
    QApplication app(argc, argv);
    Viewer w(QString::fromLocal8Bit(argv[1]));
    w.show();
    return app.exec();
}
