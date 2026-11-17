// src/screens/algo_trading/DeploymentsView.cpp
#include "screens/algo_trading/DeploymentsView.h"

#include "algo_engine/RiskLimits.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/KeySortProxy.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/algo/AlgoCurrency.h"

#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

namespace kit = fincept::screens::algo_kit;
namespace fmt = fincept::ui::formatting;
using services::algo::AlgoEquityPoint;
using services::algo::AlgoEventRow;
using services::algo::AlgoTradeRow;

namespace {

enum DvCol { kDvColState, kDvColStrategy, kDvColSymbol, kDvColMode, kDvColPnl, kDvColToday, kDvColPosition,
             kDvColLtp, kDvColTrades, kDvColWin, kDvColSignal, kDvColUpdated, kDvColUptime, kDvColTrend, kDvColCount };
enum DvFillCol { kDvFillTime, kDvFillSide, kDvFillQty, kDvFillPrice, kDvFillSlip, kDvFillFees, kDvFillPnl,
                 kDvFillReason, kDvFillCount };
enum DvLogCol { kDvLogTime, kDvLogKind, kDvLogMessage, kDvLogCount };
enum DvCondCol { kDvCondStatus, kDvCondSide, kDvCondRule, kDvCondNow, kDvCondTarget, kDvCondNote, kDvCondCount };
enum DvTab { kDvTabEquity, kDvTabFills, kDvTabLog, kDvTabConditions, kDvTabRisk, kDvTabCount };
enum DvPage { kDvPageWork = 0, kDvPageLoading = 1, kDvPageEmpty = 2 };

constexpr int kDvStaleSecs = 30;
constexpr int kDvDeadSecs = 120;
constexpr int kDvMaxChartSeries = 12;

QString dv_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return DeploymentsView::tr(text, disambiguation, n);
}

QString dv_price(double v, const QString& symbol, const QString& ccy) {
    if (!(std::isfinite(v) && v > 0))
        return kit::dash();
    return fmt::price_with_symbol(v, fmt::instrument_for_symbol(symbol, ccy));
}

/// "4 s", "12 min", "3 h", "2 d".
QString dv_span(qint64 secs) {
    if (secs < 60)
        return dv_tr("%1 s").arg(std::max<qint64>(0, secs));
    if (secs < 3600)
        return dv_tr("%1 min").arg(secs / 60);
    if (secs < 86400)
        return dv_tr("%1 h %2 min").arg(secs / 3600).arg((secs % 3600) / 60);
    return dv_tr("%1 d %2 h").arg(secs / 86400).arg((secs % 86400) / 3600);
}

QString dv_clock(qint64 ms) {
    if (ms <= 0)
        return kit::dash();
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(ms);
    if (t.date() == QDate::currentDate())
        return t.toString(QStringLiteral("HH:mm:ss"));
    return fmt::date_time(ms / 1000, fmt::DateStyle::Short, false);
}

struct DvState {
    QString text;
    kit::Tone tone = kit::Tone::Dim;
    QString tip;
};

DvState dv_state(const DeploymentsViewRow& r, qint64 now_ms) {
    if (r.stop_pending)
        return {dv_tr("STOPPING…"), kit::Tone::Warn, dv_tr("Stop requested — waiting for the engine to confirm")};
    if (r.active()) {
        if (r.paused)
            return {dv_tr("PAUSED"), kit::Tone::Warn, dv_tr("Rules, stop-loss and take-profit are NOT monitored")};
        if (r.risk_halted)
            return {dv_tr("HALTED"), kit::Tone::Warn,
                    r.halt_reason == QLatin1String("max_drawdown_limit")
                        ? dv_tr("Max drawdown limit hit; RESUME to trade again")
                        : dv_tr("Daily loss limit hit; resets on the next trading day")};
        if (r.status == QLatin1String("starting"))
            return {dv_tr("STARTING"), kit::Tone::Warn, QString()};
        if (r.last_update_ms > 0) {
            const qint64 secs = (now_ms - r.last_update_ms) / 1000;
            if (secs >= kDvDeadSecs)
                return {dv_tr("NO DATA"), kit::Tone::Danger,
                        dv_tr("No update for %1 — this deployment may not be running. Verify at your broker.")
                            .arg(dv_span(secs))};
            if (secs >= kDvStaleSecs)
                return {dv_tr("STALE"), kit::Tone::Warn, dv_tr("Last update %1 ago").arg(dv_span(secs))};
        }
        return {dv_tr("● RUNNING"), kit::Tone::Ok, QString()};
    }
    if (r.status == QLatin1String("error") || r.status == QLatin1String("crashed"))
        return {r.status.toUpper(), kit::Tone::Danger, r.alert};
    return {r.status.isEmpty() ? kit::dash() : r.status.toUpper(), kit::Tone::Dim, QString()};
}

QString dv_updated_text(const DeploymentsViewRow& r, qint64 now_ms) {
    if (r.stop_pending)
        return dv_tr("stop requested");
    if (r.paused)
        return dv_tr("paused");
    if (!r.active())
        return kit::dash();
    if (r.last_update_ms <= 0)
        return dv_tr("waiting…");
    const qint64 secs = (now_ms - r.last_update_ms) / 1000;
    return secs <= 1 ? dv_tr("just now") : dv_tr("%1 ago").arg(dv_span(secs));
}

QString dv_position(const DeploymentsViewRow& r) {
    if (r.pos_qty == 0)
        return dv_tr("FLAT");
    return QStringLiteral("%1 %2 @ %3")
        .arg(fmt::quantity(r.pos_qty), r.pos_side.toUpper(), dv_price(r.pos_entry, r.symbol, r.currency));
}

QWidget* dv_kv_cell(QWidget* parent, QLabel** key, QLabel** value) {
    auto* cell = new QWidget(parent);
    auto* v = new QVBoxLayout(cell);
    v->setContentsMargins(0, 0, 10, 0);
    v->setSpacing(0);
    *key = new QLabel(cell);
    (*key)->setObjectName(QStringLiteral("algoKey"));
    *value = new QLabel(kit::dash(), cell);
    (*value)->setObjectName(QStringLiteral("algoValue"));
    (*value)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(*key);
    v->addWidget(*value);
    cell->setMinimumWidth(QFontMetrics(cell->font()).averageCharWidth() * 13);
    return cell;
}

} // namespace

DeploymentsView::DeploymentsView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("deploymentsView"));
    build_ui();
    retranslate();
    set_page(Page::Loading);
}

