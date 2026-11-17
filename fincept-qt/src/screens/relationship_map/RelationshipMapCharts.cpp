// src/screens/relationship_map/RelationshipMapCharts.cpp
#include "screens/relationship_map/RelationshipMapCharts.h"

#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QFontMetricsF>
#include <QLocale>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace fincept::relmap {

namespace {

namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;
namespace fmt = fincept::ui::formatting;
namespace rmc_cs = fincept::ui::chart_style;

constexpr int kRmcMonths = 12;

int rmc_month_key(const QDate& d) {
    return d.year() * 12 + (d.month() - 1);
}

/// An amount in `ccy`, abbreviated per the user's scheme; "" = no currency symbol.
QString rmc_money(double v, const QString& ccy) {
    if (!std::isfinite(v))
        return fmt::placeholder();
    return ccy.isEmpty() ? fmt::compact(v) : fmt::money(v, ccy, fmt::Compact::FromSettings);
}

/// rmc_money() with an explicit sign on both sides of zero.
QString rmc_signed_money(double v, const QString& ccy) {
    if (!std::isfinite(v))
        return fmt::placeholder();
    if (!ccy.isEmpty())
        return fmt::signed_money(v, ccy, fmt::Compact::FromSettings);
    const QString t = fmt::compact(std::abs(v));
    if (v > 0)
        return QStringLiteral("+") + t;
    if (v < 0)
        return QStringLiteral("-") + t;
    return t;
}

QString rmc_month_label(const QDate& d, bool with_year) {
    return QLocale().toString(d, with_year ? QStringLiteral("MMM yy") : QStringLiteral("MMM"));
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// RelInsiderActivityChart
// ═════════════════════════════════════════════════════════════════════════════

RelInsiderActivityChart::RelInsiderActivityChart(QWidget* parent) : ui::PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    set_trades({}, QDate::currentDate());
}

void RelInsiderActivityChart::set_trades(const QVector<RelInsiderTrade>& trades, const QDate& end) {
    const QDate last = end.isValid() ? end : QDate::currentDate();
    const QDate end_month(last.year(), last.month(), 1);
    months_ = QVector<Month>(kRmcMonths);
    for (int i = 0; i < kRmcMonths; ++i)
        months_[i].first = end_month.addMonths(i - (kRmcMonths - 1));
    const int first_key = rmc_month_key(months_.first().first);

    // The open-market trades of the window, then the currency most of them carry.
    struct Pick {
        int month = 0;
        bool buy = false;
        double value = 0;
        double shares = 0;
        QString ccy;
    };
    QVector<Pick> picks;
    QMap<QString, int> per_currency; // ordered: a tie is decided the same way every time
    for (const RelInsiderTrade& t : trades) {
        const QString kind = t.kind.trimmed().toLower();
        const bool buy = kind == QLatin1String("buy");
        if (!buy && kind != QLatin1String("sell"))
            continue; // grants, gifts, option exercises, other: not open-market
        const QDate d = QDate::fromString(t.date.left(10), Qt::ISODate);
        if (!d.isValid())
            continue;
        const int m = rmc_month_key(d) - first_key;
        if (m < 0 || m >= kRmcMonths)
            continue;
        double value = std::isfinite(t.value) && t.value > 0 ? t.value : 0.0;
        if (value <= 0 && std::isfinite(t.shares) && std::isfinite(t.price) && t.shares > 0 && t.price > 0)
            value = t.shares * t.price;
        if (!(value > 0))
            continue;
        Pick pk;
        pk.month = m;
        pk.buy = buy;
        pk.value = value;
        pk.shares = std::isfinite(t.shares) ? std::abs(t.shares) : 0.0;
        pk.ccy = t.currency.trimmed();
        per_currency[pk.ccy] += 1;
        picks.append(pk);
    }
    currency_.clear();
    int best = -1;
    for (auto it = per_currency.constBegin(); it != per_currency.constEnd(); ++it) {
        // Most trades wins; a stated currency wins a tie against "not stated".
        if (it.value() > best || (it.value() == best && currency_.isEmpty() && !it.key().isEmpty())) {
            best = it.value();
            currency_ = it.key();
        }
    }
    counted_ = 0;
    excluded_ = 0;
    for (const Pick& pk : picks) {
        if (pk.ccy != currency_) {
            ++excluded_;
            continue;
        }
        Month& mo = months_[pk.month];
        if (pk.buy) {
            mo.buy_value += pk.value;
            mo.buy_shares += pk.shares;
            ++mo.buys;
        } else {
            mo.sell_value += pk.value;
            mo.sell_shares += pk.shares;
            ++mo.sells;
        }
        ++counted_;
    }
    double run = 0;
    for (Month& mo : months_) {
        run += mo.buy_value - mo.sell_value;
        mo.cumulative = run;
    }
    hover_ = -1;
    hide_tip();
    if (counted_ > 0)
        set_state(Ready);
    else
        set_state(Empty, tr("No open-market insider buys or sells in the last 12 months"));
    invalidate();
}

QString RelInsiderActivityChart::axis_text(double v) const {
    return currency_.isEmpty() ? fmt::compact(v) : fmt::axis_money(v, currency_);
}

RelInsiderActivityChart::Frame RelInsiderActivityChart::frame(const QRectF& r) const {
    Frame f;
    double hi = 0;
    double lo = 0;
    for (const Month& m : months_) {
        hi = std::max({hi, m.buy_value, m.cumulative});
        lo = std::min({lo, -m.sell_value, m.cumulative});
    }
    if (!(hi > lo))
        hi = lo + 1;
    double nice_lo = lo;
    double nice_hi = hi;
    f.ticks = rmc_cs::nice_ticks(lo, hi, 4, &nice_lo, &nice_hi);
    f.lo = nice_lo;
    f.hi = nice_hi;
    if (!(f.hi > f.lo)) {
        f.lo = lo;
        f.hi = hi;
    }
    const QFontMetricsF legend_fm(rmc_cs::font(-3));
    const QFontMetricsF axis_fm(rmc_cs::axis_font());
    const qreal pad = 6;
    f.legend = QRectF(r.left() + pad, r.top() + 2, std::max<qreal>(0.0, r.width() - 2 * pad), legend_fm.height() + 4);
    const qreal axis_w = rmc_cs::axis_label_width(f.ticks, [this](double v) { return axis_text(v); }) + 10;
    const qreal top = f.legend.bottom() + 4;
    const qreal bottom = r.bottom() - axis_fm.height() - 6;
    f.plot = QRectF(r.left() + pad + axis_w, top, std::max<qreal>(10.0, r.width() - 2 * pad - axis_w),
                    std::max<qreal>(10.0, bottom - top));
    return f;
}

void RelInsiderActivityChart::paint_chart(QPainter& p, const QRectF& r) {
    if (months_.isEmpty())
        return;
    const Frame f = frame(r);
    const QColor up(colors::POSITIVE());
    const QColor down(colors::NEGATIVE());
    const QColor line(colors::TEXT_PRIMARY());
    const QColor label = rmc_cs::label_color();

    // Legend: the arrows carry the direction as well as the colour.
    QPointF at = f.legend.topLeft();
    at.rx() += rmc_cs::draw_swatch_label(p, at, up, tr("▲ Buys"), label) + 14;
    at.rx() += rmc_cs::draw_swatch_label(p, at, down, tr("▼ Sells"), label) + 14;
    rmc_cs::draw_swatch_label(p, at, line, tr("Cumulative net"), label);

    rmc_cs::draw_value_axis(p, f.plot, f.ticks, f.lo, f.hi, Qt::Vertical, [this](double v) { return axis_text(v); });

    const double span = f.hi - f.lo;
    const auto y_of = [&f, span](double v) { return f.plot.bottom() - (v - f.lo) / span * f.plot.height(); };
    const int n = static_cast<int>(months_.size());
    const qreal slot = f.plot.width() / n;
    const qreal bar_w = std::max<qreal>(2.0, slot * 0.56);
    const qreal y0 = y_of(0.0);

    p.save();
    p.setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        const Month& m = months_[i];
        const qreal cx = f.plot.left() + slot * (i + 0.5);
        if (m.buy_value > 0) {
            const qreal y = y_of(m.buy_value);
            p.setBrush(up);
            p.drawRect(QRectF(cx - bar_w / 2, y, bar_w, std::max<qreal>(1.0, y0 - y)));
        }
        if (m.sell_value > 0) {
            const qreal y = y_of(-m.sell_value);
            p.setBrush(down);
            p.drawRect(QRectF(cx - bar_w / 2, y0, bar_w, std::max<qreal>(1.0, y - y0)));
        }
    }
    // The zero line over the bars.
    p.setPen(QPen(rmc_cs::axis_color(), 1));
    const qreal yz = std::round(y0) + 0.5;
    p.drawLine(QPointF(f.plot.left(), yz), QPointF(f.plot.right(), yz));

