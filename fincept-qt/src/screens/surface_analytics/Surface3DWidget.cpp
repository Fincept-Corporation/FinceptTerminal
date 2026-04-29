#include "Surface3DWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <vector>

namespace fincept::surface {

Surface3DWidget::Surface3DWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void Surface3DWidget::set_surface(const std::vector<std::vector<float>>& grid, const std::string& x_label,
                                  const std::string& y_label, const std::string& z_label, float min_val, float max_val,
                                  bool diverging, const std::vector<std::string>* col_labels,
                                  const std::vector<std::string>* row_labels) {
    grid_ = grid;
    x_label_ = x_label;
    y_label_ = y_label;
    z_label_ = z_label;
    min_val_ = min_val;
    max_val_ = max_val;
    diverging_ = diverging;
    if (col_labels) {
        col_labels_ = *col_labels;
        has_col_labels_ = true;
    } else {
        col_labels_.clear();
        has_col_labels_ = false;
    }
    if (row_labels) {
        row_labels_ = *row_labels;
        has_row_labels_ = true;
    } else {
        row_labels_.clear();
        has_row_labels_ = false;
    }
    update();
}

void Surface3DWidget::clear() {
    grid_.clear();
    update();
}

void Surface3DWidget::set_supported(bool supported) {
    if (supported_ == supported)
        return;
    supported_ = supported;
    update();
}

// ---- Color mapping ----
QColor Surface3DWidget::lerp_color(const QColor& a, const QColor& b, float t) {
    return QColor((int)(a.red() + (b.red() - a.red()) * t), (int)(a.green() + (b.green() - a.green()) * t),
                  (int)(a.blue() + (b.blue() - a.blue()) * t));
}

QColor Surface3DWidget::surface_color(float t, bool diverging) const {
    t = std::max(0.0f, std::min(1.0f, t));
    if (diverging) {
        // Obsidian diverging: muted crimson → dark base → muted green
        static const QColor stops[] = {
            {180, 30, 30}, // deep red
            {220, 55, 55}, // red
            {100, 30, 20}, // dark red-brown
            {14, 14, 14},  // base (near-black neutral)
            {20, 60, 35},  // dark green
            {25, 120, 55}, // green
            {30, 160, 75}, // bright green
        };
        float idx = t * 6.0f;
        int lo = std::max(0, std::min(5, (int)idx));
        return lerp_color(stops[lo], stops[lo + 1], idx - (float)lo);
    }
    // Obsidian sequential: dark base → amber → bright amber-white
    static const QColor stops[] = {
        {8, 8, 8},       // BG_BASE
        {40, 22, 4},     // very dark amber
        {90, 50, 8},     // dark amber
        {150, 80, 10},   // amber-brown
        {200, 110, 15},  // amber
        {217, 119, 6},   // AMBER brand
        {240, 175, 80},  // light amber
        {255, 220, 150}, // near-white amber peak
    };
    float idx = t * 7.0f;
    int lo = std::max(0, std::min(6, (int)idx));
    return lerp_color(stops[lo], stops[lo + 1], idx - (float)lo);
}

// ---- 3D projection ----
QPointF Surface3DWidget::project(Vec3 p, QPointF center, float scale) const {
    float cy = std::cos(cam_yaw_), sy = std::sin(cam_yaw_);
    float cp = std::cos(cam_pitch_), sp = std::sin(cam_pitch_);
    float x1 = p.x * cy - p.z * sy;
    float z1 = p.x * sy + p.z * cy;
    float y1 = p.y * cp - z1 * sp;
    float z2 = p.y * sp + z1 * cp;
    float persp = 1.0f / (1.0f + z2 * 0.15f);
    float s = scale * cam_zoom_ * persp;
    return QPointF(center.x() + x1 * s, center.y() - y1 * s);
}

// ---- Paint ----
void Surface3DWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int W = width(), H = height();

    // Obsidian background — pure dark base
    painter.fillRect(rect(), QColor(8, 8, 8));