void DeploymentsView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Command bar ─────────────────────────────────────────────────────────
    bar_ = kit::make_tab_bar(this, kit::TabDashboard);
    using Role = ui::ScreenCommandBar::Role;
    bar_->add_action(QStringLiteral("refresh"), QString(), QStringLiteral("Ctrl+R"), {}, Role::Normal, 70);
    bar_->add_action(QStringLiteral("log"), QString(), {}, {}, Role::Normal, 50);
    bar_->add_action(QStringLiteral("strategies"), QString(), {}, {}, Role::Normal, 30);
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, [this](const QString& id) {
        if (id == QLatin1String("refresh"))
            emit refresh_requested();
        else if (id == QLatin1String("log")) {
            if (const QString sel = selected_id(); !sel.isEmpty())
                emit log_requested(sel);
        } else if (id == QLatin1String("strategies"))
            emit open_strategies_requested();
    });
    root->addWidget(bar_);
    auto* refresh_sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
    refresh_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(refresh_sc, &QShortcut::activated, this, &DeploymentsView::refresh_requested);

    // ── KPI ribbon ──────────────────────────────────────────────────────────
    auto* ribbon_box = new QWidget(this);
    ribbon_box->setObjectName(QStringLiteral("algoRibbon"));
    ribbon_box->setAttribute(Qt::WA_StyledBackground, true);
    ribbon_box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* rh = new QHBoxLayout(ribbon_box);
    rh->setContentsMargins(6, 4, 6, 4);
    ribbon_ = new ui::KpiStrip(ribbon_box);
    ribbon_->set_spacing(6);
    ribbon_->set_max_columns(9);
    ribbon_->set_min_tile_width(QFontMetrics(font()).averageCharWidth() * 14);
    for (const char* key : {"running", "live", "paper", "paper_today", "positions", "orders", "winrate", "fees",
                            "halted"}) {
        ui::StatTile* t = ribbon_->add_tile(QLatin1String(key), QString());
        t->set_compact(true);
        t->set_value(kit::dash());
    }
    rh->addWidget(ribbon_);
    root->addWidget(ribbon_box);

    // ── Pages: work area | loading | nothing deployed ───────────────────────
    pages_ = new QStackedWidget(this);
    auto* split = kit::make_splitter(Qt::Horizontal, pages_);
    work_ = split;

    // Deployments list
    auto lp = kit::make_panel(QString(), split, 0);
    list_header_ = lp.header;
    stop_all_btn_ = kit::make_tool_button(QString(), QString(), lp.header);
    kit::set_tone(stop_all_btn_, kit::Tone::Danger);
    lp.header->add_control(stop_all_btn_);
    connect(stop_all_btn_, &QPushButton::clicked, this, &DeploymentsView::stop_all_requested);
    table_ = new QTableView(lp.body_widget);
    model_ = new ui::LiveTableModel<DeploymentsViewRow>(table_);
    model_->set_key([](const DeploymentsViewRow& r) { return r.id; });
    model_->set_cell([this](const DeploymentsViewRow& r, int col, int role) -> QVariant {
        using M = ui::LiveTableModel<DeploymentsViewRow>;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool money_col = col == kDvColPnl || col == kDvColToday;
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kDvColState:
                    return dv_state(r, now).text;
                case kDvColStrategy:
                    return r.strategy;
                case kDvColSymbol:
                    return r.symbol;
                case kDvColMode:
                    return r.mode.toUpper();
                case kDvColPnl:
                    return fmt::money_in_column(r.net_pnl, r.currency, mixed_, fmt::Compact::None, -1, true);
                case kDvColToday:
                    return fmt::money_in_column(r.today_pnl, r.currency, mixed_, fmt::Compact::None, -1, true);
                case kDvColPosition:
                    return dv_position(r);
                case kDvColLtp:
                    return dv_price(r.ltp, r.symbol, r.currency);
                case kDvColTrades:
                    return QString::number(r.trades);
                case kDvColWin:
                    return r.trades > 0 ? fmt::pct(r.win_rate, 1, false) : kit::dash();
                case kDvColSignal:
                    return dv_clock(r.last_signal_ms);
                case kDvColUpdated:
                    return dv_updated_text(r, now);
                case kDvColUptime:
                    return r.active() && r.started_ms > 0 ? dv_span((now - r.started_ms) / 1000) : kit::dash();
                default:
                    break;
            }
            return {};
        }
        if (role == M::SortRole) {
            switch (col) {
                case kDvColState:
                    return dv_state(r, now).text;
                case kDvColStrategy:
                    return r.strategy;
                case kDvColSymbol:
                    return r.symbol;
                case kDvColMode:
                    return r.mode;
                case kDvColPnl:
                    return fincept::ui::algo::algo_sort_key(r.net_pnl, r.currency, mixed_);
                case kDvColToday:
                    return fincept::ui::algo::algo_sort_key(r.today_pnl, r.currency, mixed_);
                case kDvColPosition:
                    return r.pos_qty;
                case kDvColLtp:
                    return r.ltp > 0 ? fincept::ui::algo::algo_sort_key(r.ltp, r.currency, mixed_) : QVariant();
                case kDvColTrades:
                    return r.trades;
                case kDvColWin:
                    return r.win_rate;
                case kDvColSignal:
                    return r.last_signal_ms > 0 ? QVariant(r.last_signal_ms) : QVariant();
                case kDvColUpdated:
                    return r.last_update_ms > 0 ? QVariant(r.last_update_ms) : QVariant();
                case kDvColUptime:
                    return r.active() && r.started_ms > 0 ? QVariant(now - r.started_ms) : QVariant();
                default:
                    break;
            }
            return {};
        }
        if (role == Qt::ForegroundRole) {
            if (col == kDvColState)
                return kit::tone_color(dv_state(r, now).tone);
            if (col == kDvColMode)
                return kit::tone_color(r.mode == QLatin1String("live") ? kit::Tone::Danger : kit::Tone::Ok);
            if (col == kDvColPnl)
                return r.net_pnl != 0 ? QVariant(kit::tone_color(kit::sign_tone(r.net_pnl))) : QVariant();
            if (col == kDvColToday)
                return r.today_pnl != 0 ? QVariant(kit::tone_color(kit::sign_tone(r.today_pnl))) : QVariant();
            if (col == kDvColUpdated && r.active() && r.last_update_ms > 0) {
                const qint64 secs = (now - r.last_update_ms) / 1000;
                if (secs >= kDvDeadSecs)
                    return kit::tone_color(kit::Tone::Danger);
                if (secs >= kDvStaleSecs)
                    return kit::tone_color(kit::Tone::Warn);
            }
            return {};
        }
        if (role == Qt::ToolTipRole) {
            if (col == kDvColState) {
                const DvState s = dv_state(r, now);
                return s.tip.isEmpty() ? (r.alert.isEmpty() ? QVariant() : QVariant(r.alert)) : QVariant(s.tip);
            }
            if (col == kDvColPnl)
                return dv_tr("Realized + unrealized, net of %1fees").arg(r.mode == QLatin1String("live") ? dv_tr("estimated ")
                                                                                                         : QString());
            if (col == kDvColTrend)
                return dv_tr("Net P&L over the last 30 days");
            return {};
        }
        if (role == Qt::TextAlignmentRole && (money_col || col == kDvColLtp || col == kDvColTrades ||
                                              col == kDvColWin || col == kDvColUptime))
            return int(Qt::AlignRight | Qt::AlignVCenter);
        if (role == ui::SparklineDelegate::kDefaultRole && col == kDvColTrend)
            return QVariant::fromValue(r.spark);
        return {};
    });
    proxy_ = new ui::KeySortProxy(table_);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(ui::LiveTableModel<DeploymentsViewRow>::SortRole);
    table_->setModel(proxy_);
    kit::setup_table(table_, QStringLiteral("algo_deployments"));
    table_->setItemDelegateForColumn(kDvColTrend, new ui::SparklineDelegate(table_));
    table_->setAccessibleName(tr("Deployments"));
    table_->installEventFilter(this);
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex&) {
        if (const QString id = selected_id(); !id.isEmpty())
            emit log_requested(id);
    });
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        render_detail();
        const QString id = selected_id();
        if (id != cond_id_ || id != history_id_)
            emit selection_changed(id);
        if (!equity_all_)
            render_equity();
    });
    lp.body->addWidget(table_, 1);
    split->addWidget(lp.frame);
    split->addWidget(build_detail(split));
    split->setStretchFactor(0, 11);
    split->setStretchFactor(1, 9);
    pages_->addWidget(split); // kDvPageWork

    loading_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, pages_);
    pages_->addWidget(loading_); // kDvPageLoading

    // Nothing deployed: what this dashboard shows and how to get something on it. Deploying
    // is the user's action (it starts a strategy) — never automatic.
    empty_ = new ui::EmptyState(pages_);
    connect(empty_, &ui::EmptyState::primary_clicked, this, &DeploymentsView::open_strategies_requested);
    connect(empty_, &ui::EmptyState::secondary_clicked, this, &DeploymentsView::open_builder_requested);
    pages_->addWidget(empty_); // kDvPageEmpty
    root->addWidget(pages_, 1);

    status_bar_ = kit::make_status_bar(this);
    root->addWidget(status_bar_);

    kit::set_column_chars(table_, {11, 18, 11, 6, 13, 12, 18, 11, 6, 6, 9, 10, 10, 12});
    table_->sortByColumn(kDvColStrategy, Qt::AscendingOrder);
}

