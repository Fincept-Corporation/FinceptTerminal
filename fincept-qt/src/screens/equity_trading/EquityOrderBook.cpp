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

#include "ui/theme/Theme.h"

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
} // namespace

namespace fincept::screens::equity {

EquityOrderBook::EquityOrderBook(QWidget* parent) : QWidget(parent) {
    setObjectName("eqOrderBook");
    // The DOM has a fixed shape (5×5 levels + bands), so pin the widget to its
    // exact content height — no stretch, no dead space. The host splitter then
    // hands all remaining room to the order-entry panel below.
    setFixedHeight(HEADER_H + COLHDR_H + 2 * MAX_LEVELS * ROW_H + SPREAD_H + FOOTER_H);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMouseTracking(true);
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this]() {
        cache_dirty_ = true;
        update();
    });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header — title + live level badge
    auto* header = new QWidget(this);
    header->setObjectName("eqObHeader");
    header->setFixedHeight(HEADER_H);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);

    title_label_ = new QLabel(tr("MARKET DEPTH"));
    title_label_->setObjectName("eqObTitle");
    h_layout->addWidget(title_label_);
    h_layout->addStretch();
    levels_label_ = new QLabel(QString());
    levels_label_->setObjectName("eqObLevels");
    h_layout->addWidget(levels_label_);

    layout->addWidget(header);

    // Canvas — transparent spacer; painting happens on `this` below the header.
    canvas_ = new QWidget(this);
    canvas_->setObjectName("eqObCanvas");
    canvas_->setAttribute(Qt::WA_TransparentForMouseEvents);
    canvas_->setMinimumHeight(COLHDR_H + 2 * MAX_LEVELS * ROW_H + SPREAD_H + FOOTER_H);
    layout->addWidget(canvas_, 1);

    // Coalesce repaints at max 20fps
    repaint_timer_ = new QTimer(this);
    repaint_timer_->setSingleShot(true);
    repaint_timer_->setInterval(50);
    connect(repaint_timer_, &QTimer::timeout, this, [this]() { update(); });
}

void EquityOrderBook::set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                               double spread, double spread_pct) {
    set_data(bids, asks, spread, spread_pct, {}, {});
}

void EquityOrderBook::set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                               double spread, double spread_pct,
                               const QVector<int>& bid_orders, const QVector<int>& ask_orders) {
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
        levels_label_->setText(QString());
    } else if (bids_.size() <= 1 && asks_.size() <= 1) {
        levels_label_->setText(tr("L1 · synthetic"));
    } else {
        levels_label_->setText(tr("L2 · %1×%2").arg(bids_.size()).arg(asks_.size()));
    }

    if (!repaint_timer_->isActive())
        repaint_timer_->start();
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
    if (title_label_)
        title_label_->setText(tr("MARKET DEPTH"));
}

void EquityOrderBook::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    cache_dirty_ = true;
}

bool EquityOrderBook::level_at_y(int widget_y, bool& is_ask, int& index) const {
    const int pix = widget_y - HEADER_H;
    if (pix >= g_ask_top_ && pix < g_ask_top_ + MAX_LEVELS * ROW_H) {
        const int slot = (pix - g_ask_top_) / ROW_H; // 0 = top (worst ask)
        const int i = MAX_LEVELS - 1 - slot;          // 0 = best ask (on spread)
        if (i >= 0 && i < asks_.size()) {
            is_ask = true;
            index = i;
            return true;
        }
    } else if (pix >= g_bid_top_ && pix < g_bid_top_ + MAX_LEVELS * ROW_H) {
        const int i = (pix - g_bid_top_) / ROW_H; // 0 = best bid (on spread)
        if (i >= 0 && i < bids_.size()) {
            is_ask = false;
            index = i;
            return true;
        }
    }
    return false;
}

void EquityOrderBook::mousePressEvent(QMouseEvent* event) {
    QMutexLocker lock(&mutex_);
    bool is_ask = false;
    int idx = 0;
    if (level_at_y(static_cast<int>(event->position().y()), is_ask, idx))
        emit price_clicked(is_ask ? asks_[idx].first : bids_[idx].first);
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
                          .arg(side[idx].first, 0, 'f', 2)
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
    p.drawPixmap(0, HEADER_H, cache_);

    // Hover highlight — drawn live on top of the cached pixmap so hovering
    // never triggers a full cache rebuild.
    if (hover_index_ >= 0) {
        const int base = hover_is_ask_ ? g_ask_top_ + (MAX_LEVELS - 1 - hover_index_) * ROW_H
                                       : g_bid_top_ + hover_index_ * ROW_H;
        const int y = HEADER_H + base;
        QColor hl = colors::BG_HOVER();
        hl.setAlpha(60);
        p.fillRect(0, y, width(), ROW_H, hl);
        QColor edge = hover_is_ask_ ? colors::NEGATIVE() : colors::POSITIVE();
        p.fillRect(0, y, 2, ROW_H, edge);
    }
}

