// src/screens/algo_trading/UniverseScannerPanel.cpp
#include "screens/algo_trading/UniverseScannerPanel.h"

#include "algo_engine/AlgoEngine.h"
#include "algo_engine/ScanMonitor.h"
#include "core/logging/Logger.h"
#include "screens/algo_trading/AlgoDeployDialog.h"
#include "screens/algo_trading/AlgoTabKit.h"
#include "screens/algo_trading/UniverseView.h"
#include "services/algo_trading/AlgoRuleLogic.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "services/algo_trading/AlgoUniverses.h"
#include "storage/repositories/ScanWatchRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/algo/AlgoCurrency.h"
#include "ui/widgets/algo/SymbolChipInput.h"

#include <QComboBox>
#include <QDateTime>
#include <QJsonObject>
#include <QMessageBox>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::algo;
namespace kit = fincept::screens::algo_kit;

UniverseScannerPanel::UniverseScannerPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    view_ = new UniverseView(this);
    root->addWidget(view_);

    QComboBox* account = view_->account_combo();
    QComboBox* universe = view_->universe_combo();
    QComboBox* strategy = view_->strategy_combo();
    populate_accounts();
    connect(universe, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &UniverseScannerPanel::on_universe_changed);
    connect(account, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        populate_universes();
        update_matches_state();
    });
    populate_universes();

    // The user's last strategy / universe / account and knobs come back on the next
    // visit (combos fill later: their values are applied as the items arrive).
    inputs_ = new fincept::ui::InputMemory(QStringLiteral("algo_trading"), QStringLiteral("universe"), this);
    inputs_->track(QStringLiteral("sweep_ms"), view_->sweep_spin());
    inputs_->track(QStringLiteral("cooldown_min"), view_->cooldown_spin());
    inputs_->restore();
    // The combos were filled above with their first items: put the remembered choice back
    // (the account first — its change refills the universes).
    if (const int i = account->findData(inputs_->value(QStringLiteral("account"))); i >= 0)
        account->setCurrentIndex(i);
    if (const int i = universe->findData(inputs_->value(QStringLiteral("universe"))); i >= 0)
        universe->setCurrentIndex(i);
    connect(strategy, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        inputs_->set_value(QStringLiteral("strategy"), view_->strategy_combo()->currentData());
        update_strategy_summary();
        update_matches_state();
    });
    connect(universe, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        inputs_->set_value(QStringLiteral("universe"), view_->universe_combo()->currentData());
        update_matches_state();
    });
    connect(account, QOverload<int>::of(&QComboBox::activated), this,
            [this](int) { inputs_->set_value(QStringLiteral("account"), view_->account_combo()->currentData()); });
    connect(view_->sweep_spin(), QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!running_)
            update_matches_state(); // the preview names the sweep
    });

    connect(view_, &UniverseView::action_requested, this, [this](const QString& action) {
        if (action == QLatin1String("start")) {
            on_start_stop();
        } else if (action == QLatin1String("clear")) {
            view_->clear_matches();
            update_matches_state();
        }
    });
    connect(view_, &UniverseView::deploy_requested, this, &UniverseScannerPanel::deploy_symbol);
    connect(view_, &UniverseView::state_action, this, &UniverseScannerPanel::on_matches_state_action);

    connect(&AlgoTradingService::instance(), &AlgoTradingService::strategies_loaded, this,
            &UniverseScannerPanel::on_strategies_loaded);
    connect(&fincept::algo::ScanMonitor::instance(), &fincept::algo::ScanMonitor::realtime_match, this,
            &UniverseScannerPanel::on_match);
    // ScanMonitor reports "error" when the universe resolves to nothing (instrument master not
    // loaded for this broker). Nothing was listening, so the panel stayed on STOP SCAN /
    // "Scanning live…" for a scan that never started.
    connect(&fincept::algo::ScanMonitor::instance(), &fincept::algo::ScanMonitor::watch_status_changed, this,
            [this](const QString& id, const QString& status) {
                if (!running_ || watch_id_.isEmpty() || id != watch_id_ || status != QLatin1String("error"))
                    return;
                fincept::ScanWatchRepository::instance().remove(watch_id_);
                watch_id_.clear();
                set_running(false);
                view_->set_status(tr("Could not start the scan: the selected universe is empty. Load this broker's "
                                     "instrument list (connect the broker in Equity Trading) or pick another universe."),
                                  kit::Tone::Danger);
                update_matches_state();
            });
    // Currency / number format changed: the PRICE cells re-render from each row's raw values.
    fincept::ui::formatting::on_format_changed(this, [this]() { view_->refresh_text(); });

    // Purge orphaned realtime watches left over from a prior session. ScanMonitor
    // intentionally skips mode=='realtime' watches at launch (they are started on
    // demand here), so any active realtime row is a stale leftover that would
    // otherwise accumulate silently.
    if (auto active = fincept::ScanWatchRepository::instance().list_active(); active.is_ok())
        for (const auto& w : active.value())
            if (w.mode == QStringLiteral("realtime"))
                fincept::ScanWatchRepository::instance().remove(w.id);

    view_->set_running(false);
    AlgoTradingService::instance().list_strategies();
    update_strategy_summary();
    update_matches_state();
    LOG_DEBUG("AlgoTrading", "UniverseScannerPanel constructed");
}

