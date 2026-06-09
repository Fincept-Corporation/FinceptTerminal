// src/screens/algo_trading/UniverseScannerPanel.cpp
#include "screens/algo_trading/UniverseScannerPanel.h"

#include "algo_engine/AlgoEngine.h"
#include "algo_engine/ScanMonitor.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/AlgoDeployDialog.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "storage/repositories/ScanWatchRepository.h"
#include "trading/AccountManager.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::algo;

namespace {
QString uMono() { return QString("font-family: %1;").arg(fincept::ui::fonts::DATA_FAMILY); }
QString uLabel() {
    return QString("color: %1; font-size: %2px; font-weight: 700; letter-spacing: 0.5px; %3"
                   " background: transparent; border: none;")
        .arg(fincept::ui::colors::TEXT_SECONDARY())
        .arg(fincept::ui::fonts::TINY)
        .arg(uMono());
}
QString uCombo() {
    return QString("QComboBox { background: %1; color: %2; border: 1px solid %3; padding: 4px 8px;"
                   " font-size: %4px; %5 }"
                   "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3;"
                   " selection-background-color: %6; %5 }")
        .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::TEXT_PRIMARY(),
             fincept::ui::colors::BORDER_DIM())
        .arg(fincept::ui::fonts::SMALL)
        .arg(uMono())
        .arg(fincept::ui::colors::BG_HOVER());
}
} // namespace

UniverseScannerPanel::UniverseScannerPanel(QWidget* parent) : QWidget(parent) {
    build_ui();

    connect(&AlgoTradingService::instance(), &AlgoTradingService::strategies_loaded,
            this, &UniverseScannerPanel::on_strategies_loaded);
    connect(&fincept::algo::ScanMonitor::instance(), &fincept::algo::ScanMonitor::realtime_match,
            this, &UniverseScannerPanel::on_match);

    // Purge orphaned realtime watches left over from a prior session. ScanMonitor
    // intentionally skips mode=='realtime' watches at launch (they are started on
    // demand here), so any active realtime row is a stale leftover that would
    // otherwise accumulate silently.
    if (auto active = fincept::ScanWatchRepository::instance().list_active(); active.is_ok())
        for (const auto& w : active.value())
            if (w.mode == QStringLiteral("realtime"))
                fincept::ScanWatchRepository::instance().remove(w.id);

    AlgoTradingService::instance().list_strategies();
    LOG_INFO("AlgoTrading", "UniverseScannerPanel constructed");
}

void UniverseScannerPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background: %1;").arg(fincept::ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(8);

    // Strategy picker
    auto* strat_lbl = new QLabel(tr("STRATEGY"), content);
    strat_lbl->setStyleSheet(uLabel());
    vl->addWidget(strat_lbl);
    strategy_combo_ = new QComboBox(content);
    strategy_combo_->setStyleSheet(uCombo());
    strategy_combo_->setFixedHeight(30);
    vl->addWidget(strategy_combo_);

    // Universe selector
    auto* uni_lbl = new QLabel(tr("UNIVERSE"), content);
    uni_lbl->setStyleSheet(uLabel());
    vl->addWidget(uni_lbl);
    universe_combo_ = new QComboBox(content);
    universe_combo_->setStyleSheet(uCombo());
    universe_combo_->setFixedHeight(30);
    universe_combo_->addItem(tr("All NSE Equity"), "NSE_EQ");
    universe_combo_->addItem(tr("All BSE Equity"), "BSE_EQ");
    universe_combo_->addItem(tr("NIFTY 50"), "NIFTY50");
    universe_combo_->addItem(tr("Custom symbols"), "CUSTOM");
    vl->addWidget(universe_combo_);
    connect(universe_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UniverseScannerPanel::on_universe_changed);

    // Custom symbols (hidden unless Custom)
    symbols_label_ = new QLabel(tr("CUSTOM SYMBOLS"), content);
    symbols_label_->setStyleSheet(uLabel());
    vl->addWidget(symbols_label_);
    symbols_input_ = new fincept::ui::algo::SymbolChipInput(content);
    symbols_input_->setMinimumHeight(48);
    vl->addWidget(symbols_input_);
    symbols_label_->setVisible(false);
    symbols_input_->setVisible(false);

    // Broker account (required — realtime needs a live feed)
    auto* acct_lbl = new QLabel(tr("BROKER ACCOUNT"), content);
    acct_lbl->setStyleSheet(uLabel());
    vl->addWidget(acct_lbl);
    account_combo_ = new QComboBox(content);
    account_combo_->setStyleSheet(uCombo());
    account_combo_->setFixedHeight(30);
    for (const auto& acct : fincept::trading::AccountManager::instance().list_accounts())
        account_combo_->addItem(QString("%1 (%2)").arg(acct.display_name, acct.broker_id),
                                acct.account_id);
    vl->addWidget(account_combo_);

    // Sweep + cooldown row
    auto* knobs = new QHBoxLayout;
    knobs->setSpacing(12);
    auto* sweep_lbl = new QLabel(tr("EVAL SWEEP (ms)"), content);
    sweep_lbl->setStyleSheet(uLabel());
    sweep_spin_ = new QSpinBox(content);
    sweep_spin_->setRange(200, 5000);
    sweep_spin_->setSingleStep(100);
    sweep_spin_->setValue(500);
    sweep_spin_->setFixedHeight(30);
    sweep_spin_->setStyleSheet(uCombo());
    auto* cd_lbl = new QLabel(tr("COOLDOWN (min)"), content);
    cd_lbl->setStyleSheet(uLabel());
    cooldown_spin_ = new QSpinBox(content);
    cooldown_spin_->setRange(0, 240);
    cooldown_spin_->setValue(5);
    cooldown_spin_->setFixedHeight(30);
    cooldown_spin_->setStyleSheet(uCombo());
    knobs->addWidget(sweep_lbl);
    knobs->addWidget(sweep_spin_);
    knobs->addWidget(cd_lbl);
    knobs->addWidget(cooldown_spin_);
    knobs->addStretch();
    vl->addLayout(knobs);

    // Start/Stop
    start_btn_ = new QPushButton(tr("START SCAN"), content);
    start_btn_->setCursor(Qt::PointingHandCursor);
    start_btn_->setFixedHeight(38);
    start_btn_->setStyleSheet(QString("QPushButton { background: rgba(167,139,250,0.12); color: #A78BFA;"
                                      " border: 1px solid #A78BFA; font-size: %1px; font-weight: 700; %2"
                                      " padding: 6px 24px; } QPushButton:hover { background: #A78BFA;"
                                      " color: %3; }")
                                  .arg(fincept::ui::fonts::DATA)
                                  .arg(uMono())
                                  .arg(fincept::ui::colors::BG_BASE()));
    connect(start_btn_, &QPushButton::clicked, this, &UniverseScannerPanel::on_start_stop);
    vl->addWidget(start_btn_);

    status_label_ = new QLabel("", content);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent;")
                                     .arg(fincept::ui::colors::TEXT_TERTIARY())
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(uMono()));
    vl->addWidget(status_label_);

    // Matches
    auto* m_lbl = new QLabel(tr("LIVE MATCHES"), content);
    m_lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 700; %3")
                             .arg(fincept::ui::colors::AMBER())
                             .arg(fincept::ui::fonts::TINY)
                             .arg(uMono()));
    vl->addWidget(m_lbl);

    matches_table_ = new QTableWidget(0, 5, content);
    matches_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("SIGNAL"), tr("PRICE"), tr("MATCHED RULES"), tr("DEPLOY")});
    matches_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    matches_table_->setColumnWidth(0, 110);
    matches_table_->setColumnWidth(1, 90);
    matches_table_->setColumnWidth(2, 90);
    matches_table_->setColumnWidth(4, 90);
    matches_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    matches_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    matches_table_->verticalHeader()->setVisible(false);
    matches_table_->setMinimumHeight(200);
    matches_table_->setStyleSheet(
        QString("QTableWidget { background: %1; border: 1px solid %2; gridline-color: %2;"
                " font-size: %3px; font-family: %4; color: %5; }"
                "QHeaderView::section { background: %6; color: %7; font-size: %3px;"
                " font-weight: 700; font-family: %4; padding: 4px 8px; border: none;"
                " border-bottom: 1px solid %2; }")
            .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::BORDER_DIM())
            .arg(fincept::ui::fonts::SMALL)
            .arg(fincept::ui::fonts::DATA_FAMILY)
            .arg(fincept::ui::colors::TEXT_PRIMARY())
            .arg(fincept::ui::colors::BG_RAISED())
            .arg(fincept::ui::colors::TEXT_TERTIARY()));
    vl->addWidget(matches_table_);
    vl->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);
}

