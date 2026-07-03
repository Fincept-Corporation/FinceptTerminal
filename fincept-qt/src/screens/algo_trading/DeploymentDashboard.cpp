// src/screens/algo_trading/DeploymentDashboard.cpp
#include "screens/algo_trading/DeploymentDashboard.h"

#include "algo_engine/AlgoEngine.h"
#include "core/currency/Currency.h"
#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTime>

#include <cmath>

// ── Shared style constants ──────────────────────────────────────────────────

namespace {

inline QString kMonoFont() {
    return QString("font-family: %1;").arg(fincept::ui::fonts::DATA_FAMILY);
}

inline QString kSectionLabel() {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                   " background: transparent; border: none;")
        .arg(fincept::ui::colors::AMBER())
        .arg(fincept::ui::fonts::TINY)
        .arg(kMonoFont());
}

// Deployment P&L is denominated in the broker's own currency (intrinsic), so we
// resolve from the broker profile. When there's no broker we fall back to the
// user's globally configured currency (Settings → General) instead of assuming $.
inline QString currency_symbol(const QString& broker_id) {
    if (broker_id.isEmpty())
        return cur::symbol();
    auto* broker = fincept::trading::BrokerRegistry::instance().get(broker_id);
    if (!broker || broker->profile().currency.isEmpty())
        return cur::symbol();
    return cur::symbol_for(broker->profile().currency);
}

} // namespace

namespace fincept::screens {

using namespace fincept::services::algo;
namespace algo_ns = fincept::algo;

// ── Constructor ─────────────────────────────────────────────────────────────

DeploymentDashboard::DeploymentDashboard(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
    LOG_INFO("AlgoTrading", "DeploymentDashboard constructed");
}

// ── Service connections ─────────────────────────────────────────────────────

void DeploymentDashboard::connect_service() {
    auto& engine = algo_ns::AlgoEngine::instance();
    connect(&engine, &algo_ns::AlgoEngine::deployments_loaded, this, &DeploymentDashboard::on_deployments_loaded);
    // Real-time per-tick updates — the card refreshes in place from these, not the
    // 5s DB poll (which only fixes the deployment list structure).
    connect(&engine, &algo_ns::AlgoEngine::live_update, this, &DeploymentDashboard::on_live_update);
    connect(&engine, &algo_ns::AlgoEngine::deployment_started, this,
            [](const QString&) { algo_ns::AlgoEngine::instance().list_deployments(); });
    connect(&engine, &algo_ns::AlgoEngine::deployment_stopped, this, [](const QString& id) {
        LOG_INFO("AlgoTrading", QString("Deployment stopped: %1").arg(id));
        algo_ns::AlgoEngine::instance().list_deployments();
    });
    connect(&engine, &algo_ns::AlgoEngine::error_occurred, this, &DeploymentDashboard::on_error);

    // Also connect to legacy service for QC deployments
    auto& svc = AlgoTradingService::instance();
    connect(&svc, &AlgoTradingService::error_occurred, this, &DeploymentDashboard::on_error);

    // Ticks the "updated Ns ago" labels once a second.
    age_timer_ = new QTimer(this);
    age_timer_->setInterval(1000);
    connect(age_timer_, &QTimer::timeout, this, &DeploymentDashboard::refresh_ages);
    age_timer_->start();
}

// ── Build summary stat card ─────────────────────────────────────────────────

static QWidget* build_stat_card(const QString& label, const QString& value, const QString& color, QLabel** out_label,
                                QWidget* parent, QLabel** out_caption = nullptr) {
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString("background: %1; border: 1px solid %2;")
                            .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_DIM()));
    card->setMinimumHeight(64);

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(2);

    auto* lbl = new QLabel(label, card);
    lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                               " background: transparent; border: none;")
                           .arg(fincept::ui::colors::TEXT_TERTIARY())
                           .arg(fincept::ui::fonts::TINY)
                           .arg(kMonoFont()));
    vl->addWidget(lbl);

    auto* val_lbl = new QLabel(value, card);
    val_lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                   " background: transparent; border: none;")
                               .arg(color)
                               .arg(fincept::ui::fonts::TITLE)
                               .arg(kMonoFont()));
    vl->addWidget(val_lbl);

    *out_label = val_lbl;
    if (out_caption)
        *out_caption = lbl;
    return card;
}

