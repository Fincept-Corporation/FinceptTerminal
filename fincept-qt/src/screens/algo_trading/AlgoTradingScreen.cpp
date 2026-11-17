// src/screens/algo_trading/AlgoTradingScreen.cpp
#include "screens/algo_trading/AlgoTradingScreen.h"

#include "algo_engine/AlgoEngine.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/algo_trading/AlertsPanel.h"
#include "screens/algo_trading/AlgoTabKit.h"
#include "screens/algo_trading/DeploymentDashboard.h"
#include "screens/algo_trading/ScannerPanel.h"
#include "screens/algo_trading/StrategyBuilderPanel.h"
#include "screens/algo_trading/StrategyListPanel.h"
#include "screens/algo_trading/UniverseScannerPanel.h"
#include "screens/common/CommandRows.h"
#include "services/algo_trading/AlgoStrategyLibrary.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "storage/sqlite/Database.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QKeySequence>
#include <QLabel>
#include <QShortcut>
#include <QSqlQuery>
#include <QThreadPool>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::algo;
namespace kit = fincept::screens::algo_kit;

AlgoTradingScreen::AlgoTradingScreen(QWidget* parent) : QWidget(parent) {
    // The one root stylesheet goes on BEFORE any child exists: every panel built later is
    // polished once, when shown (re-setting a sheet over six built panels used to stall ~0.9 s).
    setObjectName(QStringLiteral("algoScreen"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(kit::root_stylesheet());
    build_ui();
    install_shortcuts();

    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(5000);
    // Polled on every sub-tab: the LIVE / PAPER badge is fed only by deployments_loaded, so
    // gating this on the Dashboard left it at "0" while resumed live deployments were trading
    // (two small SELECTs on a worker thread).
    connect(poll_timer_, &QTimer::timeout, this, []() { fincept::algo::AlgoEngine::instance().list_deployments(); });

    connect(&fincept::algo::AlgoEngine::instance(), &fincept::algo::AlgoEngine::deployments_loaded, this,
            [this](const QVector<AlgoDeployment>& deps) {
                // "LIVE" must mean real money: paper deployments never count towards it.
                int active = 0;
                int live = 0;
                for (const auto& d : deps) {
                    if (d.status != "running" && d.status != "starting")
                        continue;
                    ++active;
                    if (d.mode == QLatin1String("live"))
                        ++live;
                }
                if (active == active_deployments_ && live == live_deployments_)
                    return;
                active_deployments_ = active;
                live_deployments_ = live;
                update_deploy_badge();
            });

    LOG_DEBUG("AlgoTrading", "Screen constructed");
}

void AlgoTradingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    poll_timer_->start();
    fincept::algo::AlgoEngine::instance().list_deployments(); // badge correct from the first frame
    if (first_show_) {
        first_show_ = false;
        // Apply the tab restore_state() chose — it runs before the first show. This builds
        // only that sub-tab's panel.
        on_tab_changed(active_tab_);
        if (!pending_draft_.isEmpty()) {
            builder()->restore_draft(pending_draft_);
            pending_draft_.clear();
        }
    }
}

void AlgoTradingScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    poll_timer_->stop();
}

void AlgoTradingScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Six sub-tabs, one panel each, built when first opened (ensure_panel()). Not even the
    // Builder is built here: the router puts this screen into its dock widget after
    // construction, and that move re-styles whatever already exists. Placeholders keep the
    // stack indices equal to the sub-tab indices.
    content_stack_ = new QStackedWidget(this);
    for (int i = 0; i < kAlgoPanelCount; ++i)
        content_stack_->addWidget(new QWidget(content_stack_));
    root->addWidget(content_stack_, 1);
}

void AlgoTradingScreen::install_shortcuts() {
    // Alt+1..6: Ctrl+1..9 are the shell's window keys (KeyRouting.h), so the sub-tabs use Alt.
    const int keys[kAlgoPanelCount] = {Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5, Qt::Key_6};
    for (int i = 0; i < kAlgoPanelCount; ++i) {
        auto* sc = new QShortcut(QKeySequence(Qt::ALT | keys[i]), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this, i]() { on_tab_changed(i); });
    }
}

