// EquityChartPanel.cpp — see header. KLineChart-preferred trading chart with
// an EquityChart (Qt Charts) fallback when Qt WebEngine is unavailable.
#include "screens/equity_trading/EquityChartPanel.h"

#include "core/currency/Currency.h"
#include "screens/equity_trading/EquityChart.h"
#include "ui/charts/KLineChartWidget.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace fincept::screens::equity {

// Must match EquityChart::TF_LABELS so the timeframe strings handed to
// stream->fetch_candles()/get_history() are identical across both backends.
static constexpr const char* kTfLabels[6] = {"1m", "5m", "15m", "1h", "1d", "1w"};

EquityChartPanel::EquityChartPanel(QWidget* parent) : QWidget(parent) {
    // A QWebEngineView has a near-zero size hint until its page loads, so give
    // the panel a real minimum + expanding policy; otherwise the splitter
    // starves it and the bottom panel takes the whole center column.
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    kline_ = new fincept::ui::KLineChartWidget(this);
    if (kline_->is_available()) {
        build_kline_ui();
        // Right-click chart trading — enable the menu and forward its actions up.
        kline_->set_trading_menu_enabled(true);
        connect(kline_, &fincept::ui::KLineChartWidget::buy_requested, this,
                &EquityChartPanel::buy_requested);
        connect(kline_, &fincept::ui::KLineChartWidget::sell_requested, this,
                &EquityChartPanel::sell_requested);
        connect(kline_, &fincept::ui::KLineChartWidget::add_to_watchlist_requested, this,
                &EquityChartPanel::add_to_watchlist_requested);
    } else {
        // No WebEngine — drop KLineChart, use the Qt-Charts chart (which carries
        // its own timeframe buttons + overlays). Forward its timeframe signal.
        delete kline_;
        kline_ = nullptr;
        fallback_ = new EquityChart(this);
        connect(fallback_, &EquityChart::timeframe_changed, this, [this](const QString& tf) {
            have_bar_ = false; // fresh history will arrive for the new timeframe
            emit timeframe_changed(tf);
        });
        connect(fallback_, &EquityChart::buy_requested, this, &EquityChartPanel::buy_requested);
        connect(fallback_, &EquityChart::sell_requested, this, &EquityChartPanel::sell_requested);
        connect(fallback_, &EquityChart::add_to_watchlist_requested, this,
                &EquityChartPanel::add_to_watchlist_requested);
        root->addWidget(fallback_);
    }

    build_position_card();
}

void EquityChartPanel::build_kline_ui() {
    auto* root = static_cast<QVBoxLayout*>(layout());

    auto* bar = new QWidget(this);
    bar->setStyleSheet(QString("background:%1;").arg(fincept::ui::colors::BG_SURFACE()));
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(8, 4, 8, 4);
    h->setSpacing(4);

    const QString inactive =
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                "border-radius:2px;padding:2px 9px;font-size:11px;font-weight:700;"
                "font-family:'Consolas',monospace;}"
                "QPushButton:hover{border-color:%3;color:%4;}")
            .arg(fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::BORDER_DIM(),
                 fincept::ui::colors::AMBER(), fincept::ui::colors::TEXT_PRIMARY());
    const QString active =
        QString("QPushButton{background:%1;color:%2;border:1px solid %1;border-radius:2px;"
                "padding:2px 9px;font-size:11px;font-weight:700;font-family:'Consolas',monospace;}")
            .arg(fincept::ui::colors::AMBER(), fincept::ui::colors::BG_BASE());

    for (int i = 0; i < 6; ++i) {
        auto* b = new QPushButton(QString::fromLatin1(kTfLabels[i]), bar);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(i == active_tf_ ? active : inactive);
        b->setProperty("tfActiveStyle", active);
        b->setProperty("tfInactiveStyle", inactive);
        connect(b, &QPushButton::clicked, this, [this, i]() { set_active_tf(i); });
        tf_buttons_[i] = b;
        h->addWidget(b);
    }
    h->addStretch(1);

    root->addWidget(bar);
    root->addWidget(kline_, 1);
}

void EquityChartPanel::set_active_tf(int idx) {
    const bool changed = (idx != active_tf_);
    active_tf_ = idx;
    for (int i = 0; i < 6; ++i) {
        if (!tf_buttons_[i])
            continue;
        tf_buttons_[i]->setStyleSheet(
            (i == idx) ? tf_buttons_[i]->property("tfActiveStyle").toString()
                       : tf_buttons_[i]->property("tfInactiveStyle").toString());
    }
    if (changed) {
        have_bar_ = false; // fresh history will arrive for the new timeframe
        emit timeframe_changed(QString::fromLatin1(kTfLabels[idx]));
    }
}

