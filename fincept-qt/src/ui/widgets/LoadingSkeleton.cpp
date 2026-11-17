// src/ui/widgets/LoadingSkeleton.cpp — see the header.

#include "ui/widgets/LoadingSkeleton.h"

#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QVector>

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <optional>

namespace fincept::ui {

namespace {

constexpr int kSkTickMs = 60;
constexpr int kSkPeriodTicks = 24; // one shimmer sweep ≈ 1.4 s

// The setting, read on first use and then kept current by set_motion_enabled()
// (Settings > Appearance calls it on save). GUI thread.
std::optional<bool>& sk_motion_flag() {
    static std::optional<bool> flag;
    return flag;
}

} // namespace

bool motion_enabled() {
    auto& flag = sk_motion_flag();
    if (!flag.has_value()) {
        const auto r = SettingsRepository::instance().get(QStringLiteral("appearance.animations"));
        flag = !(r.is_ok() && r.value().trimmed().compare(QLatin1String("false"), Qt::CaseInsensitive) == 0);
    }
    return *flag;
}

void set_motion_enabled(bool on) {
    sk_motion_flag() = on;
}

LoadingSkeleton::LoadingSkeleton(QWidget* parent) : LoadingSkeleton(Shape::Result, parent) {}

LoadingSkeleton::LoadingSkeleton(Shape shape, QWidget* parent) : QWidget(parent), shape_(shape) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(tr("Loading"));
    timer_.setInterval(kSkTickMs);
    connect(&timer_, &QTimer::timeout, this, [this]() {
        if (!motion_enabled()) { // switched off while running: settle on a still frame
            timer_.stop();
            phase_ = 0;
            update();
            return;
        }
        phase_ = (phase_ + 1) % kSkPeriodTicks;
        update();
    });
}

void LoadingSkeleton::set_shape(Shape shape) {
    if (shape_ == shape)
        return;
    shape_ = shape;
    update();
}

void LoadingSkeleton::set_message(const QString& message) {
    if (message_ == message)
        return;
    message_ = message;
    update_accessible_description();
    update();
}

void LoadingSkeleton::set_progress(double fraction, const QString& text) {
    const double f = std::isnan(fraction) || fraction < 0 ? -1.0 : std::min(1.0, fraction);
    if (f == progress_ && text == progress_text_)
        return;
    progress_ = f;
    progress_text_ = text;
    update_accessible_description();
    update();
}

void LoadingSkeleton::clear_progress() {
    set_progress(-1.0, QString());
}

void LoadingSkeleton::update_accessible_description() {
    QStringList parts;
    if (!message_.isEmpty())
        parts << message_;
    if (progress_ >= 0)
        parts << tr("%1 % done").arg(int(progress_ * 100.0 + 0.5));
    if (!progress_text_.isEmpty())
        parts << progress_text_;
    const QString desc = parts.join(QStringLiteral(" · "));
    if (accessibleDescription() != desc)
        setAccessibleDescription(desc);
}

void LoadingSkeleton::set_kpi_count(int n) {
    kpis_ = std::clamp(n, 1, 8);
    update();
}

void LoadingSkeleton::set_row_count(int n) {
    rows_ = std::clamp(n, 1, 40);
    update();
}

void LoadingSkeleton::set_opaque(bool opaque) {
    opaque_ = opaque;
    update();
}

void LoadingSkeleton::set_animated(bool animated) {
    animated_ = animated;
    if (!animated_) {
        timer_.stop();
        phase_ = 0;
    } else if (isVisible() && motion_enabled()) {
        timer_.start();
    }
    update();
}

void LoadingSkeleton::set_ghost(bool ghost) {
    ghost_ = ghost;
    if (ghost_) {
        opaque_ = false;
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    set_animated(!ghost_);
}

QSize LoadingSkeleton::sizeHint() const {
    return {fonts::font_px(0) * 30, fonts::font_px(0) * 20};
}

QSize LoadingSkeleton::minimumSizeHint() const {
    return {fonts::font_px(0) * 8, fonts::font_px(0) * 5};
}

void LoadingSkeleton::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (animated_ && motion_enabled())
        timer_.start();
}

void LoadingSkeleton::hideEvent(QHideEvent* e) {
    timer_.stop();
    QWidget::hideEvent(e);
}

