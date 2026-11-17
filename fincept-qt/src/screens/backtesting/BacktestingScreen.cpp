// src/screens/backtesting/BacktestingScreen.cpp
//
// The shell of the Backtesting tab: bands, the run pane, lazy pages, keyboard, state. Sub-tab content lives in
//   BtRunViews.h          OVERVIEW PERFORMANCE RISK TRADES EXPOSURE COSTS (BtOverviewView.cpp …)
//   BtWorkbenchTabs.h     ASSUMPTIONS · RUNS · COMPARE · RESEARCH
//   BtAnalysisTabs.h      OPTIMISE · WALK-FWD · MONTE CARLO (settings strips + panes around BtAnalysisViews.h)
//   BtSetupPanel.h        the input strip, the SETUP panel and the strategy picker
//   BtRunController.h     queue / progress / cancel / parse + save off the GUI thread
#include "screens/backtesting/BacktestingScreen.h"

#include "core/logging/Logger.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "screens/backtesting/BacktestResultData.h"
#include "screens/backtesting/BtAnalysisTabs.h"
#include "screens/backtesting/BtRunViews.h"
#include "screens/backtesting/BtSetupPanel.h"
#include "screens/backtesting/BtTabKit.h"
#include "screens/backtesting/BtWorkbenchTabs.h"
#include "screens/common/CommandRows.h"
#include "services/backtesting/BacktestExportText.h"
#include "services/backtesting/BacktestRunStore.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/StatTile.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using namespace fincept::services::backtest;
using namespace fincept::screens::bt;
namespace fmt = fincept::ui::formatting;

namespace {

const char* const kTabIds[] = {"overview", "performance", "risk",        "trades", "exposure", "costs",   "optimize",
                               "walk_forward", "monte_carlo", "compare", "assumptions", "runs", "research"};

int bt_scr_tab_index(const QString& id) {
    for (int i = 0; i < static_cast<int>(sizeof(kTabIds) / sizeof(kTabIds[0])); ++i)
        if (id == QLatin1String(kTabIds[i]))
            return i;
    return -1;
}

/// Lower-case words of `s` (anything that is not a letter or digit separates).
QString bt_scr_norm(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (const QChar c : s.toLower())
        out += c.isLetterOrNumber() ? c : QLatin1Char(' ');
    return out.simplified();
}

/// The catalogue strategy a deep link names — its id, its name, or words of them ("sma", "rsi macd",
/// "bollinger"); a runnable one first. Empty when none matches.
QString bt_scr_match_strategy(const StrategyCatalog& cat, const QString& text) {
    const QString want = bt_scr_norm(text);
    if (want.isEmpty())
        return {};
    const QString compact = QString(want).remove(QLatin1Char(' '));
    for (const Strategy& s : cat.strategies) {
        const QString id = bt_scr_norm(s.id);
        if (id == want || bt_scr_norm(s.name) == want || QString(id).remove(QLatin1Char(' ')) == compact)
            return s.id;
    }
    const QStringList words = want.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString fallback;
    for (const Strategy& s : cat.strategies) {
        const QStringList hay = (bt_scr_norm(s.name) + QLatin1Char(' ') + bt_scr_norm(s.id) + QLatin1Char(' ') +
                                 bt_scr_norm(s.category))
                                    .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        bool all = true;
        for (const QString& w : words) {
            bool hit = false;
            for (const QString& h : hay)
                if (h.startsWith(w)) {
                    hit = true;
                    break;
                }
            if (!hit) {
                all = false;
                break;
            }
        }
        if (!all)
            continue;
        if (s.supported)
            return s.id;
        if (fallback.isEmpty())
            fallback = s.id;
    }
    return fallback;
}

/// "golden cross" / "death cross": the SMA crossover with the classic 50 / 200 windows.
bool bt_scr_is_golden_cross(const QString& text) {
    const QString n = bt_scr_norm(text);
    return n == QLatin1String("golden cross") || n == QLatin1String("golden crossover") ||
           n == QLatin1String("death cross");
}

/// One strategy of a catalogue the session has loaded (for the search box's strategy rows).
struct BtScrCatalogRow {
    QString engine;
    QString id;
    QString name;
    QString category;
    bool supported = true;
};

/// engine slug -> its catalogue's rows, as last delivered (GUI thread only).
QHash<QString, QVector<BtScrCatalogRow>>& bt_scr_catalog_rows() {
    static QHash<QString, QVector<BtScrCatalogRow>> rows;
    return rows;
}

QVector<double> bt_scr_spark(const QVector<double>& v, int n = 60) {
    QVector<double> clean;
    clean.reserve(v.size());
    for (double x : v)
        if (std::isfinite(x))
            clean.append(x);
    return btview::thin(clean, n);
}

/// ENGINE menu tooltip: what the engine applies (its own capabilities when known, else the built-in table).
QString bt_scr_engine_tip(const ProviderSupport& s, int commands) {
    QStringList caps;
    caps << (s.short_selling ? BacktestingScreen::tr("long / short") : BacktestingScreen::tr("long-only"));
    caps << (s.multi_symbol ? BacktestingScreen::tr("multi-symbol") : BacktestingScreen::tr("first symbol only"));
    caps << (s.intraday ? BacktestingScreen::tr("intraday bars") : BacktestingScreen::tr("daily bars"));
    if (!s.stops)
        caps << BacktestingScreen::tr("no stops");
    caps << BacktestingScreen::tr("%n command(s)", nullptr, commands);
    return caps.join(QStringLiteral(" · "));
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

BacktestingScreen::BacktestingScreen(QWidget* parent) : QWidget(parent) {
    QElapsedTimer t;
    t.start();
    setObjectName(QStringLiteral("btScreen"));
    setAttribute(Qt::WA_StyledBackground, true);
    providers_ = all_providers();
    // The root stylesheet goes on BEFORE the children exist (each child is polished once, when shown).
    refresh_theme();
    controller_ = new BtRunController(this);
    selection_ = new BtSelection(this);
    memory_ = new ui::InputMemory(QStringLiteral("backtesting"), QStringLiteral("setup"), this);
    // Load the stored document first: every input the strip / panel track below gets its value at once,
    // before their change signals are connected.
    memory_->restore();
    const QString remembered = memory_->value(QStringLiteral("engine"), engine_).toString();
    for (const Provider& p : providers_)
        if (p.slug == remembered)
            engine_ = remembered;
    build_ui();
    install_shortcuts();

    auto& svc = BacktestingService::instance();
    connect(&svc, &BacktestingService::result_ready, this,
            [this](const QString& provider, const QString& command, const QJsonObject& data) {
                if (command == QLatin1String("get_strategies"))
                    on_catalog(provider, data);
            });
    connect(&svc, &BacktestingService::error_occurred, this, &BacktestingScreen::on_catalog_failed);
    connect(&svc, &BacktestingService::capabilities_loaded, this,
            [this](const QString& provider, const QJsonObject& caps) {
                if (engine_caps_.value(provider) == caps)
                    return;
                engine_caps_.insert(provider, caps);
                apply_engine_support(); // re-gates the current engine's inputs; refreshes every tooltip
            });
    connect(&svc, &BacktestingService::pending_portfolio_config_set, this, [this]() {
        if (!isVisible())
            return; // showEvent takes it
        const QJsonObject config = BacktestingService::instance().take_pending_portfolio_config();
        if (!config.isEmpty())
            apply_portfolio_config(config);
    });

    connect(controller_, &BtRunController::progress, this, [this](quint64 token, const BtProgress& p) {
        // A determinate bar on the skeleton once the engine reports counts (NaN fraction = text only).
        if (token == run_token_ && run_pane_)
            run_pane_->set_progress(run_request_, p.fraction(), progress_text(p));
        update_run_state();
    });
    connect(controller_, &BtRunController::state_changed, this, &BacktestingScreen::update_run_state);
    connect(controller_, &BtRunController::finished, this, [this](quint64 token, const BtBundlePtr& b) {
        if (runs_tab_)
            runs_tab_->refresh();
        if (token != run_token_ || !b)
            return;
        run_token_ = 0;
        run_pane_->deliver(run_request_, QJsonObject{{QStringLiteral("key"), b->key}}, b->as_of.toLocalTime());
        update_run_state();
    });
    connect(controller_, &BtRunController::failed, this, [this](quint64 token, const BtOutcome& o) {
        if (token != run_token_)
            return;
        run_token_ = 0;
        run_pane_->fail(run_request_, o.message, bt_failure_title(o.error_code), o.technical);
        status_->show_notice(tr("Backtest failed: %1").arg(o.message.left(160)), QString(), {}, 9000,
                             ui::ScreenStatusBar::Tone::Danger);
        update_run_state();
    });
    connect(controller_, &BtRunController::cancelled, this, [this](quint64 token) {
        if (token != run_token_)
            return;
        run_token_ = 0;
        run_pane_->cancel();
        update_run_state();
    });

    connect(selection_, &BtSelection::tab_requested, this, [this](const QString& id) {
        const int i = bt_scr_tab_index(id);
        if (i >= 0)
            tabs_->setCurrentIndex(i);
    });

    signature_timer_ = new QTimer(this);
    signature_timer_->setSingleShot(true);
    signature_timer_->setInterval(150);
    connect(signature_timer_, &QTimer::timeout, this, &BacktestingScreen::update_signature);
    ticker_ = new QTimer(this);
    ticker_->setInterval(1000);
    connect(ticker_, &QTimer::timeout, this, &BacktestingScreen::update_run_state);

    fmt::on_format_changed(this, [this]() {
        update_ribbon();
        update_status_bar();
    });
    LOG_DEBUG("Backtesting", logf("Screen constructed", {{"ms", t.elapsed()}}));
}

BacktestingScreen::~BacktestingScreen() {
    if (controller_)
        controller_->cancel_all();
}

void BacktestingScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_command_bar();
    root->addWidget(bar_);
    progress_ = new QProgressBar(this);
    progress_->setObjectName(QStringLiteral("btProgress"));
    progress_->setTextVisible(false);
    progress_->setMinimumHeight(3);
    progress_->setMaximumHeight(3);
    progress_->setRange(0, 1000);
    progress_->setValue(0);
    progress_->setVisible(false);
    root->addWidget(progress_);

    strip_ = new BtInputStrip(memory_, this);
    root->addWidget(strip_);
    bar_->set_segment(QStringLiteral("range"), strip_->range_preset()); // the remembered preset ("CUSTOM" = none lit)
    auto* sl = static_cast<QHBoxLayout*>(strip_->state_area()->layout());
    state_chip_ = make_chip(tr("READY"), Tone::Ok, strip_->state_area());
    as_of_label_ = new QLabel(strip_->state_area());
    as_of_label_->setObjectName(QStringLiteral("btNote"));
    warnings_btn_ = make_tool_button(QString(), tr("Warnings and assumptions of this result"), strip_->state_area());
    warnings_btn_->hide();
    connect(warnings_btn_, &QPushButton::clicked, this, [this]() { tabs_->setCurrentIndex(TabAssumptions); });
    sl->addWidget(state_chip_);
    sl->addWidget(as_of_label_);
    sl->addWidget(warnings_btn_);

    build_ribbon();
    root->addWidget(ribbon_box_);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setObjectName(QStringLiteral("btSplitter"));
    splitter_->setChildrenCollapsible(true);
    setup_ = new BtSetupPanel(memory_, splitter_);
    setup_->setMinimumWidth(QFontMetrics(font()).averageCharWidth() * 34);
    auto* right = new QWidget(splitter_);
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);
    tabs_ = new QTabBar(right);
    tabs_->setObjectName(QStringLiteral("btTabs"));
    tabs_->setUsesScrollButtons(true);
    tabs_->setExpanding(false);
    tabs_->setElideMode(Qt::ElideNone);
    tabs_->setDrawBase(false);
    for (int i = 0; i < TabCount; ++i)
        tabs_->addTab(QString());
    retranslate();
    rv->addWidget(tabs_);
    pages_ = new QStackedWidget(right);
    rv->addWidget(pages_, 1);
    splitter_->addWidget(setup_);
    splitter_->addWidget(right);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    root->addWidget(splitter_, 1);