// ── Build deployment card ───────────────────────────────────────────────────

QWidget* DeploymentDashboard::build_deployment_card(const AlgoDeployment& d, QWidget* parent) {
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString("background: %1; border: 1px solid %2;")
                            .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_DIM()));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(12, 10, 12, 10);
    vl->setSpacing(6);

    // ── Top row: strategy name, symbol, timeframe, mode badge, status badge ──
    auto* top = new QHBoxLayout;
    top->setSpacing(8);

    auto* name_lbl = new QLabel(d.strategy_name, card);
    name_lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                    " background: transparent; border: none;")
                                .arg(fincept::ui::colors::AMBER())
                                .arg(fincept::ui::fonts::DATA)
                                .arg(kMonoFont()));
    top->addWidget(name_lbl);

    auto* sym_lbl = new QLabel(d.symbol, card);
    sym_lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                   " background: transparent; border: none;")
                               .arg(fincept::ui::colors::TEXT_PRIMARY())
                               .arg(fincept::ui::fonts::DATA)
                               .arg(kMonoFont()));
    top->addWidget(sym_lbl);

    auto* tf_lbl = new QLabel(d.timeframe.toUpper(), card);
    tf_lbl->setStyleSheet(QString("color: %1; font-size: %2px; %3"
                                  " padding: 2px 6px; background: rgba(8,145,178,0.08);"
                                  " border: 1px solid rgba(8,145,178,0.25);")
                              .arg(fincept::ui::colors::CYAN())
                              .arg(fincept::ui::fonts::TINY)
                              .arg(kMonoFont()));
    top->addWidget(tf_lbl);

    // Mode badge
    bool is_paper = (d.mode == "paper");
    auto* mode_badge = new QLabel(d.mode.toUpper(), card);
    mode_badge->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                      " padding: 2px 6px; background: rgba(%4,0.08);"
                                      " border: 1px solid rgba(%4,0.25);")
                                  .arg(is_paper ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::AMBER())
                                  .arg(fincept::ui::fonts::TINY)
                                  .arg(kMonoFont())
                                  .arg(is_paper ? "22,163,74" : "217,119,6"));
    top->addWidget(mode_badge);

    // Broker + connection badge — so users can see WHICH broker is wired and whether
    // it's currently connected (the data source for this deployment).
    if (!d.broker_id.isEmpty()) {
        const auto cstate = fincept::trading::AccountManager::instance().connection_state(d.broker_account_id);
        const bool connected = (cstate == fincept::trading::ConnectionState::Connected);
        auto* broker_badge = new QLabel(
            d.broker_id.toUpper() + (connected ? QStringLiteral("  ● ") : QStringLiteral("  ○ ")) +
                (connected ? tr("CONNECTED") : tr("OFFLINE")),
            card);
        const QString clr = connected ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE();
        broker_badge->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                            " padding: 2px 6px; background: transparent;"
                                            " border: 1px solid %1;")
                                        .arg(clr)
                                        .arg(fincept::ui::fonts::TINY)
                                        .arg(kMonoFont()));
        broker_badge->setToolTip(connected
                                     ? tr("Broker connected — sourcing live quotes")
                                     : tr("Broker not connected — connect it in Equity Trading to get live data"));
        top->addWidget(broker_badge);
    }

    // Entry side badge
    bool is_sell = (d.entry_side == "SELL");
    auto* side_badge = new QLabel(d.entry_side.isEmpty() ? "BUY" : d.entry_side, card);
    side_badge->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                      " padding: 2px 6px; background: rgba(%4,0.08);"
                                      " border: 1px solid rgba(%4,0.25);")
                                  .arg(is_sell ? fincept::ui::colors::NEGATIVE() : fincept::ui::colors::POSITIVE())
                                  .arg(fincept::ui::fonts::TINY)
                                  .arg(kMonoFont())
                                  .arg(is_sell ? "220,38,38" : "22,163,74"));
    top->addWidget(side_badge);

    top->addStretch();

    // Status badge with dot prefix
    QColor status_clr = deployment_status_color(d.status);
    QString status_dot;
    if (d.status == "running" || d.status == "starting")
        status_dot = "● ";
    auto* status_badge = new QLabel(status_dot + d.status.toUpper(), card);
    status_badge->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                        " padding: 2px 8px; background: rgba(%4,%5,%6,0.08);"
                                        " border: 1px solid rgba(%4,%5,%6,0.25);")
                                    .arg(status_clr.name())
                                    .arg(fincept::ui::fonts::TINY)
                                    .arg(kMonoFont())
                                    .arg(status_clr.red())
                                    .arg(status_clr.green())
                                    .arg(status_clr.blue()));
    top->addWidget(status_badge);

    vl->addLayout(top);

    // Live handles for this card — updated in place by on_live_update().
    CardHandles h;
    h.broker_id = d.broker_id;
    h.mode = d.mode;

    // "updated …" line (ticks via refresh_ages()).
    h.updated = new QLabel(tr("waiting for data…"), card);
    h.updated->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                 .arg(fincept::ui::colors::TEXT_TERTIARY())
                                 .arg(fincept::ui::fonts::TINY)
                                 .arg(kMonoFont()));
    vl->addWidget(h.updated);

    // ── Metrics row ─────────────────────────────────────────────────────────
    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(16);

    auto add_metric = [&](const QString& label, const QString& value, const QString& color,
                          int font_size, QLabel** out) {
        auto* w = new QWidget(card);
        auto* ml = new QVBoxLayout(w);
        ml->setContentsMargins(0, 0, 0, 0);
        ml->setSpacing(0);
        auto* lbl = new QLabel(label, w);
        lbl->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                               .arg(fincept::ui::colors::TEXT_TERTIARY())
                               .arg(fincept::ui::fonts::TINY)
                               .arg(kMonoFont()));
        auto* val = new QLabel(value, w);
        val->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                   " background: transparent; border: none;")
                               .arg(color)
                               .arg(font_size)
                               .arg(kMonoFont()));
        ml->addWidget(lbl);
        ml->addWidget(val);
        metrics->addWidget(w);
        if (out)
            *out = val;
    };

    const auto cs = currency_symbol(d.broker_id);
    const double total_pnl = d.total_pnl + d.unrealized_pnl;

    add_metric(tr("LTP"),
               d.current_price > 0 ? QString("%1%2").arg(cs).arg(d.current_price, 0, 'f', 2)
                                   : QStringLiteral("—"),
               fincept::ui::colors::CYAN, fincept::ui::fonts::TITLE, &h.ltp);
    add_metric(tr("P&L"), QString("%1%2%3").arg(total_pnl >= 0 ? "+" : "-", cs).arg(std::abs(total_pnl), 0, 'f', 2),
               total_pnl >= 0 ? fincept::ui::colors::POSITIVE : fincept::ui::colors::NEGATIVE,
               fincept::ui::fonts::TITLE, &h.pnl);
    add_metric(tr("WIN RATE"), QString("%1%").arg(d.win_rate, 0, 'f', 1), fincept::ui::colors::TEXT_PRIMARY,
               fincept::ui::fonts::SMALL, nullptr);
    add_metric(tr("POSITION"), tr("FLAT"), fincept::ui::colors::TEXT_TERTIARY, fincept::ui::fonts::SMALL, &h.position);
    add_metric(tr("TRADES"), QString::number(d.total_trades), fincept::ui::colors::TEXT_PRIMARY,
               fincept::ui::fonts::SMALL, &h.trades);
    add_metric(tr("MAX DD"), QString("-%1%").arg(std::abs(d.max_drawdown), 0, 'f', 2), fincept::ui::colors::NEGATIVE,
               fincept::ui::fonts::SMALL, nullptr);

    metrics->addStretch();
    vl->addLayout(metrics);

    // ── Condition status (filled live by on_live_update) ─────────────────────
    auto* cond_title = new QLabel(tr("CONDITIONS"), card);
    cond_title->setStyleSheet(kSectionLabel());
    vl->addWidget(cond_title);
    auto* cond_box = new QWidget(card);
    h.conditions = new QVBoxLayout(cond_box);
    h.conditions->setContentsMargins(0, 0, 0, 0);
    h.conditions->setSpacing(2);
    vl->addWidget(cond_box);

    // ── Activity log (last few events) ───────────────────────────────────────
    h.activity = new QLabel(card);
    h.activity->setWordWrap(true);
    h.activity->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                  .arg(fincept::ui::colors::TEXT_TERTIARY())
                                  .arg(fincept::ui::fonts::TINY)
                                  .arg(kMonoFont()));
    vl->addWidget(h.activity);

    cards_.insert(d.id, h);

    // ── Win rate progress bar ────────────────────────────────────────────────
    auto* win_bar = new QProgressBar(card);
    win_bar->setFixedHeight(4);
    win_bar->setRange(0, 100);
    win_bar->setValue(static_cast<int>(d.win_rate));
    win_bar->setTextVisible(false);
    win_bar->setStyleSheet(
        QString("QProgressBar { background: %1; border: none; border-radius: 2px; }"
                "QProgressBar::chunk { background: %2; border-radius: 2px; }")
            .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::POSITIVE()));
    vl->addWidget(win_bar);

    // ── Error message if any ────────────────────────────────────────────────
    if (!d.error_message.isEmpty()) {
        auto* err = new QLabel(d.error_message, card);
        err->setWordWrap(true);
        err->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: rgba(220,38,38,0.05);"
                                   " border: none; padding: 4px;")
                               .arg(fincept::ui::colors::NEGATIVE())
                               .arg(fincept::ui::fonts::TINY)
                               .arg(kMonoFont()));
        vl->addWidget(err);
    }

    // ── Stop button ─────────────────────────────────────────────────────────
    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();

    if (d.status == "running" || d.status == "starting") {
        auto* stop_btn = new QPushButton(tr("STOP"), card);
        stop_btn->setCursor(Qt::PointingHandCursor);
        stop_btn->setFixedHeight(26);
        stop_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                                        " font-size: %2px; font-weight: 700; %3 padding: 2px 16px; }"
                                        "QPushButton:hover { background: rgba(220,38,38,0.1); }")
                                    .arg(fincept::ui::colors::NEGATIVE())
                                    .arg(fincept::ui::fonts::TINY)
                                    .arg(kMonoFont()));
        connect(stop_btn, &QPushButton::clicked, card, [dep_id = d.id]() {
            algo_ns::AlgoEngine::instance().stop_deployment(dep_id);
            LOG_INFO("AlgoTrading", QString("Stop requested: %1").arg(dep_id));
        });
        btn_row->addWidget(stop_btn);
    } else {
        auto* remove_btn = new QPushButton(tr("REMOVE"), card);
        remove_btn->setCursor(Qt::PointingHandCursor);
        remove_btn->setFixedHeight(26);
        remove_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                                          " font-size: %2px; font-weight: 700; %3 padding: 2px 16px; }"
                                          "QPushButton:hover { background: rgba(120,120,120,0.1); }")
                                      .arg(fincept::ui::colors::TEXT_TERTIARY())
                                      .arg(fincept::ui::fonts::TINY)
                                      .arg(kMonoFont()));
        connect(remove_btn, &QPushButton::clicked, card, [dep_id = d.id]() {
            algo_ns::AlgoEngine::instance().remove_deployment(dep_id);
            LOG_INFO("AlgoTrading", QString("Remove requested: %1").arg(dep_id));
        });
        btn_row->addWidget(remove_btn);
    }

    vl->addLayout(btn_row);

    return card;
}

