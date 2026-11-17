// src/screens/algo_trading/StrategyListPanel.cpp
#include "screens/algo_trading/StrategyListPanel.h"

#include "algo_engine/AlgoEngine.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/AlgoDeployDialog.h"
#include "screens/algo_trading/StrategyBuilderPanel.h"
#include "screens/algo_trading/StrategyLibraryView.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/algo/AlgoCurrency.h"

#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::algo;

namespace {

/// The strategy as a portable JSON document (everything the Builder needs to recreate it).
QJsonObject slp_strategy_json(const AlgoStrategy& s) {
    QJsonObject o;
    o["id"] = s.id;
    o["name"] = s.name;
    o["description"] = s.description;
    o["timeframe"] = s.timeframe;
    o["instrument_type"] = s.instrument_type;
    o["entry_conditions"] = s.entry_conditions;
    o["exit_conditions"] = s.exit_conditions;
    o["entry_logic"] = s.entry_logic;
    o["exit_logic"] = s.exit_logic;
    o["stop_loss"] = s.stop_loss;
    o["take_profit"] = s.take_profit;
    o["trailing_stop"] = s.trailing_stop;
    o["position_size_pct"] = s.position_size_pct;
    o["cost_model"] = s.cost_model;
    o["entry_side"] = s.entry_side;
    o["risk_free_pct"] = s.risk_free_pct;
    o["legs"] = s.legs;
    if (!s.last_backtest.isEmpty())
        o["last_backtest"] = s.last_backtest;
    return QJsonObject{{"format", "fincept.algo.strategy"}, {"version", 1}, {"strategy", o}};
}

} // namespace

StrategyListPanel::StrategyListPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    view_ = new StrategyLibraryView(this);
    root->addWidget(view_);
    connect(view_, &StrategyLibraryView::action_requested, this, &StrategyListPanel::on_action);
    connect(view_, &StrategyLibraryView::retry_requested, this,
            []() { AlgoTradingService::instance().list_strategies(); });
    connect(view_, &StrategyLibraryView::builder_requested, this, &StrategyListPanel::open_builder_requested);
    connect_service();
    // Currency / number / date format changed: re-render from the loaded strategies.
    fincept::ui::formatting::on_format_changed(this, [this]() { view_->refresh_text(); });
    LOG_DEBUG("AlgoTrading", "StrategyListPanel constructed");
}

// ── Service connections ──────────────────────────────────────────────────────

void StrategyListPanel::connect_service() {
    auto& svc = AlgoTradingService::instance();
    connect(&svc, &AlgoTradingService::strategies_loaded, this, &StrategyListPanel::on_strategies_loaded);
    connect(&svc, &AlgoTradingService::strategy_deleted, this, [&svc](const QString&) { svc.list_strategies(); });
    // A copy saved here (or a strategy saved in the Builder while this tab shows) is listed at once.
    connect(&svc, &AlgoTradingService::strategy_saved, this, [this, &svc](const QString& id) {
        if (id == select_after_load_ || isVisible())
            svc.list_strategies();
    });
    connect(&svc, &AlgoTradingService::error_occurred, this, [this](const QString& context, const QString& msg) {
        LOG_ERROR("AlgoTrading", QString("StrategyList error [%1]: %2").arg(context, msg));
        // A failed delete / copy used to be log-only: the row stayed and nothing said why.
        // (Backtest / Builder save errors are reported by the Builder, which runs those.)
        if (context == QLatin1String("delete_strategy") && isVisible())
            QMessageBox::warning(this, tr("Delete Strategy"), tr("The strategy could not be deleted:\n%1").arg(msg));
        else if (context == QLatin1String("save_strategy") && !select_after_load_.isEmpty() && isVisible()) {
            select_after_load_.clear();
            QMessageBox::warning(this, tr("Duplicate Strategy"), tr("The copy could not be saved:\n%1").arg(msg));
        }
    });
    // A finished backtest updates that strategy's LAST BACKTEST cells in place.
    connect(&svc, &AlgoTradingService::strategy_backtest_saved, this,
            [this](const QString& id, const QJsonObject& summary) {
                for (auto& s : strategies_)
                    if (s.id == id)
                        s.last_backtest = summary;
                view_->set_last_backtest(id, summary);
            });
    // Live / paper deployments of each strategy (running count, net P&L per currency).
    connect(&fincept::algo::AlgoEngine::instance(), &fincept::algo::AlgoEngine::deployments_loaded, this,
            [this](const QVector<AlgoDeployment>& deps) {
                QHash<QString, StrategyLibraryView::Deployments> by_id;
                for (const auto& d : deps) {
                    auto& a = by_id[d.strategy_id];
                    ++a.total;
                    if (d.status == QLatin1String("running") || d.status == QLatin1String("starting")) {
                        ++a.running;
                        if (d.mode == QLatin1String("live"))
                            ++a.live_running;
                    }
                    const QString sym = d.symbol.isEmpty() ? d.underlying : d.symbol;
                    // P&L is in the instrument's quote currency (same rule as the dashboard);
                    // unknown -> "" and printed without a unit, never as the display currency.
                    const QString ccy = fincept::ui::algo::algo_currency_for(sym, d.exchange, d.broker_id);
                    a.pnl_by_ccy[ccy] += d.total_pnl + d.unrealized_pnl;
                    a.trades += d.total_trades;
                }
                view_->set_deployments(by_id);
            });
}

