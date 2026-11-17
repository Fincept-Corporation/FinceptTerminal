#include "ui/theme/ChartStyle.h"

#include "ui/theme/Theme.h"

#include <QEvent>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QLocale>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>
#include <QtMath>

#include <cmath>
#include <utility>

namespace fincept::ui {

// ── PaintedChart ─────────────────────────────────────────────────────────────

PaintedChart::PaintedChart(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        on_theme_changed();
        invalidate();
    });
}

PaintedChart::~PaintedChart() = default;

void PaintedChart::set_state(State state, const QString& message) {
    if (state == state_ && message == state_message_)
        return;
    state_ = state;
    state_message_ = message;
    if (state_ != Ready)
        hide_tip();
    sync_animation();
    invalidate();
}

void PaintedChart::set_value_formatter(Formatter formatter) {
    formatter_ = std::move(formatter);
    invalidate();
}

QString PaintedChart::format_value(double v) const {
    return formatter_ ? formatter_(v) : default_format(v);
}

QString PaintedChart::default_format(double v) const {
    return chart_style::compact_number(v);
}

void PaintedChart::paint_overlay(QPainter&, const QRectF&) {}

void PaintedChart::invalidate() {
    cache_valid_ = false;
    update();
}

bool PaintedChart::chart_visible() const {
    return state_ == Ready && has_data();
}

QRectF PaintedChart::content_rect() const {
    return QRectF(contentsRect());
}

void PaintedChart::show_tip(const QPoint& local_pos, const QString& text, const QRect& hot_rect) {
    if (text.isEmpty()) {
        hide_tip();
        return;
    }
    tip_text_ = text;
    QToolTip::showText(mapToGlobal(local_pos + QPoint(14, 18)), text, this, hot_rect);
}

void PaintedChart::hide_tip() {
    if (tip_text_.isEmpty())
        return;
    tip_text_.clear();
    QToolTip::hideText();
}

void PaintedChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const QRectF r = content_rect();
    const State shown = (state_ == Ready && !has_data()) ? Empty : state_;
    if (shown != Ready) {
        p.setRenderHint(QPainter::Antialiasing, true);
        paint_state_overlay(p, r, shown);
        return;
    }
    // The DPR of the device really painted on: the backing store normally, an
    // export pixmap when the widget is render()ed / grab()bed at another scale.
    const QPaintDevice* target = p.paintEngine() ? p.paintEngine()->paintDevice() : nullptr;
    const qreal dpr = target ? target->devicePixelRatio() : devicePixelRatioF();
    const QSize px(qCeil(width() * dpr), qCeil(height() * dpr));
    if (!cache_valid_ || cache_.size() != px || !qFuzzyCompare(cache_.devicePixelRatio(), dpr)) {
        cache_ = QPixmap(px);
        cache_.setDevicePixelRatio(dpr);
        cache_.fill(Qt::transparent);
        QPainter cp(&cache_);
        cp.setRenderHint(QPainter::Antialiasing, true);
        cp.setRenderHint(QPainter::TextAntialiasing, true);
        paint_chart(cp, r);
        cache_valid_ = true;
    }
    p.drawPixmap(0, 0, cache_);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    paint_overlay(p, r);
}