QString EquityChartPanel::current_timeframe() const {
    if (fallback_)
        return fallback_->current_timeframe();
    return QString::fromLatin1(kTfLabels[active_tf_]);
}

void EquityChartPanel::set_candles(const QVector<trading::BrokerCandle>& candles) {
    if (!fallback_ && !kline_)
        return;

    if (fallback_) {
        fallback_->set_candles(candles);
    } else {
        QJsonArray arr;
        for (const auto& c : candles) {
            QJsonObject o;
            // BrokerCandle.timestamp is milliseconds; KLineChartWidget expects
            // seconds (it multiplies by 1000 internally).
            o[QStringLiteral("timestamp")] = static_cast<double>(c.timestamp / 1000);
            o[QStringLiteral("open")] = c.open;
            o[QStringLiteral("high")] = c.high;
            o[QStringLiteral("low")] = c.low;
            o[QStringLiteral("close")] = c.close;
            o[QStringLiteral("volume")] = c.volume;
            arr.append(o);
        }
        kline_->set_candles(arr);
    }

    // Seed forming-bar state from the most recent candle for live updates
    // (both backends extend this baseline in on_quote()).
    if (!candles.isEmpty()) {
        const auto& last = candles.last();
        have_bar_ = true;
        bar_ts_ms_ = last.timestamp;
        bar_open_ = last.open;
        bar_high_ = last.high;
        bar_low_ = last.low;
        bar_close_ = last.close;
        bar_vol_ = last.volume;
    } else {
        have_bar_ = false;
    }
}

void EquityChartPanel::on_quote(const trading::BrokerQuote& quote) {
    if (!fallback_ && !kline_)
        return;
    if (!have_bar_) // need a loaded history baseline to extend
        return;
    const double px = quote.ltp;
    if (px <= 0.0)
        return;

    const qint64 interval = interval_ms_for(current_timeframe());
    if (interval <= 0)
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Roll forward on the historical grid (last bar + interval) so the live bar
    // stays aligned with the bars the broker returned; the periodic history
    // reload on symbol/timeframe change corrects any session-gap drift.
    if (now >= bar_ts_ms_ + interval) {
        bar_ts_ms_ += interval;
        bar_open_ = bar_high_ = bar_low_ = bar_close_ = px;
        bar_vol_ = 0.0;
    } else {
        bar_close_ = px;
        if (px > bar_high_)
            bar_high_ = px;
        if (px < bar_low_)
            bar_low_ = px;
    }

    if (fallback_) {
        // Qt-Charts backend — hand the forming bar over as a BrokerCandle; the
        // chart updates its last bar in place (or appends on roll-forward).
        fallback_->update_last_candle(
            trading::BrokerCandle{bar_ts_ms_, bar_open_, bar_high_, bar_low_, bar_close_, bar_vol_});
        return;
    }

    QJsonObject o;
    o[QStringLiteral("timestamp")] = static_cast<double>(bar_ts_ms_ / 1000);
    o[QStringLiteral("open")] = bar_open_;
    o[QStringLiteral("high")] = bar_high_;
    o[QStringLiteral("low")] = bar_low_;
    o[QStringLiteral("close")] = bar_close_;
    o[QStringLiteral("volume")] = bar_vol_;
    kline_->update_candle(o);
}

qint64 EquityChartPanel::interval_ms_for(const QString& tf) {
    if (tf == QLatin1String("1m"))
        return 60LL * 1000;
    if (tf == QLatin1String("5m"))
        return 5LL * 60 * 1000;
    if (tf == QLatin1String("15m"))
        return 15LL * 60 * 1000;
    if (tf == QLatin1String("1h"))
        return 60LL * 60 * 1000;
    if (tf == QLatin1String("1d"))
        return 24LL * 60 * 60 * 1000;
    if (tf == QLatin1String("1w"))
        return 7LL * 24 * 60 * 60 * 1000;
    return 0;
}

// ── On-chart position overlay ────────────────────────────────────────────────