    build_status_bar();
    root->addWidget(status_);

    build_run_pane();
    page_slots_.resize(TabCount);
    for (int i = 0; i < TabCount; ++i)
        page_slots_[i] = is_run_tab(i) ? static_cast<QWidget*>(run_pane_) : nullptr;
    connect(tabs_, &QTabBar::currentChanged, this, &BacktestingScreen::on_tab_changed);

    connect(setup_, &BtSetupPanel::changed, this, &BacktestingScreen::schedule_signature);
    connect(setup_, &BtSetupPanel::strategy_changed, this, [this]() {
        bar_->set_action_text(QStringLiteral("strategy"), setup_->strategy_label().isEmpty()
                                                              ? tr("STRATEGY")
                                                              : tr("STRATEGY: %1").arg(setup_->strategy_label().toUpper()));
        schedule_signature();
    });
    connect(strip_, &BtInputStrip::changed, this, &BacktestingScreen::schedule_signature);
    connect(strip_, &BtInputStrip::symbols_changed, this, [this]() {
        apply_regional_defaults();
        if (link_group_ != SymbolGroup::None && !strip_->symbols().isEmpty())
            SymbolContext::instance().set_group_symbol(link_group_, current_symbol(), this);
    });
    connect(strip_, &BtInputStrip::range_preset_changed, this,
            [this](const QString& preset) { bar_->set_segment(QStringLiteral("range"), preset); });
    connect(strip_, &BtInputStrip::unresolved, this, [this](const QString& message) {
        status_->show_notice(message, QString(), {}, 8000, ui::ScreenStatusBar::Tone::Warn);
    });
    apply_regional_defaults();
}

void BacktestingScreen::build_command_bar() {
    bar_ = new ui::ScreenCommandBar(this);
    bar_->set_title(tr("BACKTEST"));
    engine_menu_ = new QMenu(this);
    for (const Provider& p : providers_) {
        QAction* a = engine_menu_->addAction(p.display_name);
        a->setData(p.slug);
        a->setCheckable(true);
        a->setToolTip(bt_scr_engine_tip(provider_support_from_capabilities(p.slug, engine_caps_.value(p.slug)),
                                        static_cast<int>(p.commands.size())));
        connect(a, &QAction::triggered, this, [this, slug = p.slug]() { set_engine(slug); });
    }
    engine_menu_->setToolTipsVisible(true);
    bar_->add_menu_action(QStringLiteral("engine"), tr("ENGINE"), engine_menu_, QString(),
                          tr("The backtesting engine (each states its fill model, costs and limits on the result)"), 95);
    bar_->add_action(QStringLiteral("strategy"), tr("STRATEGY"), QStringLiteral("F4"), tr("Pick a strategy (F4)"),
                     ui::ScreenCommandBar::Role::Normal, 90);
    bar_->add_segmented(QStringLiteral("range"),
                        {{QStringLiteral("1Y"), tr("1Y"), QString(), tr("One year to yesterday")},
                         {QStringLiteral("3Y"), tr("3Y"), QString(), tr("Three years")},
                         {QStringLiteral("5Y"), tr("5Y"), QString(), tr("Five years")},
                         {QStringLiteral("10Y"), tr("10Y"), QString(), tr("Ten years")},
                         {QStringLiteral("MAX"), tr("MAX"), QString(), tr("From 1990 (the engine starts at the first bar)")}},
                        70);
    bar_->set_segment(QStringLiteral("range"), strip_ ? strip_->range_preset() : QStringLiteral("5Y"));
    bar_->add_separator(60);
    bar_->add_action(QStringLiteral("pin"), tr("PIN ★"), QStringLiteral("Ctrl+S"),
                     tr("Pin the active run in RUNS with a label (pinned runs are never dropped)"),
                     ui::ScreenCommandBar::Role::Normal, 40);
    export_menu_ = new QMenu(this);
    connect(export_menu_, &QMenu::aboutToShow, this, [this]() {
        export_menu_->clear();
        const bool has = active_ && active_->run;
        auto add = [this](const QString& text, const QString& kind, bool enabled) {
            QAction* a = export_menu_->addAction(text);
            a->setEnabled(enabled);
            connect(a, &QAction::triggered, this, [this, kind]() { export_kind(kind); });
        };
        add(tr("Trades CSV"), QStringLiteral("trades"), has);
        add(tr("Equity CSV (with benchmark, drawdown, exposure)"), QStringLiteral("equity"), has);
        add(tr("Metrics CSV"), QStringLiteral("metrics"), has);
        add(tr("Drawdowns CSV"), QStringLiteral("drawdowns"), has);
        add(tr("Result JSON"), QStringLiteral("json"), has);
        export_menu_->addSeparator();
        add(tr("Optimisation CSV"), QStringLiteral("optimize"), optimize_tab_ && optimize_tab_->current());
        add(tr("Walk-forward CSV"), QStringLiteral("walk_forward"), walk_forward_tab_ && walk_forward_tab_->current());
        add(tr("Monte Carlo CSV"), QStringLiteral("monte_carlo"), monte_carlo_tab_ && monte_carlo_tab_->current());
    });
    bar_->add_menu_action(QStringLiteral("export"), tr("EXPORT"), export_menu_, QStringLiteral("Ctrl+E"),
                          tr("CSV / JSON with plain numbers and the ISO currency code"), 35);
    bar_->add_toggle(QStringLiteral("setup"), tr("SETUP"), true, QStringLiteral("Ctrl+\\"),
                     tr("Show / hide the SETUP panel"), 30);
    bar_->add_action(QStringLiteral("run"), tr("RUN BACKTEST"), QStringLiteral("Ctrl+↵"),
                     tr("Backtest this setup (Ctrl+Enter runs the open tab's own analysis)"),
                     ui::ScreenCommandBar::Role::Primary, 100);
    bar_->set_overflow_text(tr("More"));
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, [this](const QString& id) {
        if (id == QLatin1String("run")) {
            if (run_token_)
                run_pane_->cancel();
            else
                run_pane_->run();
        } else if (id == QLatin1String("strategy")) {
            setup_->open_picker(bar_->action(QStringLiteral("strategy")));
        } else if (id == QLatin1String("pin")) {
            pin_active_run();
        }
    });
    connect(bar_, &ui::ScreenCommandBar::action_toggled, this, [this](const QString& id, bool on) {
        if (id == QLatin1String("setup")) {
            setup_->setVisible(on);
            setup_collapsed_ = !on;
        }
    });
    connect(bar_, &ui::ScreenCommandBar::segment_selected, this, [this](const QString& id, const QString& key) {
        if (id == QLatin1String("range"))
            strip_->apply_range_preset(key);
    });
}

void BacktestingScreen::build_ribbon() {
    ribbon_box_ = new QWidget(this);
    ribbon_box_->setObjectName(QStringLiteral("btRibbon"));
    ribbon_box_->setAttribute(Qt::WA_StyledBackground, true);
    auto* h = new QHBoxLayout(ribbon_box_);
    h->setContentsMargins(6, 4, 6, 4);
    ribbon_ = new ui::KpiStrip(ribbon_box_);
    ribbon_->set_spacing(6);
    ribbon_->set_max_columns(10);
    ribbon_->set_min_tile_width(QFontMetrics(font()).averageCharWidth() * 13);
    struct T {
        const char* key;
        QString label;
        int tab;
    };
    const T tiles[] = {{"ret", tr("TOTAL RETURN"), TabPerformance}, {"cagr", tr("CAGR"), TabPerformance},
                       {"sharpe", tr("SHARPE"), TabRisk},           {"sortino", tr("SORTINO"), TabRisk},
                       {"maxdd", tr("MAX DRAWDOWN"), TabRisk},      {"vol", tr("VOLATILITY"), TabRisk},
                       {"win", tr("WIN RATE"), TabTrades},          {"pf", tr("PROFIT FACTOR"), TabTrades},
                       {"expo", tr("EXPOSURE"), TabExposure},       {"vs", tr("VS BENCHMARK"), TabOverview}};
    for (const T& t : tiles) {
        ui::StatTile* tile = ribbon_->add_tile(QLatin1String(t.key), t.label);
        tile->set_clickable(true);
        tile->set_state(ui::StatTile::State::Loading);
        const int tab = t.tab;
        connect(tile, &ui::StatTile::clicked, this, [this, tab]() { tabs_->setCurrentIndex(tab); });
    }
    ribbon_->tile(QStringLiteral("vol"))->set_sparkline_color(ui::StatTile::SparkColor::Neutral);
    ribbon_->tile(QStringLiteral("vol"))->set_delta_inverted(true);
    ribbon_->tile(QStringLiteral("maxdd"))->set_sparkline_color(ui::StatTile::SparkColor::Neutral);
    ribbon_->tile(QStringLiteral("sharpe"))->set_sparkline_color(ui::StatTile::SparkColor::Accent);
    h->addWidget(ribbon_);
}

void BacktestingScreen::build_status_bar() {
    using SB = ui::ScreenStatusBar;
    status_ = new SB(this);
    status_->add_item(QStringLiteral("engine"), SB::Segment::Left, QString(), SB::Tone::Accent, 100);
    status_->add_item(QStringLiteral("data"), SB::Segment::Left, QString(), SB::Tone::Normal, 90);
    status_->add_item(QStringLiteral("fill"), SB::Segment::Centre, QString(), SB::Tone::Normal, 80);
    status_->add_item(QStringLiteral("costs"), SB::Segment::Centre, QString(), SB::Tone::Normal, 70);
    status_->add_item(QStringLiteral("rf"), SB::Segment::Centre, QString(), SB::Tone::Dim, 50);
    status_->add_item(QStringLiteral("ccy"), SB::Segment::Right, QString(), SB::Tone::Dim, 60);
    status_->add_item(QStringLiteral("warn"), SB::Segment::Right, QString(), SB::Tone::Warn, 85);
    status_->set_chip(QStringLiteral("warn"), true);
    status_->set_item_visible(QStringLiteral("warn"), false);
    status_->add_item(QStringLiteral("duration"), SB::Segment::Right, QString(), SB::Tone::Dim, 40);
    status_->add_item(QStringLiteral("progress"), SB::Segment::Centre, QString(), SB::Tone::Accent, 95);
    status_->set_item_visible(QStringLiteral("progress"), false);
    status_->set_clock(true, false);
    update_status_bar();
}

