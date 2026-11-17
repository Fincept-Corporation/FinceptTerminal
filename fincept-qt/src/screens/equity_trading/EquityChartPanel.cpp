// EquityChartPanel.cpp — see header. KLineChart-preferred trading chart with
// an EquityChart (Qt Charts) fallback when Qt WebEngine is unavailable.
#include "screens/equity_trading/EquityChartPanel.h"

#include "screens/equity_trading/EquityChart.h"
#include "ui/charts/KLineChartWidget.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::equity {

namespace fmt = fincept::ui::formatting;

// "LONG 10 @ $351.71" — the entry price in the position's own currency.
static QString position_title(bool is_long, double qty, double entry, const QString& symbol, const QString& ccy) {
    const QString qty_str = QString::number(qty, 'f', qty == qRound64(qty) ? 0 : 2);
    return QStringLiteral("%1 %2 @ %3")
        .arg(is_long ? EquityChartPanel::tr("LONG") : EquityChartPanel::tr("SHORT"))
        .arg(qty_str)
        .arg(fmt::price_with_symbol(entry, fmt::instrument_for_symbol(symbol, ccy)));
}

// Must match EquityChart::TF_LABELS so the timeframe strings handed to
// stream->fetch_candles()/get_history() are identical across both backends.
static constexpr const char* kTfLabels[6] = {"1m", "5m", "15m", "1h", "1d", "1w"};

// Stylesheet of a KLineChart timeframe button, built from the active appearance.
static QString tf_button_style(bool active) {
    namespace c = fincept::ui::colors;
    if (active)
        return QString("QPushButton{background:%1;color:%2;border:1px solid %1;border-radius:2px;"
                       "padding:2px 9px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;}")
            .arg(c::AMBER(), c::TEXT_ON_ACCENT());
    return QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                   "border-radius:2px;padding:2px 9px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;}"
                   "QPushButton:hover{border-color:%3;color:%4;}")
        .arg(c::TEXT_SECONDARY(), c::BORDER_DIM(), c::AMBER(), c::TEXT_PRIMARY());
}

EquityChartPanel::EquityChartPanel(QWidget* parent) : QWidget(parent) {
    // A QWebEngineView has a near-zero size hint until its page loads, so give
    // the panel a real minimum + expanding policy; otherwise the splitter
    // starves it and the bottom panel takes the whole center column.
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    header_ = new fincept::ui::PanelHeader(tr("Chart"), this);
    root->addWidget(header_);

    kline_ = new fincept::ui::KLineChartWidget(this);
    if (kline_->is_available()) {
        build_kline_ui();
        // Built hidden; showEvent() reveals it one event-loop turn after the panel's
        // first show. Showing it creates the Qt WebEngine view, and the first such
        // (texture) view in a window makes Qt re-create the top-level window, which
        // hides and re-shows every widget. Inside the shell's screen construction
        // that re-show re-entered DockScreenRouter::materialize_screen() and built a
        // second Equity Trading screen into the same panel (two stacked screens).
        // Deferred, it happens after the shell has recorded this screen.
        kline_->hide();
        kline_reveal_pending_ = true;
        // Right-click chart trading — enable the menu and forward its actions up.
        kline_->set_trading_menu_enabled(true);
        connect(kline_, &fincept::ui::KLineChartWidget::buy_requested, this, &EquityChartPanel::buy_requested);
        connect(kline_, &fincept::ui::KLineChartWidget::sell_requested, this, &EquityChartPanel::sell_requested);
        connect(kline_, &fincept::ui::KLineChartWidget::add_to_watchlist_requested, this,
                &EquityChartPanel::add_to_watchlist_requested);
        // The runtime can be deployed yet unable to render (#411) — degrade to
        // the Qt-Charts chart instead of leaving a permanently blank chart.
        connect(kline_, &fincept::ui::KLineChartWidget::runtime_failed, this,
                &EquityChartPanel::swap_kline_for_fallback);
    } else {
        // No WebEngine — drop KLineChart, use the Qt-Charts chart.
        delete kline_;
        kline_ = nullptr;
        build_fallback_ui();
    }

    build_position_card();
    build_status_overlay();
    bind_status_body();
    // Nothing is loaded yet — the owner replaces this with the real state on its
    // first history request.
    show_status(StatusKind::Loading, tr("LOADING CHART"));

    // The position line carries its colour into the chart; draw it again in the
    // new appearance.
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::theme_changed, this,
            [this](const fincept::ui::ThemeTokens&) { draw_position_line(); });

    // Currency / number-format change: re-format the position card and line label
    // from the position already held (text only, no restyle).
    fmt::on_format_changed(this, [this]() {
        if (!has_position_)
            return;
        pos_title_lbl_->setText(position_title(pos_side_ != QLatin1String("short"), pos_qty_, pos_entry_,
                                               pos_symbol_, pos_currency_));
        refresh_position_card();
        draw_position_line();
    });
}