StrategyBuilderPanel* AlgoTradingScreen::builder() {
    ensure_panel(0);
    return builder_;
}

QWidget* AlgoTradingScreen::ensure_panel(int index) {
    QWidget* w = nullptr;
    switch (index) {
        case 0:
            if (builder_)
                return builder_;
            builder_ = new StrategyBuilderPanel(content_stack_);
            // Deploying from the Builder jumps to the Dashboard (tab 4); on_tab_changed(4)
            // refreshes list_deployments() so the just-persisted row shows immediately.
            connect(builder_, &StrategyBuilderPanel::deployed, this, [this]() { on_tab_changed(4); });
            // A remembered backtest result is part of the draft: persist it now, not only on hide.
            connect(builder_, &StrategyBuilderPanel::backtest_remembered, this,
                    [this]() { ScreenStateManager::instance().notify_changed(this); });
            w = builder_;
            break;
        case 1:
            if (strategies_)
                return strategies_;
            strategies_ = new StrategyListPanel(content_stack_);
            // OPEN in Strategies opens the Builder (tab 0) pre-filled with that strategy.
            connect(strategies_, &StrategyListPanel::edit_requested, this, [this](const AlgoStrategy& s) {
                builder()->load_strategy(s);
                on_tab_changed(0);
            });
            // BACKTEST opens the Builder (tab 0) pre-filled and runs the backtest.
            connect(strategies_, &StrategyListPanel::backtest_requested, this,
                    [this](const AlgoStrategy& s, const QString& symbol, const QString& start, const QString& end) {
                        on_tab_changed(0);
                        builder()->load_and_backtest(s, symbol, start, end);
                    });
            connect(strategies_, &StrategyListPanel::deployed, this, [this]() { on_tab_changed(4); });
            // The "No strategies yet" state's OPEN BUILDER.
            connect(strategies_, &StrategyListPanel::open_builder_requested, this, [this]() { on_tab_changed(0); });
            w = strategies_;
            break;
        case 2:
            if (scanner_)
                return scanner_;
            scanner_ = new ScannerPanel(content_stack_);
            // Scanner → Alerts hand-off: pre-fills the Alerts composer with the scan's rules
            // and switches to ALERTS (which builds that panel if needed).
            connect(scanner_, &ScannerPanel::create_alert_requested, this,
                    [this](const QJsonArray& conds, const QString& logic, const QStringList& syms,
                           const QString& tf, const QString& ds, const QString& acct) {
                        on_tab_changed(3);
                        if (alerts_)
                            alerts_->prefill(conds, logic, syms, tf, ds, acct);
                    });
            // A scanner result → the Builder with that symbol as the backtest instrument.
            connect(scanner_, &ScannerPanel::symbol_activated, this, [this](const QString& symbol) {
                builder()->set_backtest_symbol(symbol);
                on_tab_changed(0);
            });
            w = scanner_;
            break;
        case 3:
            if (alerts_)
                return alerts_;
            w = alerts_ = new AlertsPanel(content_stack_);
            connect(alerts_, &AlertsPanel::open_scanner_requested, this, [this]() { on_tab_changed(2); });
            break;
        case 4:
            if (dashboard_)
                return dashboard_;
            w = dashboard_ = new DeploymentDashboard(content_stack_);
            connect(dashboard_, &DeploymentDashboard::open_strategies_requested, this, [this]() { on_tab_changed(1); });
            connect(dashboard_, &DeploymentDashboard::open_builder_requested, this, [this]() { on_tab_changed(0); });
            break;
        case 5:
            if (universe_)
                return universe_;
            w = universe_ = new UniverseScannerPanel(content_stack_);
            connect(universe_, &UniverseScannerPanel::open_builder_requested, this, [this]() { on_tab_changed(0); });
            break;
        default:
            return nullptr;
    }
    // Swap the placeholder for the real panel at the same index.
    QWidget* placeholder = content_stack_->widget(index);
    content_stack_->insertWidget(index, w);
    if (placeholder) {
        content_stack_->removeWidget(placeholder);
        placeholder->deleteLater();
    }
    adopt_panel(w, index);
    return w;
}