void StrategyListPanel::on_strategies_loaded(const QVector<AlgoStrategy>& strategies) {
    strategies_ = strategies;
    view_->set_strategies(strategies_);
    if (!select_after_load_.isEmpty()) {
        view_->select_id(select_after_load_);
        select_after_load_.clear();
    } else if (!link_select_.isEmpty()) {
        view_->select_id(link_select_);
        link_select_.clear();
    }
    LOG_DEBUG("AlgoTrading", QString("Loaded %1 strategies").arg(strategies_.size()));
}

void StrategyListPanel::select_strategy(const QString& strategy_id) {
    // Kept for the reload the tab switch asks for (set_strategies() rebuilds the rows).
    link_select_ = strategy_id;
    AlgoStrategy s;
    if (find_strategy(strategy_id, &s))
        view_->select_id(strategy_id);
}

bool StrategyListPanel::find_strategy(const QString& id, AlgoStrategy* out) const {
    for (const auto& s : strategies_) {
        if (s.id == id) {
            *out = s;
            return true;
        }
    }
    return false;
}

// ── Actions ──────────────────────────────────────────────────────────────────

void StrategyListPanel::on_action(const QString& action, const QString& strategy_id) {
    AlgoStrategy s;
    if (!find_strategy(strategy_id, &s)) // a copy: the dialogs below outlive any reload
        return;
    if (action == QLatin1String("open"))
        emit edit_requested(s); // AlgoTradingScreen routes to the Builder
    else if (action == QLatin1String("backtest"))
        prompt_backtest(s);
    else if (action == QLatin1String("deploy"))
        deploy(s);
    else if (action == QLatin1String("duplicate"))
        duplicate(s);
    else if (action == QLatin1String("export"))
        export_json(s);
    else if (action == QLatin1String("delete"))
        confirm_delete(s);
}

void StrategyListPanel::prompt_backtest(const AlgoStrategy& strategy) {
    // Prompt for symbol + date range before running.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Backtest: %1").arg(strategy.name));
    auto* form = new QFormLayout(&dlg);

    // Pre-fill the symbol the strategy was last backtested on (no implicit default
    // market). A bare ticker is read on the connected broker's exchange / the user's
    // market region; add a Yahoo suffix to pick another market (RELIANCE.NS, VOD.L).
    // Never backtested: the Builder's default (a broad fund of the user's market, else SPY).
    QString last_symbol = strategy.last_backtest.value(QStringLiteral("symbol")).toString();
    if (last_symbol.isEmpty())
        last_symbol = StrategyBuilderPanel::default_backtest_symbol();
    auto* symbol = new QLineEdit(last_symbol, &dlg);
    symbol->setPlaceholderText(tr("e.g. AAPL, RELIANCE.NS, VOD.L, BTC-USD"));
    symbol->setAccessibleName(tr("Symbol"));
    const QDate today = QDate::currentDate();
    // Default range sized to the strategy's timeframe (daily needs years for
    // SMA200; intraday must stay under Yahoo's history cap).
    const int lookback = services::algo::algo_default_lookback_days(strategy.timeframe);
    auto* start = new QDateEdit(today.addDays(-lookback), &dlg);
    auto* end = new QDateEdit(today, &dlg);
    for (QDateEdit* d : {start, end}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        d->setMinimumDate(QDate(2000, 1, 1));
        d->setMaximumDate(today);
    }
    start->setAccessibleName(tr("Start date"));
    end->setAccessibleName(tr("End date"));
    form->addRow(tr("Symbol"), symbol);
    form->addRow(tr("Start"), start);
    form->addRow(tr("End"), end);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Run Backtest"));
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;
    if (symbol->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Backtest"), tr("Enter a symbol to backtest on."));
        return;
    }
    if (start->date() >= end->date()) {
        QMessageBox::warning(this, tr("Backtest"), tr("The start date must be before the end date."));
        return;
    }
    emit backtest_requested(strategy, symbol->text().trimmed(), start->date().toString(QStringLiteral("yyyy-MM-dd")),
                            end->date().toString(QStringLiteral("yyyy-MM-dd")));
}