void LoadingSkeleton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRect r = rect();
    if (opaque_)
        p.fillRect(r, QColor(colors::BG_BASE()));
    if (ghost_)
        p.setOpacity(0.5);

    const int fpx = fonts::font_px(0);
    const int m = std::max(12, fpx);
    const int gap = std::max(6, fpx / 2);
    const int left = r.left() + m;
    const int width = std::max(10, r.width() - 2 * m);
    const int bottom = r.bottom() - m;
    int y = r.top() + m;

    const bool dark = colors::is_dark();
    const QColor block = colors::overlay_color(dark ? 0.065 : 0.07);
    const QColor block_strong = colors::overlay_color(dark ? 0.10 : 0.11);

    // Shimmer: one diagonal-free band sweeping left → right across the whole widget.
    const double t = double(phase_) / double(kSkPeriodTicks);
    const double band_c = -0.25 + t * 1.5; // band centre, in widget widths
    QLinearGradient shimmer(r.left(), 0, r.right(), 0);
    const QColor clear = colors::overlay_color(0.0);
    const QColor hi = colors::overlay_color(dark ? 0.07 : 0.10);
    auto stop = [&](double at, const QColor& c) {
        if (at > 0.0 && at < 1.0)
            shimmer.setColorAt(at, c);
    };
    shimmer.setColorAt(0.0, clear);
    stop(band_c - 0.14, clear);
    stop(band_c, hi);
    stop(band_c + 0.14, clear);
    shimmer.setColorAt(1.0, clear);

    QVector<QRectF> blocks;
    QVector<QRectF> strong;
    auto add = [&](qreal x, qreal yy, qreal w, qreal h, bool emphasis = false) {
        if (yy + h > bottom || w <= 0 || h <= 0)
            return false;
        (emphasis ? strong : blocks).append(QRectF(x, yy, w, h));
        return true;
    };

    // Caption + indeterminate progress line: the explicit "working" signal.
    if (!message_.isEmpty() && !ghost_) {
        QFont f = font();
        f.setPixelSize(fonts::font_px(-2));
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(colors::TEXT_SECONDARY()));
        const int th = QFontMetrics(f).height();
        p.drawText(QRect(left, y, width, th), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(f).elidedText(message_, Qt::ElideRight, width));
        y += th + 6;
    }
    if (!ghost_) {
        if (progress_ >= 0) {
            // Determinate (set_progress): a thicker track filled to the fraction —
            // drawn the same with motion off (it is a value, not an animation).
            const int bar_h = std::max(3, fpx / 4);
            const QRectF track(left, y, width, bar_h);
            p.fillRect(track, colors::overlay_color(dark ? 0.10 : 0.12));
            const double done = width * progress_;
            if (done > 0)
                p.fillRect(QRectF(left, y, done, bar_h), QColor(colors::AMBER()));
            y += bar_h;
        } else {
            const QRectF track(left, y, width, 2);
            p.fillRect(track, colors::overlay_color(dark ? 0.06 : 0.08));
            const double seg = width * 0.22;
            // Still (motion off): a fixed segment at the left still says "working".
            const double x0 = timer_.isActive() ? left - seg + (width + seg) * t : double(left);
            QRectF run(std::max<double>(left, x0), y, 0, 2);
            run.setRight(std::min<double>(left + width, x0 + seg));
            if (run.width() > 0)
                p.fillRect(run, QColor(colors::AMBER()));
            y += 2;
        }
        if (!progress_text_.isEmpty() || progress_ >= 0) {
            // "42 % · Downloading 3 of 12 series" under the bar.
            QFont f = font();
            f.setPixelSize(fonts::font_px(-2));
            p.setFont(f);
            p.setPen(QColor(colors::TEXT_SECONDARY()));
            const QFontMetrics fm(f);
            QString line = progress_text_;
            if (progress_ >= 0) {
                const QString pct = tr("%1 %").arg(int(progress_ * 100.0 + 0.5));
                line = line.isEmpty() ? pct : pct + QStringLiteral(" · ") + line;
            }
            y += 4;
            p.drawText(QRect(left, y, width, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       fm.elidedText(line, Qt::ElideRight, width));
            y += fm.height();
        }
        y += gap * 2;
    }

    const int row_h = std::max(10, int(fpx * 1.5));
    switch (shape_) {
        case Shape::Result: {
            const int n = kpis_;
            const qreal tile_w = (width - gap * (n - 1)) / qreal(n);
            const int tile_h = int(fpx * 3.4);
            for (int i = 0; i < n; ++i)
                add(left + i * (tile_w + gap), y, tile_w, tile_h, true);
            y += tile_h + gap * 2;
            const int remaining = bottom - y;
            const int chart_h = std::max(row_h * 4, int(remaining * 0.5));
            if (add(left, y, width, chart_h))
                y += chart_h + gap * 2;
            static const double kCols[] = {0.34, 0.2, 0.2, 0.18};
            for (int rr = 0; rr < rows_; ++rr) {
                qreal x = left;
                for (double c : kCols) {
                    const qreal w = width * c - gap;
                    add(x, y, w * (rr % 2 ? 0.86 : 1.0), row_h * 0.7);
                    x += width * c + gap / 2.0;
                }
                y += row_h + gap / 2;
                if (y > bottom)
                    break;
            }
            break;
        }
        case Shape::Table: {
            add(left, y, width, row_h, true);
            y += row_h + gap;
            static const double kCols[] = {0.3, 0.18, 0.16, 0.16, 0.14};
            for (int rr = 0; rr < rows_ && y < bottom; ++rr) {
                qreal x = left;
                for (double c : kCols) {
                    const qreal w = width * c - gap;
                    add(x, y, w * ((rr + int(c * 10)) % 3 ? 0.9 : 0.7), row_h * 0.7);
                    x += width * c + gap / 2.0;
                }
                y += row_h + gap / 2;
            }
            break;
        }
        case Shape::Chart: {
            const int h = std::max(row_h * 3, bottom - y);
            add(left, y, width, h);
            break;
        }
        case Shape::Cards: {
            const int cols = width > fpx * 50 ? 3 : (width > fpx * 26 ? 2 : 1);
            const qreal cw = (width - gap * (cols - 1)) / qreal(cols);
            const int ch = int(fpx * 6.5);
            for (int rr = 0; rr < 4 && y < bottom; ++rr) {
                for (int c = 0; c < cols; ++c)
                    add(left + c * (cw + gap), y, cw, ch, c == 0 && rr == 0);
                y += ch + gap;
            }
            break;
        }
        case Shape::Text: {
            static const double kWidths[] = {1.0, 0.94, 0.97, 0.72, 0.0, 0.98, 0.9, 0.6};
            for (int rr = 0; rr < rows_ * 2 && y < bottom; ++rr) {
                const double w = kWidths[rr % 8];
                if (w > 0)
                    add(left, y, width * w, row_h * 0.6);
                y += row_h;
            }
            break;
        }
        case Shape::Form: {
            for (int rr = 0; rr < rows_ && y < bottom; ++rr) {
                add(left, y, width * 0.3, row_h * 0.55);
                y += row_h * 0.55 + 4;
                add(left, y, width, row_h * 1.2, true);
                y += row_h * 1.2 + gap;
            }
            break;
        }
        case Shape::List: {
            // List items: a leading mark, a title over a thinner sub-line, a short value at the right.
            const int line_h = std::max(8, int(row_h * 0.55));
            const int mark = line_h * 2 + gap / 2;
            const qreal value_w = width * 0.16;
            const qreal text_x = left + mark + gap;
            const qreal text_w = width - mark - gap * 2 - value_w;
            for (int rr = 0; rr < rows_ && y < bottom; ++rr) {
                if (!add(left, y, mark, mark, true))
                    break;
                add(text_x, y, text_w * (rr % 3 ? 0.55 : 0.72), line_h);
                add(text_x, y + line_h + gap / 2.0, text_w * (rr % 2 ? 0.86 : 0.64), line_h * 0.8);
                const qreal vw = value_w * (rr % 2 ? 0.8 : 1.0); // right-aligned, like a price column
                add(left + width - vw, y + (mark - line_h) / 2.0, vw, line_h);
                y += mark + gap + gap / 2;
            }
            break;
        }
    }

    p.setPen(Qt::NoPen);
    const bool shimmering = timer_.isActive();
    for (const QRectF& b : blocks) {
        p.setBrush(block);
        p.drawRoundedRect(b, 2, 2);
        if (shimmering) {
            p.setBrush(shimmer);
            p.drawRoundedRect(b, 2, 2);
        }
    }
    for (const QRectF& b : strong) {
        p.setBrush(block_strong);
        p.drawRoundedRect(b, 2, 2);
        if (shimmering) {
            p.setBrush(shimmer);
            p.drawRoundedRect(b, 2, 2);
        }
    }
}

} // namespace fincept::ui