void EquityChartPanel::build_fallback_ui() {
    // The Qt-Charts chart carries its own timeframe buttons + overlays. Forward
    // its timeframe signal.
    fallback_ = new EquityChart(this);
    connect(fallback_, &EquityChart::timeframe_changed, this, [this](const QString& tf) {
        have_bar_ = false; // fresh history will arrive for the new timeframe
        emit timeframe_changed(tf);
    });
    connect(fallback_, &EquityChart::buy_requested, this, &EquityChartPanel::buy_requested);
    connect(fallback_, &EquityChart::sell_requested, this, &EquityChartPanel::sell_requested);
    connect(fallback_, &EquityChart::add_to_watchlist_requested, this, &EquityChartPanel::add_to_watchlist_requested);
    layout()->addWidget(fallback_);
}

void EquityChartPanel::swap_kline_for_fallback() {
    if (!kline_)
        return;
    kline_reveal_pending_ = false;
    // We're inside the dying chart's own signal — deleteLater, never delete.
    auto* dead = kline_;
    kline_ = nullptr;
    layout()->removeWidget(dead);
    dead->hide();
    dead->deleteLater();
    if (tf_bar_) {
        layout()->removeWidget(tf_bar_);
        tf_bar_->hide();
        tf_bar_->deleteLater();
        tf_bar_ = nullptr;
    }
    for (auto*& b : tf_buttons_)
        b = nullptr;
    have_bar_ = false;

    build_fallback_ui();
    draw_position_line();
    // The new backend starts empty: cover it until the history requested below lands.
    has_candles_ = false;
    bind_status_body();
    show_status(StatusKind::Loading, tr("LOADING CHART"));
    if (pos_card_ && has_position_)
        pos_card_->raise(); // keep the floating card above the new chart

    // The panel keeps no candles and the fallback has its own timeframe state,
    // so ask the screen to load history for the timeframe the fallback shows.
    emit timeframe_changed(fallback_->current_timeframe());
}

void EquityChartPanel::build_kline_ui() {
    auto* root = static_cast<QVBoxLayout*>(layout());

    // Timeframes sit in the panel header's control slot (one strip, no second bar).
    auto* bar = new QWidget(header_->controls());
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);

    for (int i = 0; i < 6; ++i) {
        auto* b = new QPushButton(QString::fromLatin1(kTfLabels[i]), bar);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(tf_button_style(i == active_tf_));
        connect(b, &QPushButton::clicked, this, [this, i]() { set_active_tf(i); });
        tf_buttons_[i] = b;
        h->addWidget(b);
    }

    tf_bar_ = bar;
    header_->add_control(bar);
    root->addWidget(kline_, 1);
}

void EquityChartPanel::set_title(const QString& title) {
    if (header_)
        header_->set_title(title.isEmpty() ? tr("Chart") : tr("Chart \xC2\xB7 %1").arg(title));
}

void EquityChartPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!kline_ || !kline_reveal_pending_)
        return;
    QTimer::singleShot(0, this, [this]() {
        if (!kline_ || !kline_reveal_pending_ || !isVisible())
            return; // hidden again before the turn came: the next show retries
        kline_reveal_pending_ = false;
        kline_->show();
        layout_status_overlay();
    });
}