QWidget* DeploymentsView::build_detail(QWidget* parent) {
    auto dp = kit::make_panel(QString(), parent, 0);
    detail_header_ = dp.header;
    pause_btn_ = kit::make_tool_button(QString(), QString(), dp.header);
    stop_btn_ = kit::make_tool_button(QString(), QString(), dp.header);
    kit::set_tone(stop_btn_, kit::Tone::Warn);
    remove_btn_ = kit::make_tool_button(QString(), QString(), dp.header);
    kit::set_tone(remove_btn_, kit::Tone::Danger);
    log_btn_ = kit::make_tool_button(QString(), QString(), dp.header);
    for (QPushButton* b : {pause_btn_, stop_btn_, remove_btn_, log_btn_})
        dp.header->add_control(b);
    connect(pause_btn_, &QPushButton::clicked, this, [this]() {
        if (const QString id = selected_id(); !id.isEmpty())
            emit pause_toggle_requested(id);
    });
    connect(stop_btn_, &QPushButton::clicked, this, [this]() {
        if (const QString id = selected_id(); !id.isEmpty())
            emit stop_requested(id);
    });
    connect(remove_btn_, &QPushButton::clicked, this, [this]() {
        if (const QString id = selected_id(); !id.isEmpty())
            emit remove_requested(id);
    });
    connect(log_btn_, &QPushButton::clicked, this, [this]() {
        if (const QString id = selected_id(); !id.isEmpty())
            emit log_requested(id);
    });

    detail_stack_ = new QStackedWidget(dp.body_widget);
    detail_none_ = kit::make_note(QString(), detail_stack_);
    detail_none_->setAlignment(Qt::AlignCenter);
    detail_stack_->addWidget(detail_none_);

    auto* content = new QWidget(detail_stack_);
    content->setObjectName(QStringLiteral("algoDetail"));
    content->setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(content);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // Summary: chips, alert / halt lines, key figures (wraps with the width / font).
    auto* summary = new QWidget(content);
    summary->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* sv = new QVBoxLayout(summary);
    sv->setContentsMargins(8, 6, 8, 6);
    sv->setSpacing(6);
    chips_ = new QWidget(summary);
    auto* chips_flow = new ui::FlowLayout(chips_);
    state_chip_ = kit::make_chip(QString(), kit::Tone::Dim, chips_);
    mode_chip_ = kit::make_chip(QString(), kit::Tone::Ok, chips_);
    side_chip_ = kit::make_chip(QString(), kit::Tone::None, chips_);
    tf_chip_ = kit::make_chip(QString(), kit::Tone::Dim, chips_);
    broker_chip_ = kit::make_chip(QString(), kit::Tone::Dim, chips_);
    for (QLabel* c : {state_chip_, mode_chip_, side_chip_, tf_chip_, broker_chip_})
        chips_flow->addWidget(c);
    sv->addWidget(chips_);
    alert_ = kit::make_note(QString(), summary, kit::Tone::Danger);
    alert_->setVisible(false);
    sv->addWidget(alert_);
    halt_ = kit::make_note(QString(), summary, kit::Tone::Warn);
    halt_->setVisible(false);
    sv->addWidget(halt_);
    auto* kv = new QWidget(summary);
    auto* kv_flow = new ui::FlowLayout(kv);
    for (const char* key : {"ltp", "pnl", "today", "unreal", "position", "trades", "win", "maxdd", "fees", "uptime",
                            "signal", "updated"}) {
        QLabel* k = nullptr;
        QLabel* val = nullptr;
        kv_flow->addWidget(dv_kv_cell(kv, &k, &val));
        kv_keys_.insert(QLatin1String(key), k);
        kv_values_.insert(QLatin1String(key), val);
    }
    sv->addWidget(kv);
    v->addWidget(summary);

    // Tabs: EQUITY · FILLS · LOG · CONDITIONS · RISK
    tabs_ = new QTabBar(content);
    tabs_->setObjectName(QStringLiteral("algoTabs"));
    tabs_->setExpanding(false);
    tabs_->setDrawBase(false);
    tabs_->setUsesScrollButtons(true);
    for (int i = 0; i < kDvTabCount; ++i)
        tabs_->addTab(QString());
    v->addWidget(tabs_);
    tab_pages_ = new QStackedWidget(content);
    connect(tabs_, &QTabBar::currentChanged, tab_pages_, &QStackedWidget::setCurrentIndex);

    // EQUITY
    auto* eq = new QWidget(tab_pages_);
    auto* ev = new QVBoxLayout(eq);
    ev->setContentsMargins(6, 4, 6, 4);
    ev->setSpacing(4);
    equity_mode_ = kit::make_segmented({QString(), QString()}, 0,
                                       [this](int i) {
                                           equity_all_ = i == 1;
                                           render_equity();
                                       },
                                       eq);
    ev->addWidget(equity_mode_, 0, Qt::AlignLeft);
    chart_ = new ui::TimeSeriesChart(eq);
    chart_->set_transparent_background(true);
    chart_->set_crosshair_enabled(true);
    chart_->set_range_presets({QStringLiteral("1D"), QStringLiteral("1W"), QStringLiteral("1M"), QStringLiteral("ALL")});
    chart_->set_export_name(QStringLiteral("algo_pnl_history"));
    dd_pane_ = chart_->add_pane(0.35);
    ev->addWidget(chart_, 1);
    tab_pages_->addWidget(eq); // kDvTabEquity

    // FILLS
    auto* fp = new QWidget(tab_pages_);
    auto* fv = new QVBoxLayout(fp);
    fv->setContentsMargins(0, 0, 0, 0);
    fv->setSpacing(0);
    fills_note_ = kit::make_note(QString(), fp);
    fills_note_->setContentsMargins(8, 3, 8, 3);
    fv->addWidget(fills_note_);
    fills_table_ = new QTableView(fp);
    fills_model_ = new ui::LiveTableModel<AlgoTradeRow>(fills_table_);
    fills_model_->set_key([](const AlgoTradeRow& t) {
        return QStringLiteral("%1|%2|%3|%4|%5").arg(t.t_ms).arg(t.side, t.leg_symbol, t.broker_order_id).arg(t.price);
    });
    fills_model_->set_cell([this](const AlgoTradeRow& t, int col, int role) -> QVariant {
        using M = ui::LiveTableModel<AlgoTradeRow>;
        const double slip = t.ref_price > 0 ? (t.price - t.ref_price) / t.ref_price * 10000.0 *
                                                  (t.side == QLatin1String("BUY") ? 1 : -1)
                                            : std::numeric_limits<double>::quiet_NaN();
        const fmt::InstrumentFmt inst =
            fmt::instrument_for_symbol(t.leg_symbol.isEmpty() ? detail_symbol_ : t.leg_symbol, detail_ccy_);
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kDvFillTime:
                    return fmt::date_time(t.t_ms / 1000, fmt::DateStyle::Short, true);
                case kDvFillSide:
                    return t.leg_symbol.isEmpty() ? t.side : t.side + QLatin1Char(' ') + t.leg_symbol;
                case kDvFillQty:
                    return fmt::quantity(t.quantity);
                case kDvFillPrice:
                    return fmt::price(t.price, inst);
                case kDvFillSlip:
                    return std::isfinite(slip) ? dv_tr("%1 bp").arg(fmt::number(slip, 1)) : kit::dash();
                case kDvFillFees:
                    return fmt::money(t.fees, detail_ccy_);
                case kDvFillPnl:
                    return t.is_exit() ? fmt::signed_money(t.pnl, detail_ccy_) : kit::dash();
                case kDvFillReason:
                    return t.reason;
                default:
                    break;
            }
            return {};
        }
        if (role == M::SortRole) {
            switch (col) {
                case kDvFillTime:
                    return t.t_ms;
                case kDvFillSide:
                    return t.side;
                case kDvFillQty:
                    return t.quantity;
                case kDvFillPrice:
                    return t.price;
                case kDvFillSlip:
                    return std::isfinite(slip) ? QVariant(slip) : QVariant();
                case kDvFillFees:
                    return t.fees;
                case kDvFillPnl:
                    return t.is_exit() ? QVariant(t.pnl) : QVariant();
                case kDvFillReason:
                    return t.reason;
                default:
                    break;
            }
            return {};
        }
        if (role == Qt::ForegroundRole) {
            if (col == kDvFillSide)
                return kit::tone_color(t.side == QLatin1String("BUY") ? kit::Tone::Pos : kit::Tone::Neg);
            if (col == kDvFillPnl && t.is_exit() && t.pnl != 0)
                return kit::tone_color(kit::sign_tone(t.pnl));
            return {};
        }
        if (role == Qt::TextAlignmentRole && col != kDvFillSide && col != kDvFillReason && col != kDvFillTime)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    });
    fills_proxy_ = new ui::KeySortProxy(fills_table_);
    fills_proxy_->setSourceModel(fills_model_);
    fills_proxy_->setSortRole(ui::LiveTableModel<AlgoTradeRow>::SortRole);
    fills_table_->setModel(fills_proxy_);
    kit::setup_table(fills_table_, QStringLiteral("deployment_fills"));
    fills_table_->setAccessibleName(tr("Fills of the selected deployment"));
    fv->addWidget(fills_table_, 1);
    tab_pages_->addWidget(fp); // kDvTabFills

    // LOG
    auto* lpg = new QWidget(tab_pages_);
    auto* lv = new QVBoxLayout(lpg);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(0);
    log_note_ = kit::make_note(QString(), lpg);
    log_note_->setContentsMargins(8, 3, 8, 3);
    lv->addWidget(log_note_);
    log_table_ = new QTableView(lpg);
    log_model_ = new ui::LiveTableModel<AlgoEventRow>(log_table_);
    log_model_->set_key([](const AlgoEventRow& e) {
        return QStringLiteral("%1|%2|%3").arg(e.t_ms).arg(e.kind, e.message.left(48));
    });
    log_model_->set_cell([](const AlgoEventRow& e, int col, int role) -> QVariant {
        using M = ui::LiveTableModel<AlgoEventRow>;
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kDvLogTime:
                    return fmt::date_time(e.t_ms / 1000, fmt::DateStyle::Short, true);
                case kDvLogKind:
                    return e.kind.toUpper();
                case kDvLogMessage:
                    return e.message;
                default:
                    break;
            }
            return {};
        }
        if (role == M::SortRole)
            return col == kDvLogTime ? QVariant(e.t_ms) : (col == kDvLogKind ? QVariant(e.kind) : QVariant(e.message));
        if (role == Qt::ForegroundRole && col == kDvLogKind) {
            if (e.kind == QLatin1String("reject") || e.kind == QLatin1String("error"))
                return kit::tone_color(kit::Tone::Danger);
            if (e.kind == QLatin1String("risk"))
                return kit::tone_color(kit::Tone::Warn);
            return {};
        }
        if (role == Qt::ToolTipRole && col == kDvLogMessage)
            return e.message;
        return {};
    });
    log_proxy_ = new ui::KeySortProxy(log_table_);
    log_proxy_->setSourceModel(log_model_);
    log_proxy_->setSortRole(ui::LiveTableModel<AlgoEventRow>::SortRole);
    log_table_->setModel(log_proxy_);
    kit::setup_table(log_table_, QStringLiteral("deployment_log"));
    log_table_->setAccessibleName(tr("Signal, order and risk log of the selected deployment"));
    lv->addWidget(log_table_, 1);
    tab_pages_->addWidget(lpg); // kDvTabLog

    // CONDITIONS
    auto* cpg = new QWidget(tab_pages_);
    auto* cv = new QVBoxLayout(cpg);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(0);
    cond_note_ = kit::make_note(QString(), cpg);
    cond_note_->setContentsMargins(8, 3, 8, 3);
    cv->addWidget(cond_note_);
    cond_table_ = new QTableView(cpg);
    cond_model_ = new ui::LiveTableModel<DeploymentsCondRow>(cond_table_);
    cond_model_->set_key([](const DeploymentsCondRow& c) { return c.key; });
    cond_model_->set_cell([](const DeploymentsCondRow& c, int col, int role) -> QVariant {
        const bool green = c.met || c.executed;
        QString hint;
        if (!green) {
            if (c.op == QLatin1String("crosses_above") && c.computed >= c.target)
                hint = dv_tr("already above — can't cross up");
            else if (c.op == QLatin1String("crosses_below") && c.computed <= c.target)
                hint = dv_tr("already below — can't cross down");
        }
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kDvCondStatus:
                    return green ? (c.executed && !c.met ? dv_tr("✓ EXECUTED") : dv_tr("✓ MET")) : dv_tr("✗ NOT MET");
                case kDvCondSide:
                    return c.section.toUpper();
                case kDvCondRule:
                    return c.label;
                case kDvCondNow:
                    return fmt::number(c.computed, std::abs(c.computed) < 10 ? 4 : 2);
                case kDvCondTarget:
                    return fmt::number(c.target, std::abs(c.target) < 10 ? 4 : 2);
                case kDvCondNote:
                    return hint;
                default:
                    break;
            }
            return {};
        }
        if (role == Qt::ForegroundRole && (col == kDvCondStatus || col == kDvCondNote))
            return kit::tone_color(green ? kit::Tone::Ok : (hint.isEmpty() ? kit::Tone::Dim : kit::Tone::Warn));
        if (role == Qt::ToolTipRole && col == kDvCondRule)
            return c.label;
        if (role == Qt::TextAlignmentRole && (col == kDvCondNow || col == kDvCondTarget))
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    });
    cond_table_->setModel(cond_model_);
    kit::setup_table(cond_table_, QStringLiteral("deployment_conditions"));
    cond_table_->setSortingEnabled(false); // entry rows then exit rows, as the engine sends them
    cond_table_->setAccessibleName(tr("Rule status of the selected deployment"));
    cv->addWidget(cond_table_, 2);
    activity_ = new QPlainTextEdit(cpg);
    activity_->setObjectName(QStringLiteral("algoLog"));
    activity_->setReadOnly(true);
    activity_->setAccessibleName(tr("Recent activity of the selected deployment"));
    activity_->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5 + 12);
    cv->addWidget(activity_);
    tab_pages_->addWidget(cpg); // kDvTabConditions

    // RISK
    auto* rpg = new QWidget(tab_pages_);
    auto* rgrid = new QGridLayout(rpg);
    rgrid->setContentsMargins(8, 6, 8, 6);
    rgrid->setHorizontalSpacing(12);
    rgrid->setVerticalSpacing(4);
    int r = 0;
    for (const char* key : {"qty", "maxorder", "maxloss", "maxdd", "maxtrades", "window", "session", "flatten"}) {
        auto* k = new QLabel(rpg);
        k->setObjectName(QStringLiteral("algoKey"));
        auto* val = new QLabel(kit::dash(), rpg);
        val->setObjectName(QStringLiteral("algoValue"));
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rgrid->addWidget(k, r, 0);
        rgrid->addWidget(val, r, 1);
        risk_keys_.insert(QLatin1String(key), k);
        risk_values_.insert(QLatin1String(key), val);
        ++r;
    }
    rgrid->setColumnStretch(1, 1);
    rgrid->setRowStretch(r, 1);
    tab_pages_->addWidget(rpg); // kDvTabRisk

    v->addWidget(tab_pages_, 1);
    detail_stack_->addWidget(content);
    dp.body->addWidget(detail_stack_, 1);

    kit::set_column_chars(fills_table_, {15, 8, 8, 10, 8, 9, 11, 12});
    kit::set_column_chars(log_table_, {15, 7, 40});
    kit::set_column_chars(cond_table_, {11, 6, 26, 10, 10, 20});
    fills_table_->sortByColumn(kDvFillTime, Qt::DescendingOrder);
    log_table_->sortByColumn(kDvLogTime, Qt::DescendingOrder);
    return dp.frame;
}

