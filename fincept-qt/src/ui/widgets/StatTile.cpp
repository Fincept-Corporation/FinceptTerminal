#include "ui/widgets/StatTile.h"

#include "ui/charts/ChartKit.h"
#include "ui/charts/Sparkline.h"
#include "ui/theme/Theme.h"

#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace fincept::ui {

struct StatTile::Metrics {
    QFont label_f;
    QFont value_f;
    QFont delta_f;
    QFont sub_f;
    QFont badge_f;
    int pad_h = 0;
    int pad_v = 0;
    int gap = 0;
    int label_h = 0;
    int value_h = 0;
    int sub_h = 0;
    int spark_h = 0;
    int stripe = 0;
};

StatTile::StatTile(QWidget* parent) : StatTile(QString(), parent) {}

StatTile::StatTile(const QString& label, QWidget* parent) : QWidget(parent), label_(label) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAccessibleName(label_);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        updateGeometry();
        update();
    });
}

StatTile::Metrics StatTile::metrics() const {
    // A pure function of the theme's font settings and compact_, but asked for
    // on every size hint and paint — a KpiStrip asks every tile several times
    // per layout pass — and building five fonts and three QFontMetrics each
    // time showed up in screen-open profiles. Cached per (compact, font);
    // GUI thread only. Heap-allocated and never freed on purpose: fonts must
    // not be destroyed after the application object at exit.
    struct Cached {
        bool valid = false;
        int px = 0;
        QByteArray family;
        Metrics m;
    };
    static auto* cache = new Cached[2];
    const auto& theme = ThemeManager::instance();
    const QByteArray family(theme.tokens().font_family);
    Cached& slot = cache[compact_ ? 1 : 0];
    if (slot.valid && slot.px == theme.font_size_px() && slot.family == family)
        return slot.m;

    Metrics m;
    m.label_f = fonts::font(-3, QFont::Medium);
    m.value_f = fonts::font(compact_ ? 2 : 6, QFont::DemiBold);
    m.delta_f = fonts::font(-2, QFont::DemiBold);
    m.sub_f = fonts::font(-3);
    m.badge_f = fonts::font(-4, QFont::Bold);
    const QFontMetrics fl(m.label_f);
    const QFontMetrics fv(m.value_f);
    const QFontMetrics fs(m.sub_f);
    m.pad_h = fl.averageCharWidth() + fl.averageCharWidth() / 2 + 2;
    m.pad_v = fl.height() / 2 + 1;
    m.gap = std::max(2, fl.height() / 4);
    m.label_h = fl.height();
    m.value_h = fv.height();
    m.sub_h = fs.height();
    m.spark_h = std::max(fs.height(), fv.height() * 3 / 4);
    m.stripe = std::max(2, fl.averageCharWidth() / 3);
    slot = Cached{true, theme.font_size_px(), family, m};
    return m;
}

// ── Setters ──────────────────────────────────────────────────────────────────

int StatTile::stripe_width(const Metrics& m) const {
    return tone_ == Tone::Danger ? m.stripe * 2 : m.stripe;
}

int StatTile::mark_width(const Metrics& m) const {
    if (tone_ != Tone::Danger && tone_ != Tone::Success)
        return 0;
    // A square mark the height of the label's cap height, then a gap.
    const QFontMetrics fl(m.label_f);
    return std::max(6, fl.capHeight()) + m.gap * 2;
}

void StatTile::content_changed(bool geometry) {
    QString desc = value_;
    if (delta_.has_value())
        desc += QLatin1Char(' ') + (delta_text_.isEmpty() ? chartkit::format_signed_percent(*delta_) : delta_text_);
    // State tones are said, not only shown (the mark and colour are visual).
    if (tone_ == Tone::Danger)
        desc = tr("Alert: %1").arg(desc);
    else if (tone_ == Tone::Success)
        desc = tr("OK: %1").arg(desc);
    setAccessibleDescription(desc);
    if (geometry)
        updateGeometry();
    update();
}

void StatTile::set_label(const QString& label) {
    if (label_ == label)
        return;
    label_ = label;
    setAccessibleName(label_);
    content_changed(true);
}

void StatTile::set_value(const QString& text) {
    if (value_ == text)
        return;
    value_ = text;
    content_changed(false);
}

void StatTile::set_number(double value, int decimals, const QString& suffix) {
    set_value(std::isfinite(value) ? chartkit::format_number(value, decimals) + suffix : chartkit::format_number(value, 0));
}

void StatTile::set_delta(double value, const QString& text) {
    delta_ = value;
    delta_text_ = text;
    content_changed(false);
}

void StatTile::clear_delta() {
    if (!delta_.has_value())
        return;
    delta_.reset();
    delta_text_.clear();
    content_changed(false);
}