void UniverseScannerPanel::populate_accounts() {
    QComboBox* combo = view_->account_combo();
    QString keep = combo->currentData().toString();
    if (keep.isEmpty() && inputs_)
        keep = inputs_->value(QStringLiteral("account")).toString();
    QSignalBlocker blocker(combo);
    combo->clear();
    for (const auto& acct : fincept::trading::AccountManager::instance().list_accounts())
        combo->addItem(QString("%1 (%2)").arg(acct.display_name, acct.broker_id), acct.account_id);
    const int idx = combo->findData(keep);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

void UniverseScannerPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    populate_accounts();
    populate_universes();
    update_matches_state(); // a broker connected / a strategy saved meanwhile
}

void UniverseScannerPanel::populate_universes() {
    QComboBox* combo = view_->universe_combo();
    QString keep = combo->currentData().toString();
    if (keep.isEmpty() && inputs_)
        keep = inputs_->value(QStringLiteral("universe")).toString();
    QSignalBlocker blocker(combo);
    combo->clear();
    // Every cash-equity exchange the selected account's broker trades: the
    // universe is that exchange's equities from the broker's instrument master.
    const QString account_id = view_->account_combo()->currentData().toString();
    if (!account_id.isEmpty()) {
        if (auto* broker = fincept::trading::AccountManager::instance().broker_for(account_id)) {
            static const QStringList kDerivativeSegments = {"NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX", "NCO", "MCXSX"};
            for (const QString& ex : broker->profile().exchanges) {
                const QString code = ex.trimmed().toUpper();
                if (code.isEmpty() || kDerivativeSegments.contains(code))
                    continue;
                combo->addItem(tr("All %1 equity").arg(code), code + QStringLiteral("_EQ"));
            }
        }
    }
    // Your saved symbol lists (Scanner → SAVE LIST…, or algo_universes.json).
    for (const auto& u : algo_universes()) {
        if (!u.user)
            continue;
        combo->addItem(tr("List: %1 (%2)").arg(u.name).arg(u.symbols.size()), QStringLiteral("LIST:") + u.name);
    }
    combo->addItem(tr("NSE — largest NIFTY 50 names (20, partial)"), "NIFTY50");
    combo->addItem(tr("Custom symbols"), "CUSTOM");
    const int idx = combo->findData(keep);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
    blocker.unblock();
    on_universe_changed(combo->currentIndex());
}

void UniverseScannerPanel::on_universe_changed(int) {
    view_->set_custom_visible(view_->universe_combo()->currentData().toString() == QLatin1String("CUSTOM"));
}