void DeploymentsView::retranslate() {
    kit::retranslate_tab_bar(bar_);
    bar_->set_action_text(QStringLiteral("refresh"), tr("REFRESH"));
    bar_->set_action_tooltip(QStringLiteral("refresh"), tr("Read the deployments and their P&L history again (Ctrl+R)"));
    bar_->set_action_text(QStringLiteral("log"), tr("LOG"));
    bar_->set_action_tooltip(QStringLiteral("log"),
                             tr("Full history of the selected deployment: fills, signal & risk log, P&L, statistics"));
    bar_->set_action_text(QStringLiteral("strategies"), tr("STRATEGIES ›"));
    bar_->set_action_tooltip(QStringLiteral("strategies"), tr("Deploy a saved strategy from MY STRATEGIES"));

    struct Tile {
        const char* key;
        QString label;
        QString tip;
    };
    const Tile tiles[] = {
        {"running", tr("RUNNING"), tr("Deployments running now (LIVE = real money)")},
        {"live", tr("LIVE P&L"), tr("Realized + unrealized, net of estimated fees — real money")},
        {"paper", tr("PAPER P&L"), tr("Realized + unrealized, net of simulated fees")},
        {"paper_today", tr("PAPER TODAY"), tr("Realized P&L of paper deployments on the current exchange day")},
        {"positions", tr("OPEN POSITIONS"), tr("Deployments holding a position now")},
        {"orders", tr("ORDERS TODAY"), tr("Fills today, from the running deployments' live snapshots")},
        {"winrate", tr("WIN RATE"), tr("Winning trades over all closed trades, across deployments (trade-weighted)")},
        {"fees", tr("FEES"), tr("Paper: simulated fees. Live: ESTIMATED from the cost model.")},
        {"halted", tr("HALTED"), tr("Running deployments whose entries are halted by a risk limit")},
    };
    for (const Tile& t : tiles)
        if (ui::StatTile* tile = ribbon_->tile(QLatin1String(t.key))) {
            tile->set_label(t.label);
            tile->set_tooltip_text(t.tip);
        }

    list_header_->set_title(tr("Deployments"));
    stop_all_btn_->setText(tr("STOP ALL"));
    stop_all_btn_->setToolTip(tr("Stop every running deployment (asks first; open positions are not closed)"));
    stop_all_btn_->setAccessibleName(tr("Stop all running deployments"));
    model_->set_headers({tr("STATE"), tr("STRATEGY"), tr("SYMBOL"), tr("MODE"), tr("NET P&L"), tr("TODAY"),
                         tr("POSITION"), tr("LTP"), tr("TRADES"), tr("WIN %"), tr("LAST SIGNAL"), tr("UPDATED"),
                         tr("UPTIME"), tr("TREND")});
    table_->setToolTip(tr("Double-click a row (or Enter) for its full log and P&L history"));

    pause_btn_->setToolTip(tr("Pause stops evaluating rules AND protective exits until you resume. Resume also clears a "
                              "daily-loss or drawdown halt."));
    stop_btn_->setText(tr("STOP"));
    stop_btn_->setToolTip(tr("Stop the selected deployment (asks first; can close its open position)"));
    stop_btn_->setAccessibleName(tr("Stop the selected deployment"));
    remove_btn_->setText(tr("REMOVE"));
    remove_btn_->setToolTip(tr("Delete the stopped deployment's record (asks first)"));
    remove_btn_->setAccessibleName(tr("Remove the selected deployment"));
    log_btn_->setText(tr("LOG…"));
    log_btn_->setToolTip(tr("Fills, signal & risk log, P&L history and statistics in a window"));
    log_btn_->setAccessibleName(tr("Open the full log of the selected deployment"));
    detail_none_->setText(tr("Select a deployment to see its figures, fills, log and rule status."));

    const QStringList tab_names = {tr("EQUITY"), tr("FILLS"), tr("LOG"), tr("CONDITIONS"), tr("RISK")};
    for (int i = 0; i < tab_names.size() && i < tabs_->count(); ++i)
        tabs_->setTabText(i, tab_names[i]);
    const auto segs = equity_mode_->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    if (segs.size() == 2) {
        segs[0]->setText(tr("SELECTED"));
        segs[0]->setToolTip(tr("P&L history of the selected deployment with its drawdown"));
        segs[0]->setAccessibleName(tr("Show the selected deployment"));
        segs[1]->setText(tr("ALL"));
        segs[1]->setToolTip(tr("P&L history of every deployment (each line in its own currency)"));
        segs[1]->setAccessibleName(tr("Show all deployments"));
    }

    const QPair<const char*, QString> kv[] = {
        {"ltp", tr("LTP")},          {"pnl", tr("NET P&L")},      {"today", tr("TODAY")},
        {"unreal", tr("UNREALIZED")}, {"position", tr("POSITION")}, {"trades", tr("TRADES")},
        {"win", tr("WIN RATE")},     {"maxdd", tr("MAX DD")},     {"fees", tr("FEES")},
        {"uptime", tr("UPTIME")},    {"signal", tr("LAST SIGNAL")}, {"updated", tr("UPDATED")}};
    for (const auto& p : kv)
        if (QLabel* k = kv_keys_.value(QLatin1String(p.first)))
            k->setText(p.second);
    const QPair<const char*, QString> rk[] = {{"qty", tr("Quantity per entry")},
                                              {"maxorder", tr("Max order value")},
                                              {"maxloss", tr("Max daily loss")},
                                              {"maxdd", tr("Max drawdown (halt)")},
                                              {"maxtrades", tr("Max trades per day")},
                                              {"window", tr("Entry window")},
                                              {"session", tr("Regular session only")},
                                              {"flatten", tr("Close position on STOP")}};
    for (const auto& p : rk)
        if (QLabel* k = risk_keys_.value(QLatin1String(p.first)))
            k->setText(p.second);

    ui::EmptyState::Spec spec;
    spec.glyph = QStringLiteral("▶");
    spec.title = tr("No strategy is running");
    spec.message = tr("Deployed strategies trade here with live P&L, open position, fills and the signal log of each "
                      "deployment — paper or live.");
    spec.details = ui::EmptyState::bullet_rows(
        {tr("Paper deployments simulate fills on live prices — no broker account needed."),
         tr("Live deployments need a connected broker account and a confirmation."),
         tr("Every deployment can be paused, stopped or flattened from its row.")});
    spec.details_heading = tr("HOW IT WORKS");
    spec.primary = tr("DEPLOY A STRATEGY (PAPER)");
    spec.primary_tooltip = tr("Opens MY STRATEGIES — pick one and press DEPLOY");
    spec.secondary = tr("OPEN BUILDER");
    spec.illustration = true;
    spec.illustration_shape = ui::LoadingSkeleton::Shape::Table;
    empty_->apply(spec);
    loading_->set_message(tr("Loading deployments…"));
    status_bar_->set_text(QStringLiteral("data"), tr("Engine · deployments every 5 s · live ticks"));
    render_detail();
}