    // Cumulative net.
    QPainterPath path;
    for (int i = 0; i < n; ++i) {
        const QPointF pt(f.plot.left() + slot * (i + 0.5), y_of(months_[i].cumulative));
        if (i == 0)
            path.moveTo(pt);
        else
            path.lineTo(pt);
    }
    p.setPen(QPen(line, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setPen(Qt::NoPen);
    p.setBrush(line);
    for (int i = 0; i < n; ++i)
        p.drawEllipse(QPointF(f.plot.left() + slot * (i + 0.5), y_of(months_[i].cumulative)), 2.0, 2.0);

    // Month labels: every month when they fit, else every k-th counted back from the latest.
    const QFont af = rmc_cs::axis_font();
    const QFontMetricsF afm(af);
    qreal widest = 0;
    for (int i = 0; i < n; ++i)
        widest = std::max(widest, afm.horizontalAdvance(rmc_month_label(months_[i].first, true)));
    const int step = std::max(1, static_cast<int>(std::ceil((widest + 6) / std::max<qreal>(1.0, slot))));
    p.setFont(af);
    p.setPen(rmc_cs::axis_text_color());
    for (int i = 0; i < n; ++i) {
        if ((n - 1 - i) % step != 0)
            continue;
        const QDate d = months_[i].first;
        const bool year = i == 0 || d.month() == 1 || step > 1;
        const qreal cx = f.plot.left() + slot * (i + 0.5);
        p.drawText(QRectF(cx - slot * step / 2.0 - 40, f.plot.bottom() + 3, slot * step + 80, afm.height()),
                   Qt::AlignHCenter | Qt::AlignTop, rmc_month_label(d, year));
    }
    p.restore();
}

void RelInsiderActivityChart::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_ < 0 || hover_ >= months_.size() || !chart_visible())
        return;
    const Frame f = frame(r);
    const qreal slot = f.plot.width() / static_cast<qreal>(months_.size());
    p.fillRect(QRectF(f.plot.left() + slot * hover_, f.plot.top(), slot, f.plot.height()), colors::overlay_color(0.07));
}