// ── Update summary stats ────────────────────────────────────────────────────

void DeploymentDashboard::update_summary(const QVector<AlgoDeployment>& deployments) {

    int active = 0;
    double pnl_sum = 0;
    int trades_sum = 0;
    double win_rate_sum = 0;
    int win_rate_count = 0;

    for (const auto& d : deployments) {
        if (d.status == "running" || d.status == "starting")
            ++active;
        pnl_sum += d.total_pnl + d.unrealized_pnl;
        trades_sum += d.total_trades;
        if (d.total_trades > 0) {
            win_rate_sum += d.win_rate;
            ++win_rate_count;
        }
    }

    double avg_wr = (win_rate_count > 0) ? (win_rate_sum / win_rate_count) : 0;

    if (active_count_)
        active_count_->setText(QString::number(active));

    if (total_pnl_) {
        QString sum_cs = deployments.isEmpty() ? cur::symbol() : currency_symbol(deployments.first().broker_id);
        total_pnl_->setText(QString("%1%2%3").arg(pnl_sum >= 0 ? "+" : "-", sum_cs).arg(std::abs(pnl_sum), 0, 'f', 2));
        total_pnl_->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                          " background: transparent; border: none;")
                                      .arg(pnl_sum >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE())
                                      .arg(fincept::ui::fonts::TITLE)
                                      .arg(kMonoFont()));
    }

    if (total_trades_)
        total_trades_->setText(QString::number(trades_sum));

    if (avg_win_rate_)
        avg_win_rate_->setText(QString("%1%").arg(avg_wr, 0, 'f', 1));
}