void EquityChartPanel::build_position_card() {
    namespace c = fincept::ui::colors;
    namespace f = fincept::ui::fonts;

    pos_card_ = new QFrame(this);
    pos_card_->setObjectName(QStringLiteral("chartPosCard"));
    pos_card_->setStyleSheet(QString("QFrame#chartPosCard{background:%1;border:1px solid %2;"
                                     "border-radius:4px;}")
                                 .arg(c::BG_RAISED(), c::BORDER_MED()));
    pos_card_->setCursor(Qt::OpenHandCursor);  // hint: draggable
    pos_card_->installEventFilter(this);       // drag-to-move (see eventFilter)
    auto* l = new QVBoxLayout(pos_card_);
    l->setContentsMargins(10, 7, 10, 8);
    l->setSpacing(4);

    pos_title_lbl_ = new QLabel(pos_card_);
    pos_title_lbl_->setStyleSheet(QString("background:transparent;border:none;font-weight:700;"
                                          "font-size:%1px;font-family:%2;")
                                      .arg(f::TINY)
                                      .arg(f::DATA_FAMILY()));

    pos_pnl_lbl_ = new QLabel(pos_card_);
    pos_pnl_lbl_->setStyleSheet(QString("background:transparent;border:none;font-weight:700;"
                                        "font-size:%1px;font-family:%2;")
                                    .arg(f::DATA)
                                    .arg(f::DATA_FAMILY()));

    pos_exit_btn_ = new QPushButton(tr("EXIT"), pos_card_);
    pos_exit_btn_->setCursor(Qt::PointingHandCursor);
    pos_exit_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;border-radius:2px;"
                "padding:3px 14px;font-weight:700;font-size:%2px;font-family:%3;}"
                "QPushButton:hover{background:rgba(220,38,38,0.16);}")
            .arg(c::NEGATIVE())
            .arg(f::TINY)
            .arg(f::DATA_FAMILY()));
    connect(pos_exit_btn_, &QPushButton::clicked, this, [this]() {
        emit exit_position_requested(pos_symbol_, pos_exchange_, pos_product_, pos_side_, pos_qty_);
    });

    // Labels pass mouse events through to the card so a drag started on the text
    // still moves the card; only the EXIT button stays interactive.
    pos_title_lbl_->setAttribute(Qt::WA_TransparentForMouseEvents);
    pos_pnl_lbl_->setAttribute(Qt::WA_TransparentForMouseEvents);

    l->addWidget(pos_title_lbl_);
    l->addWidget(pos_pnl_lbl_);
    l->addWidget(pos_exit_btn_);

    pos_card_->hide();
}

