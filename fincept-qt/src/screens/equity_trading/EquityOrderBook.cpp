// EquityOrderBook.cpp — custom-painted stacked DOM (Depth of Market)
//
// Layout, top → bottom:
//   header (widget)            "MARKET DEPTH" + level badge
//   column header (painted)    PRICE | SIZE | TOTAL
//   asks ×5    (painted)       worst→best downward; best ask sits on the spread
//   spread band (painted)      Δ spread · bps · weighted mid   ← visual anchor
//   bids ×5    (painted)       best→worst downward; best bid sits on the spread
//   imbalance footer (painted) Σbid · pressure bar · Σask · dominant %
//
// All analytics (cumulative depth, totals, imbalance, weighted mid, bps) are
// derived here from the bid/ask arrays — no extra data is fetched.
#include "screens/equity_trading/EquityOrderBook.h"

#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLocale>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QPainter>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using namespace fincept::ui;

namespace {
// Thousands-grouped integer, e.g. 1,200. Prefixed to stay collision-free in
// unity builds.
QString eqob_grp(double v) {
    return QLocale(QLocale::English).toString(static_cast<qlonglong>(std::llround(v)));
}
// Compact magnitude for the footer totals, e.g. 4.45K / 1.20M.
QString eqob_abbrev(double v) {
    const double a = std::fabs(v);
    if (a >= 1e6)
        return QString::number(v / 1e6, 'f', a >= 1e7 ? 1 : 2) + "M";
    if (a >= 1e3)
        return QString::number(v / 1e3, 'f', a >= 1e4 ? 1 : 2) + "K";
    return QString::number(v, 'f', 0);
}
// The user's font at `offset` px from the base size, for the painted ladder.
QFont eqob_font(int offset, bool bold = false) {
    QFont f = fincept::ui::fonts::font(offset);
    f.setBold(bold);
    return f;
}
// One set of price decimals for the whole ladder (spread / mid included), taken
// from the best level's magnitude: bare prices as quoted, no unit (the panel is in
// the instrument's currency), locale digits and grouping.
fincept::ui::formatting::InstrumentFmt eqob_price_fmt(const QVector<QPair<double, double>>& bids,
                                                      const QVector<QPair<double, double>>& asks) {
    const double ref = !bids.isEmpty() ? bids[0].first : (!asks.isEmpty() ? asks[0].first : 0.0);
    fincept::ui::formatting::InstrumentFmt inst;
    inst.decimals = fincept::ui::formatting::price_decimals(ref);
    return inst;
}
} // namespace

namespace fincept::screens::equity {

EquityOrderBook::EquityOrderBook(QWidget* parent) : QWidget(parent) {
    setObjectName("eqOrderBook");
    // The DOM has a fixed shape (5×5 levels + bands), so pin the widget to its
    // exact content height — no stretch, no dead space. The host splitter then
    // hands all remaining room to the order-entry panel below. The height
    // follows the font size (update_metrics).
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMouseTracking(true);
    // Colours: repaint from the live theme. Sizes: re-fit once the new styles are final.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this]() {
        cache_dirty_ = true;
        update();
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        update_metrics();
        update();
    });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header — title + live level badge (the shared panel header).
    header_ = new ui::PanelHeader(tr("Market depth"), this);
    layout->addWidget(header_);

    // Canvas — transparent spacer; painting happens on `this` below the header.
    canvas_ = new QWidget(this);
    canvas_->setObjectName("eqObCanvas");
    canvas_->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(canvas_, 1);
    update_metrics();

    // Coalesce repaints at max 20fps
    repaint_timer_ = new QTimer(this);
    repaint_timer_->setSingleShot(true);
    repaint_timer_->setInterval(50);
    connect(repaint_timer_, &QTimer::timeout, this, [this]() { update(); });

    // Number-format change: repaint the cached ladder from the levels held.
    fincept::ui::formatting::on_format_changed(this, [this]() {
        cache_dirty_ = true;
        update();
    });
}