void UniverseScannerPanel::on_strategies_loaded(const QVector<AlgoStrategy>& strategies) {
    // A universe scan evaluates single-symbol (equity) entry rules and DEPLOY sends
    // a plain equity order for the matched stock, so Option/Future strategies —
    // whose orders are their leg baskets — are not offered here.
    strategies_.clear();
    for (const auto& s : strategies)
        if (s.instrument_type.isEmpty() || s.instrument_type == QLatin1String("equity"))
            strategies_.append(s);
    QComboBox* combo = view_->strategy_combo();
    QString prev = combo->currentData().toString();
    if (prev.isEmpty() && inputs_)
        prev = inputs_->value(QStringLiteral("strategy")).toString();
    QSignalBlocker block(combo);
    combo->clear();
    for (const auto& s : strategies_)
        combo->addItem(s.name, s.id);
    const int idx = combo->findData(prev);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
    update_strategy_summary();
    update_matches_state();
}

bool UniverseScannerPanel::current_strategy(AlgoStrategy* out) const {
    const QString id = view_->strategy_combo()->currentData().toString();
    for (const auto& s : strategies_) {
        if (s.id == id) {
            *out = s;
            return true;
        }
    }
    return false;
}

void UniverseScannerPanel::update_strategy_summary() {
    AlgoStrategy s;
    if (!current_strategy(&s)) {
        view_->set_strategy_summary(QString(), QString(),
                                    tr("Pick a saved equity strategy: its entry rules are scanned on every symbol."));
        return;
    }
    const QString tf = s.timeframe.isEmpty() ? QStringLiteral("5m") : s.timeframe;
    view_->set_strategy_summary(
        s.name, rules::rules_sentence(s.entry_conditions, s.entry_logic, rules::Side::Entry),
        tr("Evaluated on %1 bars · %2 · exits are not scanned (they apply once deployed)")
            .arg(tf, s.entry_side == QLatin1String("SELL") ? tr("short entries") : tr("long entries")));
}

void UniverseScannerPanel::set_running(bool running) {
    running_ = running;
    view_->set_running(running);
}