void EquityChartPanel::set_active_tf(int idx) {
    const bool changed = (idx != active_tf_);
    active_tf_ = idx;
    for (int i = 0; i < 6; ++i) {
        if (!tf_buttons_[i])
            continue;
        tf_buttons_[i]->setStyleSheet(tf_button_style(i == idx));
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
            const qint64 ts_secs = c.timestamp / 1000; // whole seconds, truncated on purpose
            o[QStringLiteral("timestamp")] = static_cast<double>(ts_secs);
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

    has_candles_ = !candles.isEmpty();
    if (has_candles_)
        hide_status();
    else if (status_frame_ && status_frame_->isHidden())
        show_status(StatusKind::NoData, tr("NO CHART DATA"));
}

void EquityChartPanel::clear_candles() {
    if (fallback_)
        fallback_->clear();
    else if (kline_)
        kline_->clear();
    // Drop the forming-bar baseline too — on_quote() must not extend a bar that
    // belongs to the symbol we just cleared.
    have_bar_ = false;
    has_candles_ = false;
    // Never leave a bare grid: a default note until the owner sets the real state.
    if (status_frame_ && status_frame_->isHidden())
        show_status(StatusKind::NoData, tr("NO CHART DATA"));
}

// ── Status overlay ───────────────────────────────────────────────────────────

void EquityChartPanel::build_status_overlay() {
    namespace c = fincept::ui::colors;
    namespace f = fincept::ui::fonts;

    status_frame_ = new QFrame(this);
    status_frame_->setObjectName(QStringLiteral("eqChartStatus"));
    // Opaque: the point is to hide the empty axes, not to tint them.
    status_frame_->setStyleSheet(QString("QFrame#eqChartStatus{background:%1;border:none;}").arg(c::BG_BASE()));
    auto* l = new QVBoxLayout(status_frame_);
    l->setContentsMargins(24, 16, 24, 16);
    l->setSpacing(6);
    l->addStretch(1);

    status_title_ = new QLabel(status_frame_);
    status_title_->setAlignment(Qt::AlignCenter);
    status_title_->setWordWrap(true);
    l->addWidget(status_title_);

    status_detail_ = new QLabel(status_frame_);
    status_detail_->setAlignment(Qt::AlignCenter);
    status_detail_->setWordWrap(true);
    status_detail_->setTextInteractionFlags(Qt::TextSelectableByMouse); // error text can be copied
    status_detail_->setStyleSheet(QString("background:transparent;border:none;color:%1;font-size:%2px;"
                                          "font-family:%3;")
                                      .arg(c::TEXT_SECONDARY())
                                      .arg(f::TINY.get())
                                      .arg(f::DATA_FAMILY()));
    l->addWidget(status_detail_);
    l->addStretch(1);

    status_frame_->hide();
}

void EquityChartPanel::bind_status_body() {
    if (status_body_)
        status_body_->removeEventFilter(this);
    QWidget* body = nullptr;
    if (kline_)
        body = kline_;
    else if (fallback_)
        body = fallback_->plot_widget();
    status_body_ = body;
    if (body)
        body->installEventFilter(this); // follow its geometry (layout / splitter moves)
    layout_status_overlay();
}

void EquityChartPanel::layout_status_overlay() {
    if (!status_frame_ || status_frame_->isHidden())
        return;
    QRect r = rect();
    if (status_body_ && status_body_->isVisible() && status_body_->width() > 0 && status_body_->height() > 0)
        r = QRect(status_body_->mapTo(this, QPoint(0, 0)), status_body_->size());
    else if (header_)
        r.setTop(header_->geometry().bottom() + 1);
    status_frame_->setGeometry(r);
    status_frame_->raise();
    if (pos_card_ && has_position_)
        pos_card_->raise(); // an open position's card stays reachable
}

void EquityChartPanel::show_status(StatusKind kind, const QString& title, const QString& detail) {
    if (!status_frame_)
        return;
    const int k = static_cast<int>(kind);
    if (k != status_kind_) {
        // Restyle only when the kind changes (setStyleSheet re-polishes).
        namespace c = fincept::ui::colors;
        const QString color = kind == StatusKind::Error    ? QString(c::NEGATIVE())
                              : kind == StatusKind::NoData ? QString(c::WARNING())
                                                           : QString(c::TEXT_SECONDARY());
        status_title_->setStyleSheet(QString("background:transparent;border:none;color:%1;font-weight:700;"
                                             "font-size:%2px;letter-spacing:0.5px;font-family:%3;")
                                         .arg(color)
                                         .arg(fincept::ui::fonts::SMALL.get())
                                         .arg(fincept::ui::fonts::DATA_FAMILY()));
        status_kind_ = k;
    }
    status_title_->setText(title);
    status_detail_->setText(detail);
    status_detail_->setVisible(!detail.isEmpty());
    status_frame_->show();
    layout_status_overlay();
}

void EquityChartPanel::hide_status() {
    if (status_frame_)
        status_frame_->hide();
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
    // Roll forward on the historical grid (last bar + N*interval) so the live bar
    // stays aligned with the bars the broker returned; the periodic history
    // reload on symbol/timeframe change corrects any session-gap drift.
    // Jump straight to the bucket that contains `now`. Advancing a single interval
    // per tick sprouted a flat phantom bar on EVERY quote while the history ended
    // before the current bucket (overnight/weekend gap, market closed, history that
    // lacks the forming bar) — and on the Qt-Charts fallback each one is a full
    // chart rebuild.
    if (now >= bar_ts_ms_ + interval) {
        bar_ts_ms_ += ((now - bar_ts_ms_) / interval) * interval;
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
    const qint64 bar_ts_secs = bar_ts_ms_ / 1000; // whole seconds, truncated on purpose
    o[QStringLiteral("timestamp")] = static_cast<double>(bar_ts_secs);
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
    pos_card_->setCursor(Qt::OpenHandCursor); // hint: draggable
    pos_card_->installEventFilter(this);      // drag-to-move (see eventFilter)
    auto* l = new QVBoxLayout(pos_card_);
    l->setContentsMargins(10, 7, 10, 8);
    l->setSpacing(4);

    pos_title_lbl_ = new QLabel(pos_card_);
    pos_title_lbl_->setStyleSheet(QString("background:transparent;border:none;font-weight:700;"
                                          "font-size:%1px;font-family:%2;")
                                      .arg(f::TINY.get())
                                      .arg(f::DATA_FAMILY()));

    pos_pnl_lbl_ = new QLabel(pos_card_);
    pos_pnl_lbl_->setStyleSheet(QString("background:transparent;border:none;font-weight:700;"
                                        "font-size:%1px;font-family:%2;")
                                    .arg(f::DATA.get())
                                    .arg(f::DATA_FAMILY()));

    pos_exit_btn_ = new QPushButton(tr("EXIT"), pos_card_);
    pos_exit_btn_->setProperty("fincept_agent_risk", "order");
    pos_exit_btn_->setCursor(Qt::PointingHandCursor);
    pos_exit_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;border-radius:2px;"
                "padding:3px 14px;font-weight:700;font-size:%2px;font-family:%3;}"
                "QPushButton:hover{background:%4;}")
            .arg(c::NEGATIVE())
            .arg(f::TINY.get())
            .arg(f::DATA_FAMILY(), c::alpha(c::NEGATIVE, 0.16)));
    connect(pos_exit_btn_, &QPushButton::clicked, this,
            [this]() { emit exit_position_requested(pos_symbol_, pos_exchange_, pos_product_, pos_side_, pos_qty_); });

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
    if (status_body_ && obj == status_body_.data()) {
        if (ev->type() == QEvent::Resize || ev->type() == QEvent::Move || ev->type() == QEvent::Show)
            layout_status_overlay();
        return QWidget::eventFilter(obj, ev);
    }
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

void EquityChartPanel::set_position(const QString& symbol, const QString& side, double qty, double entry_price,
                                    const QString& exchange, const QString& product_type,
                                    const QString& currency_code) {
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
        pos_title_lbl_->setText(position_title(is_long, pos_qty_, pos_entry_, pos_symbol_, pos_currency_));
        pos_title_lbl_->setStyleSheet(QString("background:transparent;border:none;font-weight:700;"
                                              "font-size:%1px;font-family:%2;color:%3;")
                                          .arg(fincept::ui::fonts::TINY.get())
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

    // P&L is qty x a price difference: an amount, shown in the major unit of the
    // position's currency (pence P&L as pounds). Empty currency = no unit, never a guess.
    const QString pnl_txt =
        fmt::signed_money(fmt::to_major_units(pnl, pos_currency_), fmt::major_currency(pos_currency_));
    const QString pct_txt = fmt::pct(pct);
    pos_pnl_lbl_->setText(pnl_txt + QStringLiteral("   ") + pct_txt);

    // setStyleSheet() forces a full style re-parse + re-polish; only do it when
    // the P&L sign — and therefore the color — actually flips.
    const int sign = pnl >= 0 ? 1 : -1;
    if (sign != pos_pnl_sign_) {
        pos_pnl_sign_ = sign;
        pos_pnl_lbl_->setStyleSheet(QString("background:transparent;border:none;font-weight:700;"
                                            "font-size:%1px;font-family:%2;color:%3;")
                                        .arg(fincept::ui::fonts::DATA.get())
                                        .arg(fincept::ui::fonts::DATA_FAMILY())
                                        .arg(pnl >= 0 ? c::POSITIVE() : c::NEGATIVE()));
    }
    pos_card_->adjustSize();
}

void EquityChartPanel::draw_position_line() {
    if (!has_position_)
        return;
    const bool is_long = (pos_side_ != QLatin1String("short"));
    const QString hex = is_long ? QString(fincept::ui::colors::POSITIVE()) : QString(fincept::ui::colors::NEGATIVE());
    // A bare price on the chart (as quoted, the axis' unit), in the instrument's decimals.
    const QString label = fmt::price(pos_entry_, fmt::instrument_for_symbol(pos_symbol_, pos_currency_));
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
    layout_status_overlay();
    layout_position_card();
}

} // namespace fincept::screens::equity