    if (!supported_) {
        painter.setPen(QColor(217, 119, 6));
        painter.setFont(QFont("Consolas", 13, QFont::Bold));
        painter.drawText(QRect(0, H / 2 - 28, W, 24), Qt::AlignCenter,
                         "3D NOT APPLICABLE");
        painter.setPen(QColor(140, 140, 140));
        painter.setFont(QFont("Consolas", 10));
        painter.drawText(QRect(0, H / 2, W, 22), Qt::AlignCenter,
                         "This surface is 1-D or categorical.");
        painter.drawText(QRect(0, H / 2 + 18, W, 22), Qt::AlignCenter,
                         "Switch to TABLE or LINE view.");
        return;
    }

    if (grid_.empty() || grid_[0].empty()) {
        painter.setPen(QColor(80, 80, 80));
        painter.setFont(QFont("Consolas", 11));
        painter.drawText(rect(), Qt::AlignCenter, "NO DATA LOADED");
        return;
    }

    QPointF center(W * 0.46f, H * 0.52f);
    float sc = std::min(W, H) * 0.34f;

    auto v3 = [](float vx, float vy, float vz) -> Vec3 { return {vx, vy, vz}; };
    auto proj = [&](Vec3 p) { return project(p, center, sc); };

    // ---- Wireframe box — Obsidian BORDER_MED/DIM ----
    QPen axis_pen(QColor(60, 60, 60, 220), 1.2f);
    QPen axis_dim_pen(QColor(35, 35, 35, 160), 1.0f);
    painter.setPen(axis_pen);
    painter.drawLine(proj(v3(-1, -1, -1)), proj(v3(1, -1, -1)));
    painter.drawLine(proj(v3(-1, -1, -1)), proj(v3(-1, -1, 1)));
    painter.drawLine(proj(v3(-1, -1, -1)), proj(v3(-1, 1, -1)));
    painter.setPen(axis_dim_pen);
    painter.drawLine(proj(v3(1, -1, -1)), proj(v3(1, -1, 1)));
    painter.drawLine(proj(v3(-1, -1, 1)), proj(v3(1, -1, 1)));
    painter.drawLine(proj(v3(1, -1, -1)), proj(v3(1, 1, -1)));
    painter.drawLine(proj(v3(-1, -1, 1)), proj(v3(-1, 1, 1)));
    painter.drawLine(proj(v3(1, -1, 1)), proj(v3(1, 1, 1)));
    painter.drawLine(proj(v3(-1, 1, -1)), proj(v3(1, 1, -1)));
    painter.drawLine(proj(v3(-1, 1, -1)), proj(v3(-1, 1, 1)));
    painter.drawLine(proj(v3(1, 1, -1)), proj(v3(1, 1, 1)));
    painter.drawLine(proj(v3(-1, 1, 1)), proj(v3(1, 1, 1)));

    // ---- Grid lines base plane ----
    QPen grid_pen(QColor(28, 28, 28, 180), 1.0f);
    painter.setPen(grid_pen);
    constexpr int n_grid = 8;
    for (int gi = 0; gi <= n_grid; gi++) {
        float gt = -1.0f + 2.0f * (float)gi / (float)n_grid;
        painter.drawLine(proj(v3(gt, -1, -1)), proj(v3(gt, -1, 1)));
        painter.drawLine(proj(v3(-1, -1, gt)), proj(v3(1, -1, gt)));
    }

    // ---- Build + depth-sort quads ----
    int rows = (int)grid_.size();
    int cols = (int)grid_[0].size();
    float range = (max_val_ > min_val_) ? (max_val_ - min_val_) : 1.0f;

    int step_r = std::max(1, rows / 40);
    int step_c = std::max(1, cols / 40);

    float cam_sy = std::sin(cam_yaw_), cam_cy = std::cos(cam_yaw_);
    float cam_sp = std::sin(cam_pitch_), cam_cp = std::cos(cam_pitch_);
    auto depth_of = [&](Vec3 pt) -> float {
        float zr = pt.x * cam_sy + pt.z * cam_cy;
        return pt.y * cam_sp + zr * cam_cp;
    };