bool EquityChartPanel::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == pos_card_) {
        if (ev->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton) {
                pos_dragging_ = true;
                pos_drag_offset_ = me->pos(); // cursor offset within the card
                pos_card_->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        } else if (ev->type() == QEvent::MouseMove && pos_dragging_) {
            auto* me = static_cast<QMouseEvent*>(ev);
            // New top-left = cursor-in-panel minus the grab offset, clamped inside.
            QPoint p = pos_card_->mapToParent(me->pos()) - pos_drag_offset_;
            const int maxx = width() - pos_card_->width();
            const int maxy = height() - pos_card_->height();
            p.setX(qBound(0, p.x(), qMax(0, maxx)));
            p.setY(qBound(0, p.y(), qMax(0, maxy)));
            pos_card_->move(p);
            pos_card_moved_ = true; // stop auto-repositioning once user drags
            return true;
        } else if (ev->type() == QEvent::MouseButtonRelease && pos_dragging_) {
            pos_dragging_ = false;
            pos_card_->setCursor(Qt::OpenHandCursor);
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void EquityChartPanel::set_position(const QString& symbol, const QString& side, double qty,
                                    double entry_price, const QString& exchange,
                                    const QString& product_type, const QString& currency_code) {
    // On a symbol change the cached last-price is for the old symbol — reset it so
    // the card shows entry-based 0 P&L until the new symbol's first quote arrives.
    if (symbol != pos_symbol_)
        pos_ltp_ = 0.0;
    pos_symbol_ = symbol;
    pos_side_ = side.toLower();
    pos_qty_ = qAbs(qty);
    pos_entry_ = entry_price;
    pos_exchange_ = exchange;
    pos_product_ = product_type;
    pos_currency_ = currency_code;
    has_position_ = (pos_qty_ > 0.0 && pos_entry_ > 0.0);

    if (!has_position_) {
        clear_position();
        return;
    }

    // Title (LONG/SHORT qty @ entry) changes only when the position changes, so
    // render it here — NOT on every per-tick P&L refresh, where setStyleSheet()
    // would force a needless style re-parse + re-polish each tick.
    {
        namespace c = fincept::ui::colors;
        const bool is_long = (pos_side_ != QLatin1String("short"));
        const QString qty_str = QString::number(pos_qty_, 'f', pos_qty_ == qRound64(pos_qty_) ? 0 : 2);
        pos_title_lbl_->setText(QStringLiteral("%1 %2 @ %3")
                                    .arg(is_long ? tr("LONG") : tr("SHORT"))
                                    .arg(qty_str)
                                    .arg(QString::number(pos_entry_, 'f', 2)));
        pos_title_lbl_->setStyleSheet(QString("background:transparent;border:none;font-weight:700;"
                                              "font-size:%1px;font-family:%2;color:%3;")
                                          .arg(fincept::ui::fonts::TINY)
                                          .arg(fincept::ui::fonts::DATA_FAMILY())
                                          .arg(is_long ? c::POSITIVE() : c::NEGATIVE()));
    }
    pos_pnl_sign_ = 0; // force the P&L color to be re-applied for the new position

    refresh_position_card();
    draw_position_line();
    if (pos_card_) {
        pos_card_->show();
        pos_card_->raise();
    }
    layout_position_card();
}

void EquityChartPanel::clear_position() {
    has_position_ = false;
    if (pos_card_)
        pos_card_->hide();
    if (kline_)
        kline_->clear_position_line();
    if (fallback_)
        fallback_->clear_position_line();
}

void EquityChartPanel::update_pnl(double ltp) {
    if (ltp > 0.0)
        pos_ltp_ = ltp;
    if (!has_position_)
        return;
    // P&L text only. refresh_position_card() already adjustSize()s the card; the
    // full layout (reposition + raise) runs on set_position()/resize, not per tick.
    refresh_position_card();
}

void EquityChartPanel::refresh_position_card() {
    if (!pos_card_ || !has_position_)
        return;
    namespace c = fincept::ui::colors;

    // Per-tick path: only the live P&L changes here. The title (LONG/SHORT qty @
    // entry) is rendered once in set_position(), not on every tick.
    const bool is_long = (pos_side_ != QLatin1String("short"));
    const double ltp = pos_ltp_ > 0.0 ? pos_ltp_ : pos_entry_;
    const double pnl = is_long ? (ltp - pos_entry_) * pos_qty_ : (pos_entry_ - ltp) * pos_qty_;
    const double basis = pos_entry_ * pos_qty_;
    const double pct = basis != 0.0 ? (pnl / basis) * 100.0 : 0.0;

    const QString pnl_txt =
        (pnl >= 0 ? QStringLiteral("+") : QString()) + cur::money(pnl, false, pos_currency_);
    const QString pct_txt = QStringLiteral("%1%2%").arg(pct >= 0 ? "+" : "").arg(pct, 0, 'f', 2);
    pos_pnl_lbl_->setText(pnl_txt + QStringLiteral("   ") + pct_txt);

    // setStyleSheet() forces a full style re-parse + re-polish; only do it when
    // the P&L sign — and therefore the color — actually flips.
    const int sign = pnl >= 0 ? 1 : -1;
    if (sign != pos_pnl_sign_) {
        pos_pnl_sign_ = sign;
        pos_pnl_lbl_->setStyleSheet(QString("background:transparent;border:none;font-weight:700;"
                                            "font-size:%1px;font-family:%2;color:%3;")
                                        .arg(fincept::ui::fonts::DATA)
                                        .arg(fincept::ui::fonts::DATA_FAMILY())
                                        .arg(pnl >= 0 ? c::POSITIVE() : c::NEGATIVE()));
    }
    pos_card_->adjustSize();
}

void EquityChartPanel::draw_position_line() {
    if (!has_position_)
        return;
    const bool is_long = (pos_side_ != QLatin1String("short"));
    const QString hex = is_long ? QString(fincept::ui::colors::POSITIVE())
                                : QString(fincept::ui::colors::NEGATIVE());
    const QString label = QString::number(pos_entry_, 'f', 2);
    if (kline_)
        kline_->set_position_line(pos_entry_, label, hex);
    if (fallback_)
        fallback_->set_position_line(pos_entry_, QColor(hex), label);
}

void EquityChartPanel::layout_position_card() {
    if (!pos_card_ || !has_position_ || !pos_card_->isVisible())
        return;
    pos_card_->adjustSize();
    if (pos_card_moved_) {
        // Respect the user's chosen spot; just keep it inside the panel on resize.
        QPoint p = pos_card_->pos();
        p.setX(qBound(0, p.x(), qMax(0, width() - pos_card_->width())));
        p.setY(qBound(0, p.y(), qMax(0, height() - pos_card_->height())));
        pos_card_->move(p);
    } else {
        // Default top-LEFT — the price axis lives on the right edge, so anchoring
        // left keeps the card off it. The user can drag it anywhere from here.
        pos_card_->move(12, 44);
    }
    pos_card_->raise();
}

void EquityChartPanel::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    layout_position_card();
}

} // namespace fincept::screens::equity