void StatTile::set_delta_inverted(bool lower_is_better) {
    delta_inverted_ = lower_is_better;
    update();
}

void StatTile::set_sub_label(const QString& text) {
    if (sub_ == text)
        return;
    const bool rows_change = sub_.isEmpty() != text.isEmpty();
    sub_ = text;
    content_changed(rows_change);
}

void StatTile::set_sparkline(const QVector<double>& values) {
    const bool rows_change = spark_.isEmpty() != values.isEmpty();
    spark_ = values;
    spark_baseline_.reset();
    content_changed(rows_change);
}

void StatTile::set_sparkline(const QVector<double>& values, double baseline) {
    const bool rows_change = spark_.isEmpty() != values.isEmpty();
    spark_ = values;
    spark_baseline_ = baseline;
    content_changed(rows_change);
}

void StatTile::clear_sparkline() {
    if (spark_.isEmpty())
        return;
    spark_.clear();
    spark_baseline_.reset();
    content_changed(true);
}

void StatTile::set_sparkline_color(SparkColor color) {
    if (spark_color_ == color)
        return;
    spark_color_ = color;
    if (!spark_.isEmpty())
        update();
}

void StatTile::set_tone(Tone tone) {
    if (tone_ == tone)
        return;
    const auto is_state = [](Tone t) { return t == Tone::Danger || t == Tone::Success; };
    // Danger / Success add a mark (and Danger a wider stripe): the size hint changes.
    const bool geometry = is_state(tone_) || is_state(tone);
    tone_ = tone;
    content_changed(geometry);
}

void StatTile::set_state(State state, const QString& reason) {
    const bool had_reason_row = state_ == State::Error && !reason_.isEmpty() && sub_.isEmpty();
    state_ = state;
    reason_ = reason;
    const bool has_reason_row = state_ == State::Error && !reason_.isEmpty() && sub_.isEmpty();
    refresh_tooltip();
    content_changed(had_reason_row != has_reason_row);
}

void StatTile::set_tooltip_text(const QString& text) {
    tooltip_ = text;
    refresh_tooltip();
}

void StatTile::refresh_tooltip() {
    QString tip = tooltip_;
    if (!reason_.isEmpty()) {
        QString prefix;
        switch (state_) {
            case State::Loading:
                prefix = tr("Loading");
                break;
            case State::Error:
                prefix = tr("Error");
                break;
            case State::Stale:
                prefix = tr("Stale");
                break;
            case State::Normal:
                break;
        }
        const QString line = prefix.isEmpty() ? reason_ : tr("%1: %2").arg(prefix, reason_);
        tip = tip.isEmpty() ? line : tip + QLatin1Char('\n') + line;
    }
    setToolTip(tip);
}

void StatTile::set_clickable(bool on) {
    clickable_ = on;
    setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setFocusPolicy(on ? Qt::TabFocus : Qt::NoFocus);
    update();
}

void StatTile::set_compact(bool on) {
    if (compact_ == on)
        return;
    compact_ = on;
    content_changed(true);
}

// ── Geometry ─────────────────────────────────────────────────────────────────

QSize StatTile::sizeHint() const {
    const Metrics m = metrics();
    const QFontMetrics fl(m.label_f);
    const QFontMetrics fv(m.value_f);
    const QFontMetrics fd(m.delta_f);
    const QFontMetrics fs(m.sub_f);

    int content_w = std::max(fl.horizontalAdvance(label_) + mark_width(m), fl.averageCharWidth() * 14);
    int value_w = fv.horizontalAdvance(value_.isEmpty() ? QStringLiteral("0000.00") : value_);
    if (delta_.has_value())
        value_w += m.gap * 2 + fd.horizontalAdvance(QStringLiteral("▲ ") + (delta_text_.isEmpty()
                                                                                  ? chartkit::format_signed_percent(*delta_)
                                                                                  : delta_text_));
    content_w = std::max(content_w, value_w);
    if (!sub_.isEmpty())
        content_w = std::max(content_w, fs.horizontalAdvance(sub_));

    int h = m.pad_v + m.label_h + m.gap + m.value_h;
    const bool sub_row = !sub_.isEmpty() || (state_ == State::Error && !reason_.isEmpty());
    if (sub_row)
        h += m.gap + m.sub_h;
    if (!spark_.isEmpty())
        h += m.gap + m.spark_h;
    h += m.pad_v;
    return {content_w + 2 * m.pad_h + stripe_width(m), h};
}

QSize StatTile::minimumSizeHint() const {
    const Metrics m = metrics();
    const QFontMetrics fl(m.label_f);
    return {fl.averageCharWidth() * 10 + 2 * m.pad_h + stripe_width(m) + mark_width(m), sizeHint().height()};
}