    struct SurfQuad {
        QPointF pts[4];
        QColor fill, edge;
        float depth = 0.0f;
        float value = 0.0f;
        int row = 0;
        int col = 0;
    };
    std::vector<SurfQuad> quads;
    quads.reserve((rows / step_r) * (cols / step_c));

    for (int i = 0; i < rows - step_r; i += step_r) {
        for (int j = 0; j < cols - step_c; j += step_c) {
            int i2 = std::min(i + step_r, rows - 1);
            int j2 = std::min(j + step_c, cols - 1);

            float x0 = -1.0f + 2.0f * (float)j / (float)(cols - 1);
            float x1 = -1.0f + 2.0f * (float)j2 / (float)(cols - 1);
            float zz0 = -1.0f + 2.0f * (float)i / (float)(rows - 1);
            float zz1 = -1.0f + 2.0f * (float)i2 / (float)(rows - 1);

            float v00 = grid_[i][j], v01 = grid_[i][j2], v10 = grid_[i2][j], v11 = grid_[i2][j2];
            float y00 = -1.0f + 2.0f * (v00 - min_val_) / range;
            float y01 = -1.0f + 2.0f * (v01 - min_val_) / range;
            float y10 = -1.0f + 2.0f * (v10 - min_val_) / range;
            float y11 = -1.0f + 2.0f * (v11 - min_val_) / range;

            float avg_val = (v00 + v01 + v10 + v11) * 0.25f;
            float ct = (avg_val - min_val_) / range;
            QColor base_col = surface_color(ct, diverging_);

            // Brighter lighting — ambient 0.65 + directional, never below 55%
            float dx1 = x1 - x0, dy1 = y01 - y00;
            float dz2 = zz1 - zz0;
            float nx = dy1 * dz2;
            float ny = -dx1 * dz2;
            float nz = 0.0f;
            float nlen = std::sqrt(nx * nx + ny * ny + 1.0f) + 0.001f;
            nx /= nlen;
            ny /= nlen;
            // Light from upper-left-front: (0.4, 0.8, -0.4) normalised
            float diffuse = 0.65f + 0.35f * std::max(0.0f, 0.4f * nx + 0.8f * ny - 0.2f * nz);
            diffuse = std::max(0.55f, std::min(1.0f, diffuse));

            QColor lit =
                QColor(std::min(255, (int)(base_col.red() * diffuse)), std::min(255, (int)(base_col.green() * diffuse)),
                       std::min(255, (int)(base_col.blue() * diffuse)));
            // Edge: slightly darker version of fill, low alpha
            QColor edge_col =
                QColor(std::max(0, lit.red() - 30), std::max(0, lit.green() - 30), std::max(0, lit.blue() - 30), 60);

            Vec3 c4 = v3((x0 + x1) * 0.5f, (y00 + y01 + y10 + y11) * 0.25f, (zz0 + zz1) * 0.5f);
            float depth = depth_of(c4);

            SurfQuad q;
            q.pts[0] = proj(v3(x0, y00, zz0));
            q.pts[1] = proj(v3(x1, y01, zz0));
            q.pts[2] = proj(v3(x1, y11, zz1));
            q.pts[3] = proj(v3(x0, y10, zz1));
            q.fill = lit;
            q.edge = edge_col;
            q.depth = depth;
            q.value = avg_val;
            quads.push_back(q);
        }
    }

    // Back-to-front sort
    std::sort(quads.begin(), quads.end(), [](const SurfQuad& a, const SurfQuad& b) { return a.depth < b.depth; });

    // Draw quads
    for (const auto& q : quads) {
        QPolygonF poly;
        for (int k = 0; k < 4; k++)
            poly << q.pts[k];
        painter.setBrush(q.fill);
        painter.setPen(QPen(q.edge, 0.5f));
        painter.drawPolygon(poly);
    }

    // ---- Axis labels — Obsidian amber ----
    QFont label_font("Consolas", 8);
    label_font.setBold(true);
    painter.setFont(label_font);
    painter.setPen(QColor(217, 119, 6)); // AMBER