void StrategyListPanel::deploy(const AlgoStrategy& strategy) {
    // An option/future strategy deploys as a multi-leg basket on an underlying + expiry
    // rule that only the Builder collects. Deploying it from here went through the
    // plain-equity path: the leg rules were ignored and the runner would evaluate and
    // place a single-symbol equity order on whatever was typed into the symbol box.
    if (strategy.instrument_type != QLatin1String("equity")) {
        QMessageBox::information(this, tr("Deploy"),
                                 tr("\"%1\" is an F&O strategy. Open it in the Builder to pick the underlying and "
                                    "expiry, then deploy from there.")
                                     .arg(strategy.name));
        emit edit_requested(strategy);
        return;
    }

    // Strategy-aware constructor: seeds the deploy dialog's timeframe from the strategy.
    // The dialog (paper by default; its OK carries the order marker) is the only way
    // anything starts from here.
    AlgoDeployDialog dialog(strategy, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    auto deployment = dialog.result();
    // Same duplicate check as the Builder's deploy.
    if (fincept::algo::AlgoEngine::instance().has_active_duplicate(deployment.strategy_id, deployment.symbol,
                                                                    deployment.mode, deployment.entry_side)) {
        const auto btn = QMessageBox::question(
            this, tr("Already deployed"),
            tr("An identical deployment is already running:\n\n%1 · %2 · %3 · %4\n\nDeploy another copy anyway?")
                .arg(deployment.strategy_name, deployment.symbol, deployment.mode.toUpper(), deployment.entry_side),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (btn != QMessageBox::Yes)
            return;
    }
    fincept::algo::AlgoEngine::instance().start_deployment(deployment, strategy);
    LOG_INFO("AlgoTrading", QString("Deploy requested: %1 on %2").arg(strategy.name, deployment.symbol));
    emit deployed(); // AlgoTradingScreen jumps to the Dashboard
}

void StrategyListPanel::duplicate(const AlgoStrategy& strategy) {
    AlgoStrategy copy = strategy;
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // A unique name: save_strategy() upserts by name when no id is given, and two rows
    // with one name read as one strategy in every picker.
    QString name = tr("%1 (copy)").arg(strategy.name);
    for (int n = 2; n < 100; ++n) {
        bool taken = false;
        for (const auto& s : std::as_const(strategies_))
            taken = taken || s.name == name;
        if (!taken)
            break;
        name = tr("%1 (copy %2)").arg(strategy.name).arg(n);
    }
    copy.name = name;
    // A library entry keeps its category in `description`; the copy is the user's own.
    if (strategy.id.startsWith(QLatin1String("LIB-")))
        copy.description = tr("Copy of %1 (%2)").arg(strategy.name, strategy.description);
    // The copy has not been backtested itself.
    copy.last_backtest = QJsonObject();
    copy.last_backtest_at.clear();
    copy.created_at.clear();
    copy.updated_at.clear();
    select_after_load_ = copy.id;
    LOG_INFO("AlgoTrading", QString("Duplicate requested: %1 -> %2").arg(strategy.name, copy.name));
    AlgoTradingService::instance().save_strategy(copy);
    if (auto* sb = view_->status_bar())
        sb->show_notice(tr("Saved a copy: %1").arg(copy.name), QString(), {}, 6000,
                        fincept::ui::ScreenStatusBar::Tone::Ok);
}

void StrategyListPanel::export_json(const AlgoStrategy& strategy) {
    QString base = strategy.name;
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export strategy"), base + QStringLiteral(".json"),
                                                      tr("Strategy JSON (*.json)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export strategy"), tr("Could not write %1:\n%2").arg(path, f.errorString()));
        return;
    }
    f.write(QJsonDocument(slp_strategy_json(strategy)).toJson(QJsonDocument::Indented));
    f.close();
    LOG_INFO("AlgoTrading", QString("Strategy exported: %1 -> %2").arg(strategy.name, path));
    if (auto* sb = view_->status_bar())
        sb->show_notice(tr("Exported %1 to %2").arg(strategy.name, path), QString(), {}, 8000,
                        fincept::ui::ScreenStatusBar::Tone::Ok);
}

void StrategyListPanel::confirm_delete(const AlgoStrategy& strategy) {
    const auto answer = QMessageBox::question(this, tr("Delete Strategy"),
                                              tr("Delete \"%1\"?\n\nIts deployments and their history are kept.")
                                                  .arg(strategy.name),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    AlgoTradingService::instance().delete_strategy(strategy.id);
    LOG_INFO("AlgoTrading", QString("Delete requested: %1").arg(strategy.name));
}

// ── Live language switch ──────────────────────────────────────────────────────

void StrategyListPanel::changeEvent(QEvent* event) {
    // The view retranslates itself (its own changeEvent).
    QWidget::changeEvent(event);
}

} // namespace fincept::screens