// ── Painting ─────────────────────────────────────────────────────────────────

void StatTile::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const Metrics m = metrics();
    const QRect r = rect();

    p.fillRect(r, chartkit::surface());
    QColor border = (clickable_ && hovered_) ? chartkit::axis_line() : chartkit::grid();
    if (clickable_ && hasFocus())
        border = chartkit::accent();
    p.setPen(QPen(border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(0, 0, -1, -1));

    QColor tone_color;
    switch (tone_) {
        case Tone::Positive:
            tone_color = chartkit::up();
            break;
        case Tone::Negative:
            tone_color = chartkit::down();
            break;
        case Tone::Warning:
            tone_color = chartkit::warning();
            break;
        case Tone::Danger:
            tone_color = QColor(colors::DANGER());
            break;
        case Tone::Success:
            tone_color = QColor(colors::SUCCESS());
            break;
        case Tone::Neutral:
            break;
    }
    const int stripe = stripe_width(m);
    if (tone_color.isValid())
        p.fillRect(QRect(r.left() + 1, r.top() + 1, stripe, r.height() - 2), tone_color);

    const bool dim = state_ == State::Stale || !isEnabled();
    const int x = r.left() + m.pad_h + (tone_color.isValid() ? stripe : 0);
    const int w = std::max(0, r.right() - m.pad_h - x + 1);
    int y = r.top() + m.pad_v;

    // Badge (top right) + label.
    QString badge;
    QColor badge_color;
    if (state_ == State::Stale) {
        badge = tr("STALE");
        badge_color = chartkit::warning();
    } else if (state_ == State::Error) {
        badge = tr("ERROR");
        badge_color = QColor(colors::DANGER());
    }
    int badge_w = 0;
    if (!badge.isEmpty()) {
        const QFontMetrics fb(m.badge_f);
        badge_w = fb.horizontalAdvance(badge) + fb.averageCharWidth() * 2;
        const QRect br(x + w - badge_w, y + (m.label_h - fb.height() - 2) / 2, badge_w, fb.height() + 2);
        p.fillRect(br, chartkit::with_alpha(badge_color, 0.16));
        p.setFont(m.badge_f);
        p.setPen(badge_color);
        p.drawText(br, Qt::AlignCenter, badge);
    }
    {
        const QFontMetrics fl(m.label_f);
        // State mark (Danger: cross, Success: tick) — a painted shape, so it
        // needs no glyph from the font and reads without colour.
        const int mark_w = std::min(mark_width(m), w);
        if (mark_w > 0) {
            const qreal s = std::max(6, fl.capHeight());
            const QRectF box(x, y + (m.label_h - s) / 2.0, s, s);
            QColor mark_col = dim ? chartkit::axis_text() : tone_color;
            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(QPen(mark_col, std::max(1.5, s / 5.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            if (tone_ == Tone::Danger) {
                const QRectF in = box.adjusted(s * 0.12, s * 0.12, -s * 0.12, -s * 0.12);
                p.drawLine(in.topLeft(), in.bottomRight());
                p.drawLine(in.topRight(), in.bottomLeft());
            } else {
                const QPointF pts[3] = {QPointF(box.left() + s * 0.08, box.top() + s * 0.55),
                                        QPointF(box.left() + s * 0.40, box.bottom() - s * 0.08),
                                        QPointF(box.right() - s * 0.04, box.top() + s * 0.10)};
                p.drawPolyline(pts, 3);
            }
            p.restore();
        }
        const int lx = x + mark_w;
        const int lw = std::max(0, w - mark_w - (badge_w > 0 ? badge_w + m.gap : 0));
        p.setFont(m.label_f);
        p.setPen(chartkit::axis_text());
        p.drawText(QRect(lx, y, lw, m.label_h), Qt::AlignLeft | Qt::AlignVCenter,
                   fl.elidedText(label_, Qt::ElideRight, lw));
    }
    y += m.label_h + m.gap;

    // Value row (+ delta, right-aligned on the same baseline).
    const QFontMetrics fv(m.value_f);
    if (state_ == State::Loading) {
        p.setFont(m.sub_f);
        p.setPen(chartkit::text_dim());
        p.drawText(QRect(x, y, w, m.value_h), Qt::AlignLeft | Qt::AlignVCenter, tr("Loading…"));
    } else {
        QString delta_str;
        QColor delta_col = chartkit::axis_text();
        int delta_w = 0;
        if (delta_.has_value() && state_ != State::Error) {
            const double d = *delta_;
            const QString body = delta_text_.isEmpty() ? chartkit::format_signed_percent(d) : delta_text_;
            if (d > 0.0)
                delta_str = QStringLiteral("▲ ") + body;
            else if (d < 0.0)
                delta_str = QStringLiteral("▼ ") + body;
            else
                delta_str = body;
            const bool good = delta_inverted_ ? d < 0.0 : d > 0.0;
            const bool bad = delta_inverted_ ? d > 0.0 : d < 0.0;
            if (good)
                delta_col = chartkit::up();
            else if (bad)
                delta_col = chartkit::down();
            // The value is the headline: the delta never takes more than half
            // the tile, and is the one that gets elided when both do not fit.
            const QFontMetrics fd(m.delta_f);
            const int full_w = fd.horizontalAdvance(delta_str);
            const QString shown = (state_ == State::Error || value_.isEmpty()) ? QStringLiteral("—") : value_;
            const int value_w = fv.horizontalAdvance(shown);
            const int room = std::max(w / 2, w - value_w - m.gap * 2);
            if (full_w > room) {
                delta_str = fd.elidedText(delta_str, Qt::ElideRight, std::max(0, room));
                delta_w = fd.horizontalAdvance(delta_str);
            } else {
                delta_w = full_w;
            }
        }

        QString vtext = (state_ == State::Error || value_.isEmpty()) ? QStringLiteral("—") : value_;
        QColor vcol = chartkit::text();
        if (state_ == State::Error)
            vcol = chartkit::text_dim();
        else if (dim)
            vcol = chartkit::axis_text();
        else if (tone_color.isValid())
            vcol = tone_color;

        const int vw = std::max(0, w - (delta_w > 0 ? delta_w + m.gap * 2 : 0));
        const int baseline = y + fv.ascent();
        p.setFont(m.value_f);
        p.setPen(vcol);
        p.drawText(QPoint(x, baseline), fv.elidedText(vtext, Qt::ElideRight, vw));
        if (delta_w > 0) {
            p.setFont(m.delta_f);
            p.setPen(dim ? chartkit::axis_text() : delta_col);
            p.drawText(QPoint(x + w - delta_w, baseline), delta_str);
        }
    }
    y += m.value_h;

    // Sub-label (or the error reason).
    const bool show_reason = state_ == State::Error && !reason_.isEmpty();
    const QString sub_text = show_reason && sub_.isEmpty() ? reason_ : sub_;
    if (!sub_text.isEmpty()) {
        y += m.gap;
        const QFontMetrics fs(m.sub_f);
        p.setFont(m.sub_f);
        p.setPen(show_reason && sub_.isEmpty() ? QColor(colors::DANGER()) : chartkit::text_dim());
        p.drawText(QRect(x, y, w, m.sub_h), Qt::AlignLeft | Qt::AlignVCenter, fs.elidedText(sub_text, Qt::ElideRight, w));
        y += m.sub_h;
    }

    // Sparkline.
    if (!spark_.isEmpty()) {
        y += m.gap;
        SparklineStyle st;
        st.fill = true;
        st.last_dot = true;
        st.baseline = spark_baseline_;
        st.invert = delta_inverted_;
        if (spark_color_ == SparkColor::Accent) {
            st.color_mode = SparklineStyle::ColorMode::Accent;
        } else if (spark_color_ == SparkColor::Neutral) {
            st.color_mode = SparklineStyle::ColorMode::Custom; // made legible on the theme by Sparkline
            st.custom_color = QColor(colors::TEXT_SECONDARY());
        }
        if (state_ != State::Normal || !isEnabled())
            st.color_mode = SparklineStyle::ColorMode::Muted;
        Sparkline::paint(p, QRectF(x, y, w, m.spark_h), spark_, st);
    }
}

// ── Interaction ──────────────────────────────────────────────────────────────

void StatTile::mousePressEvent(QMouseEvent* e) {
    if (clickable_ && e->button() == Qt::LeftButton) {
        pressed_ = true;
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void StatTile::mouseReleaseEvent(QMouseEvent* e) {
    if (clickable_ && pressed_ && e->button() == Qt::LeftButton) {
        pressed_ = false;
        if (rect().contains(e->position().toPoint()))
            emit clicked();
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void StatTile::keyPressEvent(QKeyEvent* e) {
    if (clickable_ && (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
        emit clicked();
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

void StatTile::enterEvent(QEnterEvent* e) {
    hovered_ = true;
    if (clickable_)
        update();
    QWidget::enterEvent(e);
}

void StatTile::leaveEvent(QEvent* e) {
    hovered_ = false;
    pressed_ = false;
    if (clickable_)
        update();
    QWidget::leaveEvent(e);
}

void StatTile::changeEvent(QEvent* e) {
    switch (e->type()) {
        case QEvent::LanguageChange:
            refresh_tooltip();
            updateGeometry();
            update();
            break;
        case QEvent::EnabledChange:
            update();
            break;
        default:
            break;
    }
    QWidget::changeEvent(e);
}

} // namespace fincept::ui