void UniverseScannerPanel::set_status(const QString& text, const QString& color) {
    status_label_->setText(text);
    status_label_->setStyleSheet(QString("color: %1; font-size: %2px; %3 background: transparent;")
                                     .arg(color)
                                     .arg(fincept::ui::fonts::SMALL)
                                     .arg(uMono()));
}

void UniverseScannerPanel::on_universe_changed(int) {
    const bool custom = universe_combo_->currentData().toString() == "CUSTOM";
    symbols_label_->setVisible(custom);
    symbols_input_->setVisible(custom);
}

void UniverseScannerPanel::on_strategies_loaded(QVector<AlgoStrategy> strategies) {
    strategies_ = strategies;
    const QString prev = strategy_combo_->currentData().toString();
    strategy_combo_->clear();
    for (const auto& s : strategies_)
        strategy_combo_->addItem(s.name, s.id);
    const int idx = strategy_combo_->findData(prev);
    if (idx >= 0)
        strategy_combo_->setCurrentIndex(idx);
}

bool UniverseScannerPanel::current_strategy(AlgoStrategy* out) const {
    const QString id = strategy_combo_->currentData().toString();
    for (const auto& s : strategies_) {
        if (s.id == id) {
            *out = s;
            return true;
        }
    }
    return false;
}

void UniverseScannerPanel::on_start_stop() {
    if (running_) {
        // Stop: tear down the runner and remove the persisted config (alert history
        // in scan_watch_events is kept).
        fincept::algo::ScanMonitor::instance().stop_watch(watch_id_);
        if (!watch_id_.isEmpty())
            fincept::ScanWatchRepository::instance().remove(watch_id_);
        watch_id_.clear();
        running_ = false;
        start_btn_->setText(tr("START SCAN"));
        set_status(tr("Stopped."), fincept::ui::colors::TEXT_TERTIARY());
        return;
    }

    AlgoStrategy strat;
    if (!current_strategy(&strat)) {
        set_status(tr("Select a strategy first."), fincept::ui::colors::NEGATIVE());
        return;
    }
    if (strat.entry_conditions.isEmpty()) {
        set_status(tr("The selected strategy has no entry conditions."),
                   fincept::ui::colors::NEGATIVE());
        return;
    }
    const QString account_id = account_combo_->currentData().toString();
    if (account_id.isEmpty()) {
        set_status(tr("Select a connected broker account (live feed required)."),
                   fincept::ui::colors::NEGATIVE());
        return;
    }

    const QString universe = universe_combo_->currentData().toString();
    const QString tf = strat.timeframe.isEmpty() ? QStringLiteral("5m") : strat.timeframe;

    fincept::ScanWatch w;
    w.name = QString("Universe: %1").arg(strat.name);
    w.conditions = strat.entry_conditions;
    w.logic = strat.entry_logic;
    w.universe = universe;
    w.symbols = (universe == "CUSTOM") ? symbols_input_->symbols() : QStringList();
    w.timeframe = tf;
    w.lookback_days = algo_default_lookback_days(tf == "live" ? QStringLiteral("1m") : tf);
    w.data_source = QStringLiteral("Broker");
    w.account_id = account_id;
    w.broker_id = fincept::trading::AccountManager::instance().get_account(account_id).broker_id;
    w.mode = QStringLiteral("realtime");
    w.cooldown_min = cooldown_spin_->value();
    QJsonObject actions;
    actions["toast"] = true;
    actions["providers"] = false;
    actions["sweep_ms"] = sweep_spin_->value();
    actions["strategy_id"] = strat.id;
    w.actions = actions;
    w.active = true;

    if (universe == "CUSTOM" && w.symbols.isEmpty()) {
        set_status(tr("Enter custom symbols or pick a universe."),
                   fincept::ui::colors::NEGATIVE());
        return;
    }

    auto r = fincept::ScanWatchRepository::instance().create(w);
    if (r.is_err()) {
        set_status(tr("Could not save scan: %1").arg(QString::fromStdString(r.error())),
                   fincept::ui::colors::NEGATIVE());
        return;
    }
    watch_id_ = r.value().id;
    matches_table_->setRowCount(0);
    fincept::algo::ScanMonitor::instance().reload(watch_id_);

    running_ = true;
    start_btn_->setText(tr("STOP SCAN"));
    set_status(tr("Scanning live — warming history, matches will appear below."),
               "#A78BFA");
}