void PaintedChart::paint_state_overlay(QPainter& p, const QRectF& r, State shown) {
    QString text = state_message_;
    if (shown == Empty && state_ == Ready)
        text.clear(); // no data while Ready: never show a stale message
    if (text.isEmpty()) {
        switch (shown) {
            case Loading:
                text = tr("Loading...");
                break;
            case Error:
                text = tr("Could not load data");
                break;
            case Empty:
            case Ready:
                text = tr("No data");
                break;
        }
    }

    const QFont f = chart_style::font(-2);
    const QFontMetricsF fm(f);
    p.setFont(f);
    QRectF text_rect = r.adjusted(12, 0, -12, 0);

    if (shown == Loading) {
        // Indeterminate bar above the text.
        const qreal w = qMin<qreal>(160.0, r.width() * 0.5);
        const QRectF track(r.center().x() - w / 2.0, r.center().y() - fm.height() - 4, w, 3);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(colors::BORDER_DIM()));
        p.drawRoundedRect(track, 1.5, 1.5);
        const qreal seg = w * 0.3;
        const qreal t = (anim_phase_ % 60) / 60.0;
        const qreal x = track.left() - seg + (w + seg) * t;
        const QRectF bar = QRectF(x, track.top(), seg, track.height()).intersected(track);
        p.setBrush(QColor(colors::AMBER()));
        p.drawRoundedRect(bar, 1.5, 1.5);
        text_rect.setTop(track.bottom() + 6);
        text_rect.setHeight(fm.height() * 3);
        p.setPen(QColor(colors::TEXT_TERTIARY()));
        p.drawText(text_rect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, text);
        return;
    }

    if (shown == Error) {
        const QColor danger(colors::DANGER());
        const qreal d = fm.height();
        const QRectF badge(r.center().x() - d / 2.0, r.center().y() - d - 6, d, d);
        p.setPen(QPen(danger, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(badge);
        QFont bf = chart_style::font(-3, QFont::Bold);
        p.setFont(bf);
        p.drawText(badge, Qt::AlignCenter, QStringLiteral("!"));
        p.setFont(f);
        text_rect.setTop(badge.bottom() + 6);
        text_rect.setHeight(fm.height() * 3);
        p.setPen(danger);
        p.drawText(text_rect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, text);
        return;
    }

    p.setPen(QColor(colors::TEXT_TERTIARY()));
    p.drawText(text_rect, Qt::AlignCenter | Qt::TextWordWrap, text);
}

void PaintedChart::sync_animation() {
    const bool want = state_ == Loading && isVisible();
    if (want && !anim_) {
        anim_ = new QTimer(this);
        anim_->setInterval(40);
        connect(anim_, &QTimer::timeout, this, [this]() {
            anim_phase_ = (anim_phase_ + 1) % 600;
            update();
        });
    }
    if (!anim_)
        return;
    if (want && !anim_->isActive())
        anim_->start();
    else if (!want && anim_->isActive())
        anim_->stop();
}

void PaintedChart::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    hide_tip();
    invalidate();
}

void PaintedChart::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    switch (e->type()) {
        case QEvent::FontChange:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
        case QEvent::LayoutDirectionChange:
        case QEvent::ContentsRectChange:
            invalidate();
            break;
        default:
            break;
    }
}

void PaintedChart::leaveEvent(QEvent* e) {
    QWidget::leaveEvent(e);
    hide_tip();
    on_hover_left();
    update();
}

void PaintedChart::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    sync_animation();
}

void PaintedChart::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    sync_animation();
    hide_tip();
}

// ── chart_style ──────────────────────────────────────────────────────────────