void AlgoTradingScreen::adopt_panel(QWidget* panel, int index) {
    if (!panel)
        return;
    AdoptedBars a;
    a.tab = index;
    a.bar = panel->findChild<ui::ScreenCommandBar*>(QStringLiteral("algoTabBar"));
    a.status = panel->findChild<ui::ScreenStatusBar*>(QStringLiteral("algoStatusBar"));
    if (a.bar) {
        a.bar->set_segment(QStringLiteral("tab"), kit::tab_key(index));
        a.badge = a.bar->findChild<QLabel*>(QStringLiteral("algoDeployBadge"));
        connect(a.bar, &ui::ScreenCommandBar::segment_selected, this, [this](const QString& id, const QString& key) {
            if (id != QLatin1String("tab"))
                return;
            const int tab = kit::tab_from_key(key);
            if (tab >= 0)
                on_tab_changed(tab);
        });
    }
    adopted_.append(a);
    update_deploy_badge();
}

void AlgoTradingScreen::on_tab_changed(int index) {
    if (index < 0 || index >= kAlgoPanelCount)
        return;
    ensure_panel(index);
    const bool changed = index != active_tab_ || content_stack_->currentIndex() != index;
    active_tab_ = index;
    content_stack_->setCurrentIndex(index);
    // A click on a segment moved that bar's check; every bar shows its own sub-tab again
    // (set_segment emits nothing).
    for (const AdoptedBars& a : std::as_const(adopted_))
        if (a.bar)
            a.bar->set_segment(QStringLiteral("tab"), kit::tab_key(a.tab));
    if (changed)
        ScreenStateManager::instance().notify_changed(this);

    // Refresh data when switching tabs
    if (index == 1)
        AlgoTradingService::instance().list_strategies();
    if (index == 4)
        fincept::algo::AlgoEngine::instance().list_deployments();
}

void AlgoTradingScreen::update_deploy_badge() {
    const bool has_live = live_deployments_ > 0;
    const int paper = active_deployments_ - live_deployments_;
    QString text;
    if (has_live)
        text = paper > 0 ? tr("▶ %1 LIVE · %2 PAPER").arg(live_deployments_).arg(paper)
                         : tr("▶ %1 LIVE").arg(live_deployments_);
    else
        text = tr("▶ %1 PAPER").arg(paper);
    const QString tip = tr("%1 deployment(s) running — %2 live (real money), %3 paper. Open DASHBOARD (Alt+5).")
                            .arg(active_deployments_)
                            .arg(live_deployments_)
                            .arg(paper);
    for (const AdoptedBars& a : std::as_const(adopted_)) {
        if (a.badge) {
            if (a.badge->text() != text)
                a.badge->setText(text);
            a.badge->setToolTip(tip);
            a.badge->setAccessibleDescription(tip);
            // Real money is the danger state; paper is a safe (ok) state.
            kit::set_tone(a.badge, has_live ? kit::Tone::Danger : kit::Tone::Ok);
            a.badge->setVisible(active_deployments_ > 0);
        }
        if (a.status) {
            using SB = ui::ScreenStatusBar;
            a.status->set_item(QStringLiteral("deploy"), tr("%n RUNNING", nullptr, active_deployments_),
                               has_live ? SB::Tone::Danger : SB::Tone::Ok);
            a.status->set_tooltip(QStringLiteral("deploy"), tip);
            a.status->set_item_visible(QStringLiteral("deploy"), active_deployments_ > 0);
        }
    }
}

void AlgoTradingScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AlgoTradingScreen::retranslateUi() {
    for (const AdoptedBars& a : std::as_const(adopted_))
        kit::retranslate_tab_bar(a.bar);
    update_deploy_badge();
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap AlgoTradingScreen::save_state() const {
    QVariantMap state{{"tab_index", active_tab_}};
    if (builder_)
        state["builder_draft"] = builder_->save_draft();
    else if (!pending_draft_.isEmpty())
        state["builder_draft"] = pending_draft_; // not built yet: keep the restored draft
    return state;
}

void AlgoTradingScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("tab_index", 0).toInt();
    if (first_show_) {
        // Not shown (nor docked) yet: record only; showEvent() builds and applies it.
        if (idx >= 0 && idx < kAlgoPanelCount)
            active_tab_ = idx;
        if (state.contains("builder_draft"))
            pending_draft_ = state.value("builder_draft").toMap();
        return;
    }
    if (idx >= 0 && idx < kAlgoPanelCount)
        on_tab_changed(idx);
    if (state.contains("builder_draft"))
        builder()->restore_draft(state.value("builder_draft").toMap());
}

// ── ISectionLinked ────────────────────────────────────────────────────────────

bool AlgoTradingScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus);
    const QString raw = section.trimmed();
    if (raw.isEmpty())
        return true; // nothing on these tabs takes a focus of its own
    QString rest;
    QString strategy;
    int tab = -1;
    if (command_rows::split_kind(raw, "strategy", &rest)) {
        if (rest.isEmpty())
            return false;
        strategy = rest;
        tab = kit::tab_from_key(QStringLiteral("strategies"));
    } else if (command_rows::split_kind(raw, "deployment", &rest)) {
        tab = kit::tab_from_key(QStringLiteral("dashboard"));
    } else {
        QString key = raw.toLower();
        if (key.startsWith(QLatin1String("tab:")))
            key = key.mid(4);
        if (key == QLatin1String("my_strategies"))
            key = QStringLiteral("strategies");
        else if (key == QLatin1String("deployments"))
            key = QStringLiteral("dashboard");
        tab = kit::tab_from_key(key);
    }
    if (tab < 0)
        return false;
    if (first_show_) {
        active_tab_ = tab; // showEvent() builds and opens it (the strategy row is not pre-selected)
        return true;
    }
    on_tab_changed(tab); // the segment's own path (MY STRATEGIES reloads its list)
    if (!strategy.isEmpty() && strategies_)
        strategies_->select_strategy(strategy);
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QString algo_cmd_where() {
    return AlgoTradingScreen::tr("Algo Trading");
}

QList<command::CommandItem> algo_command_rows() {
    namespace rows = command_rows;
    const QString screen = QStringLiteral("algo_trading");
    const QString where = algo_cmd_where();
    QList<command::CommandItem> out = {
        rows::section(screen, QStringLiteral("builder"), AlgoTradingScreen::tr("Algo strategy builder"),
                      {QStringLiteral("build strategy"), QStringLiteral("rules"), QStringLiteral("conditions"),
                       QStringLiteral("algo builder"), QStringLiteral("new strategy")},
                      where),
        rows::section(screen, QStringLiteral("strategies"), AlgoTradingScreen::tr("My algo strategies"),
                      {QStringLiteral("strategies"), QStringLiteral("strategy library"), QStringLiteral("my strategies"),
                       QStringLiteral("saved strategies")},
                      where),
        rows::section(screen, QStringLiteral("scanner"), AlgoTradingScreen::tr("Algo scanner"),
                      {QStringLiteral("scan"), QStringLiteral("screen symbols"), QStringLiteral("signals now")}, where),
        rows::section(screen, QStringLiteral("alerts"), AlgoTradingScreen::tr("Algo alerts"),
                      {QStringLiteral("alerts"), QStringLiteral("condition alerts"), QStringLiteral("notify")}, where),
        rows::section(screen, QStringLiteral("dashboard"), AlgoTradingScreen::tr("Algo deployments dashboard"),
                      {QStringLiteral("deployments"), QStringLiteral("running strategies"), QStringLiteral("live algos"),
                       QStringLiteral("paper algos")},
                      where),
        rows::section(screen, QStringLiteral("universe"), AlgoTradingScreen::tr("Universe scanner"),
                      {QStringLiteral("universe"), QStringLiteral("symbol universe"), QStringLiteral("index members")},
                      where),
    };
    // The seed's MY STRATEGIES row used the id "my_strategies": this row replaces it.
    out[1].id = QStringLiteral("algo_trading.my_strategies");
    // The curated library (fixed LIB-* ids, seeded into every profile's list): selected, never run.
    for (const auto& s : services::algo::algo_library_strategies()) {
        command::CommandItem it = rows::section(
            screen, QStringLiteral("strategy:") + s.id, s.name,
            {s.description, s.timeframe, QStringLiteral("algo strategy"), QStringLiteral("strategy template"),
             QStringLiteral("library")},
            where, AlgoTradingScreen::tr("Algo Trading › My Strategies › %1 (selects it; nothing runs)").arg(s.name));
        it.id = QStringLiteral("algo_trading.library.") + s.id.toLower();
        it.group = QString::fromLatin1(command::groups::kData);
        out.append(it);
    }
    return out;
}