void EquityOrderBook::update_metrics() {
    const auto line_h = [](int offset, bool bold) { return QFontMetrics(eqob_font(offset, bold)).height(); };
    row_h_ = std::max(ROW_H, line_h(0, true) + 5);
    colhdr_h_ = std::max(COLHDR_H, line_h(-2, true) + 4);
    spread_h_ = std::max(SPREAD_H, line_h(0, true) + 9);
    footer_h_ = std::max(FOOTER_H, line_h(-1, true) + 10);
    header_h_ = std::max(HEADER_H, ui::PanelHeader::strip_height());
    header_->setFixedHeight(header_h_);
    const int canvas_h = colhdr_h_ + 2 * levels_ * row_h_ + spread_h_ + footer_h_;
    canvas_->setMinimumHeight(canvas_h);
    setFixedHeight(header_h_ + canvas_h);
    cache_dirty_ = true;
}

void EquityOrderBook::set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                               double spread, double spread_pct) {
    set_data(bids, asks, spread, spread_pct, {}, {});
}

void EquityOrderBook::set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                               double spread, double spread_pct, const QVector<int>& bid_orders,
                               const QVector<int>& ask_orders) {
    QMutexLocker lock(&mutex_);
    bids_ = bids.mid(0, MAX_LEVELS);
    asks_ = asks.mid(0, MAX_LEVELS);
    bid_orders_ = bid_orders.mid(0, MAX_LEVELS);
    ask_orders_ = ask_orders.mid(0, MAX_LEVELS);
    spread_ = spread;
    spread_pct_ = spread_pct;
    has_data_ = !bids_.isEmpty() || !asks_.isEmpty();
    cache_dirty_ = true;

    // Level badge: L2 with real depth, or a synthetic single-level fallback.
    if (!has_data_) {
        levels_badge_.clear();
    } else if (bids_.size() <= 1 && asks_.size() <= 1) {
        levels_badge_ = tr("L1 · synthetic");
    } else {
        levels_badge_ = tr("L2 · %1×%2").arg(bids_.size()).arg(asks_.size());
    }
    header_->set_badge(levels_badge_); // no-op unless the text changes

    if (!repaint_timer_->isActive())
        repaint_timer_->start();
}

void EquityOrderBook::clear() {
    set_data({}, {}, 0.0, 0.0);
}

void EquityOrderBook::set_visible_levels(int levels) {
    levels = std::clamp(levels, 1, int(MAX_LEVELS));
    if (levels == levels_)
        return;
    levels_ = levels;
    hover_index_ = -1;
    update_metrics(); // the ladder's fixed height follows the level count
    update();
}

void EquityOrderBook::set_empty_message(const QString& text) {
    if (text == empty_message_)
        return;
    empty_message_ = text;
    if (!has_data_) {
        cache_dirty_ = true; // the empty state is painted into the cache
        update();
    }
}

void EquityOrderBook::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        cache_dirty_ = true; // column headers / "No depth data" are painted
        update();
    }
    QWidget::changeEvent(event);
}

void EquityOrderBook::retranslateUi() {
    if (header_)
        header_->set_title(tr("Market depth"));
}

void EquityOrderBook::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    cache_dirty_ = true;
}

bool EquityOrderBook::level_at_y(int widget_y, bool& is_ask, int& index) const {
    const int pix = widget_y - header_h_;
    if (pix >= g_ask_top_ && pix < g_ask_top_ + levels_ * row_h_) {
        const int slot = (pix - g_ask_top_) / row_h_; // 0 = top (worst ask)
        const int i = levels_ - 1 - slot;         // 0 = best ask (on spread)
        if (i >= 0 && i < asks_.size()) {
            is_ask = true;
            index = i;
            return true;
        }
    } else if (pix >= g_bid_top_ && pix < g_bid_top_ + levels_ * row_h_) {
        const int i = (pix - g_bid_top_) / row_h_; // 0 = best bid (on spread)
        if (i >= 0 && i < bids_.size()) {
            is_ask = false;
            index = i;
            return true;
        }
    }
    return false;
}