void BacktestingScreen::build_run_pane() {
    run_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, pages_);
    run_stack_ = new QStackedWidget(run_pane_->content_parent());
    run_pane_->set_content(run_stack_);
    for (int i = 0; i < TabCount; ++i) {
        if (!is_run_tab(i))
            continue;
        auto* slot = new QWidget(run_stack_);
        run_stack_->addWidget(slot);
        run_slots_.insert(i, slot);
    }
    pages_->addWidget(run_pane_);
    run_pane_->set_requirements({ui::Requirement::python()});
    run_pane_->set_max_age(ui::autoload::kNeverRefresh); // a backtest is deterministic: never re-run by itself
    run_pane_->set_persist_limit(0);                    // the run store is the memory
    run_pane_->set_timeout(180);
    run_pane_->set_status_label(as_of_label_);
    run_pane_->set_loading_text(tr("Preparing the backtest…"));
    run_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    run_pane_->skeleton()->set_kpi_count(10);
    run_pane_->set_remembered_signed_source(
        [this](const QString&, QJsonValue* payload, QDateTime* as_of, QString* signature) {
            const SavedRun s = controller_->latest_saved(QStringLiteral("backtest"));
            if (s.id <= 0)
                return false;
            *payload = QJsonObject{{QStringLiteral("store"), QString::number(s.id)}};
            QDateTime t = QDateTime::fromString(s.created_at, Qt::ISODate);
            t.setTimeZone(QTimeZone::UTC);
            *as_of = t.toLocalTime();
            *signature = s.signature();
            return true;
        });
    run_pane_->set_start([this](quint64 id) { start_run_pane(id); });
    run_pane_->set_cancel([this](quint64) {
        pending_start_ = 0;
        const quint64 tok = run_token_;
        run_token_ = 0;
        if (tok)
            controller_->cancel(tok);
        update_run_state();
    });
    run_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_run_ticket(p); });
    connect(run_pane_, &ui::AutoLoadPane::notice_action, this, [this]() { tabs_->setCurrentIndex(TabAssumptions); });
    connect(run_pane_, &ui::AutoLoadPane::state_changed, this, [this]() {
        update_run_state();
        update_ribbon();
    });
}

void BacktestingScreen::install_shortcuts() {
    auto add = [this](const QKeySequence& seq, std::function<void()> fn) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, std::move(fn));
    };
    add(QKeySequence(Qt::CTRL | Qt::Key_Return), [this]() { run_primary(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Enter), [this]() { run_primary(); });
    add(QKeySequence(Qt::Key_Escape), [this]() { cancel_current(); });
    add(QKeySequence(Qt::Key_F4), [this]() { setup_->open_picker(bar_->action(QStringLiteral("strategy"))); });
    add(QKeySequence(Qt::CTRL | Qt::Key_L), [this]() { strip_->focus_symbols(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_B), [this]() { strip_->focus_benchmark(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_S), [this]() { pin_active_run(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_E), [this]() {
        if (QPushButton* b = bar_->action(QStringLiteral("export")))
            export_menu_->popup(b->mapToGlobal(QPoint(0, b->height())));
    });
    add(QKeySequence(Qt::CTRL | Qt::Key_Backslash), [this]() {
        const bool on = !setup_->isVisible();
        setup_->setVisible(on);
        setup_collapsed_ = !on;
        bar_->set_action_checked(QStringLiteral("setup"), on);
    });
    add(QKeySequence(Qt::CTRL | Qt::Key_PageDown), [this]() { tabs_->setCurrentIndex((tabs_->currentIndex() + 1) % TabCount); });
    add(QKeySequence(Qt::CTRL | Qt::Key_PageUp),
        [this]() { tabs_->setCurrentIndex((tabs_->currentIndex() + TabCount - 1) % TabCount); });
    add(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), [this]() {
        if (active_ && active_->store_id > 0) {
            page_for(TabCompare);
            compare_tab_->add_run(active_->store_id);
            tabs_->setCurrentIndex(TabCompare);
        }
    });
    add(QKeySequence(Qt::CTRL | Qt::Key_F), [this]() {
        if (tabs_->currentIndex() == TabTrades)
            if (auto* v = qobject_cast<BtTradesView*>(run_view(TabTrades)))
                v->focus_filter();
    });
}

void BacktestingScreen::retranslate() {
    const QString labels[TabCount] = {tr("OVERVIEW"),    tr("PERFORMANCE"), tr("RISK"),    tr("TRADES"),
                                      tr("EXPOSURE"),    tr("COSTS"),       tr("OPTIMISE"), tr("WALK-FWD"),
                                      tr("MONTE CARLO"), tr("COMPARE"),     tr("ASSUMPTIONS"), tr("RUNS"),
                                      tr("RESEARCH")};
    for (int i = 0; i < TabCount && i < tabs_->count(); ++i)
        tabs_->setTabText(i, labels[i]);
    update_tab_badges();
}

// ═════════════════════════════════════════════════════════════════════════════
// Theme — one root stylesheet from tokens, set again only when its text changes
// ═════════════════════════════════════════════════════════════════════════════

void BacktestingScreen::refresh_theme() {
    using namespace fincept::ui;
    const QString base = colors::BG_BASE, surface = colors::BG_SURFACE, raised = colors::BG_RAISED,
                  hover = colors::BG_HOVER, bdim = colors::BORDER_DIM, bmed = colors::BORDER_MED,
                  bbright = colors::BORDER_BRIGHT, tp = colors::TEXT_PRIMARY, ts = colors::TEXT_SECONDARY,
                  tt = colors::TEXT_TERTIARY, td = colors::TEXT_DIM, accent = colors::AMBER,
                  on_accent = colors::TEXT_ON_ACCENT, pos = colors::POSITIVE, neg = colors::NEGATIVE,
                  warn = colors::WARNING, ok = colors::SUCCESS, danger = colors::DANGER, abg = colors::ACCENT_BG,
                  alt = colors::ROW_ALT;
    QString css;
    css += QStringLiteral("#btScreen{background:%1;}").arg(base);
    css += QStringLiteral("#btScreen QWidget{font-size:%1px;}").arg(ui::fonts::px(0));
    css += QStringLiteral("QFrame#btPanel{background:%1;border:1px solid %2;}").arg(surface, bdim);
    css += QStringLiteral("#btPanelBody{background:transparent;}");
    css += QStringLiteral("QLabel#btKvKey{color:%1;font-size:%2px;}").arg(tt).arg(ui::fonts::px(-2));
    css += QStringLiteral("QLabel#btKvValue{color:%1;font-weight:600;font-size:%2px;}").arg(tp).arg(ui::fonts::px(-1));
    css += QStringLiteral("QLabel#btNote{color:%1;font-size:%2px;}").arg(ts).arg(ui::fonts::px(-3));
    css += QStringLiteral("QLabel#btHero{color:%1;font-weight:700;font-size:%2px;}").arg(tp).arg(ui::fonts::px(8));
    css += QStringLiteral("QLabel#btSectionTitle{color:%1;font-weight:700;letter-spacing:1px;font-size:%2px;}")
               .arg(tt)
               .arg(ui::fonts::px(-4));
    css += QStringLiteral("QLabel#btFieldLabel{color:%1;font-weight:700;font-size:%2px;}").arg(ts).arg(ui::fonts::px(-3));
    css += QStringLiteral("QLabel#btFieldLabel[changed=\"true\"]{color:%1;}").arg(accent);
    css += QStringLiteral("QLabel#btFieldLabel[error=\"true\"]{color:%1;}").arg(danger);
    css += QStringLiteral("QLabel#btChip{border:1px solid %1;color:%2;padding:1px 6px;font-weight:700;font-size:%3px;}")
               .arg(bmed, ts)
               .arg(ui::fonts::px(-3));
    // tones (text colour; chips also take the border)
    const QPair<const char*, QString> tones[] = {{"pos", pos},  {"neg", neg},   {"warn", warn},  {"dim", td},
                                                 {"accent", accent}, {"ok", ok}, {"danger", danger}};
    for (const auto& t : tones) {
        // An id selector outranks an attribute one: "QLabel#btChip{color}" beats a bare
        // "QLabel[tone]", so the tone rule names every toned id too, or it never colours them.
        css += QStringLiteral("QLabel[tone=\"%1\"],QPushButton[tone=\"%1\"],QLabel#btChip[tone=\"%1\"],"
                              "QLabel#btNote[tone=\"%1\"],QLabel#btHero[tone=\"%1\"],QLabel#btKvKey[tone=\"%1\"],"
                              "QLabel#btKvValue[tone=\"%1\"],QPushButton#btToolButton[tone=\"%1\"]{color:%2;}")
                   .arg(QLatin1String(t.first), t.second);
        css += QStringLiteral("QLabel#btChip[tone=\"%1\"]{border-color:%2;}").arg(QLatin1String(t.first), t.second);
    }
    css += QStringLiteral("QPushButton#btToolButton{background:%1;border:1px solid %2;color:%3;padding:2px 8px;"
                          "font-weight:700;font-size:%4px;}")
               .arg(raised, bmed, tp)
               .arg(ui::fonts::px(-3));
    css += QStringLiteral("QPushButton#btToolButton:hover{border-color:%1;background:%2;}").arg(bbright, hover);
    css += QStringLiteral("QPushButton#btToolButton:disabled{color:%1;border-color:%2;}").arg(td, bdim);
    css += QStringLiteral("QPushButton#btToolButton[active=\"true\"]{color:%1;border-color:%1;}").arg(accent);
    css += QStringLiteral("QPushButton#btSeg{background:%1;border:1px solid %2;color:%3;padding:2px 8px;"
                          "font-weight:700;font-size:%4px;}")
               .arg(raised, bmed, ts)
               .arg(ui::fonts::px(-3));
    css += QStringLiteral("QPushButton#btSeg:checked{background:%1;color:%2;border-color:%1;}").arg(accent, on_accent);
    css += QStringLiteral("QPushButton#btSeg:disabled{color:%1;}").arg(td);
    css += QStringLiteral("QPushButton#btPrimary{background:%1;color:%2;border:1px solid %1;padding:3px 12px;"
                          "font-weight:700;font-size:%3px;}")
               .arg(accent, on_accent)
               .arg(ui::fonts::px(-2));
    css += QStringLiteral("QPushButton#btPrimary:disabled{background:%1;color:%2;border-color:%3;}").arg(raised, td, bdim);
    css += QStringLiteral("QPushButton#btPrimary[tone=\"danger\"]{background:transparent;color:%1;border-color:%1;}").arg(danger);
    css += QStringLiteral("QLineEdit#btFilter,QComboBox#btCombo{background:%1;border:1px solid %2;color:%3;"
                          "padding:1px 4px;font-size:%4px;}")
               .arg(base, bmed, tp)
               .arg(ui::fonts::px(-2));
    css += QStringLiteral("QTableView#btTable{background:%1;alternate-background-color:%2;border:none;color:%3;"
                          "selection-background-color:%4;selection-color:%3;font-size:%5px;}")
               .arg(base, alt, tp, abg)
               .arg(ui::fonts::px(-2));
    css += QStringLiteral("QTableView#btTable QHeaderView::section{background:%1;color:%2;border:none;"
                          "border-bottom:1px solid %3;padding:2px 6px;font-weight:700;font-size:%4px;}")
               .arg(surface, tt, bdim)
               .arg(ui::fonts::px(-4));
    css += QStringLiteral("QScrollArea#btScroll{background:transparent;border:none;}");
    css += QStringLiteral("QScrollArea#btSetup,#btSetupBody{background:%1;border:none;}").arg(surface);
    css += QStringLiteral("QScrollArea#btSetup{border-right:1px solid %1;}").arg(bdim);
    css += QStringLiteral("QToolButton#btSectionHead{text-align:left;background:%1;color:%2;border:none;"
                          "border-bottom:1px solid %3;padding:4px 8px;font-weight:700;font-size:%4px;}")
               .arg(raised, ts, bdim)
               .arg(ui::fonts::px(-3));
    css += QStringLiteral("QToolButton#btSectionHead:hover{color:%1;}").arg(tp);
    css += QStringLiteral("QPushButton#btStrategyButton{text-align:left;background:%1;border:1px solid %2;color:%3;"
                          "padding:4px 8px;font-weight:700;font-size:%4px;}")
               .arg(base, bmed, tp)
               .arg(ui::fonts::px(-1));
    css += QStringLiteral("QPushButton#btStrategyButton:hover{border-color:%1;}").arg(accent);
    css += QStringLiteral("QToolButton#btParamReset{color:%1;border:none;background:transparent;}").arg(accent);
    css += QStringLiteral("#btInputStrip,#btSettingsStrip{background:%1;border-bottom:1px solid %2;}").arg(surface, bdim);
    css += QStringLiteral("#btSettingsExtra{background:%1;border-bottom:1px solid %2;}").arg(surface, bdim);
    css += QStringLiteral("QToolButton#btSymbolChip{background:%1;border:1px solid %2;color:%3;padding:1px 6px;"
                          "font-weight:700;font-size:%4px;}")
               .arg(raised, bmed, tp)
               .arg(ui::fonts::px(-2));
    css += QStringLiteral("QToolButton#btSymbolChip:hover{border-color:%1;}").arg(danger);
    css += QStringLiteral("QToolButton#btSymbolChip[dim=\"true\"]{color:%1;}").arg(td);
    css += QStringLiteral("QDateEdit[error=\"true\"]{border:1px solid %1;}").arg(danger);
    css += QStringLiteral("#btRibbon{background:%1;border-bottom:1px solid %2;}").arg(base, bdim);
    css += QStringLiteral("QTabBar#btTabs{background:%1;border-bottom:1px solid %2;}").arg(surface, bdim);
    css += QStringLiteral("QTabBar#btTabs::tab{background:%1;color:%2;padding:4px 12px;border:none;"
                          "border-bottom:2px solid transparent;font-weight:700;font-size:%3px;}")
               .arg(surface, ts)
               .arg(ui::fonts::px(-3));
    css += QStringLiteral("QTabBar#btTabs::tab:selected{color:%1;border-bottom-color:%1;background:%2;}").arg(accent, base);
    css += QStringLiteral("QTabBar#btTabs::tab:hover{color:%1;}").arg(tp);
    css += QStringLiteral("QProgressBar#btProgress{background:%1;border:none;}").arg(surface);
    css += QStringLiteral("QProgressBar#btProgress::chunk{background:%1;}").arg(accent);
    css += QStringLiteral("QFrame#btPicker{background:%1;border:1px solid %2;}").arg(surface, bbright);
    css += QStringLiteral("QListView#btPickerList{background:%1;border:1px solid %2;color:%3;}").arg(base, bdim, tp);
    css += QStringLiteral("QListView#btPickerList::item{padding:3px 4px;}");
    css += QStringLiteral("QListView#btPickerList::item:selected{background:%1;color:%2;}").arg(abg, tp);
    css += QStringLiteral("QSplitter#btSplitter::handle{background:%1;width:1px;}").arg(bdim);
    if (css == last_stylesheet_)
        return;
    last_stylesheet_ = css;
    setStyleSheet(css);
}

// ═════════════════════════════════════════════════════════════════════════════
// Show / hide / resize
// ═════════════════════════════════════════════════════════════════════════════

void BacktestingScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    status_->start_clock();
    if (first_show_) {
        first_show_ = false;
        QElapsedTimer t;
        t.start();
        set_engine(engine_);
        if (!restored_sizes_.isEmpty())
            splitter_->setSizes(restored_sizes_);
        else
            splitter_->setSizes({QFontMetrics(font()).averageCharWidth() * 40, width()});
        if (setup_collapsed_) {
            setup_->hide();
            bar_->set_action_checked(QStringLiteral("setup"), false);
        }
        tabs_->setCurrentIndex(restored_tab_);
        on_tab_changed(restored_tab_);
        // A v1 session kept its last result in the screen state: move it into the run store once.
        if (!pending_restore_payload_.isEmpty()) {
            const QJsonObject payload = pending_restore_payload_;
            pending_restore_payload_ = QJsonObject();
            const QString engine = engine_;
            (void)QtConcurrent::run([payload, engine]() {
                auto& store = BacktestRunStore::instance();
                auto latest = store.latest(QStringLiteral("backtest"));
                if (latest.is_err() || latest.value().id > 0)
                    return;
                QJsonObject meta{{QStringLiteral("providerSlug"), engine}, {QStringLiteral("command"), QStringLiteral("backtest")}};
                auto id = store.save(meta, QJsonObject(), payload, QString());
                if (id.is_ok())
                    store.rename(id.value(), QCoreApplication::translate("fincept::screens::BacktestingScreen",
                                                                         "Restored from previous session"));
            });
        }
        LOG_DEBUG("Backtesting", logf("First show", {{"ms", t.elapsed()}}));
    }
    const QJsonObject config = BacktestingService::instance().take_pending_portfolio_config();
    if (!config.isEmpty())
        apply_portfolio_config(config);
    update_run_state();
}

void BacktestingScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    status_->stop_clock();
    ticker_->stop(); // no timer while hidden; a user run keeps going
}

