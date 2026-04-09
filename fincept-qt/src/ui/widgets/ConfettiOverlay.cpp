#include "ui/widgets/ConfettiOverlay.h"

#include <QPainterPath>
#include <QRandomGenerator>

#include <cmath>

namespace fincept::ui {

static const QColor COLORS[] = {
    QColor(217, 119, 6),   // amber
    QColor(255, 191, 36),  // gold
    QColor(22, 163, 74),   // green
    QColor(52, 211, 153),  // emerald
    QColor(8, 145, 178),   // cyan
    QColor(56, 189, 248),  // sky
    QColor(220, 38, 38),   // red
    QColor(251, 113, 133), // rose
    QColor(147, 51, 234),  // purple
    QColor(192, 132, 252), // violet
    QColor(234, 179, 8),   // yellow
    QColor(59, 130, 246),  // blue
    QColor(236, 72, 153),  // pink
    QColor(245, 158, 11),  // orange
};
static const int NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);

ConfettiOverlay::ConfettiOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    hide();

    timer_ = new QTimer(this);
    timer_->setInterval(16); // ~60 fps for smooth animation
    connect(timer_, &QTimer::timeout, this, &ConfettiOverlay::tick);

    end_timer_ = new QTimer(this);
    end_timer_->setSingleShot(true);
    connect(end_timer_, &QTimer::timeout, this, [this]() {
        timer_->stop();
        hide();
        particles_.clear();
    });
}

void ConfettiOverlay::show_confetti() {
    if (auto* p = parentWidget()) {
        setGeometry(0, 0, p->width(), p->height());
    }
    elapsed_ms_ = 0;
    particles_.clear();

    int w = width();

    // Initial big burst from center
    spawn_burst(120, w * 0.5f, w * 0.4f, 2.0f);

    // Side bursts
    spawn_burst(60, w * 0.15f, w * 0.15f, 2.5f);
    spawn_burst(60, w * 0.85f, w * 0.15f, 2.5f);

    // Delayed secondary bursts (staggered for waves)
    spawn_burst(50, w * 0.3f, w * 0.2f, 1.8f);
    spawn_burst(50, w * 0.7f, w * 0.2f, 1.8f);

    // Late gentle rain
    spawn_burst(40, w * 0.5f, w * 0.5f, 1.0f);

    // Set staggered delays for wave effect
    auto* rng = QRandomGenerator::global();
    int idx = 0;
    for (auto& p : particles_) {
        if (idx < 120)
            p.delay_ms = rng->bounded(200); // first burst: immediate
        else if (idx < 240)
            p.delay_ms = 300 + rng->bounded(400); // side bursts: 0.3-0.7s
        else if (idx < 340)
            p.delay_ms = 1000 + rng->bounded(800); // secondary: 1-1.8s
        else
            p.delay_ms = 2500 + rng->bounded(1500); // rain: 2.5-4s
        idx++;
    }

    show();
    raise();
    timer_->start();
    end_timer_->start(10000);
}

void ConfettiOverlay::spawn_burst(int count, float origin_x, float spread, float base_vy) {
    auto* rng = QRandomGenerator::global();
    int w = width();

    for (int i = 0; i < count; ++i) {
        Particle p;
        p.x = origin_x + (rng->generateDouble() - 0.5f) * spread;
        p.y = -rng->bounded(60) - 10;
        p.vx = (rng->generateDouble() - 0.5f) * 6.0f;
        p.vy = rng->generateDouble() * base_vy + 1.0f;
        p.rotation = rng->bounded(360);
        p.rotation_speed = (rng->generateDouble() - 0.5f) * 12.0f;
        p.size = rng->bounded(4, 12);
        p.sway_offset = rng->generateDouble() * 6.28f;
        p.sway_amplitude = rng->generateDouble() * 2.0f + 0.5f;
        p.shimmer_phase = rng->generateDouble() * 6.28f;
        p.delay_ms = 0;
        p.base_color = COLORS[rng->bounded(NUM_COLORS)];
        p.color = p.base_color;

        // Mix of shapes: 40% rect, 25% circle, 20% streamer, 15% star
        int shape_roll = rng->bounded(100);
        if (shape_roll < 40)
            p.shape = Shape::Rect;
        else if (shape_roll < 65)
            p.shape = Shape::Circle;
        else if (shape_roll < 85)
            p.shape = Shape::Streamer;
        else
            p.shape = Shape::Star;

        // Streamers are taller and narrower
        if (p.shape == Shape::Streamer)
            p.size = rng->bounded(3, 6);

        particles_.push_back(p);
    }
}