void UniverseScannerPanel::on_match(const QString& watch_id, const QString& symbol,
                                    double price, const QString& detail) {
    if (watch_id != watch_id_ || watch_id_.isEmpty())
        return;

    const int row = matches_table_->rowCount();
    matches_table_->insertRow(row);

    auto* sym_item = new QTableWidgetItem(symbol);
    sym_item->setForeground(QColor(fincept::ui::colors::AMBER()));
    matches_table_->setItem(row, 0, sym_item);

    // Bullish/bearish from the detail's operators (best-effort).
    QString sig = tr("MATCH"), sig_color = fincept::ui::colors::TEXT_SECONDARY();
    if (detail.contains('>') || detail.contains("crosses_above")) {
        sig = tr("BULLISH");
        sig_color = fincept::ui::colors::POSITIVE();
    } else if (detail.contains('<') || detail.contains("crosses_below")) {
        sig = tr("BEARISH");
        sig_color = fincept::ui::colors::NEGATIVE();
    }
    auto* sig_item = new QTableWidgetItem(sig);
    sig_item->setForeground(QColor(sig_color));
    matches_table_->setItem(row, 1, sig_item);

    matches_table_->setItem(row, 2,
                            new QTableWidgetItem(price > 0 ? QString::number(price, 'f', 2)
                                                           : QStringLiteral("—")));
    matches_table_->setItem(row, 3, new QTableWidgetItem(detail));

    auto* deploy_btn = new QPushButton(tr("DEPLOY"), matches_table_);
    deploy_btn->setCursor(Qt::PointingHandCursor);
    deploy_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1;"
                                      " border: 1px solid %2; font-size: %3px; %4 padding: 2px 8px; }"
                                      "QPushButton:hover { color: %5; border-color: %5; }")
                                  .arg(fincept::ui::colors::TEXT_SECONDARY(),
                                       fincept::ui::colors::BORDER_DIM())
                                  .arg(fincept::ui::fonts::TINY)
                                  .arg(uMono())
                                  .arg(fincept::ui::colors::POSITIVE()));
    connect(deploy_btn, &QPushButton::clicked, this, [this, symbol]() { deploy_symbol(symbol); });
    matches_table_->setCellWidget(row, 4, deploy_btn);

    matches_table_->scrollToBottom();
}

void UniverseScannerPanel::deploy_symbol(const QString& symbol) {
    AlgoStrategy strat;
    if (!current_strategy(&strat)) {
        set_status(tr("Strategy no longer selected — cannot deploy."),
                   fincept::ui::colors::NEGATIVE());
        return;
    }
    auto* dlg = new AlgoDeployDialog(strat, this);
    dlg->set_symbol(symbol);
    if (dlg->exec() == QDialog::Accepted) {
        auto dep = dlg->deployment();
        AlgoTradingService::instance().save_strategy(strat);
        fincept::algo::AlgoEngine::instance().start_deployment(dep, strat);
        set_status(tr("Deployed %1.").arg(symbol), fincept::ui::colors::POSITIVE());
    }
    dlg->deleteLater();
}

} // namespace fincept::screens
