#pragma once
#include <QPainter>
#include <QTimer>
#include <QWidget>

#include <vector>

namespace fincept::ui {

/// Transparent overlay that renders a 10-second confetti burst animation
/// with multiple particle shapes, shimmer, sway, and staggered bursts.
class ConfettiOverlay : public QWidget {
    Q_OBJECT
  public:
    explicit ConfettiOverlay(QWidget* parent = nullptr);

    void show_confetti();

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    enum class Shape { Rect, Circle, Star, Streamer };

    struct Particle {
        float x, y;
        float vx, vy;
        float rotation, rotation_speed;
        float size;
        float sway_offset;   // phase offset for sine sway
        float sway_amplitude;
        float shimmer_phase;
        float delay_ms;      // staggered spawn delay
        QColor base_color;
        QColor color;
        Shape shape;
    };

    void spawn_burst(int count, float origin_x, float spread, float base_vy);
    void tick();

    std::vector<Particle> particles_;
    QTimer* timer_ = nullptr;
    QTimer* end_timer_ = nullptr;
    int elapsed_ms_ = 0;

    void draw_star(QPainter& painter, float size);
    void draw_streamer(QPainter& painter, float size, float rotation);
};

} // namespace fincept::ui