void EquityOrderBook::rebuild_cache() {
    QMutexLocker lock(&mutex_);
    const int w = width();
    const int h = height() - HEADER_H;
    if (w <= 0 || h <= 0)
        return;

    cache_ = QPixmap(w, h);
    cache_.fill(colors::BG_SURFACE());

    QPainter p(&cache_);
    p.setRenderHint(QPainter::Antialiasing, false);

    if (!has_data_) {
        p.setPen(colors::TEXT_SECONDARY());
        p.setFont(QFont("Consolas", 10));
        p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, tr("No depth data"));
        g_ask_top_ = g_bid_top_ = 0;
        cache_dirty_ = false;
        return;
    }

    // ── Band geometry (top-anchored; widget is pinned to content height) ─────
    const int top = 0;
    const int y_colhdr = top;
    g_ask_top_ = top + COLHDR_H;
    const int y_spread = g_ask_top_ + MAX_LEVELS * ROW_H;
    g_bid_top_ = y_spread + SPREAD_H;
    const int y_footer = g_bid_top_ + MAX_LEVELS * ROW_H;

    // ── Column metrics ───────────────────────────────────────────────────────
    const int pad = 8;
    const int inner_l = pad;
    const int inner_r = w - pad;
    const int inner_w = inner_r - inner_l;
    const int price_r = inner_l + inner_w * 42 / 100;
    const int size_r = inner_l + inner_w * 72 / 100;
    auto cell = [&](QPainter& g, int x_left, int x_right, int y, const QString& t) {
        g.drawText(QRect(x_left, y, x_right - x_left, ROW_H), Qt::AlignRight | Qt::AlignVCenter, t);
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
    QFont hdr_font("Consolas", 9);
    hdr_font.setBold(true);
    p.setFont(hdr_font);
    p.setPen(colors::TEXT_DIM());
    cell(p, inner_l, price_r, y_colhdr, tr("PRICE"));
    cell(p, price_r, size_r, y_colhdr, tr("SIZE"));
    cell(p, size_r, inner_r, y_colhdr, tr("TOTAL"));

    QFont row_font("Consolas", 11);
    QFont row_bold("Consolas", 11);
    row_bold.setBold(true);

    // ── Ask rows (best ask nearest the spread band) ──────────────────────────
    for (int i = 0; i < asks_.size() && i < MAX_LEVELS; ++i) {
        const auto& [price, qty] = asks_[i];
        const int y = g_ask_top_ + (MAX_LEVELS - 1 - i) * ROW_H;
        const bool best = (i == 0);

        const int bar_w = static_cast<int>(w * (ask_cum[i] / max_cum));
        QColor bar = colors::NEGATIVE();
        bar.setAlpha(46);
        p.fillRect(w - bar_w, y, bar_w, ROW_H, bar);
        if (best) {
            QColor tint = colors::NEGATIVE_BG();
            tint.setAlpha(70);
            p.fillRect(0, y, w, ROW_H, tint);
            p.fillRect(0, y, 2, ROW_H, colors::NEGATIVE());
        }

        p.setFont(best ? row_bold : row_font);
        p.setPen(best ? colors::NEGATIVE() : colors::NEGATIVE_DIM());
        cell(p, inner_l, price_r, y, QString::number(price, 'f', 2));
        p.setPen(best ? colors::TEXT_PRIMARY() : colors::TEXT_SECONDARY());
        cell(p, price_r, size_r, y, eqob_grp(qty));
        p.setPen(colors::TEXT_TERTIARY());
        cell(p, size_r, inner_r, y, eqob_grp(ask_cum[i]));
    }

    // ── Bid rows (best bid nearest the spread band) ──────────────────────────
    for (int i = 0; i < bids_.size() && i < MAX_LEVELS; ++i) {
        const auto& [price, qty] = bids_[i];
        const int y = g_bid_top_ + i * ROW_H;
        const bool best = (i == 0);

        const int bar_w = static_cast<int>(w * (bid_cum[i] / max_cum));
        QColor bar = colors::POSITIVE();
        bar.setAlpha(46);
        p.fillRect(w - bar_w, y, bar_w, ROW_H, bar);
        if (best) {
            QColor tint = colors::POSITIVE_BG();
            tint.setAlpha(70);
            p.fillRect(0, y, w, ROW_H, tint);
            p.fillRect(0, y, 2, ROW_H, colors::POSITIVE());
        }

        p.setFont(best ? row_bold : row_font);
        p.setPen(best ? colors::POSITIVE() : colors::POSITIVE_DIM());
        cell(p, inner_l, price_r, y, QString::number(price, 'f', 2));
        p.setPen(best ? colors::TEXT_PRIMARY() : colors::TEXT_SECONDARY());
        cell(p, price_r, size_r, y, eqob_grp(qty));
        p.setPen(colors::TEXT_TERTIARY());
        cell(p, size_r, inner_r, y, eqob_grp(bid_cum[i]));
    }

    // ── Spread band (weighted mid / micro-price) ─────────────────────────────
    p.fillRect(0, y_spread, w, SPREAD_H, colors::BG_RAISED());
    p.setPen(colors::BORDER_DIM());
    p.drawLine(0, y_spread, w, y_spread);
    p.drawLine(0, y_spread + SPREAD_H - 1, w, y_spread + SPREAD_H - 1);

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

    QFont sp_label("Consolas", 8);
    sp_label.setBold(true);
    p.setFont(sp_label);
    p.setPen(colors::TEXT_DIM());
    p.drawText(QRect(inner_l, y_spread, inner_w, SPREAD_H), Qt::AlignLeft | Qt::AlignVCenter, tr("SPREAD"));

    QFont sp_val("Consolas", 11);
    sp_val.setBold(true);
    p.setFont(sp_val);
    p.setPen(colors::TEXT_PRIMARY());
    const QString spread_text =
        QString("%1  ·  %2 bps  ·  %3").arg(spread_, 0, 'f', 2).arg(bps, 0, 'f', 1).arg(mid, 0, 'f', 2);
    p.drawText(QRect(inner_l, y_spread, inner_w, SPREAD_H), Qt::AlignRight | Qt::AlignVCenter, spread_text);

    // ── Imbalance footer (footer fill itself is the pressure bar) ────────────
    const double tot = total_bid + total_ask;
    const double bid_ratio = tot > 0 ? total_bid / tot : 0.5;
    const int split = static_cast<int>(w * bid_ratio);
    QColor bidc = colors::POSITIVE();
    bidc.setAlpha(34);
    QColor askc = colors::NEGATIVE();
    askc.setAlpha(34);
    p.fillRect(0, y_footer, split, FOOTER_H, bidc);
    p.fillRect(split, y_footer, w - split, FOOTER_H, askc);
    p.setPen(colors::BORDER_DIM());
    p.drawLine(0, y_footer, w, y_footer);
    p.setPen(colors::BORDER_MED());
    p.drawLine(split, y_footer + 4, split, y_footer + FOOTER_H - 4);

    QFont ft("Consolas", 10);
    ft.setBold(true);
    p.setFont(ft);
    p.setPen(colors::POSITIVE());
    p.drawText(QRect(inner_l, y_footer, inner_w, FOOTER_H), Qt::AlignLeft | Qt::AlignVCenter,
               QString("Σ %1").arg(eqob_abbrev(total_bid)));
    p.setPen(colors::NEGATIVE());
    p.drawText(QRect(inner_l, y_footer, inner_w, FOOTER_H), Qt::AlignRight | Qt::AlignVCenter,
               QString("%1 Σ").arg(eqob_abbrev(total_ask)));
    const bool bid_dom = bid_ratio >= 0.5;
    const int pct = static_cast<int>(std::lround((bid_dom ? bid_ratio : 1.0 - bid_ratio) * 100.0));
    p.setPen(bid_dom ? colors::POSITIVE() : colors::NEGATIVE());
    p.drawText(QRect(inner_l, y_footer, inner_w, FOOTER_H), Qt::AlignCenter,
               tr("%1% %2").arg(pct).arg(bid_dom ? tr("BID") : tr("ASK")));

    cache_dirty_ = false;
}

} // namespace fincept::screens::equity