void ConfettiOverlay::tick() {
    elapsed_ms_ += timer_->interval();
    int h = height();
    int w = width();
    float t = elapsed_ms_ / 1000.0f; // time in seconds

    // Fade out in the last 3 seconds
    float fade = 1.0f;
    if (elapsed_ms_ > 7000)
        fade = 1.0f - (elapsed_ms_ - 7000) / 3000.0f;
    fade = std::max(0.0f, fade);

    for (auto& p : particles_) {
        // Skip particles that haven't spawned yet
        if (elapsed_ms_ < p.delay_ms)
            continue;

        float local_t = (elapsed_ms_ - p.delay_ms) / 1000.0f;

        p.vy += 0.04f;  // gravity
        p.vx *= 0.997f; // air resistance

        // Sinusoidal sway for fluttering effect
        float sway = std::sin(local_t * 3.0f + p.sway_offset) * p.sway_amplitude;
        p.x += p.vx + sway * 0.3f;
        p.y += p.vy;
        p.rotation += p.rotation_speed;

        // Shimmer: oscillate alpha
        float shimmer = 0.7f + 0.3f * std::sin(local_t * 8.0f + p.shimmer_phase);

        // Respawn particles that fall off-screen (first 6 seconds only)
        if (p.y > h + 30 && elapsed_ms_ < 6000) {
            auto* rng = QRandomGenerator::global();
            p.y = -rng->bounded(40) - 10;
            p.x = rng->bounded(w);
            p.vy = rng->generateDouble() * 1.5f + 0.8f;
            p.vx = (rng->generateDouble() - 0.5f) * 3.0f;
        }

        // Apply fade + shimmer
        QColor c = p.base_color;
        c.setAlphaF(fade * shimmer);
        p.color = c;
    }

    update();
}

void ConfettiOverlay::draw_star(QPainter& painter, float size) {
    float outer = size;
    float inner = size * 0.4f;
    QPolygonF star;
    for (int i = 0; i < 10; ++i) {
        float r = (i % 2 == 0) ? outer : inner;
        float angle = (i * 36.0f - 90.0f) * 3.14159f / 180.0f;
        star << QPointF(r * std::cos(angle), r * std::sin(angle));
    }
    painter.drawPolygon(star);
}

void ConfettiOverlay::draw_streamer(QPainter& painter, float size, float rotation) {
    // Wavy ribbon
    float w = size;
    float h = size * 4.0f;
    float wave = std::sin(rotation * 0.1f) * w * 0.5f;

    QPainterPath path;
    path.moveTo(-w / 2, -h / 2);
    path.cubicTo(-w / 2 + wave, -h / 6, w / 2 - wave, h / 6, w / 2, h / 2);
    path.lineTo(w / 2 - 1, h / 2);
    path.cubicTo(w / 2 - 1 - wave, h / 6, -w / 2 + 1 + wave, -h / 6, -w / 2 + 1, -h / 2);
    path.closeSubpath();
    painter.drawPath(path);
}

void ConfettiOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    for (const auto& p : particles_) {
        // Skip particles that haven't spawned yet
        if (elapsed_ms_ < p.delay_ms)
            continue;

        if (p.color.alphaF() < 0.01f)
            continue;

        painter.save();
        painter.translate(p.x, p.y);
        painter.rotate(p.rotation);
        painter.setPen(Qt::NoPen);
        painter.setBrush(p.color);

        switch (p.shape) {
            case Shape::Rect: {
                // 3D-ish tumbling rectangle: vary width by rotation
                float w = p.size * (0.5f + 0.5f * std::abs(std::cos(p.rotation * 0.05f)));
                float h = p.size * 1.4f;
                painter.drawRect(QRectF(-w / 2, -h / 2, w, h));
                break;
            }
            case Shape::Circle:
                painter.drawEllipse(QPointF(0, 0), p.size * 0.5f, p.size * 0.5f);
                break;
            case Shape::Star:
                draw_star(painter, p.size * 0.6f);
                break;
            case Shape::Streamer:
                draw_streamer(painter, p.size, p.rotation);
                break;
        }

        painter.restore();
    }
}

} // namespace fincept::ui