void BacktestingScreen::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void BacktestingScreen::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    const bool narrow = e->size().width() < 1300;
    if (narrow == ribbon_compact_)
        return;
    ribbon_compact_ = narrow;
    for (int i = 0; i < ribbon_->count(); ++i)
        if (ui::StatTile* t = ribbon_->tile(i))
            t->set_compact(narrow);
}

// ═════════════════════════════════════════════════════════════════════════════
// Engine and catalogue
// ═════════════════════════════════════════════════════════════════════════════

QString BacktestingScreen::engine_label() const {
    for (const Provider& p : providers_)
        if (p.slug == engine_)
            return p.display_name;
    return engine_;
}

bool BacktestingScreen::engine_offers(const QString& command) const {
    for (const Provider& p : providers_)
        if (p.slug == engine_)
            return p.commands.contains(command);
    return false;
}

void BacktestingScreen::set_engine(const QString& slug) {
    engine_ = slug;
    memory_->set_value(QStringLiteral("engine"), slug);
    for (QAction* a : engine_menu_->actions())
        a->setChecked(a->data().toString() == slug);
    bar_->set_action_text(QStringLiteral("engine"), tr("ENGINE: %1").arg(engine_label().toUpper()));
    bar_->set_action_text(QStringLiteral("strategy"), tr("STRATEGY: LOADING…"));
    setup_->set_engine(slug, engine_label());
    apply_engine_support(); // the built-in table now, the engine's own capabilities once they arrive
    BacktestingService::instance().load_capabilities(slug); // cached 24 h; engines without it keep the table
    BacktestingService::instance().load_strategies(slug);
    for (BtAnalysisTab* t : {optimize_tab_, walk_forward_tab_, monte_carlo_tab_})
        if (t)
            t->refresh();
    if (research_tab_)
        research_tab_->refresh();
    schedule_signature();
    update_status_bar();
}

void BacktestingScreen::apply_engine_support() {
    const QJsonObject caps = engine_caps_.value(engine_);
    setup_->set_engine_capabilities(caps);
    const ProviderSupport sup = provider_support_from_capabilities(engine_, caps);
    strip_->set_multi_symbol(sup.multi_symbol, engine_label());
    strip_->set_intraday(sup.intraday, engine_label());
    for (QAction* a : engine_menu_->actions()) {
        const QString slug = a->data().toString();
        for (const Provider& p : providers_)
            if (p.slug == slug)
                a->setToolTip(bt_scr_engine_tip(provider_support_from_capabilities(slug, engine_caps_.value(slug)),
                                                static_cast<int>(p.commands.size())));
    }
}

void BacktestingScreen::on_catalog(const QString& provider, const QJsonObject& data) {
    if (provider != engine_)
        return;
    const StrategyCatalog cat = catalog_from_json(data);
    {
        // The search box's strategy rows read this snapshot (no I/O there).
        QVector<BtScrCatalogRow> rows;
        rows.reserve(cat.strategies.size());
        for (const Strategy& s : cat.strategies)
            rows.append({provider, s.id, s.name, s.category, s.supported});
        bt_scr_catalog_rows().insert(provider, rows);
    }
    setup_->set_catalog(cat);
    // A fresh profile's first backtest: SMA crossover 20 / 50 (the catalogue's own default is 10 / 20).
    if (!memory_->value(QStringLiteral("initialised"), false).toBool()) {
        memory_->set_value(QStringLiteral("initialised"), true);
        if (const Strategy* s = setup_->strategy(); s && s->id == QLatin1String("sma_crossover"))
            setup_->apply_params(QJsonObject{{QStringLiteral("fastPeriod"), 20}, {QStringLiteral("slowPeriod"), 50}});
    }
    bar_->set_action_text(QStringLiteral("strategy"), setup_->strategy_label().isEmpty()
                                                          ? tr("STRATEGY")
                                                          : tr("STRATEGY: %1").arg(setup_->strategy_label().toUpper()));
    if (!pending_link_strategy_.isEmpty())
        select_linked_strategy(pending_link_strategy_); // a deep link that arrived before the list
    update_signature();
    for (BtAnalysisTab* t : {optimize_tab_, walk_forward_tab_, monte_carlo_tab_})
        if (t)
            t->refresh();
    if (pending_start_ && run_pane_->is_current(pending_start_)) {
        // The waiting request was started before the strategy existed (its signature names none):
        // supersede it with a run of the complete setup.
        pending_start_ = 0;
        run_pane_->run();
    }
    if (pending_handoff_run_) {
        pending_handoff_run_ = false;
        run_pane_->run();
    }
}