void UniverseScannerPanel::on_start_stop() {
    if (running_) {
        // Stop: tear down the runner and remove the persisted config (alert history
        // in scan_watch_events is kept).
        fincept::algo::ScanMonitor::instance().stop_watch(watch_id_);
        if (!watch_id_.isEmpty())
            fincept::ScanWatchRepository::instance().remove(watch_id_);
        watch_id_.clear();
        set_running(false);
        view_->set_status(tr("Stopped."), kit::Tone::Dim);
        update_matches_state();
        return;
    }

    AlgoStrategy strat;
    if (!current_strategy(&strat)) {
        view_->set_status(tr("Select a strategy first."), kit::Tone::Danger);
        return;
    }
    if (strat.entry_conditions.isEmpty()) {
        view_->set_status(tr("The selected strategy has no entry conditions."), kit::Tone::Danger);
        return;
    }
    // A rule that can never change (the old blank form's "Close > 0") would match every symbol.
    for (const auto& issue : rules::check_conditions(strat.entry_conditions)) {
        if (issue.blocking) {
            view_->set_status(tr("The strategy's entry rule %1 cannot be scanned: %2 Fix it in the Builder.")
                                  .arg(QString::number(issue.index + 1), issue.reason),
                              kit::Tone::Danger);
            return;
        }
    }
    const QString account_id = view_->account_combo()->currentData().toString();
    if (account_id.isEmpty()) {
        view_->set_status(tr("Select a connected broker account (live feed required)."), kit::Tone::Danger);
        return;
    }

    const QString universe = view_->universe_combo()->currentData().toString();
    const QString tf = strat.timeframe.isEmpty() ? QStringLiteral("5m") : strat.timeframe;

    fincept::ScanWatch w;
    w.name = QString("Universe: %1").arg(strat.name);
    w.conditions = strat.entry_conditions;
    w.logic = strat.entry_logic;
    w.universe = universe;
    w.symbols = (universe == "CUSTOM") ? view_->symbols_input()->symbols() : QStringList();
    if (universe.startsWith(QLatin1String("LIST:"))) {
        // A saved list runs as a custom symbol set.
        const QString name = universe.mid(5);
        for (const auto& u : algo_universes())
            if (u.user && u.name == name)
                w.symbols = u.symbols;
        w.universe = QStringLiteral("CUSTOM");
    }
    w.timeframe = tf;
    w.lookback_days = algo_default_lookback_days(tf == "live" ? QStringLiteral("1m") : tf);
    w.data_source = QStringLiteral("Broker");
    w.account_id = account_id;
    w.broker_id = fincept::trading::AccountManager::instance().get_account(account_id).broker_id;
    // Match prices are shown in the listing's currency: "NSE_EQ" -> NSE, the NIFTY list -> NSE;
    // saved / custom lists are resolved per symbol.
    scan_broker_id_ = w.broker_id;
    scan_exchange_hint_ = universe.endsWith(QLatin1String("_EQ")) ? universe.chopped(3)
                          : universe == QLatin1String("NIFTY50") ? QStringLiteral("NSE")
                                                                 : QString();
    w.mode = QStringLiteral("realtime");
    w.cooldown_min = view_->cooldown_spin()->value();
    QJsonObject actions;
    actions["toast"] = true;
    actions["providers"] = false;
    actions["sweep_ms"] = view_->sweep_spin()->value();
    actions["strategy_id"] = strat.id;
    w.actions = actions;
    w.active = true;

    if (w.universe == "CUSTOM" && w.symbols.isEmpty()) {
        view_->set_status(tr("Enter custom symbols or pick a universe."), kit::Tone::Danger);
        return;
    }

    auto r = fincept::ScanWatchRepository::instance().create(w);
    if (r.is_err()) {
        view_->set_status(tr("Could not save scan: %1").arg(QString::fromStdString(r.error())), kit::Tone::Danger);
        return;
    }
    watch_id_ = r.value().id;
    view_->clear_matches();

    // Flip the UI state BEFORE starting the monitor: it can report "error" synchronously
    // (empty universe) and the status handler above only acts on a running scan.
    set_running(true);
    view_->set_status(tr("Scanning live — warming history; matches appear in LIVE MATCHES."), kit::Tone::Accent);
    update_matches_state();
    fincept::algo::ScanMonitor::instance().reload(watch_id_);
}

void UniverseScannerPanel::update_matches_state() {
    if (view_->match_count() > 0)
        return; // the table is on screen
    fincept::ui::EmptyState::Spec spec;
    state_action_.clear();
    const QString universe_name = view_->universe_combo()->currentText();
    AlgoStrategy strat;
    const bool have_strategy = current_strategy(&strat);
    if (running_) {
        spec.glyph = QStringLiteral("◉");
        spec.glyph_tone = fincept::ui::EmptyState::Tone::Info;
        spec.title = tr("Scanning live — no match yet");
        spec.message = tr("Every sweep evaluates %1 on %2. Symbols that meet its entry rules appear here, each with "
                          "a DEPLOY action.")
                           .arg(have_strategy ? strat.name : tr("the strategy"), universe_name);
        spec.illustration = true;
        spec.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Table;
    } else if (const auto st = fincept::ui::check_requirements({fincept::ui::Requirement::broker()}); !st.ok) {
        // Live feed required: no connected broker (or its stored credentials could not
        // be read — then RETRY, never "connect a broker").
        spec = fincept::ui::EmptyState::needs_setup_spec(st);
        state_action_ = st.retry ? QStringLiteral("retry") : QStringLiteral("setup");
        setup_screen_ = st.link.screen_id;
        setup_section_ = st.link.section;
        setup_focus_ = st.link.focus;
        spec.details = fincept::ui::EmptyState::bullet_rows(
            {tr("A universe scan runs a saved strategy's entry rules on every symbol of an exchange or list."),
             tr("It reads your broker's live feed, so it needs a connected account.")});
        spec.details_heading = tr("WHAT THIS TAB DOES");
    } else if (!have_strategy) {
        spec = fincept::ui::EmptyState::no_data_spec(
            tr("No equity strategy to run"),
            tr("A universe scan runs a saved strategy's entry rules across a whole market. Build one in the "
               "Builder and press Save, then pick it above."),
            tr("BUILD A STRATEGY"));
        state_action_ = QStringLiteral("builder");
    } else {
        // Class C: a persistent watch on the broker feed — shown, never started by itself.
        spec = fincept::ui::EmptyState::preview_spec(
            tr("Live universe scan"),
            tr("Runs the entry rules of %1 on every symbol of %2 on each sweep and lists the symbols that match, "
               "with a DEPLOY action per match.")
                .arg(strat.name, universe_name),
            tr("START SCAN"),
            tr("Starts a background watch on your broker's live feed — it runs until you stop it. It alerts; it "
               "places no order."),
            fincept::ui::EmptyState::Tone::Warning, fincept::ui::LoadingSkeleton::Shape::Table,
            {{tr("Strategy"), strat.name},
             {tr("Universe"), universe_name},
             {tr("Account"), view_->account_combo()->currentText()},
             {tr("Sweep"), tr("%1 ms").arg(view_->sweep_spin()->value())}},
            tr("WHAT WILL RUN"));
        state_action_ = QStringLiteral("start");
    }
    view_->show_state(spec);
}

