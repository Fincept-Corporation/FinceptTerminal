#pragma once
// Surface3DWidget — 3D surface renderer using QPainter
// Supports drag-to-rotate, scroll-to-zoom, color mapping (sequential + diverging)

#include <QWidget>

#include <string>
#include <vector>

namespace fincept::surface {

class Surface3DWidget : public QWidget {
    Q_OBJECT
  public:
    explicit Surface3DWidget(QWidget* parent = nullptr);

    void set_surface(const std::vector<std::vector<float>>& grid, const std::string& x_label,
                     const std::string& y_label, const std::string& z_label, float min_val, float max_val,
                     bool diverging = false, const std::vector<std::string>* col_labels = nullptr,
                     const std::vector<std::string>* row_labels = nullptr);

    // When false the widget paints an overlay telling the user the active
    // chart type cannot be shown in 3D and to switch to TABLE / LINE view.
    void set_supported(bool supported);

    void clear();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

  private:
    struct Vec3 {
        float x, y, z;
    };
    struct SurfQuad {
        QPointF pts[4];
        QColor color;
        float depth;
        int row, col;
        float value;
    };

    QPointF project(Vec3 p, QPointF center, float scale) const;
    QColor surface_color(float t, bool diverging) const;
    static QColor lerp_color(const QColor& a, const QColor& b, float t);

    std::vector<std::vector<float>> grid_;
    std::string x_label_, y_label_, z_label_;
    float min_val_ = 0, max_val_ = 1;
    bool diverging_ = false;
    std::vector<std::string> col_labels_, row_labels_;
    bool has_col_labels_ = false, has_row_labels_ = false;

    float cam_yaw_ = 0.55f;   // initial yaw — slight left angle
    float cam_pitch_ = 0.50f; // initial pitch — slightly elevated view
    float cam_zoom_ = 1.25f;  // slightly zoomed in for better fill

    bool dragging_ = false;
    QPoint drag_start_;
    float drag_start_yaw_ = 0, drag_start_pitch_ = 0;

    bool supported_ = true;
};

} // namespace fincept::surface