// ── Build UI ────────────────────────────────────────────────────────────────

void DeploymentDashboard::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Scroll area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea { background: %1; border: none; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::BORDER_MED()));

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background: %1;").arg(fincept::ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(12);

    // ── Summary stat cards ──────────────────────────────────────────────────
    auto* summary_row = new QHBoxLayout;
    summary_row->setSpacing(8);

    summary_row->addWidget(build_stat_card(tr("ACTIVE DEPLOYMENTS"), "0", fincept::ui::colors::CYAN, &active_count_,
                                           content, &active_caption_));
    summary_row->addWidget(build_stat_card(tr("TOTAL P&L"), "0.00", fincept::ui::colors::POSITIVE, &total_pnl_,
                                           content, &total_pnl_caption_));
    summary_row->addWidget(build_stat_card(tr("TOTAL TRADES"), "0", fincept::ui::colors::TEXT_PRIMARY, &total_trades_,
                                           content, &total_trades_caption_));
    summary_row->addWidget(build_stat_card(tr("AVG WIN RATE"), "0.0%", fincept::ui::colors::TEXT_PRIMARY, &avg_win_rate_,
                                           content, &avg_win_rate_caption_));

    vl->addLayout(summary_row);

    // ── Equity curve placeholder ─────────────────────────────────────────────
    equity_placeholder_ = new QFrame(content);
    equity_placeholder_->setFixedHeight(140);
    equity_placeholder_->setFrameShape(QFrame::NoFrame);
    equity_placeholder_->setStyleSheet(
        QString("QFrame { background: %1; border: 1px solid %2; }")
            .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_DIM()));
    {
        auto* eq_vl = new QVBoxLayout(equity_placeholder_);
        eq_vl->setContentsMargins(12, 8, 12, 8);
        eq_vl->setSpacing(4);

        auto* eq_header_row = new QHBoxLayout;
        eq_title_ = new QLabel(tr("EQUITY CURVE"), equity_placeholder_);
        eq_title_->setStyleSheet(kSectionLabel());
        eq_header_row->addWidget(eq_title_);
        eq_header_row->addStretch();
        eq_vl->addLayout(eq_header_row);

        eq_vl->addStretch();

        eq_hint_ = new QLabel(tr("Equity curve — not yet available"), equity_placeholder_);
        eq_hint_->setAlignment(Qt::AlignCenter);
        eq_hint_->setStyleSheet(
            QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                .arg(fincept::ui::colors::TEXT_TERTIARY())
                .arg(fincept::ui::fonts::SMALL)
                .arg(kMonoFont()));
        eq_vl->addWidget(eq_hint_);
        eq_vl->addStretch();
    }
    vl->addWidget(equity_placeholder_);

    // ── Control bar ─────────────────────────────────────────────────────────
    auto* control_bar = new QHBoxLayout;
    control_bar->setSpacing(8);

    refresh_btn_ = new QPushButton(tr("REFRESH"), content);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setFixedHeight(30);
    refresh_btn_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                " font-size: %4px; font-weight: 700; %5 padding: 4px 16px; }"
                "QPushButton:hover { background: %6; color: %7; }")
            .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::TEXT_SECONDARY(), fincept::ui::colors::BORDER_DIM())
            .arg(fincept::ui::fonts::TINY)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::BG_HOVER(), fincept::ui::colors::TEXT_PRIMARY()));
    connect(refresh_btn_, &QPushButton::clicked, this, []() { algo_ns::AlgoEngine::instance().list_deployments(); });
    control_bar->addWidget(refresh_btn_);

    control_bar->addStretch();

    stop_all_btn_ = new QPushButton(tr("STOP ALL"), content);
    stop_all_btn_->setCursor(Qt::PointingHandCursor);
    stop_all_btn_->setFixedHeight(30);
    stop_all_btn_->setStyleSheet(
        QString("QPushButton { background: rgba(220,38,38,0.1); color: %1; border: 1px solid %1;"
                " font-size: %2px; font-weight: 700; %3 padding: 4px 16px; }"
                "QPushButton:hover { background: %1; color: %4; }")
            .arg(fincept::ui::colors::NEGATIVE())
            .arg(fincept::ui::fonts::TINY)
            .arg(kMonoFont())
            .arg(fincept::ui::colors::TEXT_PRIMARY()));
    connect(stop_all_btn_, &QPushButton::clicked, this, []() {
        algo_ns::AlgoEngine::instance().stop_all();
        LOG_INFO("AlgoTrading", "Stop all deployments requested");
    });
    control_bar->addWidget(stop_all_btn_);

    vl->addLayout(control_bar);

    // ── Deployments section ─────────────────────────────────────────────────
    dep_title_ = new QLabel(tr("DEPLOYMENTS"), content);
    dep_title_->setStyleSheet(kSectionLabel());
    vl->addWidget(dep_title_);

    // Status label
    status_label_ = new QLabel(tr("No deployments loaded."), content);
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                     .arg(fincept::ui::colors::TEXT_TERTIARY())
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(kMonoFont()));
    vl->addWidget(status_label_);

    // Deployments container
    auto* dep_container = new QWidget(content);
    deployments_layout_ = new QVBoxLayout(dep_container);
    deployments_layout_->setContentsMargins(0, 0, 0, 0);
    deployments_layout_->setSpacing(8);
    vl->addWidget(dep_container);

    vl->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void DeploymentDashboard::on_deployments_loaded(QVector<AlgoDeployment> deployments) {
    // This is the STRUCTURAL refresh (5s DB poll). Live values come from
    // on_live_update(); we must NOT tear down/rebuild cards here when the set of
    // deployments is unchanged — doing so blanked every card to its (stale, zero)
    // DB values for a moment between live snapshots, which read as a 1s blink.
    update_summary(deployments);
    deployment_count_ = deployments.size();

    if (deployments.isEmpty()) {
        status_label_->setText(tr("No active deployments."));
        status_label_->setVisible(true);
    } else {
        status_label_->setText(tr("%1 deployment(s)").arg(deployments.size()));
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::TEXT_SECONDARY())
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
    }

    // Signature includes status so a running→stopped transition (e.g. after STOP)
    // forces a rebuild — the card then swaps RUNNING/STOP → STOPPED/REMOVE. Without
    // status here, stopping a deployment left the card frozen on STOP (looked broken).
    QStringList new_sig;
    new_sig.reserve(deployments.size());
    for (const auto& d : deployments)
        new_sig << (d.id + QLatin1Char('|') + d.status);

    // Same deployments AND same statuses → keep the live-updated cards untouched.
    if (new_sig == card_order_ && new_sig.size() == cards_.size())
        return;

    // Structure changed (deploy/remove) → rebuild the card list. Old widgets are
    // deleted, so drop their handles first.
    cards_.clear();
    card_order_.clear();
    while (deployments_layout_->count() > 0) {
        auto* item = deployments_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    for (const auto& d : deployments) {
        deployments_layout_->addWidget(build_deployment_card(d, nullptr));
        card_order_.append(d.id + QLatin1Char('|') + d.status);
    }

    LOG_INFO("AlgoTrading", QString("Dashboard rebuilt: %1 deployments").arg(deployments.size()));
}