void UniverseScannerPanel::on_matches_state_action() {
    if (state_action_ == QLatin1String("start")) {
        on_start_stop();
    } else if (state_action_ == QLatin1String("builder")) {
        emit open_builder_requested();
    } else if (state_action_ == QLatin1String("retry")) {
        populate_accounts();
        update_matches_state(); // the live check reads the stored credentials again
    } else if (state_action_ == QLatin1String("setup")) {
        fincept::ui::open_setup_link(fincept::ui::SetupLink{setup_screen_, setup_section_, setup_focus_}, this);
    }
}

void UniverseScannerPanel::on_match(const QString& watch_id, const QString& symbol, double price,
                                    const QString& detail) {
    if (watch_id != watch_id_ || watch_id_.isEmpty())
        return;
    // Bullish / bearish from the detail's operators (best-effort).
    int dir = 0;
    if (detail.contains('>') || detail.contains("crosses_above"))
        dir = 1;
    else if (detail.contains('<') || detail.contains("crosses_below"))
        dir = -1;
    // The price in the listing's own currency.
    const QString hint =
        scan_exchange_hint_.isEmpty() ? AlgoTradingService::default_exchange_hint(symbol) : scan_exchange_hint_;
    view_->add_match(QDateTime::currentMSecsSinceEpoch(), symbol, dir, price,
                     fincept::ui::algo::algo_currency_for(symbol, hint, scan_broker_id_), detail);
}

void UniverseScannerPanel::deploy_symbol(const QString& symbol) {
    AlgoStrategy strat;
    if (!current_strategy(&strat)) {
        view_->set_status(tr("Strategy no longer selected — cannot deploy."), kit::Tone::Danger);
        return;
    }
    // Same guard as My Strategies: an F&O strategy must not be deployed as a plain
    // single-symbol equity order on the matched stock.
    if (!strat.instrument_type.isEmpty() && strat.instrument_type != QLatin1String("equity")) {
        QMessageBox::information(this, tr("Deploy"),
                                 tr("\"%1\" is an Option/Future strategy — deploy it from the Builder, not on a "
                                    "scanned stock.")
                                     .arg(strat.name));
        return;
    }
    auto* dlg = new AlgoDeployDialog(strat, this);
    dlg->set_symbol(symbol);
    if (dlg->exec() == QDialog::Accepted) {
        auto dep = dlg->deployment();
        AlgoTradingService::instance().save_strategy(strat);
        fincept::algo::AlgoEngine::instance().start_deployment(dep, strat);
        view_->set_status(tr("Deployed %1.").arg(symbol), kit::Tone::Ok);
    }
    dlg->deleteLater();
}

} // namespace fincept::screens