    auto draw_label = [&](Vec3 pos, const QString& text) {
        QPointF tp = proj(pos);
        QFontMetrics fm(painter.font());
        QRectF br = fm.boundingRect(text);
        painter.fillRect(QRectF(tp.x() - 3, tp.y() - 2, br.width() + 6, br.height() + 4), QColor(8, 8, 8, 200));
        painter.drawText(tp + QPointF(0, br.height()), text);
    };

    draw_label(v3(0.0f, -1.45f, -1.0f), QString::fromStdString(x_label_));
    draw_label(v3(-1.55f, 0.0f, -1.0f), QString::fromStdString(y_label_));
    draw_label(v3(-1.0f, -1.45f, 0.0f), QString::fromStdString(z_label_));

    // ---- Tick labels — bright white for visibility ----
    QFont tick_font("Consolas", 7);
    painter.setFont(tick_font);
    painter.setPen(QColor(220, 220, 220)); // near-white

    constexpr int n_ticks = 5;
    for (int ti = 0; ti <= n_ticks; ti++) {
        float frac = (float)ti / (float)n_ticks;
        float norm = -1.0f + 2.0f * frac;

        QFontMetrics tfm(tick_font);
        auto draw_tick = [&](QPointF tp, const QString& txt) {
            QRectF br = tfm.boundingRect(txt);
            painter.fillRect(QRectF(tp.x() - 1, tp.y() - br.height() + 2, br.width() + 4, br.height() + 2),
                             QColor(8, 8, 8, 190));
            painter.drawText(tp, txt);
        };

        if (has_col_labels_ && !col_labels_.empty()) {
            int idx = (int)(frac * (col_labels_.size() - 1));
            idx = std::max(0, std::min((int)col_labels_.size() - 1, idx));
            draw_tick(proj(v3(norm, -1.0f, -1.15f)), QString::fromStdString(col_labels_[idx]));
        }

        float yval = min_val_ + frac * range;
        draw_tick(proj(v3(-1.2f, norm, -1.0f)), QString::number((double)yval, 'f', 1));

        if (has_row_labels_ && !row_labels_.empty()) {
            int idx = (int)(frac * (row_labels_.size() - 1));
            idx = std::max(0, std::min((int)row_labels_.size() - 1, idx));
            draw_tick(proj(v3(-1.15f, -1.0f, norm)), QString::fromStdString(row_labels_[idx]));
        }
    }

    // ---- Color legend bar ----
    int bar_x = W - 26, bar_y = H / 4, bar_h = H / 2, bar_w = 12;
    for (int py = 0; py < bar_h; py++) {
        float t = 1.0f - (float)py / (float)bar_h;
        QColor c = surface_color(t, diverging_);
        painter.setPen(c);
        painter.drawLine(bar_x, bar_y + py, bar_x + bar_w, bar_y + py);
    }
    painter.setPen(QColor(50, 50, 50));
    painter.drawRect(bar_x, bar_y, bar_w, bar_h);
    QFont tiny_font("Consolas", 7);
    painter.setFont(tiny_font);
    painter.setPen(QColor(230, 230, 230));
    painter.drawText(bar_x - 2, bar_y - 4, QString::number((double)max_val_, 'f', 1));
    painter.drawText(bar_x - 2, bar_y + bar_h + 12, QString::number((double)min_val_, 'f', 1));
}

// ---- Mouse / wheel ----
void Surface3DWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        drag_start_ = event->pos();
        drag_start_yaw_ = cam_yaw_;
        drag_start_pitch_ = cam_pitch_;
    }
}

void Surface3DWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        float dx = event->pos().x() - drag_start_.x();
        float dy = event->pos().y() - drag_start_.y();
        // 3× faster rotation for responsive feel
        cam_yaw_ = drag_start_yaw_ + dx * 0.012f;
        cam_pitch_ = std::max(0.05f, std::min(1.55f, drag_start_pitch_ + dy * 0.010f));
        update();
    }
}

void Surface3DWidget::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
}

void Surface3DWidget::wheelEvent(QWheelEvent* event) {
    float delta = event->angleDelta().y() / 120.0f;
    cam_zoom_ = std::max(0.4f, std::min(4.0f, cam_zoom_ + delta * 0.18f));
    update();
}

} // namespace fincept::surface