int RelInsiderActivityChart::month_at(const QPointF& pos) const {
    if (months_.isEmpty())
        return -1;
    const Frame f = frame(content_rect());
    if (!f.plot.adjusted(0, -4, 0, 4).contains(pos))
        return -1;
    const qreal slot = f.plot.width() / static_cast<qreal>(months_.size());
    const int i = static_cast<int>((pos.x() - f.plot.left()) / slot);
    return i >= 0 && i < months_.size() ? i : -1;
}

QString RelInsiderActivityChart::tooltip_for(int month) const {
    const Month& m = months_[month];
    QVector<QPair<QString, QString>> rows;
    rows.append({tr("Buys"), m.buys > 0 ? tr("▲ %1 · %n trade(s)", nullptr, m.buys).arg(rmc_money(m.buy_value, currency_))
                                        : fmt::placeholder()});
    rows.append({tr("Sells"), m.sells > 0
                                  ? tr("▼ %1 · %n trade(s)", nullptr, m.sells).arg(rmc_money(m.sell_value, currency_))
                                  : fmt::placeholder()});
    rows.append({tr("Net"), rmc_signed_money(m.buy_value - m.sell_value, currency_)});
    rows.append({tr("Shares bought / sold"), QStringLiteral("%1 / %2").arg(fmt::volume(m.buy_shares), fmt::volume(m.sell_shares))});
    rows.append({tr("Cumulative net"), rmc_signed_money(m.cumulative, currency_)});
    return rmc_cs::tooltip_html(QLocale().toString(m.first, QStringLiteral("MMMM yyyy")), rows);
}

void RelInsiderActivityChart::on_hover_left() {
    hover_ = -1;
}