void BacktestingScreen::on_catalog_failed(const QString& context, const QString& message) {
    if (context != QStringLiteral("load_strategies/") + engine_)
        return;
    bar_->set_action_text(QStringLiteral("strategy"), tr("STRATEGY: LIST FAILED"));
    if (pending_start_ && run_pane_->is_current(pending_start_)) {
        const quint64 id = pending_start_;
        pending_start_ = 0;
        run_pane_->fail(id, tr("The strategy list of %1 could not be loaded: %2").arg(engine_label(), message.left(300)),
                        tr("The engine failed"), message);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Setup
// ═════════════════════════════════════════════════════════════════════════════

BtSetup BacktestingScreen::current_setup() const {
    BtSetup s;
    setup_->fill_setup(s);
    s.engine = engine_;
    s.symbols = strip_->symbols();
    s.start = strip_->start();
    s.end = strip_->end();
    s.interval = strip_->interval();
    s.capital = strip_->capital();
    s.benchmark = strip_->benchmark();
    const QJsonArray w = property("btHandoffWeights").toJsonArray();
    if (w.size() == s.symbols.size())
        s.weights = w;
    return s;
}

QString BacktestingScreen::setup_problem() const {
    if (strip_->symbols().isEmpty())
        return tr("Add at least one symbol.");
    const QString period = strip_->period_error();
    if (!period.isEmpty())
        return period;
    const QString panel = setup_->validation_error();
    if (!panel.isEmpty())
        return panel;
    return validate_setup(current_setup());
}

QVector<StrategyParam> BacktestingScreen::strategy_params() const {
    return setup_->params_schema();
}

QString BacktestingScreen::setup_currency() const {
    return btview::run_currency(strip_->symbols());
}

BtRunPtr BacktestingScreen::active_run() const {
    return active_ ? active_->run : nullptr;
}

QString BacktestingScreen::backtest_signature() const {
    return run_signature(engine_, QStringLiteral("backtest"), build_backtest_request(current_setup()));
}

void BacktestingScreen::schedule_signature() {
    signature_timer_->start();
}

void BacktestingScreen::update_signature() {
    // Until the strategy list is in, the setup names no strategy: no signature, so a remembered result
    // does not flash "Inputs changed" during the first second.
    if (setup_->catalog_ready())
        run_pane_->set_signature(backtest_signature());
    for (BtAnalysisTab* t : {optimize_tab_, walk_forward_tab_, monte_carlo_tab_})
        if (t && t->isVisible())
            t->refresh();
    if (research_tab_ && research_tab_->isVisible())
        research_tab_->refresh();
    const QString problem = setup_problem();
    bar_->set_action_enabled(QStringLiteral("run"), problem.isEmpty() || run_token_ != 0);
    bar_->set_action_tooltip(QStringLiteral("run"), problem.isEmpty() ? tr("Backtest this setup (Ctrl+Enter)") : problem);
    update_ribbon();
}

void BacktestingScreen::apply_regional_defaults() {
    const QStringList syms = strip_->symbols();
    if (syms.isEmpty())
        return;
    const QString first = syms.first();
    strip_->set_currency(btview::run_currency(syms));
    const QString bench = btview::default_benchmark_for(first);
    if (!bench.isEmpty())
        strip_->set_benchmark(bench, false);
    setup_->set_venue_cost_default(default_cost_preset_for(first));
}

void BacktestingScreen::apply_params(const QJsonObject& params) {
    setup_->apply_params(params);
    setup_->reveal_section(QStringLiteral("strategy"));
    if (!setup_->isVisible()) {
        setup_->show();
        bar_->set_action_checked(QStringLiteral("setup"), true);
    }
    notify(tr("Parameters applied to the setup — RUN BACKTEST to test them."));
}

void BacktestingScreen::run_backtest_with(const QJsonObject& params) {
    setup_->apply_params(params);
    update_signature();
    tabs_->setCurrentIndex(TabOverview);
    run_pane_->run();
}

void BacktestingScreen::apply_portfolio_config(const QJsonObject& config) {
    QStringList syms;
    for (const QJsonValue& s : config.value(QStringLiteral("symbols")).toArray())
        syms << s.toString();
    if (!syms.isEmpty())
        strip_->set_symbols(syms);
    if (config.contains(QStringLiteral("initialCapital")))
        strip_->set_capital(config.value(QStringLiteral("initialCapital")).toDouble());
    setProperty("btHandoffWeights", config.value(QStringLiteral("weights")).toArray());
    LOG_INFO("Backtesting", logf("Hand-off applied", {{"symbols", syms.size()}}));
    if (config.value(QStringLiteral("autoRun")).toBool()) {
        tabs_->setCurrentIndex(TabOverview);
        if (setup_->catalog_ready())
            QTimer::singleShot(0, this, [this]() { run_pane_->run(); });
        else
            pending_handoff_run_ = true;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// The run pane
// ═════════════════════════════════════════════════════════════════════════════

void BacktestingScreen::start_run_pane(quint64 id) {
    run_request_ = id;
    if (pending_open_id_ > 0) {
        const qint64 sid = pending_open_id_;
        pending_open_id_ = 0;
        run_pane_->set_loading_text(tr("Opening the saved run…"));
        controller_->load_saved(sid, [this, id](BtBundlePtr b, QString err) {
            if (!run_pane_->is_current(id))
                return;
            if (b)
                run_pane_->deliver(id, QJsonObject{{QStringLiteral("key"), b->key}}, b->as_of.toLocalTime());
            else
                run_pane_->fail(id, err, tr("Could not open the run"));
        });
        return;
    }
    if (!setup_->catalog_ready()) {
        pending_start_ = id;
        run_pane_->set_loading_text(tr("Loading the %1 strategy list…").arg(engine_label()));
        return;
    }
    const QString problem = setup_problem();
    if (!problem.isEmpty()) {
        run_pane_->fail(id, problem, bt_failure_title(QStringLiteral("INPUT")));
        return;
    }
    if (run_token_)
        controller_->cancel(run_token_);
    const BtSetup s = current_setup();
    const QJsonObject req = build_backtest_request(s);
    QString why;
    run_auto_ = run_pane_->is_automatic_run(id);
    run_token_ = controller_->start(BtKind::Backtest, engine_, QStringLiteral("backtest"), req,
                                    run_signature(engine_, QStringLiteral("backtest"), req), setup_currency(), &why);
    if (!run_token_) {
        run_pane_->fail(id, why, tr("Queue full"));
        return;
    }
    const int pos = controller_->queue_position(run_token_);
    run_pane_->set_loading_text(pos > 0 ? tr("Queued (%1) — waiting for the run in progress").arg(pos)
                                        : tr("Backtesting %1 on %2 with %3…")
                                              .arg(s.strategy_name, s.symbols.join(QStringLiteral(", ")), engine_label()));
    update_run_state();
}

void BacktestingScreen::render_run_ticket(const QJsonValue& payload) {
    const QJsonObject o = payload.toObject();
    const QString key = o.value(QStringLiteral("key")).toString();
    if (!key.isEmpty()) {
        if (const BtBundlePtr b = controller_->bundle(key)) {
            set_active(b);
            return;
        }
    }
    const qint64 store = o.value(QStringLiteral("store")).toString().toLongLong();
    if (store <= 0)
        return;
    QPointer<BacktestingScreen> self = this;
    controller_->load_saved(store, [self](BtBundlePtr b, QString err) {
        if (!self)
            return;
        if (b)
            self->set_active(b);
        else if (!err.isEmpty())
            self->run_pane_->set_notice(err, ui::EmptyState::Tone::Danger);
    });
}

void BacktestingScreen::set_active(const BtBundlePtr& bundle) {
    if (!bundle || !bundle->run)
        return;
    active_ = bundle;
    ++active_generation_;
    // The result's own caveats on the pane (the pane's "Inputs changed" strip sits above it).
    const BtRun& r = *bundle->run;
    QStringList caveats;
    if (r.synthetic)
        caveats << tr("SYNTHETIC DATA — no market meaning");
    if (r.serious_warning_count() > 0)
        caveats << tr("%n warning(s) affect this result", nullptr, r.serious_warning_count());
    if (!r.data_disclosures.isEmpty())
        caveats << tr("derived prices used");
    if (bundle->label.size() > 0)
        caveats << tr("pinned: %1").arg(bundle->label);
    if (caveats.isEmpty())
        run_pane_->clear_notice();
    else
        run_pane_->set_notice(caveats.join(QStringLiteral(" · ")),
                              r.synthetic ? ui::EmptyState::Tone::Danger : ui::EmptyState::Tone::Warning, tr("VIEW"));
    update_ribbon();
    update_status_bar();
    update_tab_badges();
    show_active_in_visible_view();
    for (BtAnalysisTab* t : {monte_carlo_tab_})
        if (t)
            t->refresh();
    if (runs_tab_ && bundle->store_id > 0)
        runs_tab_->select_run(bundle->store_id);
}

void BacktestingScreen::show_active_in_visible_view() {
    const int tab = tabs_->currentIndex();
    if (!is_run_tab(tab))
        return;
    BtRunView* v = run_view(tab);
    if (!v)
        return;
    if (view_shown_.value(tab) == active_generation_)
        return;
    view_shown_.insert(tab, active_generation_);
    v->show_run(active_ ? active_->run : nullptr);
}

// ═════════════════════════════════════════════════════════════════════════════
// Ribbon, status bar, run state
// ═════════════════════════════════════════════════════════════════════════════

void BacktestingScreen::update_ribbon() {
    using ST = ui::StatTile;
    const bool loading = !active_ && (run_pane_->state() == ui::AutoLoadPane::State::Loading ||
                                      run_pane_->state() == ui::AutoLoadPane::State::Queued || run_pane_->state() == ui::AutoLoadPane::State::Idle);
    if (!active_ || !active_->run) {
        for (int i = 0; i < ribbon_->count(); ++i)
            ribbon_->tile(i)->set_state(loading ? ST::State::Loading : ST::State::Error,
                                        loading ? QString() : tr("No backtest yet"));
        return;
    }
    const BtRun& r = *active_->run;
    const QString ccy = r.currency;
    auto tile = [this](const char* k) { return ribbon_->tile(QLatin1String(k)); };
    set_tile_metric(tile("ret"), r, QStringLiteral("performance.totalReturn"), Unit::SignedFraction, Rule::Sign);
    tile("ret")->set_sparkline(bt_scr_spark(r.equity), r.initial_capital());
    tile("ret")->set_sub_label(tr("%1 → %2").arg(money_compact(r.initial_capital(), ccy), money_compact(r.final_equity(), ccy)));
    set_tile_metric(tile("cagr"), r, QStringLiteral("performance.cagr"), Unit::SignedFraction, Rule::Sign);
    tile("cagr")->set_sub_label(std::isfinite(r.years()) ? tr("%1 years").arg(num(r.years(), 1)) : QString());
    set_tile_metric(tile("sharpe"), r, QStringLiteral("performance.sharpeRatio"), Unit::Ratio, Rule::Negative);
    tile("sharpe")->set_sparkline(bt_scr_spark(r.rolling_sharpe.v));
    tile("sharpe")->set_sub_label(r.assumptions.risk_free_source.isEmpty()
                                      ? QString()
                                      : tr("rf %1 %2").arg(r.assumptions.risk_free_source, pct(r.assumptions.risk_free_rate, 2)));
    set_tile_metric(tile("sortino"), r, QStringLiteral("performance.sortinoRatio"), Unit::Ratio, Rule::Negative);
    set_tile(tile("maxdd"), -r.metric(QStringLiteral("performance.maxDrawdown")), Unit::SignedFraction, Rule::Drawdown, ccy,
             r.why_blank(QStringLiteral("performance.maxDrawdown")));
    tile("maxdd")->set_sparkline(bt_scr_spark(r.drawdown), 0.0);
    if (!r.drawdowns.isEmpty())
        tile("maxdd")->set_sub_label(tr("trough %1 · %2").arg(r.drawdowns.first().trough_date, bars(r.drawdowns.first().duration_bars)));
    set_tile_metric(tile("vol"), r, QStringLiteral("performance.volatility"), Unit::Fraction, Rule::None);
    tile("vol")->set_sparkline(bt_scr_spark(r.rolling_volatility.v));
    set_tile_metric(tile("win"), r, QStringLiteral("tradeStats.winRate"), Unit::Fraction, Rule::None);
    tile("win")->set_sub_label(tr("%1 / %2 trades")
                                   .arg(count(r.metric(QStringLiteral("tradeStats.winningTrades"))), count(r.closed_trades())));
    if (!std::isfinite(r.metric(QStringLiteral("tradeStats.profitFactor"))) &&
        r.metric(QStringLiteral("tradeStats.profitFactorInfinite")) > 0.5) {
        tile("pf")->set_state(ST::State::Normal);
        tile("pf")->set_value(QStringLiteral("∞"));
        tile("pf")->set_tone(ST::Tone::Neutral);
        tile("pf")->set_tooltip_text(r.why_blank(QStringLiteral("tradeStats.profitFactor")));
    } else {
        set_tile_metric(tile("pf"), r, QStringLiteral("tradeStats.profitFactor"), Unit::Ratio, Rule::BelowOne);
    }
    set_tile_metric(tile("expo"), r, QStringLiteral("exposure.timeInMarket"), Unit::Fraction, Rule::None);
    const double avg_gross = r.metric(QStringLiteral("exposure.avgGrossExposure"));
    tile("expo")->set_sub_label(std::isfinite(avg_gross) ? tr("avg %1 gross").arg(pct(avg_gross, 0)) : QString());
    ST* vs = tile("vs");
    const double bench = r.metric(QStringLiteral("benchmark.totalReturn"));
    vs->set_label(r.benchmark_symbol.isEmpty() ? tr("VS BENCHMARK") : tr("VS %1").arg(r.benchmark_symbol));
    set_tile(vs, r.metric(QStringLiteral("performance.totalReturn")) - bench, Unit::PctPoints, Rule::Sign, ccy,
             r.why_blank(QStringLiteral("benchmark.totalReturn")).isEmpty() ? tr("No benchmark in this run")
                                                                           : r.why_blank(QStringLiteral("benchmark.totalReturn")));
    vs->set_sub_label(std::isfinite(bench) ? tr("%1 %2 · α %3 · β %4")
                                                 .arg(r.benchmark_symbol, pct(bench, 1, true),
                                                      pct(r.metric(QStringLiteral("performance.alpha")), 1, true),
                                                      ratio(r.metric(QStringLiteral("performance.beta"))))
                                           : QString());
    if (run_pane_->inputs_changed())
        for (int i = 0; i < ribbon_->count(); ++i)
            if (ribbon_->tile(i)->state() == ST::State::Normal)
                ribbon_->tile(i)->set_state(ST::State::Stale, tr("Inputs changed since this run — RUN AGAIN"));
}

void BacktestingScreen::update_status_bar() {
    using SB = ui::ScreenStatusBar;
    if (!active_ || !active_->run) {
        status_->set_item(QStringLiteral("engine"), engine_label(), SB::Tone::Accent);
        status_->set_item(QStringLiteral("data"), tr("no result yet"), SB::Tone::Dim);
        status_->set_text(QStringLiteral("fill"), setup_ ? setup_->fill_short() : QString());
        status_->set_text(QStringLiteral("costs"), setup_ ? setup_->cost_short() : QString());
        status_->set_text(QStringLiteral("rf"), setup_ ? setup_->risk_free_short() : QString());
        status_->set_text(QStringLiteral("ccy"), strip_ ? btview::run_currency(strip_->symbols()) : QString());
        status_->set_item_visible(QStringLiteral("warn"), false);
        status_->set_text(QStringLiteral("duration"), QString());
        return;
    }
    const BtRun& r = *active_->run;
    QString engine = r.engine.isEmpty() ? active_->engine : r.engine;
    for (const Provider& p : providers_)
        if (p.slug == engine)
            engine = p.display_name;
    QString eng_text = engine;
    if (!r.engine_version.isEmpty())
        eng_text += QStringLiteral(" ") + r.engine_version;
    status_->set_item(QStringLiteral("engine"), eng_text, SB::Tone::Accent);
    status_->set_tooltip(QStringLiteral("engine"),
                         r.engine_implementation.isEmpty() ? tr("This engine does not report its implementation yet")
                                                           : r.engine_implementation);
    const BtProvenance& p = r.provenance;
    QString data;
    if (p.present)
        data = tr("%1 · %2 · %3 bars · %4 → %5")
                   .arg(p.data_source, p.adjustment, count(p.bars), p.first_bar, p.last_bar);
    else if (!r.dates.isEmpty())
        data = tr("%1 bars · %2 → %3 · provenance not reported").arg(count(r.dates.size()), r.dates.first(), r.dates.last());
    status_->set_item(QStringLiteral("data"), data, r.synthetic ? SB::Tone::Danger : SB::Tone::Normal);
    if (r.synthetic)
        status_->set_text(QStringLiteral("data"), tr("SYNTHETIC DATA · ") + data);
    const QString fill = r.assumptions.fill_at.isEmpty() ? tr("fill not reported") : fill_timing_text(r.assumptions.fill_at);
    status_->set_item(QStringLiteral("fill"), fill,
                      r.assumptions.fill_at == QLatin1String("same_close") ? SB::Tone::Warn : SB::Tone::Normal);
    status_->set_tooltip(QStringLiteral("fill"), r.assumptions.fill_description);
    QString costs = r.costs.model.preset.isEmpty() ? tr("costs") : r.costs.model.preset;
    if (std::isfinite(r.costs.total_costs))
        costs += QStringLiteral(" · ") + money_compact(r.costs.total_costs, r.currency);
    const bool gross = std::isfinite(r.costs.total_costs) && r.costs.total_costs <= 0;
    status_->set_item(QStringLiteral("costs"), gross ? tr("GROSS — no costs") : costs, gross ? SB::Tone::Warn : SB::Tone::Normal);
    status_->set_tooltip(QStringLiteral("costs"), r.costs.model.source.isEmpty() ? r.assumptions.costs : r.costs.model.source);
    status_->set_text(QStringLiteral("rf"), r.assumptions.risk_free_source.isEmpty()
                                                ? QString()
                                                : tr("rf %1 %2").arg(r.assumptions.risk_free_source, pct(r.assumptions.risk_free_rate, 2)));
    status_->set_tooltip(QStringLiteral("rf"), r.assumptions.risk_free_description);
    status_->set_text(QStringLiteral("ccy"), r.currency);
    const int w = r.serious_warning_count();
    status_->set_item_visible(QStringLiteral("warn"), !r.warnings.isEmpty());
    status_->set_item(QStringLiteral("warn"), tr("%n note(s)", nullptr, static_cast<int>(r.warnings.size())),
                      w > 0 ? SB::Tone::Warn : SB::Tone::Info);
    status_->set_text(QStringLiteral("duration"),
                      p.duration_ms >= 0 ? tr("engine %1 s").arg(num(p.duration_ms / 1000.0, 1)) : QString());
    warnings_btn_->setText(r.synthetic ? tr("⚠ SYNTHETIC") : tr("⚠ %n NOTE(S)", nullptr, static_cast<int>(r.warnings.size())));
    warnings_btn_->setVisible(!r.warnings.isEmpty());
    set_tone(warnings_btn_, r.synthetic ? Tone::Danger : (w > 0 ? Tone::Warn : Tone::Dim));
}

void BacktestingScreen::update_run_state() {
    if (!state_chip_)
        return;
    // The run that matters for the open tab: its own analysis, else the backtest.
    quint64 token = run_token_;
    const int tab = tabs_ ? tabs_->currentIndex() : TabOverview;
    if (BtAnalysisTab* a = analysis_tab(tab); a && a->is_running())
        token = a->token();
    const bool running = token != 0 && controller_->is_active(token);
    if (running && isVisible() && !ticker_->isActive())
        ticker_->start();
    if (!running)
        ticker_->stop();
    const QString run_text = run_token_ ? tr("CANCEL BACKTEST") : tr("RUN BACKTEST");
    if (run_text != run_button_text_) {
        run_button_text_ = run_text;
        bar_->set_action_text(QStringLiteral("run"), run_text);
        bar_->set_action_hint(QStringLiteral("run"), run_token_ ? QStringLiteral("Esc") : QStringLiteral("Ctrl+↵"));
        // CANCEL reads as the stop direction (Danger), RUN as the screen's main action. Neither state carries
        // an agent-risk marker: a backtest runs a built-in engine on free data (no credits, no user code), and
        // cancelling one's own run is the safe direction.
        bar_->set_action_role(QStringLiteral("run"), run_token_ ? ui::ScreenCommandBar::Role::Danger
                                                                : ui::ScreenCommandBar::Role::Primary);
    }
    if (!running) {
        progress_->setVisible(false);
        status_->set_item_visible(QStringLiteral("progress"), false);
        const auto st = run_pane_ ? run_pane_->state() : ui::AutoLoadPane::State::Idle;
        if (st == ui::AutoLoadPane::State::Failed) {
            state_chip_->setText(tr("FAILED"));
            set_tone(state_chip_, Tone::Danger);
        } else if (st == ui::AutoLoadPane::State::NeedsSetup) {
            state_chip_->setText(tr("NEEDS SETUP"));
            set_tone(state_chip_, Tone::Warn);
        } else if (active_ && run_pane_->inputs_changed()) {
            state_chip_->setText(tr("INPUTS CHANGED"));
            set_tone(state_chip_, Tone::Warn);
        } else {
            state_chip_->setText(active_ ? tr("DONE") : tr("READY"));
            set_tone(state_chip_, Tone::Ok);
        }
        return;
    }
    const int pos = controller_->queue_position(token);
    if (pos > 0) {
        state_chip_->setText(tr("QUEUED (%1)").arg(pos));
        set_tone(state_chip_, Tone::Warn);
        progress_->setVisible(false);
        return;
    }
    const BtProgress p = controller_->last_progress(token);
    const qint64 secs = controller_->elapsed_ms(token) / 1000;
    QString text = tr("RUNNING %1 s").arg(secs);
    if (p.determinate())
        text += QStringLiteral(" · %1 / %2").arg(QLocale().toString(p.done), QLocale().toString(p.total));
    const QString eta = format_eta(p.eta_sec);
    if (!eta.isEmpty())
        text += QStringLiteral(" · ") + tr("ETA %1").arg(eta);
    state_chip_->setText(text);
    set_tone(state_chip_, Tone::Accent);
    state_chip_->setToolTip(p.phase.isEmpty() ? QString() : progress_text(p));
    progress_->setVisible(true);
    if (p.determinate()) {
        progress_->setRange(0, 1000);
        progress_->setValue(static_cast<int>(p.fraction() * 1000));
    } else if (ui::motion_enabled()) {
        progress_->setRange(0, 0); // indeterminate sweep
    } else {
        progress_->setRange(0, 1000);
        progress_->setValue(1000);
    }
    status_->set_item_visible(QStringLiteral("progress"), !p.phase.isEmpty());
    if (!p.phase.isEmpty())
        status_->set_text(QStringLiteral("progress"), progress_text(p));
}

void BacktestingScreen::update_tab_badges() {
    if (!tabs_ || tabs_->count() <= TabAssumptions)
        return;
    const int n = active_ && active_->run ? static_cast<int>(active_->run->warnings.size()) : 0;
    tabs_->setTabText(TabAssumptions, n > 0 ? tr("ASSUMPTIONS · %1").arg(n) : tr("ASSUMPTIONS"));
}

// ═════════════════════════════════════════════════════════════════════════════
// Tabs
// ═════════════════════════════════════════════════════════════════════════════

bool BacktestingScreen::is_run_tab(int tab) const {
    return (tab >= TabOverview && tab <= TabCosts) || tab == TabAssumptions;
}

BtAnalysisTab* BacktestingScreen::analysis_tab(int tab) const {
    if (tab == TabOptimize)
        return optimize_tab_;
    if (tab == TabWalkForward)
        return walk_forward_tab_;
    if (tab == TabMonteCarlo)
        return monte_carlo_tab_;
    return nullptr;
}

BtRunView* BacktestingScreen::run_view(int tab) {
    if (!is_run_tab(tab))
        return nullptr;
    if (BtRunView* v = run_views_.value(tab))
        return v;
    BtRunView* v = nullptr;
    switch (tab) {
        case TabOverview:
            v = new BtOverviewView(selection_, run_stack_);
            break;
        case TabPerformance:
            v = new BtPerformanceView(selection_, run_stack_);
            break;
        case TabRisk:
            v = new BtRiskView(selection_, run_stack_);
            break;
        case TabTrades:
            v = new BtTradesView(selection_, run_stack_);
            break;
        case TabExposure:
            v = new BtExposureView(selection_, run_stack_);
            break;
        case TabCosts:
            v = new BtCostsView(selection_, run_stack_);
            break;
        case TabAssumptions: {
            auto* a = new BtAssumptionsView(selection_, run_stack_);
            connect(a, &BtAssumptionsView::raw_json_requested, this, &BacktestingScreen::show_raw_json);
            v = a;
            break;
        }
        default:
            return nullptr;
    }
    QWidget* slot = run_slots_.value(tab);
    const int at = run_stack_->indexOf(slot);
    run_stack_->insertWidget(at, v);
    run_stack_->removeWidget(slot);
    slot->deleteLater();
    run_slots_.remove(tab);
    run_views_.insert(tab, v);
    return v;
}

QWidget* BacktestingScreen::page_for(int tab) {
    if (QWidget* p = page_slots_.value(tab))
        return p;
    QWidget* page = nullptr;
    switch (tab) {
        case TabOptimize:
            optimize_tab_ = new BtOptimizeTab(this, pages_);
            connect(optimize_tab_, &BtAnalysisTab::running_changed, this, &BacktestingScreen::update_run_state);
            page = optimize_tab_;
            break;
        case TabWalkForward:
            walk_forward_tab_ = new BtWalkForwardTab(this, pages_);
            connect(walk_forward_tab_, &BtAnalysisTab::running_changed, this, &BacktestingScreen::update_run_state);
            page = walk_forward_tab_;
            break;
        case TabMonteCarlo:
            monte_carlo_tab_ = new BtMonteCarloTab(this, pages_);
            connect(monte_carlo_tab_, &BtAnalysisTab::running_changed, this, &BacktestingScreen::update_run_state);
            page = monte_carlo_tab_;
            break;
        case TabCompare:
            compare_tab_ = new BtCompareTab(this, pages_);
            page = compare_tab_;
            break;
        case TabRuns:
            runs_tab_ = new BtRunsTab(this, pages_);
            connect(runs_tab_, &BtRunsTab::compare_requested, this, [this](const QVector<qint64>& picked) {
                QVector<qint64> ids = picked;
                // One run asked for: compare it with the active run.
                if (ids.size() < 2 && active_ && active_->store_id > 0 && !ids.contains(active_->store_id))
                    ids.prepend(active_->store_id);
                page_for(TabCompare);
                compare_tab_->set_runs(ids);
                tabs_->setCurrentIndex(TabCompare);
            });
            connect(runs_tab_, &BtRunsTab::runs_changed, this, [this]() {
                if (compare_tab_)
                    compare_tab_->prune();
            });
            page = runs_tab_;
            break;
        case TabResearch:
            research_tab_ = new BtResearchTab(this, pages_);
            connect(research_tab_, &BtResearchTab::running_changed, this, &BacktestingScreen::update_run_state);
            page = research_tab_;
            break;
        default:
            return run_pane_;
    }
    pages_->addWidget(page);
    page_slots_[tab] = page;
    return page;
}

void BacktestingScreen::on_tab_changed(int index) {
    if (index < 0)
        return;
    QWidget* page = page_for(index);
    pages_->setCurrentWidget(page);
    if (is_run_tab(index)) {
        if (BtRunView* v = run_view(index))
            run_stack_->setCurrentWidget(v);
        show_active_in_visible_view();
    } else if (BtAnalysisTab* a = analysis_tab(index)) {
        a->refresh();
    } else if (index == TabRuns && runs_tab_) {
        runs_tab_->refresh();
    } else if (index == TabResearch && research_tab_) {
        research_tab_->refresh();
    }
    update_run_state();
}

void BacktestingScreen::run_primary() {
    const int tab = tabs_->currentIndex();
    if (BtAnalysisTab* a = analysis_tab(tab)) {
        a->run();
        return;
    }
    if (tab == TabResearch && research_tab_) {
        research_tab_->run();
        return;
    }
    if (!run_token_)
        run_pane_->run();
}

void BacktestingScreen::cancel_current() {
    const int tab = tabs_->currentIndex();
    if (BtAnalysisTab* a = analysis_tab(tab); a && a->is_running()) {
        a->cancel();
        return;
    }
    if (tab == TabResearch && research_tab_ && research_tab_->is_running()) {
        research_tab_->cancel();
        return;
    }
    if (run_token_ || pending_start_)
        run_pane_->cancel();
}

// ═════════════════════════════════════════════════════════════════════════════
// Saved runs, pin, export
// ═════════════════════════════════════════════════════════════════════════════

void BacktestingScreen::open_saved_run(qint64 store_id) {
    pending_open_id_ = store_id;
    tabs_->setCurrentIndex(TabOverview);
    run_pane_->run();
}

void BacktestingScreen::load_saved_inputs(qint64 store_id, bool run) {
    QPointer<BacktestingScreen> self = this;
    controller_->load_saved(store_id, [self, run](BtBundlePtr b, QString err) {
        if (!self)
            return;
        if (!b) {
            self->notify(err);
            return;
        }
        if (b->engine != self->engine_)
            self->set_engine(b->engine);
        const BtSetup s = setup_from_request(b->request, b->engine);
        self->strip_->set_symbols(s.symbols);
        self->strip_->set_period(s.start, s.end);
        self->strip_->set_interval(s.interval);
        self->strip_->set_capital(s.capital);
        self->strip_->set_benchmark(s.benchmark, true);
        self->setup_->apply_setup(s);
        self->notify(tr("Inputs of the saved run loaded into the setup."));
        if (run) {
            self->tabs_->setCurrentIndex(TabOverview);
            if (self->setup_->catalog_ready())
                self->run_pane_->run();
            else
                self->pending_handoff_run_ = true;
        }
    });
}

void BacktestingScreen::notify(const QString& text) {
    status_->show_notice(text, QString(), {}, 7000, ui::ScreenStatusBar::Tone::Normal);
}

void BacktestingScreen::pin_active_run() {
    if (!active_ || active_->store_id <= 0) {
        notify(tr("There is no saved run to pin yet."));
        return;
    }
    QString suggested = active_->label;
    if (suggested.isEmpty() && active_->run)
        suggested = QStringLiteral("%1 %2").arg(active_->run->provenance.strategy_name,
                                               active_->run->symbols().join(QLatin1Char(' ')));
    bool ok = false;
    const QString label = QInputDialog::getText(this, tr("Pin run"), tr("Label (pinned runs are never dropped):"),
                                                QLineEdit::Normal, suggested.trimmed(), &ok);
    if (!ok)
        return;
    auto r = BacktestRunStore::instance().rename(active_->store_id, label.trimmed());
    if (r.is_err()) {
        notify(tr("The run could not be pinned."));
        return;
    }
    notify(label.trimmed().isEmpty() ? tr("Run unpinned.") : tr("Run pinned as \"%1\".").arg(label.trimmed()));
    if (runs_tab_)
        runs_tab_->refresh();
}

bool BacktestingScreen::write_file(const QString& suggested, const QString& filter, const QByteArray& bytes) {
    const QString path = QFileDialog::getSaveFileName(this, tr("Export"), suggested, filter);
    if (path.isEmpty())
        return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(bytes) != bytes.size()) {
        LOG_WARN("Backtesting", logf("Export could not be written", {{"path", log_path(path)}, {"error", f.errorString()}}));
        QMessageBox::warning(this, tr("Export"), tr("The file could not be written: %1").arg(f.errorString()));
        return false;
    }
    notify(tr("Exported to %1").arg(QDir::toNativeSeparators(path)));
    return true;
}

void BacktestingScreen::export_kind(const QString& kind) {
    const QString csv = tr("CSV files (*.csv);;All files (*)");
    if (kind == QLatin1String("optimize") || kind == QLatin1String("walk_forward") || kind == QLatin1String("monte_carlo")) {
        BtAnalysisTab* t = kind == QLatin1String("optimize")       ? optimize_tab_
                           : kind == QLatin1String("walk_forward") ? walk_forward_tab_
                                                                   : monte_carlo_tab_;
        const BtBundlePtr b = t ? t->current() : nullptr;
        if (!b)
            return;
        QString text;
        if (b->optimize) {
            BtExportMeta m = bt_export_meta(b->optimize->provenance, b->optimize->assumptions, kind);
            m.currency = b->currency;
            text = bt_optimize_csv(*b->optimize, m);
        } else if (b->walk_forward) {
            BtExportMeta m = bt_export_meta(b->walk_forward->provenance, b->walk_forward->assumptions, kind);
            m.currency = b->currency;
            text = bt_walk_forward_csv(*b->walk_forward, m);
        } else if (b->monte_carlo) {
            BtExportMeta m = bt_export_meta(b->monte_carlo->provenance, b->monte_carlo->assumptions, kind);
            m.currency = b->currency;
            text = bt_monte_carlo_csv(*b->monte_carlo, m);
        }
        write_file(kind + QStringLiteral(".csv"), csv, text.toUtf8());
        return;
    }
    if (!active_ || !active_->run)
        return;
    const BtRun& r = *active_->run;
    BtExportMeta meta = bt_export_meta(r);
    const QString tag = (r.symbols().value(0) + QLatin1Char('_') + r.provenance.strategy_type).replace(QLatin1Char('^'), QString()) +
                        (r.synthetic ? QStringLiteral("_SYNTHETIC") : QString());
    if (kind == QLatin1String("trades"))
        write_file(tag + QStringLiteral("_trades.csv"), csv, bt_trades_csv(r, meta).toUtf8());
    else if (kind == QLatin1String("equity"))
        write_file(tag + QStringLiteral("_equity.csv"), csv, bt_equity_csv(r, meta).toUtf8());
    else if (kind == QLatin1String("metrics"))
        write_file(tag + QStringLiteral("_metrics.csv"), csv, bt_metrics_csv(r, meta).toUtf8());
    else if (kind == QLatin1String("drawdowns"))
        write_file(tag + QStringLiteral("_drawdowns.csv"), csv, bt_drawdowns_csv(r, meta).toUtf8());
    else if (kind == QLatin1String("json"))
        write_file(tag + QStringLiteral("_result.json"), tr("JSON files (*.json);;All files (*)"),
                   bt_result_json(r.payload, meta));
}

void BacktestingScreen::show_raw_json() {
    if (!active_ || !active_->run)
        return;
    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Result JSON"));
    auto* v = new QVBoxLayout(dlg);
    auto* edit = new QPlainTextEdit(dlg);
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setPlainText(QString::fromUtf8(QJsonDocument(active_->run->payload).toJson(QJsonDocument::Indented)));
    v->addWidget(edit, 1);
    auto* box = new QDialogButtonBox(dlg);
    QPushButton* copy = box->addButton(tr("COPY"), QDialogButtonBox::ActionRole);
    QPushButton* save = box->addButton(tr("SAVE…"), QDialogButtonBox::ActionRole);
    box->addButton(QDialogButtonBox::Close);
    connect(copy, &QPushButton::clicked, dlg, [edit]() { QApplication::clipboard()->setText(edit->toPlainText()); });
    connect(save, &QPushButton::clicked, this, [this]() { export_kind(QStringLiteral("json")); });
    connect(box, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    v->addWidget(box);
    const QFontMetrics fm(font());
    dlg->resize(fm.averageCharWidth() * 110, fm.height() * 40);
    dlg->show();
}

// ═════════════════════════════════════════════════════════════════════════════
// IStatefulScreen / IGroupLinked
// ═════════════════════════════════════════════════════════════════════════════

QVariantMap BacktestingScreen::save_state() const {
    QVariantMap s;
    s.insert(QStringLiteral("engine"), engine_);
    const int tab = tabs_ ? tabs_->currentIndex() : TabOverview;
    s.insert(QStringLiteral("tab"), QLatin1String(kTabIds[std::clamp(tab, 0, TabCount - 1)]));
    s.insert(QStringLiteral("setup_collapsed"), setup_collapsed_);
    if (splitter_) {
        QVariantList sizes;
        for (int v : splitter_->sizes())
            sizes << v;
        s.insert(QStringLiteral("sizes"), sizes);
    }
    return s;
}

void BacktestingScreen::restore_state(const QVariantMap& state) {
    QString engine = state.value(QStringLiteral("engine")).toString();
    if (engine.isEmpty()) {
        // v1: provider_slug (or a legacy index) and the command index.
        engine = state.value(QStringLiteral("provider_slug")).toString();
        const int cmd = state.value(QStringLiteral("command"), -1).toInt();
        restored_tab_ = cmd == 1 ? TabOptimize : cmd == 2 ? TabWalkForward : cmd == 11 ? TabMonteCarlo
                        : (cmd >= 3 && cmd <= 10) ? TabResearch : TabOverview;
        const QString last = state.value(QStringLiteral("last_result")).toString();
        if (!last.isEmpty() && state.value(QStringLiteral("last_command")).toString() == QLatin1String("backtest")) {
            const QJsonDocument d = QJsonDocument::fromJson(last.toUtf8());
            if (d.isObject())
                pending_restore_payload_ = d.object();
        }
        if (state.contains(QStringLiteral("symbols")) && memory_->value(QStringLiteral("symbols")).isNull()) {
            QStringList syms;
            for (const QString& x : state.value(QStringLiteral("symbols")).toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
                syms << x.trimmed();
            if (!syms.isEmpty())
                strip_->set_symbols(syms);
        }
    } else {
        const int t = bt_scr_tab_index(state.value(QStringLiteral("tab")).toString());
        restored_tab_ = t >= 0 ? t : TabOverview;
        setup_collapsed_ = state.value(QStringLiteral("setup_collapsed"), false).toBool();
        restored_sizes_.clear();
        for (const QVariant& v : state.value(QStringLiteral("sizes")).toList())
            restored_sizes_ << v.toInt();
    }
    for (const Provider& p : providers_)
        if (p.slug == engine)
            engine_ = engine;
    if (!first_show_) {
        set_engine(engine_);
        tabs_->setCurrentIndex(restored_tab_);
    }
}

void BacktestingScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!ref.is_valid() || (strip_->symbols().size() == 1 && strip_->symbols().first() == ref.symbol))
        return;
    const SymbolGroup g = link_group_;
    link_group_ = SymbolGroup::None; // no echo back to the group
    strip_->set_symbols({ref.symbol});
    link_group_ = g;
}

SymbolRef BacktestingScreen::current_symbol() const {
    const QStringList syms = strip_ ? strip_->symbols() : QStringList();
    return syms.isEmpty() ? SymbolRef{} : SymbolRef::equity(syms.first());
}

// ═════════════════════════════════════════════════════════════════════════════
// Deep links (ISectionLinked) + the search box's rows
// ═════════════════════════════════════════════════════════════════════════════

void BacktestingScreen::select_linked_strategy(const QString& text) {
    if (!setup_->catalog_ready()) {
        pending_link_strategy_ = text; // on_catalog() comes back here
        return;
    }
    pending_link_strategy_.clear();
    const bool golden = bt_scr_is_golden_cross(text);
    const QString id = bt_scr_match_strategy(setup_->catalog(), golden ? QStringLiteral("sma_crossover") : text);
    if (id.isEmpty()) {
        notify(tr("No strategy matching \"%1\" in the %2 list.").arg(text.trimmed(), engine_label()));
        return;
    }
    setup_->select_strategy(id); // strategy_changed: bar text + "inputs changed"; nothing runs
    if (golden && id == QLatin1String("sma_crossover"))
        setup_->apply_params(QJsonObject{{QStringLiteral("fastPeriod"), 50}, {QStringLiteral("slowPeriod"), 200}});
    // Shown the way APPLY of an optimised set shows it.
    setup_->reveal_section(QStringLiteral("strategy"));
    if (!setup_->isVisible()) {
        setup_->show();
        setup_collapsed_ = false;
        bar_->set_action_checked(QStringLiteral("setup"), true);
    }
}

bool BacktestingScreen::open_section(const QString& section, const QString& focus) {
    namespace rows = command_rows;
    const QString raw = section.trimmed();
    const QString key = raw.toLower();
    QString focus_strategy;
    const bool has_focus_strategy = rows::split_kind(focus.trimmed(), "strategy", &focus_strategy) &&
                                    !focus_strategy.isEmpty();
    QString rest;
    bool handled = false;
    if (key.isEmpty()) {
        handled = true; // a focus alone acts in the view that is open
    } else if (rows::split_kind(raw, "engine", &rest)) {
        QString slug;
        for (const Provider& p : providers_)
            if (p.slug.compare(rest, Qt::CaseInsensitive) == 0)
                slug = p.slug;
        if (slug.isEmpty())
            return false;
        if (first_show_)
            engine_ = slug; // the first show selects it (and loads its list)
        else if (slug != engine_)
            set_engine(slug); // the ENGINE menu's own path: capabilities + strategy list, no run
        handled = true;
    } else if (rows::split_kind(raw, "strategy", &rest)) {
        if (rest.isEmpty())
            return false;
        select_linked_strategy(rest);
        handled = true;
    } else if (rows::split_kind(raw, "run", &rest)) {
        bool ok = false;
        const qint64 store_id = rest.toLongLong(&ok);
        if (!ok || store_id <= 0)
            return false;
        open_saved_run(store_id); // RUNS ▸ OPEN: the stored result, never re-run
        return true;
    } else {
        QString id = key;
        if (id == QLatin1String("optimise"))
            id = QStringLiteral("optimize");
        id.replace(QLatin1Char('-'), QLatin1Char('_'));
        const int tab = bt_scr_tab_index(id);
        if (tab < 0)
            return false;
        if (first_show_)
            restored_tab_ = tab; // the first show opens it
        else
            tabs_->setCurrentIndex(tab); // currentChanged -> on_tab_changed, as a click
        handled = true;
    }
    if (has_focus_strategy)
        select_linked_strategy(focus_strategy);
    return handled;
}

namespace {

namespace bt_cmd {

QString engine_title(const QString& slug) {
    for (const Provider& p : all_providers())
        if (p.slug == slug)
            return p.display_name;
    return slug;
}

QList<command::CommandItem> tab_rows() {
    namespace rows = command_rows;
    const QString screen = QStringLiteral("backtesting");
    const QString where = BacktestingScreen::tr("Backtesting");
    struct TabRow {
        const char* id;
        const char* title;
        const char* keywords;
    };
    static const TabRow kTabs[] = {
        {"overview", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Backtest overview"),
         "summary, results, equity curve, kpis"},
        {"performance", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Backtest performance"),
         "returns, cagr, sharpe, monthly returns, benchmark"},
        {"risk", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Backtest risk"),
         "drawdown, max drawdown, volatility, var, sortino"},
        {"trades", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Backtest trades"),
         "trade list, entries, exits, win rate"},
        {"exposure", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Backtest exposure"),
         "exposure, time in market, positions, leverage"},
        {"costs", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Backtest costs"),
         "fees, commission, slippage, transaction costs"},
        {"optimize", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Optimize strategy parameters"),
         "optimise, optimization, parameter sweep, grid search, best parameters"},
        {"walk_forward", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Walk-forward test"),
         "walk forward, out of sample, overfitting, robustness"},
        {"monte_carlo", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Monte Carlo simulation"),
         "monte carlo, simulation, resampling, risk of ruin, bootstrap"},
        {"compare", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Compare backtests"),
         "compare runs, side by side, comparison"},
        {"assumptions", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Backtest assumptions"),
         "assumptions, fill model, data source, settings, methodology"},
        {"runs", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Saved backtest runs"),
         "runs, history, results, saved backtests, pinned"},
        {"research", QT_TRANSLATE_NOOP("fincept::screens::BacktestingScreen", "Backtest research"),
         "research, indicators, signals, labels, returns analysis"},
    };
    QList<command::CommandItem> out;
    for (const TabRow& t : kTabs) {
        QStringList kw = QString::fromLatin1(t.keywords).split(QStringLiteral(", "), Qt::SkipEmptyParts);
        kw << QStringLiteral("backtest");
        out.append(rows::section(screen, QString::fromLatin1(t.id),
                                 QCoreApplication::translate("fincept::screens::BacktestingScreen", t.title), kw, where));
    }
    // Engines: the ENGINE menu (switching loads the engine's strategy list; nothing runs).
    for (const Provider& p : all_providers()) {
        command::CommandItem it = rows::section(
            screen, QStringLiteral("engine:") + p.slug, BacktestingScreen::tr("%1 backtest engine").arg(p.display_name),
            {p.slug, p.display_name, QStringLiteral("engine"), QStringLiteral("backtest engine")}, where,
            BacktestingScreen::tr("Backtesting › engine %1 (selects it; nothing runs)").arg(p.display_name));
        out.append(it);
    }
    // The classic golden cross: SMA crossover 50 / 200 on the default engine, set up — not run.
    out.append(rows::section(screen, QStringLiteral("strategy:golden_cross"), BacktestingScreen::tr("Golden cross backtest"),
                             {QStringLiteral("golden cross"), QStringLiteral("death cross"), QStringLiteral("sma 50 200"),
                              QStringLiteral("moving average crossover"), QStringLiteral("trend following")},
                             where,
                             BacktestingScreen::tr("Backtesting › SMA crossover 50 / 200 set up (press RUN to test it)")));
    return out;
}

/// Strategies of the catalogues this session has loaded (Backtesting's own snapshot; nothing is read
/// from disk or run): "engine:<slug>" + focus "strategy:<id>" selects it, never runs it.
void query_strategies(const QString& query, quint64, command::ResultSink sink) {
    QList<command::CommandItem> all;
    const auto& snapshot = bt_scr_catalog_rows();
    for (auto it = snapshot.constBegin(); it != snapshot.constEnd(); ++it) {
        const QString engine_name = engine_title(it.key());
        for (const BtScrCatalogRow& r : it.value()) {
            command::CommandItem row;
            row.id = QStringLiteral("backtesting.strategy.") + r.engine + QLatin1Char('.') + r.id;
            row.title = r.name.isEmpty() ? r.id : r.name;
            row.keywords = {r.id, r.category, engine_name, QStringLiteral("strategy"), QStringLiteral("backtest")};
            row.group = QString::fromLatin1(command::groups::kData);
            row.subtitle = r.supported
                               ? BacktestingScreen::tr("Backtesting › %1 strategy (selects it; nothing runs)").arg(engine_name)
                               : BacktestingScreen::tr("Backtesting › %1 strategy (not runnable here)").arg(engine_name);
            row.screen_id = QStringLiteral("backtesting");
            row.section = QStringLiteral("engine:") + r.engine;
            row.focus = QStringLiteral("strategy:") + r.id;
            row.owner = QStringLiteral("backtesting");
            row.context = BacktestingScreen::tr("Backtest strategy");
            row.priority = r.supported ? 0 : -20;
            all.append(row);
        }
    }
    sink(command_rows::best_matches(all, query, 6), true);
}

void install() {
    command_rows::install(QStringLiteral("backtesting"), &tab_rows);
    command::ProviderSpec p;
    p.id = QStringLiteral("backtesting.strategies");
    p.group = QString::fromLatin1(command::groups::kData);
    p.query = &query_strategies;
    p.owner = QStringLiteral("backtesting");
    p.cap = 6;
    p.min_chars = 2;
    command::CommandRegistry::instance().register_provider(p);
}

} // namespace bt_cmd

void bt_install_commands() {
    bt_cmd::install();
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(bt_install_commands)

} // namespace fincept::screens