void DeploymentDashboard::on_live_update(const QString& deployment_id,
                                         const fincept::algo::AlgoLiveSnapshot& snap) {
    auto it = cards_.find(deployment_id);
    if (it == cards_.end())
        return; // structure not built yet — next list_deployments() will add it
    CardHandles& h = it.value();
    h.last_update_ms = snap.last_update_ms;

    const QString cs = currency_symbol(h.broker_id);
    const auto& m = snap.metrics;

    if (h.ltp)
        h.ltp->setText(QString("%1%2").arg(cs).arg(snap.current_price, 0, 'f', 2));

    if (h.pnl) {
        const double total = m.total_pnl + m.unrealized_pnl;
        h.pnl->setText(QString("%1%2%3").arg(total >= 0 ? "+" : "-", cs).arg(std::abs(total), 0, 'f', 2));
        h.pnl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3"
                                     " background: transparent; border: none;")
                                 .arg(total >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE())
                                 .arg(fincept::ui::fonts::TITLE)
                                 .arg(kMonoFont()));
    }

    if (h.position) {
        if (m.current_position_qty != 0)
            h.position->setText(QString("%1 %2 @ %3")
                                    .arg(m.current_position_qty, 0, 'f', 0)
                                    .arg(m.current_position_side.toUpper())
                                    .arg(m.current_position_entry, 0, 'f', 2));
        else
            h.position->setText(tr("FLAT"));
    }
    if (h.trades)
        h.trades->setText(QString::number(m.total_trades));

    // Update the per-condition status rows IN PLACE (only rebuild if the count
    // changed) so they don't flicker every tick.
    if (h.conditions) {
        const int n = static_cast<int>(snap.conditions.size());
        const bool same_count = (h.conditions->count() == n);
        if (!same_count) {
            while (h.conditions->count() > 0) {
                auto* item = h.conditions->takeAt(0);
                if (item->widget())
                    item->widget()->deleteLater();
                delete item;
            }
        }
        const bool in_position = (m.current_position_qty != 0);
        for (int i = 0; i < n; ++i) {
            const auto& c = snap.conditions[i];
            // GREEN = currently met OR already executed. Entry rows stay green once a
            // position is open (they matched & fired) even if the live price has since
            // moved off the level — showing them grey there confused users.
            const bool executed = (c.section == QLatin1String("entry") && in_position);
            const bool green = c.met || executed;

            QString hint;
            if (!green) {
                if (c.op == "crosses_above" && c.computed >= c.target)
                    hint = tr("  ⚠ already above — can't cross up");
                else if (c.op == "crosses_below" && c.computed <= c.target)
                    hint = tr("  ⚠ already below — can't cross down");
            }
            const QString suffix = (executed && !c.met) ? tr("  · executed") : QString();

            const QString tick = green ? QStringLiteral("✓") : QStringLiteral("✗");
            const QString clr = green ? fincept::ui::colors::POSITIVE()
                                      : (hint.isEmpty() ? fincept::ui::colors::TEXT_SECONDARY()
                                                        : fincept::ui::colors::WARNING());
            const QString text = QString("%1  %2   now %3%4%5")
                                     .arg(tick, c.label).arg(c.computed, 0, 'f', 2).arg(hint, suffix);
            const QString ss = QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                   .arg(clr)
                                   .arg(fincept::ui::fonts::TINY)
                                   .arg(kMonoFont());
            QLabel* row = same_count ? qobject_cast<QLabel*>(h.conditions->itemAt(i)->widget()) : nullptr;
            if (row) {
                row->setText(text);
                row->setStyleSheet(ss);
            } else {
                row = new QLabel(text, nullptr);
                row->setStyleSheet(ss);
                h.conditions->addWidget(row);
            }
        }
    }

    // Append activity note (keep last 4).
    if (h.activity && !snap.note.isEmpty()) {
        const QString ts = QTime::currentTime().toString("HH:mm:ss");
        h.activity_lines.prepend(QString("%1  %2").arg(ts, snap.note));
        while (h.activity_lines.size() > 4)
            h.activity_lines.removeLast();
        h.activity->setText(h.activity_lines.join("\n"));
    }

    refresh_ages();
}