void RelInsiderActivityChart::mouseMoveEvent(QMouseEvent* e) {
    ui::PaintedChart::mouseMoveEvent(e);
    const int m = chart_visible() ? month_at(e->position()) : -1;
    if (m == hover_)
        return;
    hover_ = m;
    update();
    if (m < 0) {
        hide_tip();
        return;
    }
    const Frame f = frame(content_rect());
    const qreal slot = f.plot.width() / static_cast<qreal>(months_.size());
    const QRect hot = QRectF(f.plot.left() + slot * m, f.plot.top() - 4, slot, f.plot.height() + 8).toAlignedRect();
    show_tip(e->position().toPoint(), tooltip_for(m), hot);
}

QSize RelInsiderActivityChart::sizeHint() const {
    const QFontMetrics fm(fonts::font(-3));
    return {fm.averageCharWidth() * 48, fm.height() * 10};
}

QSize RelInsiderActivityChart::minimumSizeHint() const {
    const QFontMetrics fm(fonts::font(-3));
    return {fm.averageCharWidth() * 24, fm.height() * 6};
}

// ═════════════════════════════════════════════════════════════════════════════
// RelPeerBulletChart
// ═════════════════════════════════════════════════════════════════════════════

RelPeerBulletChart::RelPeerBulletChart(QWidget* parent) : ui::PaintedChart(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    set_metrics(QString(), {});
}

void RelPeerBulletChart::set_metrics(const QString& company_label, const QVector<RelBulletMetric>& metrics) {
    company_ = company_label;
    rows_.clear();
    for (const RelBulletMetric& m : metrics) {
        Row row;
        row.metric = m;
        for (double v : m.peers)
            if (std::isfinite(v))
                row.sorted.append(v);
        std::sort(row.sorted.begin(), row.sorted.end());
        const int n = static_cast<int>(row.sorted.size());
        if (n > 0)
            row.median = (n % 2) ? row.sorted[n / 2] : 0.5 * (row.sorted[n / 2 - 1] + row.sorted[n / 2]);
        rows_.append(row);
    }
    hover_ = -1;
    hide_tip();
    if (has_data())
        set_state(Ready);
    else
        set_state(Empty, tr("Not enough peer data to compare"));
    updateGeometry();
    invalidate();
}

bool RelPeerBulletChart::has_data() const {
    for (const Row& r : rows_)
        if (std::isfinite(r.metric.company) || !r.sorted.isEmpty())
            return true;
    return false;
}

qreal RelPeerBulletChart::nominal_row_height() {
    return QFontMetricsF(rmc_cs::font(-3, QFont::DemiBold)).height() + 12;
}

QString RelPeerBulletChart::value_text(const Row& row, double v) const {
    if (!std::isfinite(v))
        return fmt::placeholder();
    return row.metric.fraction ? fmt::fraction_pct(v, 1, false) : fmt::ratio(v, 1);
}

QString RelPeerBulletChart::median_text(const Row& row) const {
    return tr("med %1").arg(value_text(row, row.median));
}

RelPeerBulletChart::Geometry RelPeerBulletChart::geometry(const QRectF& r) const {
    Geometry g;
    g.area = r.adjusted(8, 4, -8, -4);
    const QFontMetricsF lf(rmc_cs::font(-3, QFont::DemiBold));
    const QFontMetricsF vf(rmc_cs::font(-3, QFont::Bold));
    const QFontMetricsF mf(rmc_cs::font(-4));
    for (const Row& row : rows_) {
        g.label_w = std::max(g.label_w, lf.horizontalAdvance(row.metric.label));
        g.company_w = std::max(g.company_w, vf.horizontalAdvance(value_text(row, row.metric.company)));
        g.median_w = std::max(g.median_w, mf.horizontalAdvance(median_text(row)));
    }
    g.label_w += 12;
    g.company_w += 10;
    g.median_w += 6;
    const qreal gap = 24;
    const qreal column_min = g.label_w + g.company_w + g.median_w + lf.averageCharWidth() * 16;
    const int n = static_cast<int>(rows_.size());
    g.columns = (n > 1 && g.area.width() >= 2 * column_min + gap) ? 2 : 1;
    g.rows_per_column = (n + g.columns - 1) / g.columns;
    const qreal nominal = nominal_row_height();
    g.row_h = g.rows_per_column > 0 ? std::min(nominal, g.area.height() / g.rows_per_column) : nominal;
    g.row_h = std::max(g.row_h, lf.height() + 2);
    return g;
}