void EquityOrderBook::mousePressEvent(QMouseEvent* event) {
    double clicked_price = 0.0;
    bool hit = false;
    {
        QMutexLocker lock(&mutex_);
        bool is_ask = false;
        int idx = 0;
        if (level_at_y(static_cast<int>(event->position().y()), is_ask, idx)) {
            clicked_price = is_ask ? asks_[idx].first : bids_[idx].first;
            hit = true;
        }
    }
    // Emit with the (non-recursive) mutex released: the slot runs synchronously and a
    // handler that calls back into set_data() would otherwise deadlock on mutex_.
    if (hit)
        emit price_clicked(clicked_price);
}

void EquityOrderBook::mouseMoveEvent(QMouseEvent* event) {
    QMutexLocker lock(&mutex_);
    bool is_ask = false;
    int idx = 0;
    const bool hit = level_at_y(static_cast<int>(event->position().y()), is_ask, idx);

    if (hit && (idx != hover_index_ || is_ask != hover_is_ask_)) {
        hover_index_ = idx;
        hover_is_ask_ = is_ask;
        update();
    } else if (!hit && hover_index_ != -1) {
        hover_index_ = -1;
        update();
    }

    if (hit) {
        const auto& side = is_ask ? asks_ : bids_;
        const auto& ords = is_ask ? ask_orders_ : bid_orders_;
        double cum = 0;
        for (int i = 0; i <= idx && i < side.size(); ++i)
            cum += side[i].second;
        QString tip = tr("%1  %2\nSize  %3\nCumulative  %4")
                          .arg(is_ask ? tr("ASK") : tr("BID"))
                          .arg(fincept::ui::formatting::price(side[idx].first, eqob_price_fmt(bids_, asks_)))
                          .arg(eqob_grp(side[idx].second))
                          .arg(eqob_grp(cum));
        if (idx < ords.size() && ords[idx] > 0)
            tip += tr("\nOrders  %1").arg(ords[idx]);
        QToolTip::showText(event->globalPosition().toPoint(), tip, this);
    } else {
        QToolTip::hideText();
    }
}