/// The user's own strategies and their deployments, read on a worker (two small SELECTs of the
/// local database); library entries are the static rows above. Rows only navigate.
void algo_query_saved(const QString& query, quint64, command::ResultSink sink) {
    QThreadPool::globalInstance()->start([query, sink]() {
        QList<command::CommandItem> all;
        const QString screen = QStringLiteral("algo_trading");
        const QString where = algo_cmd_where();
        auto db = fincept::Database::instance().connection();
        {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT id, name, description, timeframe FROM algo_strategies "
                                      "WHERE is_active = 1 ORDER BY updated_at DESC LIMIT 200"))) {
                while (q.next()) {
                    const QString id = q.value(0).toString();
                    const QString name = q.value(1).toString().trimmed();
                    if (id.isEmpty() || name.isEmpty() || id.startsWith(QLatin1String("LIB-")))
                        continue;
                    command::CommandItem it = command_rows::section(
                        screen, QStringLiteral("strategy:") + id, name,
                        {q.value(2).toString(), q.value(3).toString(), QStringLiteral("algo strategy"),
                         QStringLiteral("my strategy")},
                        where,
                        AlgoTradingScreen::tr("Algo Trading › My Strategies › %1 (selects it; nothing runs)").arg(name));
                    it.id = QStringLiteral("algo_trading.strategy.") + id;
                    it.group = QString::fromLatin1(command::groups::kSaved);
                    all.append(it);
                }
            }
        }
        {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT id, strategy_name, symbol, underlying, mode, status FROM algo_deployments "
                                      "ORDER BY created_at DESC LIMIT 100"))) {
                while (q.next()) {
                    const QString id = q.value(0).toString();
                    const QString name = q.value(1).toString().trimmed();
                    const QString sym = q.value(2).toString().isEmpty() ? q.value(3).toString() : q.value(2).toString();
                    if (id.isEmpty() || name.isEmpty())
                        continue;
                    const QString mode = q.value(4).toString().toUpper();
                    const QString status = q.value(5).toString();
                    const QString title = sym.isEmpty() ? name : QStringLiteral("%1 · %2").arg(name, sym);
                    command::CommandItem it = command_rows::section(
                        screen, QStringLiteral("deployment:") + id, title,
                        {sym, mode, status, QStringLiteral("deployment"), QStringLiteral("algo")}, where,
                        AlgoTradingScreen::tr("Algo Trading › Dashboard (%1 deployment, %2)").arg(mode, status));
                    it.id = QStringLiteral("algo_trading.deployment.") + id;
                    it.group = QString::fromLatin1(command::groups::kSaved);
                    all.append(it);
                }
            }
        }
        sink(command_rows::best_matches(all, query, 6), true);
    });
}

void algo_install_commands() {
    command_rows::install(QStringLiteral("algo_trading"), &algo_command_rows);
    command::ProviderSpec p;
    p.id = QStringLiteral("algo_trading.saved");
    p.group = QString::fromLatin1(command::groups::kSaved);
    p.query = &algo_query_saved;
    p.owner = QStringLiteral("algo_trading");
    p.cap = 6;
    p.min_chars = 2;
    p.slow = true; // local database on a worker
    command::CommandRegistry::instance().register_provider(p);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(algo_install_commands)

} // namespace fincept::screens