QRectF RelPeerBulletChart::row_rect(const Geometry& g, int index) const {
    if (g.rows_per_column <= 0)
        return {};
    const qreal gap = 24;
    const int column = index / g.rows_per_column;
    const int row = index % g.rows_per_column;
    const qreal column_w = (g.area.width() - (g.columns - 1) * gap) / g.columns;
    return QRectF(g.area.left() + column * (column_w + gap), g.area.top() + row * g.row_h, column_w, g.row_h);
}

void RelPeerBulletChart::paint_chart(QPainter& p, const QRectF& r) {
    const Geometry g = geometry(r);
    const QFont label_font = rmc_cs::font(-3, QFont::DemiBold);
    const QFont value_font = rmc_cs::font(-3, QFont::Bold);
    const QFont median_font = rmc_cs::font(-4);
    const QFontMetricsF lfm(label_font);
    const QColor label_c = rmc_cs::muted_color();
    const QColor text_c = rmc_cs::label_color();
    const QColor dim(colors::TEXT_TERTIARY());
    const QColor track_fill(colors::BG_RAISED());
    const QColor track_edge(colors::BORDER_DIM());
    const QColor band(colors::BORDER_MED());
    const QColor median_c(colors::TEXT_SECONDARY());
    const QColor accent(colors::AMBER());
    const QColor edge(colors::BG_BASE());
    const QColor zero_c = rmc_cs::axis_color();

    p.save();
    for (int i = 0; i < rows_.size(); ++i) {
        const Row& row = rows_[i];
        const QRectF rr = row_rect(g, i);
        if (rr.isEmpty() || rr.top() > g.area.bottom() - 2)
            continue; // no room at this height
        // Label (left) and the two values (right).
        p.setFont(label_font);
        p.setPen(label_c);
        p.drawText(QRectF(rr.left(), rr.top(), g.label_w - 6, rr.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   lfm.elidedText(row.metric.label, Qt::ElideRight, g.label_w - 6));
        const QRectF median_r(rr.right() - g.median_w, rr.top(), g.median_w, rr.height());
        const QRectF company_r(median_r.left() - g.company_w, rr.top(), g.company_w, rr.height());
        p.setFont(value_font);
        p.setPen(std::isfinite(row.metric.company) ? text_c : dim);
        p.drawText(company_r, Qt::AlignRight | Qt::AlignVCenter, value_text(row, row.metric.company));
        p.setFont(median_font);
        p.setPen(dim);
        p.drawText(median_r, Qt::AlignRight | Qt::AlignVCenter, median_text(row));

        // The track.
        const qreal th = std::min(rr.height() * 0.5, lfm.height() * 0.8);
        const qreal left = rr.left() + g.label_w;
        const qreal right = company_r.left() - 10;
        if (right - left < 8)
            continue;
        const qreal cy = rr.center().y();
        const QRectF track(left, cy - th / 2, right - left, th);
        p.setPen(QPen(track_edge, 1));
        p.setBrush(track_fill);
        p.drawRect(track);

        // Domain: the peers and the company (and zero for percentages).
        double lo = std::numeric_limits<double>::infinity();
        double hi = -std::numeric_limits<double>::infinity();
        if (!row.sorted.isEmpty()) {
            lo = row.sorted.first();
            hi = row.sorted.last();
        }
        if (std::isfinite(row.metric.company)) {
            lo = std::min(lo, row.metric.company);
            hi = std::max(hi, row.metric.company);
        }
        if (!std::isfinite(lo) || !std::isfinite(hi))
            continue;
        if (row.metric.fraction) {
            lo = std::min(lo, 0.0);
            hi = std::max(hi, 0.0);
        }
        double pad = (hi - lo) * 0.06;
        if (!(pad > 0))
            pad = std::max(std::abs(hi) * 0.1, row.metric.fraction ? 0.01 : 0.5);
        lo -= pad;
        hi += pad;
        const auto x_of = [&track, lo, hi](double v) { return track.left() + (v - lo) / (hi - lo) * track.width(); };

        if (lo < 0 && hi > 0) {
            const qreal x0 = std::round(x_of(0.0)) + 0.5;
            p.setPen(QPen(zero_c, 1, Qt::DotLine));
            p.drawLine(QPointF(x0, track.top() - 2), QPointF(x0, track.bottom() + 2));
        }
        qreal xm = std::numeric_limits<qreal>::quiet_NaN();
        if (!row.sorted.isEmpty()) {
            const qreal x1 = x_of(row.sorted.first());
            const qreal x2 = x_of(row.sorted.last());
            p.setPen(Qt::NoPen);
            p.setBrush(band);
            p.drawRect(QRectF(x1, track.top() + th * 0.2, std::max<qreal>(2.0, x2 - x1), th * 0.6));
            p.setBrush(dim);
            for (double v : row.sorted)
                p.drawEllipse(QPointF(x_of(v), cy), 1.6, 1.6);
            xm = x_of(row.median);
            p.setPen(QPen(median_c, 2));
            p.drawLine(QPointF(xm, track.top() + 1), QPointF(xm, track.bottom() - 1));
        }
        if (std::isfinite(row.metric.company)) {
            const qreal xc = x_of(row.metric.company);
            if (std::isfinite(xm)) {
                p.setPen(QPen(accent, 2));
                p.drawLine(QPointF(xm, cy), QPointF(xc, cy));
            }
            const qreal d = std::max<qreal>(4.0, th * 0.6);
            QPolygonF diamond;
            diamond << QPointF(xc, cy - d) << QPointF(xc + d * 0.7, cy) << QPointF(xc, cy + d) << QPointF(xc - d * 0.7, cy);
            p.setPen(QPen(edge, 1));
            p.setBrush(accent);
            p.drawPolygon(diamond);
        }
    }
    p.restore();
}

void RelPeerBulletChart::paint_overlay(QPainter& p, const QRectF& r) {
    if (hover_ < 0 || hover_ >= rows_.size() || !chart_visible())
        return;
    p.fillRect(row_rect(geometry(r), hover_), colors::overlay_color(0.05));
}

int RelPeerBulletChart::row_at(const QPointF& pos) const {
    const Geometry g = geometry(content_rect());
    for (int i = 0; i < rows_.size(); ++i)
        if (row_rect(g, i).contains(pos))
            return i;
    return -1;
}

QString RelPeerBulletChart::tooltip_for(int index) const {
    const Row& row = rows_[index];
    const int n = static_cast<int>(row.sorted.size());
    QVector<QPair<QString, QString>> rows;
    rows.append({company_.isEmpty() ? tr("Company") : company_, value_text(row, row.metric.company)});
    rows.append({tr("Peer median"), value_text(row, row.median)});
    rows.append({tr("Peer range"), n > 0 ? QStringLiteral("%1 – %2").arg(value_text(row, row.sorted.first()),
                                                                          value_text(row, row.sorted.last()))
                                         : fmt::placeholder()});
    rows.append({tr("Peers with data"), fmt::number(n, 0)});
    if (std::isfinite(row.metric.company) && n > 0) {
        int rank = 1;
        for (double v : row.sorted)
            if (v > row.metric.company)
                ++rank;
        rows.append({tr("Rank, highest first"), tr("%1 of %2").arg(fmt::number(rank, 0), fmt::number(n + 1, 0))});
    }
    return rmc_cs::tooltip_html(row.metric.label, rows);
}

void RelPeerBulletChart::on_hover_left() {
    hover_ = -1;
}

void RelPeerBulletChart::mouseMoveEvent(QMouseEvent* e) {
    ui::PaintedChart::mouseMoveEvent(e);
    const int i = chart_visible() ? row_at(e->position()) : -1;
    if (i == hover_)
        return;
    hover_ = i;
    update();
    if (i < 0) {
        hide_tip();
        return;
    }
    show_tip(e->position().toPoint(), tooltip_for(i), row_rect(geometry(content_rect()), i).toAlignedRect());
}

QSize RelPeerBulletChart::sizeHint() const {
    const QFontMetrics fm(fonts::font(-3));
    const int n = std::max(1, static_cast<int>(rows_.size()));
    const int per_column = (n + 1) / 2;
    return {fm.averageCharWidth() * 64, static_cast<int>(std::ceil(per_column * nominal_row_height())) + 8};
}

QSize RelPeerBulletChart::minimumSizeHint() const {
    const QFontMetrics fm(fonts::font(-3));
    return {fm.averageCharWidth() * 30, static_cast<int>(std::ceil(nominal_row_height())) + 8};
}

} // namespace fincept::relmap