void DeploymentDashboard::refresh_ages() {
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = cards_.begin(); it != cards_.end(); ++it) {
        CardHandles& h = it.value();
        if (!h.updated)
            continue;
        if (h.last_update_ms <= 0) {
            h.updated->setText(tr("waiting for data…"));
            continue;
        }
        const int secs = static_cast<int>((now - h.last_update_ms) / 1000);
        h.updated->setText(secs <= 1 ? tr("updated just now") : tr("updated %1s ago").arg(secs));
    }
}

void DeploymentDashboard::on_error(const QString& context, const QString& msg) {
    if (status_label_) {
        status_label_->setText(tr("Error [%1]: %2").arg(context, msg));
        status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent; border: none;")
                                         .arg(fincept::ui::colors::NEGATIVE())
                                         .arg(fincept::ui::fonts::SMALL)
                                         .arg(kMonoFont()));
    }
}

// ── Live language switch ──────────────────────────────────────────────────────

void DeploymentDashboard::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DeploymentDashboard::retranslateUi() {
    if (active_caption_)        active_caption_->setText(tr("ACTIVE DEPLOYMENTS"));
    if (total_pnl_caption_)     total_pnl_caption_->setText(tr("TOTAL P&L"));
    if (total_trades_caption_)  total_trades_caption_->setText(tr("TOTAL TRADES"));
    if (avg_win_rate_caption_)  avg_win_rate_caption_->setText(tr("AVG WIN RATE"));
    if (eq_title_)   eq_title_->setText(tr("EQUITY CURVE"));
    if (eq_hint_)    eq_hint_->setText(tr("Equity curve — not yet available"));
    if (refresh_btn_)  refresh_btn_->setText(tr("REFRESH"));
    if (stop_all_btn_) stop_all_btn_->setText(tr("STOP ALL"));
    if (dep_title_)    dep_title_->setText(tr("DEPLOYMENTS"));

    // Status label reflects last load state.
    if (status_label_)
        status_label_->setText(deployment_count_ > 0 ? tr("%1 deployment(s)").arg(deployment_count_)
                                                      : tr("No active deployments."));
    // Per-deployment cards (metric captions, badges, STOP/REMOVE) rebuild on the
    // next poll/refresh — they pick up the new language then.
}

} // namespace fincept::screens