void EquityOrderBook::leaveEvent(QEvent* event) {
    if (hover_index_ != -1) {
        hover_index_ = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

void EquityOrderBook::paintEvent(QPaintEvent* /*event*/) {
    if (cache_dirty_)
        rebuild_cache();
    QPainter p(this);
    p.drawPixmap(0, header_h_, cache_);

    // Hover highlight — drawn live on top of the cached pixmap so hovering
    // never triggers a full cache rebuild.
    if (hover_index_ >= 0) {
        const int base =
            hover_is_ask_ ? g_ask_top_ + (levels_ - 1 - hover_index_) * row_h_ : g_bid_top_ + hover_index_ * row_h_;
        const int y = header_h_ + base;
        QColor hl = colors::BG_HOVER();
        hl.setAlpha(60);
        p.fillRect(0, y, width(), row_h_, hl);
        QColor edge = hover_is_ask_ ? colors::NEGATIVE() : colors::POSITIVE();
        p.fillRect(0, y, 2, row_h_, edge);
    }
}

void EquityOrderBook::rebuild_cache() {
    QMutexLocker lock(&mutex_);
    const int w = width();
    const int h = height() - header_h_;
    if (w <= 0 || h <= 0)
        return;

    cache_ = QPixmap(w, h);
    cache_.fill(colors::BG_SURFACE());

    QPainter p(&cache_);
    p.setRenderHint(QPainter::Antialiasing, false);

    if (!has_data_) {
        p.setPen(colors::TEXT_SECONDARY());
        p.setFont(eqob_font(-1));
        if (empty_message_.isEmpty())
            p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, tr("No depth data"));
        else
            p.drawText(QRect(12, 0, qMax(0, w - 24), h), Qt::AlignCenter | Qt::TextWordWrap, empty_message_);
        g_ask_top_ = g_bid_top_ = 0;
        cache_dirty_ = false;
        return;
    }

    // ── Band geometry (top-anchored; widget is pinned to content height) ─────
    const int top = 0;
    const int y_colhdr = top;
    g_ask_top_ = top + colhdr_h_;
    const int y_spread = g_ask_top_ + levels_ * row_h_;
    g_bid_top_ = y_spread + spread_h_;
    const int y_footer = g_bid_top_ + levels_ * row_h_;

    // ── Column metrics ───────────────────────────────────────────────────────
    const int pad = 8;
    const int inner_l = pad;
    const int inner_r = w - pad;
    const int inner_w = inner_r - inner_l;
    const int price_r = inner_l + inner_w * 42 / 100;
    const int size_r = inner_l + inner_w * 72 / 100;
    auto cell = [&](QPainter& g, int x_left, int x_right, int y, const QString& t) {
        g.drawText(QRect(x_left, y, x_right - x_left, row_h_), Qt::AlignRight | Qt::AlignVCenter, t);
    };

    // ── Cumulative depth + side totals ───────────────────────────────────────
    QVector<double> ask_cum(asks_.size(), 0), bid_cum(bids_.size(), 0);
    double total_ask = 0, total_bid = 0;
    for (int i = 0; i < asks_.size(); ++i) {
        total_ask += asks_[i].second;
        ask_cum[i] = total_ask;
    }
    for (int i = 0; i < bids_.size(); ++i) {
        total_bid += bids_[i].second;
        bid_cum[i] = total_bid;
    }
    const double max_cum = std::max({total_ask, total_bid, 1.0});

    // ── Column header ────────────────────────────────────────────────────────
    p.setFont(eqob_font(-2, true));
    p.setPen(colors::TEXT_DIM());
    cell(p, inner_l, price_r, y_colhdr, tr("PRICE"));
    cell(p, price_r, size_r, y_colhdr, tr("SIZE"));
    cell(p, size_r, inner_r, y_colhdr, tr("TOTAL"));

    const QFont row_font = eqob_font(0);
    const QFont row_bold = eqob_font(0, true);
    const auto px_fmt = eqob_price_fmt(bids_, asks_);
    auto px = [&px_fmt](double v) { return fincept::ui::formatting::price(v, px_fmt); };

    // ── Ask rows (best ask nearest the spread band) ──────────────────────────
    for (int i = 0; i < asks_.size() && i < levels_; ++i) {
        const auto& [price, qty] = asks_[i];
        const int y = g_ask_top_ + (levels_ - 1 - i) * row_h_;
        const bool best = (i == 0);

        const int bar_w = static_cast<int>(w * (ask_cum[i] / max_cum));
        QColor bar = colors::NEGATIVE();
        bar.setAlpha(46);
        p.fillRect(w - bar_w, y, bar_w, row_h_, bar);
        if (best) {
            QColor tint = colors::NEGATIVE_BG();
            tint.setAlpha(70);
            p.fillRect(0, y, w, row_h_, tint);
            p.fillRect(0, y, 2, row_h_, colors::NEGATIVE());
        }

        // Every level's price in the side colour; the best level is bold.
        p.setFont(best ? row_bold : row_font);
        p.setPen(colors::NEGATIVE());
        cell(p, inner_l, price_r, y, px(price));
        p.setPen(best ? colors::TEXT_PRIMARY() : colors::TEXT_SECONDARY());
        cell(p, price_r, size_r, y, eqob_grp(qty));
        p.setPen(colors::TEXT_TERTIARY());
        cell(p, size_r, inner_r, y, eqob_grp(ask_cum[i]));
    }

    // ── Bid rows (best bid nearest the spread band) ──────────────────────────
    for (int i = 0; i < bids_.size() && i < levels_; ++i) {
        const auto& [price, qty] = bids_[i];
        const int y = g_bid_top_ + i * row_h_;
        const bool best = (i == 0);

        const int bar_w = static_cast<int>(w * (bid_cum[i] / max_cum));
        QColor bar = colors::POSITIVE();
        bar.setAlpha(46);
        p.fillRect(w - bar_w, y, bar_w, row_h_, bar);
        if (best) {
            QColor tint = colors::POSITIVE_BG();
            tint.setAlpha(70);
            p.fillRect(0, y, w, row_h_, tint);
            p.fillRect(0, y, 2, row_h_, colors::POSITIVE());
        }

        p.setFont(best ? row_bold : row_font);
        p.setPen(colors::POSITIVE());
        cell(p, inner_l, price_r, y, px(price));
        p.setPen(best ? colors::TEXT_PRIMARY() : colors::TEXT_SECONDARY());
        cell(p, price_r, size_r, y, eqob_grp(qty));
        p.setPen(colors::TEXT_TERTIARY());
        cell(p, size_r, inner_r, y, eqob_grp(bid_cum[i]));
    }

    // ── Spread band (weighted mid / micro-price) ─────────────────────────────
    p.fillRect(0, y_spread, w, spread_h_, colors::BG_RAISED());
    p.setPen(colors::BORDER_DIM());
    p.drawLine(0, y_spread, w, y_spread);
    p.drawLine(0, y_spread + spread_h_ - 1, w, y_spread + spread_h_ - 1);

    double mid = 0;
    if (!bids_.isEmpty() && !asks_.isEmpty()) {
        const double bp = bids_[0].first, bq = bids_[0].second;
        const double ap = asks_[0].first, aq = asks_[0].second;
        mid = (bq + aq) > 0 ? (bp * aq + ap * bq) / (bq + aq) : (bp + ap) / 2.0;
    } else if (!bids_.isEmpty()) {
        mid = bids_[0].first;
    } else if (!asks_.isEmpty()) {
        mid = asks_[0].first;
    }
    const double bps = spread_pct_ * 100.0;

    p.setFont(eqob_font(-3, true));
    p.setPen(colors::TEXT_SECONDARY()); // on the raised band
    p.drawText(QRect(inner_l, y_spread, inner_w, spread_h_), Qt::AlignLeft | Qt::AlignVCenter, tr("SPREAD"));

    p.setFont(eqob_font(0, true));
    p.setPen(colors::TEXT_PRIMARY());
    // Spread and mid in the ladder's price decimals; bps is a ratio, not a price.
    const QString spread_text = QString("%1  ·  %2  ·  %3")
                                    .arg(px(spread_), fincept::ui::formatting::bp(bps, 1, false), px(mid));
    p.drawText(QRect(inner_l, y_spread, inner_w, spread_h_), Qt::AlignRight | Qt::AlignVCenter, spread_text);

    // ── Imbalance footer (footer fill itself is the pressure bar) ────────────
    const double tot = total_bid + total_ask;
    const double bid_ratio = tot > 0 ? total_bid / tot : 0.5;
    const int split = static_cast<int>(w * bid_ratio);
    QColor bidc = colors::POSITIVE();
    bidc.setAlpha(34);
    QColor askc = colors::NEGATIVE();
    askc.setAlpha(34);
    p.fillRect(0, y_footer, split, footer_h_, bidc);
    p.fillRect(split, y_footer, w - split, footer_h_, askc);
    p.setPen(colors::BORDER_DIM());
    p.drawLine(0, y_footer, w, y_footer);
    p.setPen(colors::BORDER_MED());
    p.drawLine(split, y_footer + 4, split, y_footer + footer_h_ - 4);

    p.setFont(eqob_font(-1, true));
    p.setPen(colors::POSITIVE());
    p.drawText(QRect(inner_l, y_footer, inner_w, footer_h_), Qt::AlignLeft | Qt::AlignVCenter,
               QString("Σ %1").arg(eqob_abbrev(total_bid)));
    p.setPen(colors::NEGATIVE());
    p.drawText(QRect(inner_l, y_footer, inner_w, footer_h_), Qt::AlignRight | Qt::AlignVCenter,
               QString("%1 Σ").arg(eqob_abbrev(total_ask)));
    const bool bid_dom = bid_ratio >= 0.5;
    const int pct = static_cast<int>(std::lround((bid_dom ? bid_ratio : 1.0 - bid_ratio) * 100.0));
    p.setPen(bid_dom ? colors::POSITIVE() : colors::NEGATIVE());
    p.drawText(QRect(inner_l, y_footer, inner_w, footer_h_), Qt::AlignCenter,
               tr("%1% %2").arg(pct).arg(bid_dom ? tr("BID") : tr("ASK")));

    cache_dirty_ = false;
}

} // namespace fincept::screens::equity