void DeploymentsView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DeploymentsView::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    status_bar_->start_clock();
}

void DeploymentsView::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    status_bar_->stop_clock();
}

bool DeploymentsView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == table_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && ke->modifiers() == Qt::NoModifier) {
            if (const QString id = selected_id(); !id.isEmpty())
                emit log_requested(id);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── Data ────────────────────────────────────────────────────────────────────

void DeploymentsView::set_page(Page page) {
    switch (page) {
        case Page::Loading:
            pages_->setCurrentIndex(kDvPageLoading);
            break;
        case Page::Empty:
            pages_->setCurrentIndex(kDvPageEmpty);
            break;
        case Page::Table:
            pages_->setCurrentIndex(kDvPageWork);
            break;
    }
}

void DeploymentsView::set_rows(const QVector<DeploymentsViewRow>& rows) {
    // Money columns: one currency → its unit in the header; several → ISO codes in the cells
    // and USD-normalised sort keys.
    fmt::CurrencyMix mix;
    bool any_unknown = false;
    int running = 0, live = 0;
    for (const auto& r : rows) {
        any_unknown = any_unknown || r.currency.isEmpty();
        mix.add(r.currency);
        if (r.active()) {
            ++running;
            if (r.mode == QLatin1String("live"))
                ++live;
        }
    }
    const bool mixed = mix.mixed() || (any_unknown && !mix.isEmpty());
    const QStringList heads = {tr("STATE"),
                               tr("STRATEGY"),
                               tr("SYMBOL"),
                               tr("MODE"),
                               mixed ? tr("NET P&L") : fmt::header(tr("NET P&L"), mix),
                               mixed ? tr("TODAY") : fmt::header(tr("TODAY"), mix),
                               tr("POSITION"),
                               tr("LTP"),
                               tr("TRADES"),
                               tr("WIN %"),
                               tr("LAST SIGNAL"),
                               tr("UPDATED"),
                               tr("UPTIME"),
                               tr("TREND")};
    if (mixed != mixed_) {
        mixed_ = mixed;
        model_->clear(); // every money cell changes its text
    }
    // Headers only when their text changes (this runs on every live tick).
    if (model_->headerData(kDvColPnl, Qt::Horizontal).toString() != heads.at(kDvColPnl) ||
        model_->headerData(kDvColToday, Qt::Horizontal).toString() != heads.at(kDvColToday) ||
        model_->headerData(kDvColState, Qt::Horizontal).toString() != heads.at(kDvColState))
        model_->set_headers(heads);
    const QString keep = selected_id();
    model_->upsert(rows);
    // Keep the selection by identity; else the first row.
    const int row = model_->index_of_key(keep);
    QModelIndex pick;
    if (row >= 0)
        pick = proxy_->mapFromSource(model_->index(row, 0));
    else if (proxy_->rowCount() > 0)
        pick = proxy_->index(0, 0);
    if (pick.isValid() && selected_id() != keep)
        table_->selectionModel()->setCurrentIndex(pick, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    else if (pick.isValid() && !table_->selectionModel()->hasSelection())
        table_->selectionModel()->setCurrentIndex(pick, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    list_header_->set_badge(running > 0 ? tr("%1 · %2 RUNNING").arg(rows.size()).arg(running)
                                        : QString::number(rows.size()));
    status_bar_->set_text(QStringLiteral("info"), tr("%1 deployments · %2 running · %3 live")
                                                      .arg(rows.size())
                                                      .arg(running)
                                                      .arg(live));
    render_detail();
}

QString DeploymentsView::selected_id() const {
    if (!table_->selectionModel())
        return {};
    const QModelIndexList sel = table_->selectionModel()->selectedRows();
    if (sel.isEmpty())
        return {};
    const DeploymentsViewRow* r = model_->row_at(proxy_->mapToSource(sel.first()));
    return r ? r->id : QString();
}

const DeploymentsViewRow* DeploymentsView::row(const QString& id) const {
    return id.isEmpty() ? nullptr : model_->row_for_key(id);
}

void DeploymentsView::tick() {
    table_->viewport()->update(); // ages are computed when painted
    render_detail();
}

void DeploymentsView::set_kv(const QString& key, const QString& value, kit::Tone tone, const QString& tip) {
    QLabel* l = kv_values_.value(key);
    if (!l)
        return;
    if (l->text() != value)
        l->setText(value);
    l->setToolTip(tip);
    kit::set_tone(l, tone);
}

void DeploymentsView::render_detail() {
    const DeploymentsViewRow* r = row(selected_id());
    if (!r) {
        detail_stack_->setCurrentIndex(0);
        detail_header_->set_title(tr("Detail"));
        detail_header_->set_badge(QString());
        for (QPushButton* b : {pause_btn_, stop_btn_, remove_btn_, log_btn_})
            b->setVisible(false);
        return;
    }
    detail_stack_->setCurrentIndex(1);
    detail_ccy_ = r->currency;
    detail_symbol_ = r->symbol;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    detail_header_->set_title(QStringLiteral("%1 · %2").arg(r->strategy, r->symbol));
    detail_header_->set_badge(r->mode.toUpper());

    // Actions: a running deployment pauses / resumes and stops; a stopped one is removed.
    const bool active = r->active();
    pause_btn_->setVisible(active);
    stop_btn_->setVisible(active);
    remove_btn_->setVisible(!active);
    log_btn_->setVisible(true);
    pause_btn_->setText(r->paused ? tr("RESUME") : tr("PAUSE"));
    pause_btn_->setAccessibleName(r->paused ? tr("Resume the selected deployment") : tr("Pause the selected deployment"));
    pause_btn_->setEnabled(!r->pause_pending && !r->stop_pending);
    stop_btn_->setEnabled(!r->stop_pending);
    stop_btn_->setText(r->stop_pending ? tr("STOPPING…") : tr("STOP"));

    const DvState st = dv_state(*r, now);
    state_chip_->setText(st.text);
    state_chip_->setToolTip(st.tip);
    kit::set_tone(state_chip_, st.tone);
    const bool live = r->mode == QLatin1String("live");
    mode_chip_->setText(live ? tr("LIVE") : tr("PAPER"));
    mode_chip_->setToolTip(live ? tr("Real money at the broker") : tr("Simulated fills on live prices"));
    kit::set_tone(mode_chip_, live ? kit::Tone::Danger : kit::Tone::Ok);
    side_chip_->setText(r->side == QLatin1String("SELL") ? tr("SHORT") : tr("LONG"));
    side_chip_->setToolTip(tr("Entry side"));
    tf_chip_->setText(r->timeframe.toUpper());
    tf_chip_->setToolTip(tr("Bar timeframe the rules evaluate on"));
    broker_chip_->setVisible(!r->broker_id.isEmpty());
    if (!r->broker_id.isEmpty()) {
        broker_chip_->setText(r->broker_id.toUpper() +
                              (r->broker_connected ? tr("  ● CONNECTED") : tr("  ○ OFFLINE")));
        broker_chip_->setToolTip(r->broker_connected
                                     ? tr("Broker connected — sourcing live quotes")
                                     : tr("Broker not connected — connect it in Equity Trading to get live data"));
        kit::set_tone(broker_chip_, r->broker_connected ? kit::Tone::Ok : kit::Tone::Danger);
    }
    alert_->setText(r->alert);
    alert_->setVisible(!r->alert.isEmpty());
    QString halt;
    if (r->paused)
        halt = tr("PAUSED — rules, stop-loss and take-profit are NOT monitored");
    else if (r->risk_halted)
        halt = r->halt_reason == QLatin1String("max_drawdown_limit")
                   ? tr("HALTED — max drawdown limit hit; RESUME to trade again")
                   : tr("HALTED — daily loss limit hit; resets on the next trading day");
    halt_->setText(halt);
    halt_->setVisible(!halt.isEmpty());

    set_kv(QStringLiteral("ltp"), dv_price(r->ltp, r->symbol, r->currency));
    set_kv(QStringLiteral("pnl"), fmt::signed_money(r->net_pnl, r->currency), kit::sign_tone(r->net_pnl),
           tr("Realized + unrealized, net of %1fees").arg(live ? tr("estimated ") : QString()));
    set_kv(QStringLiteral("today"), fmt::signed_money(r->today_pnl, r->currency), kit::sign_tone(r->today_pnl),
           tr("Realized P&L of the current exchange trading day"));
    set_kv(QStringLiteral("unreal"), fmt::signed_money(r->unrealized, r->currency), kit::sign_tone(r->unrealized));
    set_kv(QStringLiteral("position"), dv_position(*r));
    set_kv(QStringLiteral("trades"),
           r->trades_today > 0 ? tr("%1 (%2 today)").arg(r->trades).arg(r->trades_today) : QString::number(r->trades));
    set_kv(QStringLiteral("win"), r->trades > 0 ? fmt::pct(r->win_rate, 1, false) : kit::dash());
    set_kv(QStringLiteral("maxdd"), fmt::money(-std::abs(r->max_dd), r->currency), kit::Tone::None,
           tr("Largest fall of realized + unrealized P&L from its peak"));
    set_kv(QStringLiteral("fees"), fmt::money(r->fees, r->currency), kit::Tone::None,
           live ? tr("Estimated from the cost model") : tr("Simulated"));
    set_kv(QStringLiteral("uptime"), active && r->started_ms > 0 ? dv_span((now - r->started_ms) / 1000) : kit::dash());
    set_kv(QStringLiteral("signal"), dv_clock(r->last_signal_ms));
    kit::Tone age_tone = kit::Tone::None;
    if (active && r->last_update_ms > 0) {
        const qint64 secs = (now - r->last_update_ms) / 1000;
        age_tone = secs >= kDvDeadSecs ? kit::Tone::Danger : (secs >= kDvStaleSecs ? kit::Tone::Warn : kit::Tone::None);
    }
    set_kv(QStringLiteral("updated"), dv_updated_text(*r, now), age_tone, st.tip);

    // RISK page: the deployment's limits.
    const auto limits = fincept::algo::DeploymentRiskLimits::from_json(r->risk_limits);
    auto risk = [this](const char* key, const QString& value) {
        if (QLabel* l = risk_values_.value(QLatin1String(key)))
            l->setText(value);
    };
    const QString off = tr("off");
    risk("qty", r->quantity > 0 ? fmt::quantity(r->quantity) : kit::dash());
    risk("maxorder", r->max_order_value > 0 ? fmt::money(r->max_order_value, r->currency) : tr("no limit"));
    risk("maxloss", r->max_daily_loss > 0 ? fmt::money(r->max_daily_loss, r->currency) : tr("no limit"));
    risk("maxdd", limits.max_drawdown > 0 ? fmt::money(limits.max_drawdown, r->currency) : off);
    risk("maxtrades", limits.max_trades_per_day > 0 ? QString::number(limits.max_trades_per_day) : tr("unlimited"));
    risk("window", limits.has_window() ? tr("%1 – %2 (exchange time)").arg(limits.window_start, limits.window_end)
                                       : tr("any time"));
    risk("session", limits.regular_session_only ? tr("yes") : tr("no"));
    risk("flatten", limits.flatten_on_stop ? tr("yes (pre-ticked in the STOP confirmation)") : tr("no"));
}

void DeploymentsView::set_curves(const QHash<QString, QVector<AlgoEquityPoint>>& curves) {
    curves_ = curves;
    render_equity();
}

void DeploymentsView::render_equity() {
    using TS = ui::TimeSeriesChart;
    chart_->clear();
    if (equity_all_) {
        chart_->set_pane_visible(dd_pane_, false);
        QStringList currencies;
        int added = 0;
        for (const auto& r : model_->rows()) {
            const auto pts = curves_.value(r.id);
            if (pts.size() < 2 || added >= kDvMaxChartSeries)
                continue;
            TS::SeriesSpec s;
            s.name = QStringLiteral("%1 · %2%3")
                         .arg(r.strategy, r.symbol, r.mode == QLatin1String("live") ? QStringLiteral(" · LIVE") : QString());
            for (const auto& p : pts) {
                s.xs.append(static_cast<double>(p.t_ms));
                s.ys.append(p.equity);
            }
            s.rebase = false;
            s.id = r.id;
            const QString ccy = r.currency;
            s.formatter = [ccy](double v) { return fmt::signed_money(v, ccy); };
            if (!currencies.contains(ccy))
                currencies << ccy;
            chart_->add_series(s);
            ++added;
        }
        if (added == 0) {
            chart_->set_state(TS::State::Empty, tr("No P&L history recorded yet (snapshots are taken while a deployment "
                                                   "runs, about once a minute)."));
            return;
        }
        chart_->set_state(TS::State::Ready);
        chart_->add_hline(0.0, QString(), ui::ChartColorRole::Muted);
        if (currencies.size() == 1) {
            const QString ccy = currencies.first();
            chart_->set_value_formatter(
                TS::Axis::Left, [ccy](double v) { return fmt::money(v, ccy, fmt::Compact::FromSettings); }, 0);
        } else {
            chart_->set_value_formatter(TS::Axis::Left, [](double v) { return fmt::number(v, 0); }, 0);
        }
        chart_->set_title(currencies.size() > 1 ? tr("Mixed currencies — each line in its own currency") : QString());
        return;
    }
    chart_->set_title(QString());
    const DeploymentsViewRow* r = row(selected_id());
    const auto pts = r ? curves_.value(r->id) : QVector<AlgoEquityPoint>();
    if (!r || pts.size() < 2) {
        chart_->set_pane_visible(dd_pane_, false);
        chart_->set_state(TS::State::Empty, tr("No P&L history recorded yet for this deployment (snapshots are taken "
                                               "while it runs, about once a minute)."));
        return;
    }
    chart_->set_state(TS::State::Ready);
    chart_->set_pane_visible(dd_pane_, true);
    QVector<double> xs, ys, dds;
    double peak = pts.first().equity;
    for (const auto& p : pts) {
        xs.append(static_cast<double>(p.t_ms));
        ys.append(p.equity);
        peak = std::max(peak, p.equity);
        dds.append(p.equity - peak);
    }
    TS::SeriesSpec s;
    s.name = tr("Net P&L");
    s.xs = xs;
    s.ys = ys;
    s.style = TS::Style::Baseline;
    s.baseline = 0.0;
    s.rebase = false;
    chart_->add_series(s);
    TS::SeriesSpec u;
    u.name = tr("Drawdown");
    u.xs = xs;
    u.ys = dds;
    u.style = TS::Style::Area;
    u.color = ui::ChartColorRole::Negative;
    u.baseline = 0.0;
    u.pane = dd_pane_;
    u.rebase = false;
    chart_->add_series(u);
    const QString ccy = r->currency; // abbreviated per the user's scheme (K/M/B or lakh / crore)
    chart_->set_value_formatter(TS::Axis::Left, [ccy](double v) { return fmt::money(v, ccy, fmt::Compact::FromSettings); },
                                0);
    chart_->set_value_formatter(TS::Axis::Left, [ccy](double v) { return fmt::money(v, ccy, fmt::Compact::FromSettings); },
                                dd_pane_);
    chart_->set_pane_title(dd_pane_, tr("Drawdown"));
}

void DeploymentsView::set_history(const QString& id, const QVector<AlgoTradeRow>& trades,
                                  const QVector<AlgoEventRow>& events, bool loading) {
    if (id != history_id_) {
        fills_model_->clear();
        log_model_->clear();
    }
    history_id_ = id;
    if (loading) {
        fills_note_->setText(tr("Loading fills…"));
        log_note_->setText(tr("Loading the log…"));
        return;
    }
    fills_model_->upsert(trades);
    log_model_->upsert(events);
    fills_proxy_->resort();
    log_proxy_->resort();
    fills_note_->setText(trades.isEmpty() ? tr("No fills yet.")
                                          : tr("%1 fills, newest first (Price in the instrument's currency).")
                                                .arg(trades.size()));
    log_note_->setText(events.isEmpty() ? tr("Nothing logged yet.") : tr("%1 log lines, newest first.").arg(events.size()));
}

void DeploymentsView::set_conditions(const QString& id, const QVector<DeploymentsCondRow>& rows, qint64 as_of_ms,
                                     const QStringList& activity) {
    if (id != cond_id_)
        cond_model_->clear();
    cond_id_ = id;
    cond_model_->upsert(rows);
    if (rows.isEmpty())
        cond_note_->setText(tr("No rule status yet — it arrives with the deployment's first live update."));
    else
        cond_note_->setText(as_of_ms > 0 ? tr("As evaluated %1 (bar strategies: the last closed bar).")
                                               .arg(QDateTime::fromMSecsSinceEpoch(as_of_ms).toString(QStringLiteral("HH:mm:ss")))
                                         : tr("As evaluated on the latest tick."));
    const QString text = activity.join(QLatin1Char('\n'));
    if (activity_->toPlainText() != text)
        activity_->setPlainText(text);
    activity_->setPlaceholderText(tr("Recent activity appears here."));
}

} // namespace fincept::screens