namespace chart_style {

QFont font(int offset, int weight) {
    return fonts::font(offset, weight);
}

QColor grid_color() {
    return QColor(colors::BORDER_DIM());
}
QColor axis_color() {
    return QColor(colors::BORDER_MED());
}
QColor axis_text_color() {
    return QColor(colors::TEXT_SECONDARY());
}
QColor label_color() {
    return QColor(colors::TEXT_PRIMARY());
}
QColor muted_color() {
    return QColor(colors::TEXT_TERTIARY());
}
QColor highlight_color() {
    return QColor(colors::TEXT_PRIMARY());
}
QColor empty_cell_color() {
    return QColor(colors::BG_RAISED());
}

QString compact_number(double v, int decimals) {
    if (!std::isfinite(v))
        return QStringLiteral("-");
    const QLocale loc;
    const double a = std::abs(v);
    struct Scale {
        double div;
        const char* suffix;
    };
    static const Scale kScales[] = {{1e12, "T"}, {1e9, "B"}, {1e6, "M"}, {1e4, "K"}};
    for (const Scale& s : kScales) {
        if (a >= s.div) {
            const double scaled = v / s.div;
            const int d = std::abs(scaled) >= 100 ? 0 : (std::abs(scaled) >= 10 ? 1 : 2);
            return loc.toString(scaled, 'f', qMin(d, qMax(decimals, 0))) + QLatin1String(s.suffix);
        }
    }
    if (a >= 1000)
        return loc.toString(v, 'f', 0);
    QString text = loc.toString(v, 'f', qMax(decimals, 0));
    // Trim trailing zeros after the decimal point ("12.50" → "12.5", "3.00" → "3").
    const QChar dp = loc.decimalPoint().isEmpty() ? QLatin1Char('.') : loc.decimalPoint().at(0);
    if (text.contains(dp)) {
        while (text.endsWith(QLatin1Char('0')))
            text.chop(1);
        if (text.endsWith(dp))
            text.chop(1);
    }
    if (text == QLatin1String("-0"))
        text = QStringLiteral("0");
    return text;
}

QString percent_text(double v, int decimals, bool show_sign) {
    if (!std::isfinite(v))
        return QStringLiteral("-");
    const int d = qBound(0, decimals, 8);
    const double scale = std::pow(10.0, d);
    double rounded = std::round(v * scale) / scale;
    if (rounded == 0.0)
        rounded = 0.0; // no "-0.00%"
    const QString num = QLocale().toString(rounded, 'f', d);
    return (show_sign && rounded > 0 ? QStringLiteral("+") : QString()) + num + QLatin1Char('%');
}

namespace {
double cs_nice_num(double x, bool round) {
    if (x <= 0 || !std::isfinite(x))
        return 1.0;
    const double exp = std::floor(std::log10(x));
    const double f = x / std::pow(10.0, exp);
    double nf;
    if (round)
        nf = f < 1.5 ? 1 : (f < 3 ? 2 : (f < 7 ? 5 : 10));
    else
        nf = f <= 1 ? 1 : (f <= 2 ? 2 : (f <= 5 ? 5 : 10));
    return nf * std::pow(10.0, exp);
}
} // namespace

QVector<double> nice_ticks(double lo, double hi, int target, double* nice_lo, double* nice_hi) {
    QVector<double> out;
    if (!std::isfinite(lo) || !std::isfinite(hi))
        return out;
    if (hi < lo)
        std::swap(lo, hi);
    if (hi - lo < 1e-12) {
        const double pad = std::abs(lo) > 1e-12 ? std::abs(lo) * 0.1 : 1.0;
        lo -= pad;
        hi += pad;
    }
    target = qMax(2, target);
    const double range = cs_nice_num(hi - lo, false);
    const double step = cs_nice_num(range / (target - 1), true);
    const double first = std::floor(lo / step) * step;
    const double last = std::ceil(hi / step) * step;
    for (double v = first; v <= last + step * 0.5 && out.size() < 64; v += step)
        out.append(std::abs(v) < step * 1e-9 ? 0.0 : v);
    if (nice_lo)
        *nice_lo = first;
    if (nice_hi)
        *nice_hi = last;
    return out;
}

QFont axis_font() {
    return font(-4);
}

qreal axis_label_width(const QVector<double>& ticks, const std::function<QString(double)>& fmt) {
    const QFontMetricsF fm(axis_font());
    qreal w = 0;
    for (double t : ticks)
        w = std::max(w, fm.horizontalAdvance(fmt ? fmt(t) : compact_number(t)));
    return w;
}

void draw_value_axis(QPainter& p, const QRectF& plot, const QVector<double>& ticks, double lo, double hi,
                     Qt::Orientation orientation, const std::function<QString(double)>& fmt) {
    if (!(hi > lo))
        return;
    const QFont f = axis_font();
    const QFontMetricsF fm(f);
    p.save();
    p.setFont(f);
    const QColor grid = grid_color();
    const QColor axis = axis_color();
    const QColor text = axis_text_color();
    for (double t : ticks) {
        if (t < lo - 1e-9 * (hi - lo) || t > hi + 1e-9 * (hi - lo))
            continue;
        const double k = (t - lo) / (hi - lo);
        const QString label = fmt ? fmt(t) : compact_number(t);
        const bool zero = std::abs(t) < 1e-12 * std::max(1.0, std::abs(hi - lo)) && lo < 0 && hi > 0;
        p.setPen(QPen(zero ? axis : grid, 1));
        if (orientation == Qt::Vertical) {
            const qreal y = std::round(plot.bottom() - k * plot.height()) + 0.5;
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            p.setPen(text);
            p.drawText(QRectF(plot.left() - 200 - 6, y - fm.height() / 2, 200, fm.height()),
                       Qt::AlignRight | Qt::AlignVCenter, label);
        } else {
            const qreal x = std::round(plot.left() + k * plot.width()) + 0.5;
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            p.setPen(text);
            p.drawText(QRectF(x - 100, plot.bottom() + 3, 200, fm.height()), Qt::AlignHCenter | Qt::AlignTop, label);
        }
    }
    p.restore();
}

bool draw_cell_text(QPainter& p, const QRectF& cell, const QStringList& lines, const QColor& color, int max_offset,
                    int min_offset) {
    if (lines.isEmpty() || lines.first().isEmpty())
        return false;
    const qreal pad_x = 4.0;
    const qreal pad_y = 2.0;
    const qreal avail_w = cell.width() - 2 * pad_x;
    const qreal avail_h = cell.height() - 2 * pad_y;
    if (avail_w < 8 || avail_h < 6)
        return false;

    for (int offset = max_offset; offset >= min_offset; --offset) {
        const QFont main_font = fonts::font(offset, QFont::Bold);
        const QFont sub_font = fonts::font(offset - 2);
        const QFontMetricsF fm_main(main_font);
        const QFontMetricsF fm_sub(sub_font);
        if (fm_main.horizontalAdvance(lines.first()) > avail_w || fm_main.height() > avail_h)
            continue;
        // First line fits: add the following lines while they fit.
        QVector<int> shown = {0};
        qreal total_h = fm_main.height();
        for (int i = 1; i < lines.size() && i < 3; ++i) {
            if (lines.at(i).isEmpty())
                continue;
            if (fm_sub.horizontalAdvance(lines.at(i)) > avail_w || total_h + fm_sub.height() > avail_h)
                break;
            shown.append(i);
            total_h += fm_sub.height();
        }
        qreal y = cell.center().y() - total_h / 2.0;
        p.setPen(color);
        for (int idx : shown) {
            const bool first = idx == 0;
            p.setFont(first ? main_font : sub_font);
            const qreal h = first ? fm_main.height() : fm_sub.height();
            p.drawText(QRectF(cell.left() + pad_x, y, avail_w, h), Qt::AlignCenter, lines.at(idx));
            y += h;
        }
        return true;
    }
    // Nothing fits even at the smallest size: show the first line elided
    // rather than leaving a coloured cell with no label at all.
    const QFont small_font = fonts::font(min_offset, QFont::Bold);
    const QFontMetricsF fm_small(small_font);
    if (fm_small.height() > avail_h)
        return false;
    const QString elided = fm_small.elidedText(lines.first(), Qt::ElideRight, avail_w);
    if (elided.isEmpty() || elided == QStringLiteral("…"))
        return false;
    p.setPen(color);
    p.setFont(small_font);
    p.drawText(QRectF(cell.left() + pad_x, cell.center().y() - fm_small.height() / 2.0, avail_w, fm_small.height()),
               Qt::AlignCenter, elided);
    return true;
}

int legend_height() {
    return 8 + 3 + qCeil(QFontMetricsF(font(-4)).height());
}

void draw_scale_legend(QPainter& p, const QRectF& r, bool diverging, double lo, double hi,
                       const std::function<QString(double)>& fmt) {
    if (r.width() < 40)
        return;
    const QRectF ramp(r.left(), r.top(), r.width(), 8);
    QLinearGradient g(ramp.topLeft(), ramp.topRight());
    constexpr int kStops = 16;
    for (int i = 0; i <= kStops; ++i) {
        const double t = static_cast<double>(i) / kStops;
        g.setColorAt(t, diverging ? colors::diverging_color(-1.0 + 2.0 * t) : colors::sequential_color(t));
    }
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(ramp, 2, 2);

    const QFont f = font(-4);
    const QFontMetricsF fm(f);
    p.setFont(f);
    p.setPen(axis_text_color());
    const qreal ty = ramp.bottom() + 3;
    const QRectF text_row(r.left(), ty, r.width(), fm.height());
    const auto text_of = [&fmt](double v) { return fmt ? fmt(v) : compact_number(v); };
    p.drawText(text_row, Qt::AlignLeft | Qt::AlignVCenter, text_of(lo));
    p.drawText(text_row, Qt::AlignRight | Qt::AlignVCenter, text_of(hi));
    const double mid = (diverging && lo < 0 && hi > 0) ? 0.0 : (lo + hi) / 2.0;
    const QString mid_text = text_of(mid);
    const qreal mid_x = r.left() + r.width() * ((mid - lo) / ((hi - lo) == 0 ? 1.0 : (hi - lo)));
    const qreal mw = fm.horizontalAdvance(mid_text);
    const qreal lw = fm.horizontalAdvance(text_of(lo));
    const qreal rw = fm.horizontalAdvance(text_of(hi));
    if (mid_x - mw / 2 > r.left() + lw + 6 && mid_x + mw / 2 < r.right() - rw - 6)
        p.drawText(QRectF(mid_x - mw / 2, ty, mw, fm.height()), Qt::AlignCenter, mid_text);
    p.restore();
}

qreal draw_swatch_label(QPainter& p, const QPointF& top_left, const QColor& swatch, const QString& label,
                        const QColor& text_color) {
    const QFont f = font(-3);
    const QFontMetricsF fm(f);
    const qreal s = qMax<qreal>(8.0, qRound(fm.height() * 0.6));
    const qreal cy = top_left.y() + fm.height() / 2.0;
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(swatch);
    p.drawRoundedRect(QRectF(top_left.x(), cy - s / 2.0, s, s), 2, 2);
    p.setFont(f);
    p.setPen(text_color);
    const qreal tx = top_left.x() + s + 5;
    const qreal tw = fm.horizontalAdvance(label);
    p.drawText(QRectF(tx, top_left.y(), tw + 1, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, label);
    p.restore();
    return s + 5 + tw;
}

QString tooltip_html(const QString& title, const QVector<QPair<QString, QString>>& rows) {
    QString html = QStringLiteral("<div style='white-space:nowrap'>");
    if (!title.isEmpty())
        html += QStringLiteral("<b>%1</b>").arg(title.toHtmlEscaped());
    if (!rows.isEmpty()) {
        html += QStringLiteral("<table cellspacing='0' cellpadding='0' style='margin-top:2px'>");
        const QString muted = QColor(colors::TEXT_SECONDARY()).name();
        for (const auto& row : rows) {
            html += QStringLiteral("<tr><td style='color:%1;padding-right:10px'>%2</td><td align='right'>%3</td></tr>")
                        .arg(muted, row.first.toHtmlEscaped(), row.second.toHtmlEscaped());
        }
        html += QStringLiteral("</table>");
    }
    html += QStringLiteral("</div>");
    return html;
}

} // namespace chart_style

} // namespace fincept::ui
