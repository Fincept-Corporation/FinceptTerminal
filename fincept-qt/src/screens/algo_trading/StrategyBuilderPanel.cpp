// src/screens/algo_trading/StrategyBuilderPanel.cpp
#include "screens/algo_trading/StrategyBuilderPanel.h"

#include "algo_engine/AlgoEngine.h"
#include "algo_engine/AlgoMarket.h"
#include "algo_engine/fno/FnoAlgoTypes.h"
#include "algo_engine/fno/FnoExecution.h"
#include "algo_engine/fno/FnoStrategyPreview.h"
#include "core/agent/AgentRiskMarkers.h"
#include "core/events/EventBus.h"
#include "core/instrument/Instrument.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "screens/algo_trading/AlgoDeployDialog.h"
#include "screens/algo_trading/AlgoTabKit.h"
#include "screens/algo_trading/StrategyOptimizeDialog.h"
#include "screens/fno/BuilderAnalyticsRibbon.h"
#include "screens/fno/FnoFormat.h"
#include "screens/fno/PayoffChartWidget.h"
#include "services/algo_trading/AlgoRuleLogic.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "services/instruments/InstrumentResolver.h"
#include "services/options/OptionChainService.h"
#include "services/options/StrategyAnalytics.h"
#include "trading/AccountManager.h"
#include "trading/BrokerAccount.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadGate.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/algo/AlgoCurrency.h"
#include "ui/widgets/algo/FnoLegRuleEditor.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QFontMetrics>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <utility>

namespace fincept::screens {
namespace algo_ns = fincept::algo;
namespace kit = fincept::screens::algo_kit;
namespace rules = fincept::services::algo::rules;

namespace {

using SbpSB = fincept::ui::ScreenStatusBar;

// Bounded, non-blocking fetch of the current LTP for deploy-time sanity checks.
//
// The broker round-trip runs on a worker and `on_result` is invoked EXACTLY ONCE on
// `receiver`'s thread. 0.0 means "no usable quote": missing broker/account/symbol, a failed
// request, or a probe that outran `timeout_ms` (logged).
template <typename OnResult>
void fetch_quote_ltp_async(const QString& broker_id, const QString& account_id, const QString& symbol, int timeout_ms,
                           QObject* receiver, OnResult on_result) {
    auto answered = std::make_shared<std::atomic<bool>>(false);
    QPointer<QObject> guard(receiver);

    auto deliver = [answered, guard, on_result](double ltp) {
        if (answered->exchange(true, std::memory_order_acq_rel))
            return; // worker vs timeout race — first answer wins
        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, on_result, ltp]() {
                if (guard)
                    on_result(ltp);
            },
            Qt::QueuedConnection);
    };

    if (broker_id.isEmpty() || account_id.isEmpty() || symbol.isEmpty()) {
        deliver(0.0);
        return;
    }

    (void)QtConcurrent::run([broker_id, account_id, symbol, deliver]() {
        auto* broker = trading::BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            deliver(0.0);
            return;
        }
        auto creds = trading::AccountManager::instance().load_credentials(account_id);
        auto resp = broker->get_quotes(creds, {symbol});
        deliver(resp.success && resp.data.has_value() && !resp.data->isEmpty() ? resp.data->first().ltp : 0.0);
    });

    QTimer::singleShot(timeout_ms, receiver, [answered, deliver, symbol, timeout_ms]() {
        if (answered->load(std::memory_order_acquire))
            return;
        LOG_WARN("AlgoTrading", QString("Deploy sanity check: quote probe for %1 did not answer within %2 ms — "
                                        "skipping the unreachable-rule scan")
                                    .arg(symbol)
                                    .arg(timeout_ms));
        deliver(0.0);
    });
}

// Collects human-readable warnings for price-based leaves that can't fire at `price`
// (crossover already on the wrong side, or threshold wildly far from price). Only
// price-scale indicators are checked — RSI<30 etc. are left alone.
void scan_unreachable(const QJsonArray& conds, const QString& section, double price,
                      const fincept::ui::formatting::InstrumentFmt& inst, QStringList& out) {
    namespace fmt = fincept::ui::formatting;
    static const QSet<QString> price_inds = {"CLOSE", "OPEN", "HIGH", "LOW", "VWAP"};
    for (const auto& v : conds) {
        const QJsonObject o = v.toObject();
        if (o.contains("children")) {
            scan_unreachable(o.value("children").toArray(), section, price, inst, out);
            continue;
        }
        if (o.value("compare_mode").toString("value") != "value")
            continue;
        const QString ind = o.value("indicator").toString();
        if (!price_inds.contains(ind))
            continue;
        const QString op = o.value("operator").toString();
        const double val = o.value("value").toDouble();
        if (val <= 0)
            continue;
        if (op == "crosses_above" && price > val)
            out << QCoreApplication::translate("StrategyBuilderPanel",
                                               "• %1 '%2 crosses above %3' — price %4 is already above; it can never "
                                               "cross up.")
                       .arg(section, ind, fmt::price(val, inst), fmt::price(price, inst));
        else if (op == "crosses_below" && price < val)
            out << QCoreApplication::translate("StrategyBuilderPanel",
                                               "• %1 '%2 crosses below %3' — price %4 is already below; it can never "
                                               "cross down.")
                       .arg(section, ind, fmt::price(val, inst), fmt::price(price, inst));
        else if (std::abs(price - val) / price > 0.25)
            out << QCoreApplication::translate("StrategyBuilderPanel", "• %1 '%2 %3 %4' — threshold is %5 away from price %6.")
                       .arg(section, ind, op, fmt::price(val, inst),
                            fmt::pct(std::abs(price - val) / price * 100.0, 0, false), fmt::price(price, inst));
    }
}

/// What a backtest result depends on: everything but the identity (id / name /
/// description). Equal fingerprints = the on-screen report describes these rules.
QString sbp_definition_key(const services::algo::AlgoStrategy& s) {
    const QJsonObject o{{"tf", s.timeframe},
                        {"entry", s.entry_conditions},
                        {"exit", s.exit_conditions},
                        {"entry_logic", s.entry_logic},
                        {"exit_logic", s.exit_logic},
                        {"sl", s.stop_loss},
                        {"tp", s.take_profit},
                        {"trail", s.trailing_stop},
                        {"size", s.position_size_pct},
                        {"costs", s.cost_model},
                        {"side", s.entry_side},
                        {"rf", s.risk_free_pct},
                        {"type", s.instrument_type},
                        {"legs", s.legs}};
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

/// The symbol a brand-new draft is first backtested on: a broad, liquid fund of the
/// market the user's number-format locale points to (Yahoo tickers the native
/// engine resolves to a venue and currency), else SPY.
QString sbp_default_backtest_symbol() {
    const QString cc =
        QLocale::territoryToCode(fincept::ui::formatting::FormatSettings::instance().locale().territory());
    struct Row {
        const char* territory;
        const char* symbol;
    };
    static constexpr Row kRows[] = {
        {"IN", "NIFTYBEES.NS"}, {"GB", "ISF.L"},  {"DE", "EXS1.DE"}, {"JP", "1321.T"}, {"HK", "2800.HK"},
        {"CA", "XIU.TO"},       {"AU", "STW.AX"},
    };
    for (const auto& r : kRows)
        if (cc == QLatin1String(r.territory))
            return QString::fromLatin1(r.symbol);
    return QStringLiteral("SPY");
}

/// The summary kept as a strategy's last backtest, stamped with when the result on
/// `panel` actually ran (a remembered result keeps its original run time).
QJsonObject sbp_report_summary(const ui::algo::BacktestReportPanel* panel) {
    QJsonObject s = services::algo::AlgoTradingService::backtest_summary(panel->result());
    if (panel->ran_at().isValid())
        s[QStringLiteral("run_at")] = panel->ran_at().toUTC().toString(Qt::ISODate);
    return s;
}

// Remembered results live in the draft (screen state, cache.db): a few drafts, a few MB.
// A result larger than one entry's budget is not remembered (the draft runs again on reopen).
constexpr int kSbpMemoryMaxEntries = 4;
constexpr qsizetype kSbpMemoryMaxPayloadChars = 3 * 1024 * 1024;
constexpr qsizetype kSbpMemoryMaxTotalChars = 6 * 1024 * 1024;

/// Keeps the newest entries (by ran_at) within the entry and size budgets.
void sbp_trim_backtest_memory(QVariantMap& memory) {
    QList<QPair<QString, QString>> by_age; // (ran_at, key)
    for (auto it = memory.constBegin(); it != memory.constEnd(); ++it)
        by_age.append({it.value().toMap().value(QStringLiteral("ran_at")).toString(), it.key()});
    std::sort(by_age.begin(), by_age.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    qsizetype total = 0;
    int kept = 0;
    for (const auto& entry : by_age) {
        const QString& key = entry.second;
        const qsizetype size = memory.value(key).toMap().value(QStringLiteral("payload")).toString().size();
        if (kept >= kSbpMemoryMaxEntries || size == 0 || size > kSbpMemoryMaxPayloadChars ||
            total + size > kSbpMemoryMaxTotalChars) {
            memory.remove(key);
            continue;
        }
        total += size;
        ++kept;
    }
}

QLabel* sbp_caption(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("algoFieldLabel"));
    return l;
}

int sbp_chars(const QWidget* w, int n) {
    return QFontMetrics(w->font()).averageCharWidth() * n + 16;
}

} // namespace

StrategyBuilderPanel::StrategyBuilderPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("strategyBuilderPanel"));
    build_ui();
    install_shortcuts();
    connect_service();
    retranslateUi();
    refresh_definition_feedback();
}

// ═════════════════════════════════════════════════════════════════════════════
// Layout
// ═════════════════════════════════════════════════════════════════════════════

void StrategyBuilderPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_command_bar();
    root->addWidget(bar_);
    root->addWidget(build_header_strip());
    root->addWidget(build_templates_strip());

    splitter_ = kit::make_splitter(Qt::Horizontal, this);
    splitter_->addWidget(build_definition_zone(splitter_));
    splitter_->addWidget(build_result_zone(splitter_));
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    root->addWidget(splitter_, 1);

    status_ = kit::make_status_bar(this);
    root->addWidget(status_);

    on_symbol_changed();
    apply_rules_arrangement();
}

void StrategyBuilderPanel::build_command_bar() {
    using Role = ui::ScreenCommandBar::Role;
    bar_ = kit::make_tab_bar(this, kit::TabBuilder);
    bar_->add_action(QStringLiteral("optimize"), QString(), QString(), QString(), Role::Normal, 40);
    bar_->add_action(QStringLiteral("saveas"), QString(), QStringLiteral("Ctrl+Shift+S"), QString(), Role::Normal, 45);
    bar_->add_action(QStringLiteral("save"), QString(), QStringLiteral("Ctrl+S"), QString(), Role::Normal, 70);
    QPushButton* deploy_action = bar_->add_action(QStringLiteral("deploy"), QString(), QString(), QString(),
                                                  Role::Normal, 60);
    // Starts trading (paper by default, live after its confirmation): the deploy dialog's OK is
    // the order step; this button opens it and carries the same kind.
    deploy_action->setProperty("fincept_agent_risk", "order");
    bar_->add_action(QStringLiteral("run"), QString(), QStringLiteral("Ctrl+↵"), QString(), Role::Primary, 100);
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, [this](const QString& id) {
        if (id == QLatin1String("run")) {
            if (run_in_flight_)
                services::algo::AlgoTradingService::instance().cancel_backtest();
            else
                on_backtest();
        } else if (id == QLatin1String("save")) {
            on_save();
        } else if (id == QLatin1String("saveas")) {
            on_save_as();
        } else if (id == QLatin1String("deploy")) {
            on_deploy();
        } else if (id == QLatin1String("optimize")) {
            on_optimize();
        }
    });
}

QWidget* StrategyBuilderPanel::build_header_strip() {
    ui::FlowLayout* flow = nullptr;
    QWidget* strip = kit::make_strip(this, &flow);

    name_edit_ = new QLineEdit(strip);
    kit::mark_field(name_edit_, tr("Strategy name"));
    name_edit_->setMinimumWidth(sbp_chars(name_edit_, 16));
    desc_edit_ = new QLineEdit(strip);
    kit::mark_field(desc_edit_, tr("Description"));
    desc_edit_->setMinimumWidth(sbp_chars(desc_edit_, 24));

    symbol_box_ = new ui::SymbolSearchBox(strip);
    symbol_box_->set_clear_on_pick(false);
    symbol_box_->set_compact(true);
    symbol_box_->set_recent_key(QStringLiteral("algo_builder"));
    {
        services::ResolveOptions opts;
        using core::instrument::AssetClass;
        opts.ctx.allowed_classes = {AssetClass::Equity, AssetClass::Etf,    AssetClass::Index,
                                    AssetClass::Fx,     AssetClass::Crypto, AssetClass::Future};
        symbol_box_->set_options(opts);
    }
    symbol_box_->setMinimumWidth(sbp_chars(symbol_box_, 14));
    symbol_box_->setAccessibleName(tr("Instrument"));
    connect(symbol_box_, &ui::SymbolSearchBox::instrument_picked, this,
            [this](const core::instrument::Instrument&) { on_symbol_changed(); });
    if (QLineEdit* le = symbol_box_->line_edit())
        connect(le, &QLineEdit::editingFinished, this, [this]() { on_symbol_changed(); });

    instrument_type_combo_ = new QComboBox(strip);
    kit::mark_field(instrument_type_combo_, tr("Instrument type"));
    instrument_type_combo_->addItem(QString(), QStringLiteral("equity"));
    instrument_type_combo_->addItem(QString(), QStringLiteral("option"));
    instrument_type_combo_->addItem(QString(), QStringLiteral("future"));
    // Futures legs cannot be resolved by the engine yet (only CE/PE from the option chain), so
    // "Future" is not offered for new strategies. It stays in the list (selectable
    // programmatically) so a saved future strategy still loads as one.
    if (auto* model = qobject_cast<QStandardItemModel*>(instrument_type_combo_->model())) {
        if (auto* item = model->item(2))
            item->setEnabled(false);
    }

    timeframe_combo_ = new QComboBox(strip);
    kit::mark_field(timeframe_combo_, tr("Bar timeframe"));
    for (const auto& tf : services::algo::algo_timeframes())
        timeframe_combo_->addItem(tf);
    timeframe_combo_->setCurrentText(QStringLiteral("1d"));

    // The cost model and the entry side are strategy properties (ExecutionCostPanel); the side
    // reads best next to the instrument, so its combo is shown here.
    cost_panel_ = new ui::algo::ExecutionCostPanel(this);
    QComboBox* side = cost_panel_->take_side_combo();
    side->setAccessibleName(tr("Entry side"));

    bt_capital_ = new QDoubleSpinBox(strip);
    kit::mark_field(bt_capital_, tr("Initial capital"));
    bt_capital_->setRange(1000, 100000000);
    bt_capital_->setDecimals(0);
    bt_capital_->setSingleStep(10000);
    bt_capital_->setGroupSeparatorShown(true);
    bt_capital_->setValue(100000);
    bt_capital_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    kit::guard_wheel(bt_capital_);

    const QDate today = QDate::currentDate();
    bt_start_date_ = new QDateEdit(strip);
    bt_end_date_ = new QDateEdit(strip);
    for (QDateEdit* e : {bt_start_date_, bt_end_date_}) {
        e->setObjectName(QStringLiteral("algoField"));
        e->setCalendarPopup(true);
        e->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        e->setMinimumDate(QDate(2000, 1, 1));
        e->setMaximumDate(today);
        kit::guard_wheel(e);
    }
    bt_start_date_->setAccessibleName(tr("Backtest from"));
    bt_end_date_->setAccessibleName(tr("Backtest to"));
    bt_start_date_->setDate(today.addYears(-3));
    bt_end_date_->setDate(today);
    auto* arrow = new QLabel(QStringLiteral("→"), strip);
    arrow->setObjectName(QStringLiteral("algoFieldLabel"));
    range_seg_ = kit::make_segmented({tr("1Y"), tr("3Y"), tr("5Y"), tr("MAX")}, 1,
                                     [this](int i) { apply_range_preset(i == 0 ? 1 : i == 1 ? 3 : i == 2 ? 5 : 25); },
                                     strip);

    state_chip_ = kit::make_chip(QString(), kit::Tone::Accent, strip);

    flow->addWidget(kit::field_group(strip, tr("NAME"), {name_edit_}));
    flow->addWidget(kit::field_group(strip, tr("DESC"), {desc_edit_}, tr("Description (optional)")));
    flow->addWidget(kit::field_group(strip, tr("INSTRUMENT"), {symbol_box_}));
    flow->addWidget(kit::field_group(strip, tr("TYPE"), {instrument_type_combo_}, tr("Instrument type this strategy trades")));
    flow->addWidget(kit::field_group(strip, tr("TF"), {timeframe_combo_}, tr("Bar timeframe the rules evaluate on")));
    flow->addWidget(kit::field_group(strip, tr("SIDE"), {side}, tr("Direction of the entry signal (long or short)")));
    equity_only_fields_ = new QWidget(strip);
    {
        auto* h = new QHBoxLayout(equity_only_fields_);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(10);
        h->addWidget(kit::field_group(equity_only_fields_, tr("CAPITAL"), {bt_capital_}));
        h->addWidget(kit::field_group(equity_only_fields_, tr("PERIOD"), {bt_start_date_, arrow, bt_end_date_, range_seg_},
                                      tr("Backtest period (exchange calendar dates)")));
    }
    flow->addWidget(equity_only_fields_);
    flow->addWidget(state_chip_);

    connect(timeframe_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        on_symbol_changed(); // intraday vs daily cost preset
        refresh_definition_feedback();
    });
    connect(instrument_type_combo_, &QComboBox::currentIndexChanged, this,
            [this](int) { on_instrument_type_changed(); });
    for (QDateEdit* e : {bt_start_date_, bt_end_date_})
        connect(e, &QDateEdit::dateChanged, this, [this](const QDate&) { kit::set_segment(range_seg_, -1); });
    connect(name_edit_, &QLineEdit::textEdited, this, [this](const QString&) { refresh_definition_feedback(); });
    return strip;
}

QWidget* StrategyBuilderPanel::build_templates_strip() {
    ui::FlowLayout* flow = nullptr;
    QWidget* strip = kit::make_strip(this, &flow);
    templates_caption_ = sbp_caption(QString(), strip);
    flow->addWidget(templates_caption_);
    const auto tpls = services::algo::algo_strategy_templates();
    for (int i = 0; i < tpls.size(); ++i) {
        auto* b = new QPushButton(tpls[i].name, strip);
        b->setObjectName(QStringLiteral("algoTemplate"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::StrongFocus);
        b->setAccessibleName(tr("Template %1").arg(tpls[i].name));
        b->setToolTip(QStringLiteral("%1\n%2\n%3\n%4")
                          .arg(tpls[i].description,
                               rules::rules_sentence(tpls[i].entry, tpls[i].entry_logic, rules::Side::Entry),
                               rules::rules_sentence(tpls[i].exit, tpls[i].exit_logic, rules::Side::Exit),
                               tr("Timeframe %1 · stop %2 % · target %3 %")
                                   .arg(tpls[i].timeframe)
                                   .arg(tpls[i].stop_loss)
                                   .arg(tpls[i].take_profit)));
        connect(b, &QPushButton::clicked, this, [this, i]() { load_template(i); });
        flow->addWidget(b);
        template_buttons_.append(b);
    }
    return strip;
}

QWidget* StrategyBuilderPanel::build_definition_zone(QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setObjectName(QStringLiteral("algoScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded); // only at very large fonts
    auto* content = new QWidget(scroll);
    scroll->setWidget(content);
    auto* v = new QVBoxLayout(content);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(6);

    validation_note_ = kit::make_note(QString(), content, kit::Tone::Danger);
    validation_note_->setVisible(false);
    v->addWidget(validation_note_);

    // Entry / exit rule tables (side by side on wide screens — apply_rules_arrangement()).
    rules_box_ = new QWidget(content);
    rules_grid_ = new QGridLayout(rules_box_);
    rules_grid_->setContentsMargins(0, 0, 0, 0);
    rules_grid_->setSpacing(6);
    auto entry = kit::make_panel(QString(), rules_box_);
    entry_rules_ = new ui::algo::RuleTableEditor(ui::algo::RuleTableEditor::Kind::Entry, entry.body_widget);
    entry.body->addWidget(entry_rules_);
    entry.header->add_control(entry_rules_->header_controls());
    entry_panel_ = entry.frame;
    entry_header_ = entry.header;
    auto exit = kit::make_panel(QString(), rules_box_);
    exit_rules_ = new ui::algo::RuleTableEditor(ui::algo::RuleTableEditor::Kind::Exit, exit.body_widget);
    exit.body->addWidget(exit_rules_);
    exit.header->add_control(exit_rules_->header_controls());
    exit_panel_ = exit.frame;
    exit_header_ = exit.header;
    for (QWidget* p : {entry_panel_, exit_panel_})
        p->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    v->addWidget(rules_box_);
    connect(entry_rules_, &ui::algo::RuleTableEditor::conditions_changed, this, [this]() { refresh_definition_feedback(); });
    connect(exit_rules_, &ui::algo::RuleTableEditor::conditions_changed, this, [this]() { refresh_definition_feedback(); });

    // Risk & sizing.
    auto risk = kit::make_panel(QString(), content);
    risk_panel_ = new ui::algo::RiskManagementPanel(risk.body_widget);
    risk.body->addWidget(risk_panel_);
    risk.frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    risk_box_ = risk.frame;
    risk_header_ = risk.header;
    v->addWidget(risk.frame);
    connect(risk_panel_, &ui::algo::RiskManagementPanel::values_changed, this, [this]() { refresh_definition_feedback(); });

    // Execution & costs: one summary line; EDIT shows the editor.
    auto exec = kit::make_panel(QString(), content);
    exec_summary_ = kit::make_note(QString(), exec.body_widget);
    exec.body->addWidget(exec_summary_);
    cost_panel_->setParent(exec.body_widget);
    exec.body->addWidget(cost_panel_);
    cost_panel_->setVisible(false);
    exec_toggle_ = kit::make_tool_button(QString(), QString(), exec.header);
    exec_toggle_->setCheckable(true);
    exec.header->add_control(exec_toggle_);
    exec.frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    exec_box_ = exec.frame;
    exec_header_ = exec.header;
    v->addWidget(exec.frame);
    connect(exec_toggle_, &QPushButton::toggled, this, [this](bool on) {
        cost_panel_->setVisible(on);
        retranslateUi();
    });
    connect(cost_panel_, &ui::algo::ExecutionCostPanel::changed, this, [this]() { refresh_definition_feedback(); });

    // Option / future legs (hidden for equity).
    build_fno_section(content, nullptr);
    v->addWidget(fno_section_);
    fno_section_->setVisible(false);

    v->addStretch(1);
    return scroll;
}

QWidget* StrategyBuilderPanel::build_result_zone(QWidget* parent) {
    auto* zone = new QWidget(parent);
    auto* v = new QVBoxLayout(zone);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    report_panel_ = new ui::algo::BacktestReportPanel(zone);
    v->addWidget(report_panel_, 1);
    build_fno_section(nullptr, zone);
    v->addWidget(fno_result_, 1);
    fno_result_->setVisible(false);

    connect(report_panel_, &ui::algo::BacktestReportPanel::cancel_requested, this,
            []() { services::algo::AlgoTradingService::instance().cancel_backtest(); });
    // The idle page's RUN action takes the RUN BACKTEST path (validation included).
    connect(report_panel_, &ui::algo::BacktestReportPanel::run_requested, this, &StrategyBuilderPanel::on_backtest);
    return zone;
}

void StrategyBuilderPanel::install_shortcuts() {
    auto add = [this](const QKeySequence& seq, std::function<void()> fn) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, std::move(fn));
        return sc;
    };
    add(QKeySequence(Qt::CTRL | Qt::Key_Return), [this]() { on_backtest(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Enter), [this]() { on_backtest(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_S), [this]() { on_save(); });
    add(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), [this]() { on_save_as(); });
    add(QKeySequence(Qt::Key_Escape), [this]() {
        if (run_in_flight_)
            services::algo::AlgoTradingService::instance().cancel_backtest();
    });
}

void StrategyBuilderPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    apply_rules_arrangement();
}

void StrategyBuilderPanel::apply_rules_arrangement() {
    if (!rules_grid_)
        return;
    // Side by side when the whole sub-tab is wide (>= 1500 px at the default font, more for a
    // larger one): each table then still has the width of a stacked one on a 1280 screen.
    const int threshold = std::max(1500, QFontMetrics(font()).averageCharWidth() * 190);
    const bool wide = width() >= threshold;
    if (wide == rules_side_by_side_ && rules_grid_->count() == 2)
        return;
    rules_side_by_side_ = wide;
    rules_grid_->removeWidget(entry_panel_);
    rules_grid_->removeWidget(exit_panel_);
    rules_grid_->addWidget(entry_panel_, 0, 0, Qt::AlignTop);
    if (wide)
        rules_grid_->addWidget(exit_panel_, 0, 1, Qt::AlignTop);
    else
        rules_grid_->addWidget(exit_panel_, 1, 0, Qt::AlignTop);
    rules_grid_->setColumnStretch(0, 1);
    rules_grid_->setColumnStretch(1, wide ? 1 : 0);
}

void StrategyBuilderPanel::apply_range_preset(int years) {
    const QDate today = QDate::currentDate();
    QSignalBlocker b1(bt_start_date_);
    QSignalBlocker b2(bt_end_date_);
    bt_start_date_->setDate(years >= 25 ? QDate(2000, 1, 1) : today.addYears(-years));
    bt_end_date_->setDate(today);
}

QString StrategyBuilderPanel::symbol_text() const {
    return symbol_box_ ? symbol_box_->text().trimmed() : QString();
}

void StrategyBuilderPanel::set_symbol_text(const QString& symbol) {
    if (symbol_box_)
        symbol_box_->set_text(symbol.trimmed());
}

QString StrategyBuilderPanel::data_source_text() const {
    // AlgoTradingService: a connected broker's bars when one is connected, else Yahoo.
    auto& accts = trading::AccountManager::instance();
    for (const auto& acc : accts.active_accounts())
        if (accts.connection_state(acc.account_id) == trading::ConnectionState::Connected)
            return tr("DATA · %1 (connected broker)").arg(acc.broker_id.toUpper());
    return tr("DATA · Yahoo Finance (daily bars adjusted)");
}

void StrategyBuilderPanel::notify(const QString& text, int tone) {
    if (status_)
        status_->show_notice(text, QString(), {}, 8000, static_cast<SbpSB::Tone>(tone));
}

void StrategyBuilderPanel::set_run_busy(bool busy) {
    if (!bar_)
        return;
    bar_->set_action_text(QStringLiteral("run"), busy ? tr("CANCEL") : tr("RUN BACKTEST"));
    bar_->set_action_hint(QStringLiteral("run"), busy ? QStringLiteral("Esc") : QStringLiteral("Ctrl+↵"));
    bar_->set_action_role(QStringLiteral("run"), busy ? ui::ScreenCommandBar::Role::Danger
                                                      : ui::ScreenCommandBar::Role::Primary);
    bar_->set_action_tooltip(QStringLiteral("run"), busy ? tr("Cancel the running backtest (Esc)")
                                                         : tr("Backtest these rules on the instrument (Ctrl+Enter)"));
    kit::set_state(status_, busy ? kit::RunState::Running : kit::RunState::Ready);
}

void StrategyBuilderPanel::show_validation(const QString& text) {
    validation_note_->setText(text);
    validation_note_->setVisible(!text.isEmpty());
}

void StrategyBuilderPanel::refresh_definition_feedback() {
    if (!entry_rules_ || !status_)
        return;
    const auto strat_tf = timeframe_combo_ ? timeframe_combo_->currentText() : QString();
    // Inline validation follows every edit (RUN refuses with the same message).
    const bool equity = !instrument_type_combo_ || instrument_type_combo_->currentData().toString() == QLatin1String("equity");
    show_validation(equity ? validate() : QString());
    exec_summary_->setText(cost_panel_->summary_text());
    // Template chips: the one whose rules are on screen is lit.
    const auto tpls = services::algo::algo_strategy_templates();
    const QJsonArray entry = entry_rules_->conditions();
    const QJsonArray exit = exit_rules_->conditions();
    for (int i = 0; i < template_buttons_.size() && i < tpls.size(); ++i) {
        const bool on = tpls[i].entry == entry && tpls[i].exit == exit;
        QPushButton* b = template_buttons_[i];
        if (b->property("active").toBool() != on) {
            b->setProperty("active", on);
            b->style()->unpolish(b);
            b->style()->polish(b);
        }
    }
    const QString name = name_edit_->text().trimmed();
    status_->set_text(QStringLiteral("info"),
                      tr("%1 · %2 entry / %3 exit rule(s) · %4")
                          .arg(name.isEmpty() ? tr("unnamed draft") : name)
                          .arg(entry_rules_->row_count())
                          .arg(exit_rules_->row_count())
                          .arg(strat_tf));
    if (bar_)
        bar_->set_action_tooltip(QStringLiteral("run"),
                                 run_in_flight_ ? tr("Cancel the running backtest (Esc)")
                                                : (validate().isEmpty() ? tr("Backtest these rules on the instrument (Ctrl+Enter)")
                                                                        : tr("Cannot run yet: %1").arg(validate())));
}

void StrategyBuilderPanel::on_symbol_changed() {
    if (!bt_capital_)
        return;
    const QString sym = symbol_text();
    const QString hint = sym.isEmpty() ? QString() : services::algo::AlgoTradingService::default_exchange_hint(sym);
    const auto* venue = sym.isEmpty() ? nullptr : algo_ns::algo_venue(sym, hint);
    // Capital, the order-value limit and flat commissions are in the instrument's quote
    // currency (the engine buys the symbol with it): "BTC-USD" -> USD, "VOD.L" -> GBp.
    // Unknown -> no unit (never the display currency, which would mislabel it).
    const QString ccy = sym.isEmpty() ? QString() : ui::algo::algo_currency_for(sym, hint);
    ui::algo::algo_bind_money_spin(bt_capital_, ccy);
    if (risk_panel_)
        risk_panel_->set_currency(ccy);
    if (cost_panel_)
        cost_panel_->set_currency(ccy);
    bt_capital_->setToolTip(ccy.isEmpty() ? tr("Starting capital")
                                          : tr("Starting capital, in %1 (the currency %2 trades in)").arg(ccy, sym));
    if (cost_panel_ && venue) {
        const QString tf = timeframe_combo_ ? timeframe_combo_->currentText() : QString();
        cost_panel_->suggest_for(algo_ns::venue_country_of(venue), algo_ns::bar_seconds_for(tf) < 86400);
    }
    if (status_)
        status_->set_text(QStringLiteral("data"), data_source_text());
    refresh_definition_feedback();
}

// ── F&O Section ──────────────────────────────────────────────────────────────

void StrategyBuilderPanel::build_fno_section(QWidget* left_parent, QWidget* right_parent) {
    if (left_parent) {
        auto legs = kit::make_panel(tr("OPTION LEGS"), left_parent);
        fno_section_ = legs.frame;
        fno_section_->setObjectName(QStringLiteral("algoPanel"));
        legs.frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        QWidget* body = legs.body_widget;
        auto* header_row = new QHBoxLayout();
        header_row->setSpacing(8);

        underlying_combo_ = new QComboBox(body);
        kit::mark_field(underlying_combo_, tr("Underlying"), tr("Underlying instrument"));
        underlying_combo_->setMinimumWidth(sbp_chars(underlying_combo_, 10));
        populate_underlyings();

        expiry_mode_combo_ = new QComboBox(body);
        kit::mark_field(expiry_mode_combo_, tr("Expiry rule"), tr("Expiry selection rule — applied to every leg"));
        // userData is the engine's rule token (fno::resolve_expiry compares these).
        expiry_mode_combo_->addItem(tr("Weekly"), QStringLiteral("WEEKLY"));
        expiry_mode_combo_->addItem(tr("Monthly"), QStringLiteral("MONTHLY"));
        expiry_mode_combo_->addItem(tr("Nearest DTE"), QStringLiteral("NEAREST_DTE"));
        expiry_mode_combo_->addItem(tr("Absolute"), QStringLiteral("ABSOLUTE"));

        expiry_dte_spin_ = new QSpinBox(body);
        kit::mark_field(expiry_dte_spin_, tr("Minimum days to expiry"),
                        tr("Trade the first expiry at least this many days away"));
        expiry_dte_spin_->setRange(0, 365);
        expiry_dte_spin_->setValue(7);
        expiry_dte_spin_->setSuffix(tr(" DTE"));

        expiry_abs_combo_ = new QComboBox(body);
        kit::mark_field(expiry_abs_combo_, tr("Expiry"), tr("Trade this exact expiry"));

        header_row->addWidget(sbp_caption(tr("UNDERLYING"), body));
        header_row->addWidget(underlying_combo_, 1);
        header_row->addWidget(sbp_caption(tr("EXPIRY"), body));
        header_row->addWidget(expiry_mode_combo_, 1);
        header_row->addWidget(expiry_dte_spin_);
        header_row->addWidget(expiry_abs_combo_);
        legs.body->addLayout(header_row);
        expiry_dte_spin_->setVisible(false);
        expiry_abs_combo_->setVisible(false);

        leg_editor_ = new fincept::ui::algo::FnoLegRuleEditor(body);
        legs.body->addWidget(leg_editor_);
        return;
    }

    // Right zone: the payoff preview is an option strategy's result.
    auto pay = kit::make_panel(tr("PAYOFF PREVIEW"), right_parent, 6);
    fno_result_ = pay.frame;
    QWidget* body = pay.body_widget;
    fno_preview_note_ = kit::make_note(QString(), body);
    fno_preview_note_->setVisible(false);
    pay.body->addWidget(fno_preview_note_);
    analytics_ribbon_ = new fincept::screens::fno::BuilderAnalyticsRibbon(body);
    pay.body->addWidget(analytics_ribbon_);
    payoff_chart_ = new fincept::screens::fno::PayoffChartWidget(body);
    payoff_chart_->setMinimumHeight(QFontMetrics(font()).height() * 10);
    pay.body->addWidget(payoff_chart_, 1);

    // Preview triggers. Underlying / expiry changes re-target the chain the preview
    // reads (ensure_preview_chain), then redraw.
    auto retarget = [this]() {
        sync_expiry_inputs();
        ensure_preview_chain();
        refresh_fno_preview();
    };
    connect(leg_editor_, &fincept::ui::algo::FnoLegRuleEditor::legs_changed, this,
            &StrategyBuilderPanel::refresh_fno_preview);
    connect(underlying_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, retarget);
    connect(expiry_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, retarget);
    connect(expiry_dte_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        ensure_preview_chain();
        refresh_fno_preview();
    });
    connect(expiry_abs_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        ensure_preview_chain();
        refresh_fno_preview();
    });
    // Only the selected series is adopted — a late publish of another underlying /
    // expiry (e.g. the F&O screen's chain) must not replace the preview.
    connect(&fincept::services::options::OptionChainService::instance(),
            &fincept::services::options::OptionChainService::chain_published, this,
            [this](const fincept::services::options::OptionChain& chain) {
                if (preview_topic_.isEmpty() || chain.broker_id != preview_broker_ ||
                    chain.underlying != preview_underlying_ || chain.expiry != preview_expiry_)
                    return;
                preview_chain_ = chain;
                refresh_fno_preview();
            });
}

QString StrategyBuilderPanel::expiry_mode() const {
    const QString m = expiry_mode_combo_ ? expiry_mode_combo_->currentData().toString() : QString();
    return m.isEmpty() ? QStringLiteral("WEEKLY") : m;
}

QString StrategyBuilderPanel::expiry_value() const {
    const QString m = expiry_mode();
    if (m == QLatin1String("NEAREST_DTE") && expiry_dte_spin_)
        return QString::number(expiry_dte_spin_->value());
    if (m == QLatin1String("ABSOLUTE") && expiry_abs_combo_)
        return expiry_abs_combo_->currentText().trimmed();
    return QString();
}

QString StrategyBuilderPanel::expiry_rule_label() const {
    const QString m = expiry_mode();
    QString label;
    if (m == QLatin1String("MONTHLY"))
        label = tr("monthly expiry");
    else if (m == QLatin1String("NEAREST_DTE"))
        label = tr("first expiry ≥ %1 DTE").arg(expiry_value());
    else if (m == QLatin1String("ABSOLUTE"))
        label = tr("expiry %1").arg(expiry_value());
    else
        label = tr("nearest expiry");
    if (m != QLatin1String("ABSOLUTE") && underlying_combo_) {
        const QString now = fincept::algo::fno::resolve_expiry(
            m, expiry_value(),
            fincept::services::options::OptionChainService::instance().list_expiries(fno_broker_id(),
                                                                                      underlying_combo_->currentText()),
            QDate::currentDate());
        if (!now.isEmpty())
            label += tr(" (today: %1)").arg(now);
    }
    return label;
}

void StrategyBuilderPanel::sync_expiry_inputs() {
    if (!expiry_mode_combo_)
        return;
    const QString m = expiry_mode();
    if (expiry_dte_spin_)
        expiry_dte_spin_->setVisible(m == QLatin1String("NEAREST_DTE"));
    if (expiry_abs_combo_) {
        expiry_abs_combo_->setVisible(m == QLatin1String("ABSOLUTE"));
        if (m == QLatin1String("ABSOLUTE")) {
            const QString keep = expiry_abs_combo_->currentText();
            const QStringList list =
                underlying_combo_ ? fincept::services::options::OptionChainService::instance().list_expiries(
                                        fno_broker_id(), underlying_combo_->currentText())
                                  : QStringList();
            QSignalBlocker blocker(expiry_abs_combo_);
            expiry_abs_combo_->clear();
            expiry_abs_combo_->addItems(list);
            if (!keep.isEmpty()) {
                if (expiry_abs_combo_->findText(keep) < 0)
                    expiry_abs_combo_->addItem(keep);
                expiry_abs_combo_->setCurrentText(keep);
            }
        }
    }
}

void StrategyBuilderPanel::apply_expiry_rule(const QString& mode, const QString& value) {
    if (!expiry_mode_combo_)
        return;
    const int idx = expiry_mode_combo_->findData(mode.trimmed().toUpper());
    {
        QSignalBlocker blocker(expiry_mode_combo_);
        expiry_mode_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    const QString m = expiry_mode();
    if (m == QLatin1String("NEAREST_DTE") && expiry_dte_spin_) {
        QSignalBlocker blocker(expiry_dte_spin_);
        expiry_dte_spin_->setValue(value.toInt());
    }
    if (m == QLatin1String("ABSOLUTE") && expiry_abs_combo_ && !value.isEmpty()) {
        QSignalBlocker blocker(expiry_abs_combo_);
        if (expiry_abs_combo_->findText(value) < 0)
            expiry_abs_combo_->addItem(value);
        expiry_abs_combo_->setCurrentText(value);
    }
    sync_expiry_inputs();
}

void StrategyBuilderPanel::ensure_preview_chain() {
    if (!underlying_combo_ || !underlying_combo_->isEnabled())
        return;
    auto& svc = fincept::services::options::OptionChainService::instance();
    const QString broker = fno_broker_id();
    const QString underlying = underlying_combo_->currentText();
    const QString expiry = fincept::algo::fno::resolve_expiry(expiry_mode(), expiry_value(),
                                                              svc.list_expiries(broker, underlying), QDate::currentDate());
    preview_broker_ = broker;
    preview_underlying_ = underlying;
    preview_expiry_ = expiry;
    const QString topic = expiry.isEmpty() ? QString() : svc.chain_topic(broker, underlying, expiry);
    if (topic == preview_topic_)
        return;
    release_preview_chain();
    preview_topic_ = topic;
    preview_chain_ = fincept::services::options::OptionChain{};
    if (topic.isEmpty())
        return;
    // Adopt the service's current chain when it already is this series.
    const auto& last = svc.last_chain();
    if (last.broker_id == broker && last.underlying == underlying && last.expiry == expiry)
        preview_chain_ = last;
    // Subscribing wakes the producer; the data itself arrives via chain_published.
    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe(this, topic, [](const QVariant&) {});
    hub.request(topic, /*force*/ false);
}

void StrategyBuilderPanel::release_preview_chain() {
    if (!preview_topic_.isEmpty())
        fincept::datahub::DataHub::instance().unsubscribe(this, preview_topic_);
    preview_topic_.clear();
}

void StrategyBuilderPanel::populate_underlyings() {
    if (!underlying_combo_)
        return;
    const QStringList list =
        fincept::services::options::OptionChainService::instance().list_underlyings(fno_broker_id());
    QSignalBlocker blocker(underlying_combo_);
    underlying_combo_->clear();
    if (list.isEmpty()) {
        underlying_combo_->addItem(tr("(connect broker)"));
        underlying_combo_->setEnabled(false);
    } else {
        for (const auto& u : list)
            underlying_combo_->addItem(u);
        underlying_combo_->setEnabled(true);
    }
}

void StrategyBuilderPanel::refresh_fno_preview() {
    namespace an = fincept::services::options::analytics;
    // The chain of the SELECTED underlying + expiry (see ensure_preview_chain), not
    // the service-wide last publish.
    const auto& chain = preview_chain_;
    const auto legs = leg_editor_->legs();
    if (fno_preview_note_) {
        QString note;
        const QString underlying = underlying_combo_ ? underlying_combo_->currentText() : QString();
        if (underlying_combo_ && !underlying_combo_->isEnabled())
            note = tr("Connect a broker to load option chains.");
        else if (preview_expiry_.isEmpty())
            note = tr("No %1 expiry matches the selected rule (expiry list not loaded yet?).").arg(underlying);
        else if (chain.rows.isEmpty())
            note = tr("Loading %1 %2 option chain…").arg(underlying, preview_expiry_);
        else if (legs.isEmpty())
            note = tr("Add a leg to see the payoff (%1 %2).").arg(underlying, preview_expiry_);
        fno_preview_note_->setText(note);
        fno_preview_note_->setVisible(!note.isEmpty());
    }
    if (legs.isEmpty() || chain.rows.isEmpty()) {
        analytics_ribbon_->clear();
        payoff_chart_->clear_payoff();
        return;
    }
    fincept::services::options::Strategy s = fincept::algo::fno::build_preview_strategy(legs, chain);
    an::PayoffComputeOptions opts;
    opts.current_spot = chain.spot;
    const auto curve = an::compute_payoff(s, opts);
    const auto bes = an::compute_breakevens(curve);
    const auto a = an::compute_all(s, chain, opts);
    // Premiums, P&L and margin are in the chain's currency, by its source (the F&O
    // screen's rule). Both widgets ignore an unchanged currency.
    const QString chain_ccy = fincept::screens::fno::fno_chain_ccy(chain.broker_id);
    payoff_chart_->set_currency(chain_ccy);
    analytics_ribbon_->set_currency(chain_ccy);
    payoff_chart_->set_payoff(curve, chain.spot, bes);
    analytics_ribbon_->update_from(s, a);
}

void StrategyBuilderPanel::on_instrument_type_changed() {
    if (!instrument_type_combo_)
        return;
    const bool equity = instrument_type_combo_->currentData().toString() == QLatin1String("equity");
    // Equity: rules + backtest. Option / Future: legs + payoff preview (no backtest here).
    rules_box_->setVisible(equity);
    equity_only_fields_->setVisible(equity);
    report_panel_->setVisible(equity);
    fno_section_->setVisible(!equity);
    fno_result_->setVisible(!equity);
    bar_->set_action_enabled(QStringLiteral("run"), equity);
    bar_->set_action_enabled(QStringLiteral("optimize"), equity);
    if (equity) {
        release_preview_chain();
        if (isVisible())
            schedule_default_backtest(); // back to equity with an empty result zone
    } else {
        if (underlying_combo_)
            populate_underlyings();
        sync_expiry_inputs();
        ensure_preview_chain();
        refresh_fno_preview();
    }
    refresh_definition_feedback();
}

QString StrategyBuilderPanel::fno_broker_id() const {
    // Mirror ChainSubTab: prefer a Connected account's broker_id, else "databento".
    for (const auto& acc : fincept::trading::AccountManager::instance().active_accounts()) {
        if (fincept::trading::AccountManager::instance().connection_state(acc.account_id) ==
            fincept::trading::ConnectionState::Connected)
            return acc.broker_id;
    }
    return QStringLiteral("databento");
}

// ═════════════════════════════════════════════════════════════════════════════
// Service
// ═════════════════════════════════════════════════════════════════════════════

void StrategyBuilderPanel::connect_service() {
    auto& svc = services::algo::AlgoTradingService::instance();
    connect(&svc, &services::algo::AlgoTradingService::backtest_result, this,
            &StrategyBuilderPanel::on_backtest_result);
    connect(&svc, &services::algo::AlgoTradingService::error_occurred, this, &StrategyBuilderPanel::on_error);
    connect(&svc, &services::algo::AlgoTradingService::strategy_saved, this, [this](const QString& id) {
        if (id != loaded_strategy_id_)
            return;
        state_chip_->setText(tr("SAVED"));
        kit::set_tone(state_chip_, kit::Tone::Ok);
        notify(tr("Strategy saved — it is in STRATEGIES (Alt+2)."), static_cast<int>(SbpSB::Tone::Ok));
    });
    connect(&svc, &services::algo::AlgoTradingService::backtest_started, this,
            [this](const QString& symbol, const QString& yahoo) {
                if (!run_in_flight_)
                    return;
                report_panel_->set_loading(symbol == yahoo || yahoo.isEmpty()
                                               ? tr("Backtesting %1…").arg(symbol)
                                               : tr("Backtesting %1 (data: %2)…").arg(symbol, yahoo));
            });
    connect(&svc, &services::algo::AlgoTradingService::backtest_progress, this,
            [this](int pct, const QString& stage) { report_panel_->set_progress(pct, stage); });
    connect(&svc, &services::algo::AlgoTradingService::backtest_cancelled, this, [this]() {
        if (!run_in_flight_)
            return;
        run_in_flight_ = false;
        set_run_busy(false);
        pending_memory_entry_.clear(); // nothing to remember
        release_auto_slot();
        report_panel_->set_cancelled();
        kit::set_state(status_, kit::RunState::Cancelled);
    });
}

services::algo::AlgoStrategy StrategyBuilderPanel::build_strategy() {
    if (loaded_strategy_id_.isEmpty())
        loaded_strategy_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);

    services::algo::AlgoStrategy strat;
    strat.id = loaded_strategy_id_;
    strat.name = name_edit_->text().trimmed();
    strat.description = desc_edit_->text().trimmed();
    strat.timeframe = timeframe_combo_->currentText();
    strat.entry_conditions = entry_rules_->conditions();
    strat.exit_conditions = exit_rules_->conditions();
    strat.entry_logic = entry_rules_->combined_logic();
    strat.exit_logic = exit_rules_->combined_logic();
    strat.stop_loss = risk_panel_->stop_loss();
    strat.take_profit = risk_panel_->take_profit();
    strat.trailing_stop = risk_panel_->trailing_stop();
    strat.position_size_pct = risk_panel_->capital_pct();
    strat.cost_model = cost_panel_->cost_model().to_json();
    strat.entry_side = cost_panel_->entry_side();
    strat.risk_free_pct = cost_panel_->risk_free_pct();
    strat.instrument_type =
        instrument_type_combo_ ? instrument_type_combo_->currentData().toString() : QStringLiteral("equity");
    if (strat.instrument_type != QLatin1String("equity") && leg_editor_) {
        // The engine reads the expiry rule from the legs; the leg editor has no expiry
        // column, so stamp the header's rule on every leg.
        auto legs = leg_editor_->legs();
        const QString mode = expiry_mode();
        const QString value = expiry_value();
        for (auto& l : legs) {
            l.expiry_mode = mode;
            l.expiry_value = value;
        }
        strat.legs = fincept::algo::fno::fno_legs_to_json(legs);
    }
    return strat;
}

void StrategyBuilderPanel::on_save() {
    if (name_edit_->text().trimmed().isEmpty()) {
        notify(tr("Enter a strategy name (NAME) before saving."), static_cast<int>(SbpSB::Tone::Warn));
        name_edit_->setFocus();
        return;
    }
    auto strat = build_strategy();
    // A draft backtested before its first save had no row for the run to update, so
    // STRATEGIES showed "not backtested". Carry the on-screen report along — only
    // while it still describes these rules.
    if (report_panel_->has_result() && shown_backtest_def_ == sbp_definition_key(strat))
        strat.last_backtest = sbp_report_summary(report_panel_);
    services::algo::AlgoTradingService::instance().save_strategy(strat);
    notify(tr("Saving %1…").arg(strat.name), static_cast<int>(SbpSB::Tone::Normal));
}

void StrategyBuilderPanel::on_save_as() {
    bool ok = false;
    const QString current = name_edit_->text().trimmed();
    const QString name = QInputDialog::getText(this, tr("Save as"), tr("Name of the new strategy:"), QLineEdit::Normal,
                                               current.isEmpty() ? tr("New strategy") : tr("%1 (copy)").arg(current), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    // A new strategy: a fresh id (the one being edited stays as it was saved).
    loaded_strategy_id_.clear();
    loaded_last_backtest_ = QJsonObject();
    name_edit_->setText(name);
    on_save();
}

void StrategyBuilderPanel::on_backtest() {
    start_backtest(false);
}

void StrategyBuilderPanel::start_backtest(bool automatic) {
    // The user's run supersedes a default run waiting for (or holding) a gate slot.
    if (!automatic)
        release_auto_slot();
    if (run_in_flight_)
        return; // one run at a time (the bar's RUN is CANCEL meanwhile)
    if (instrument_type_combo_ && instrument_type_combo_->currentData().toString() != QLatin1String("equity")) {
        if (automatic)
            release_auto_slot();
        return; // option / future strategies have the payoff preview, no backtest here
    }
    // Backtesting is non-persisting, so it needs no name — only valid rules and a sane
    // date range. (Save / Deploy still require a name.)
    if (bt_start_date_->date() >= bt_end_date_->date()) {
        show_validation(tr("PERIOD: the start date must be before the end date."));
        notify(tr("Cannot run: the start date must be before the end date."), static_cast<int>(SbpSB::Tone::Danger));
        if (automatic)
            release_auto_slot();
        return;
    }
    if (const QString err = validate(); !err.isEmpty()) {
        show_validation(err);
        notify(tr("Cannot run: %1").arg(err), static_cast<int>(SbpSB::Tone::Danger));
        if (automatic)
            release_auto_slot();
        return;
    }
    // No implicit default symbol for a click (the default run picks one for an empty draft).
    const QString symbol = symbol_text();
    if (symbol.isEmpty()) {
        show_validation(tr("INSTRUMENT: pick a symbol to backtest (e.g. AAPL, RELIANCE.NS, VOD.L, BTC-USD)."));
        notify(tr("Cannot run: pick an instrument."), static_cast<int>(SbpSB::Tone::Danger));
        symbol_box_->focus_and_select();
        if (automatic)
            release_auto_slot();
        return;
    }
    show_validation(QString());

    // Backtest the current UI state directly, without persisting.
    auto strat = build_strategy();
    run_in_flight_ = true;
    auto_run_ = automatic;
    set_run_busy(true);
    pending_backtest_def_ = sbp_definition_key(strat);
    // The result is remembered for the draft and inputs it was started with.
    pending_memory_entry_ = QVariantMap{
        {QStringLiteral("key"), draft_memory_key()},
        {QStringLiteral("def"), pending_backtest_def_},
        {QStringLiteral("inputs"), backtest_inputs_key()},
        {QStringLiteral("symbol"), symbol},
        {QStringLiteral("start"), bt_start_date_->date().toString(Qt::ISODate)},
        {QStringLiteral("end"), bt_end_date_->date().toString(Qt::ISODate)},
        {QStringLiteral("capital"), bt_capital_->value()},
    };
    ++memory_token_; // a remembered result still being parsed no longer applies
    remembered_loading_ = false;
    report_panel_->set_loading(tr("Backtesting %1…").arg(symbol));
    services::algo::AlgoTradingService::instance().run_backtest(
        strat, symbol, bt_start_date_->date().toString(QStringLiteral("yyyy-MM-dd")),
        bt_end_date_->date().toString(QStringLiteral("yyyy-MM-dd")), bt_capital_->value());
    if (strat.timeframe.compare(QStringLiteral("live"), Qt::CaseInsensitive) == 0)
        notify(tr("Backtesting on %1 bars — the bars a 'live' deployment aggregates; it evaluates per tick when "
                  "deployed, so fills there can differ.")
                   .arg(algo_ns::live_bar_timeframe()),
               static_cast<int>(SbpSB::Tone::Info));
}

// ── Default run on first show ─────────────────────────────────────────────────

void StrategyBuilderPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (status_)
        status_->start_clock();
    if (!splitter_sized_ && splitter_) {
        splitter_sized_ = true;
        // Definition | result: an even split at 1280; the definition gets a little more on wide
        // screens, where the entry and exit tables sit side by side. Sized once the layout has
        // given the panel its real width.
        QTimer::singleShot(0, this, [this]() {
            const int w = std::max(1, splitter_->width());
            const int left = width() >= std::max(1500, QFontMetrics(font()).averageCharWidth() * 190) ? w * 56 / 100
                                                                                                      : w * 50 / 100;
            splitter_->setSizes({left, w - left});
            apply_rules_arrangement();
        });
    }
    ++visit_;
    schedule_default_backtest();
}

void StrategyBuilderPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (status_)
        status_->stop_clock();
    // Leaving the tab (or the screen) cancels the DEFAULT run — it runs again on the next
    // visit. A run the user started keeps going.
    if (auto_queued_) {
        release_auto_slot();
        auto_inputs_.clear();
        report_panel_->clear();
        kit::set_state(status_, kit::RunState::Ready);
    } else if (auto_run_ && run_in_flight_) {
        auto_inputs_.clear();
        services::algo::AlgoTradingService::instance().cancel_backtest(); // → backtest_cancelled
    }
}

void StrategyBuilderPanel::schedule_default_backtest() {
    if (default_scheduled_)
        return;
    default_scheduled_ = true;
    // Next turn: the screen applies a restored draft right after building this panel.
    QTimer::singleShot(0, this, [this]() {
        default_scheduled_ = false;
        run_default_backtest();
    });
}

QString StrategyBuilderPanel::backtest_inputs_key() {
    return sbp_definition_key(build_strategy()) + QLatin1Char('|') + symbol_text() + QLatin1Char('|') +
           bt_start_date_->date().toString(Qt::ISODate) + QLatin1Char('|') + bt_end_date_->date().toString(Qt::ISODate) +
           QLatin1Char('|') + QString::number(bt_capital_->value(), 'f', 0);
}

void StrategyBuilderPanel::run_default_backtest() {
    // Only the visible Builder, only with nothing on the result zone, never over a run
    // (nor while a remembered result is being loaded onto it).
    if (!isVisible() || run_in_flight_ || auto_queued_ || remembered_loading_ || report_panel_->has_result())
        return;
    if (instrument_type_combo_ && instrument_type_combo_->currentData().toString() != QLatin1String("equity"))
        return; // Option / Future: the payoff preview is the result

    // A draft nobody wrote — no loaded strategy and no rules, or only the "Close > 0"
    // placeholder row old drafts were saved with (it trades every bar and loses to costs:
    // the −55 % / Sharpe −67 default result) — opens on the Golden Cross template with the
    // locale's default symbol and the template's look-back.
    if (loaded_strategy_id_.isEmpty() &&
        rules::is_blank_rules(entry_rules_->conditions(), exit_rules_->conditions())) {
        const auto tpls = services::algo::algo_strategy_templates();
        if (!tpls.isEmpty()) {
            load_template(0);
            const QDate today = QDate::currentDate();
            bt_start_date_->setDate(today.addDays(-services::algo::algo_default_lookback_days(tpls.first().timeframe)));
            bt_end_date_->setDate(today);
            notify(tr("Opened on the %1 template — edit the rules or pick another template, then RUN BACKTEST.")
                       .arg(tpls.first().name),
                   static_cast<int>(SbpSB::Tone::Info));
        }
    }
    if (symbol_text().isEmpty()) {
        set_symbol_text(default_backtest_symbol());
        on_symbol_changed();
    }
    // A blank or incomplete rule set never runs: the reason is shown instead.
    if (const QString err = validate(); !err.isEmpty() || bt_start_date_->date() >= bt_end_date_->date()) {
        show_validation(err.isEmpty() ? tr("PERIOD: the start date must be before the end date.") : err);
        return;
    }

    // One automatic attempt per visit and inputs (a failed default run is retried on
    // the next visit, not in a loop).
    const QString inputs = backtest_inputs_key();
    // Re-run only when the inputs changed: the remembered result of exactly these
    // inputs is shown ("as of" its run) instead.
    if (offer_remembered_backtest(true))
        return;
    if (inputs == auto_inputs_ && auto_visit_ == visit_)
        return;
    auto_inputs_ = inputs;
    auto_visit_ = visit_;

    QPointer<StrategyBuilderPanel> self(this);
    quint64 ticket = 0;
    const bool now = ui::AutoLoadGate::instance().enter(
        this,
        [self]() {
            if (!self)
                return;
            self->auto_queued_ = false;
            if (!self->isVisible() || self->run_in_flight_) {
                self->release_auto_slot();
                return;
            }
            self->start_backtest(true);
        },
        0, &ticket);
    auto_ticket_ = ticket;
    if (now) {
        start_backtest(true);
    } else {
        auto_queued_ = true;
        report_panel_->set_queued(tr("Another default load is running; the backtest of %1 starts next.").arg(symbol_text()));
        kit::set_state(status_, kit::RunState::Queued);
    }
}

void StrategyBuilderPanel::release_auto_slot() {
    auto_queued_ = false;
    auto_run_ = false;
    if (auto_ticket_ != 0)
        ui::AutoLoadGate::instance().leave(std::exchange(auto_ticket_, 0));
}

// ── Remembered results ────────────────────────────────────────────────────────

QString StrategyBuilderPanel::draft_memory_key() const {
    return loaded_strategy_id_.isEmpty() ? name_edit_->text().trimmed() : loaded_strategy_id_;
}

void StrategyBuilderPanel::remember_backtest(const QJsonObject& payload) {
    QVariantMap entry = std::exchange(pending_memory_entry_, QVariantMap());
    const QString key = entry.take(QStringLiteral("key")).toString();
    if (key.isEmpty() || payload.isEmpty())
        return; // not a run this panel started
    entry.insert(QStringLiteral("ran_at"), report_panel_->ran_at().toUTC().toString(Qt::ISODateWithMs));
    entry.insert(QStringLiteral("duration_ms"), report_panel_->run_duration_ms());
    // Serialised on a worker: a long intraday result is megabytes of JSON.
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, key, entry]() mutable {
        watcher->deleteLater();
        const QString json = watcher->result();
        if (json.isEmpty() || json.size() > kSbpMemoryMaxPayloadChars) {
            backtest_memory_.remove(key); // too large to keep: this draft runs again when reopened
            return;
        }
        entry.insert(QStringLiteral("payload"), json);
        backtest_memory_.insert(key, entry);
        sbp_trim_backtest_memory(backtest_memory_);
        emit backtest_remembered();
    });
    watcher->setFuture(QtConcurrent::run(
        [payload]() { return QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)); }));
}

bool StrategyBuilderPanel::offer_remembered_backtest(bool same_inputs) {
    if (instrument_type_combo_ && instrument_type_combo_->currentData().toString() != QLatin1String("equity"))
        return false; // Option / Future: no backtest on this zone
    const QString key = draft_memory_key();
    const QVariantMap entry = key.isEmpty() ? QVariantMap() : backtest_memory_.value(key).toMap();
    if (entry.isEmpty())
        return false;
    const QString inputs = entry.value(QStringLiteral("inputs")).toString();
    if (!same_inputs) {
        // Reopened: same rules and symbol → the strip takes the run's dates and capital.
        if (entry.value(QStringLiteral("def")).toString() != sbp_definition_key(build_strategy()) ||
            entry.value(QStringLiteral("symbol")).toString() != symbol_text())
            return false;
        const QDate start = QDate::fromString(entry.value(QStringLiteral("start")).toString(), Qt::ISODate);
        const QDate end = QDate::fromString(entry.value(QStringLiteral("end")).toString(), Qt::ISODate);
        if (!start.isValid() || !end.isValid() || start >= end)
            return false;
        const QDate old_start = bt_start_date_->date();
        const QDate old_end = bt_end_date_->date();
        const double old_capital = bt_capital_->value();
        bt_start_date_->setDate(start);
        bt_end_date_->setDate(end);
        bt_capital_->setValue(entry.value(QStringLiteral("capital")).toDouble());
        if (backtest_inputs_key() != inputs) { // a widget clamped a value: not the same run
            bt_start_date_->setDate(old_start);
            bt_end_date_->setDate(old_end);
            bt_capital_->setValue(old_capital);
            return false;
        }
    } else if (backtest_inputs_key() != inputs) {
        return false; // the inputs changed: run again
    }

    // Parsed on a worker; the default run waits for it (remembered_loading_).
    const quint64 token = ++memory_token_;
    remembered_loading_ = true;
    const QString json = entry.value(QStringLiteral("payload")).toString();
    const QDateTime ran_at =
        QDateTime::fromString(entry.value(QStringLiteral("ran_at")).toString(), Qt::ISODateWithMs).toLocalTime();
    const qint64 duration_ms = entry.value(QStringLiteral("duration_ms"), -1).toLongLong();
    const QString def = entry.value(QStringLiteral("def")).toString();
    auto* watcher = new QFutureWatcher<QJsonObject>(this);
    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, token, ran_at, duration_ms, def, inputs, key]() {
                watcher->deleteLater();
                if (token != memory_token_)
                    return; // superseded: a run, a template or another draft took the zone
                remembered_loading_ = false;
                const QJsonObject payload = watcher->result();
                if (!payload.isEmpty() && !run_in_flight_ && !report_panel_->has_result() &&
                    backtest_inputs_key() == inputs) {
                    shown_backtest_def_ = def;
                    report_panel_->set_remembered_result(payload, ran_at, duration_ms);
                    status_->set_text(QStringLiteral("asof"), report_panel_->run_info_text());
                    return;
                }
                if (payload.isEmpty())
                    backtest_memory_.remove(key); // unreadable: run it again instead
                if (isVisible())
                    schedule_default_backtest(); // the default run was held back for this
            });
    watcher->setFuture(QtConcurrent::run([json]() {
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        return doc.isObject() ? doc.object() : QJsonObject();
    }));
    return true;
}

// ── Deploy ───────────────────────────────────────────────────────────────────

void StrategyBuilderPanel::on_deploy() {
    // Re-entrancy: the deploy dialog is modal and the post-accept broker probe is
    // asynchronous, so without the gate a second click would deploy twice.
    if (deploying_)
        return;
    deploying_ = true;
    bar_->set_action_enabled(QStringLiteral("deploy"), false);
    // Releases the click gate on every SYNCHRONOUS exit below. Once the deploy hands off to
    // the async quote probe the guard is disarmed and the queued continuation owns it.
    struct DeployGuard {
        StrategyBuilderPanel* self;
        bool armed = true;
        ~DeployGuard() {
            if (!armed)
                return;
            self->deploying_ = false;
            self->bar_->set_action_enabled(QStringLiteral("deploy"), true);
        }
    } deploy_guard{this};

    if (name_edit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Deploy"), tr("Enter a strategy name before deploying."));
        name_edit_->setFocus();
        return;
    }
    const bool is_fno_type =
        instrument_type_combo_ && instrument_type_combo_->currentData().toString() != QLatin1String("equity");
    if (!is_fno_type) {
        if (const QString err = validate(); !err.isEmpty()) {
            show_validation(err);
            QMessageBox::warning(this, tr("Deploy"), tr("The rules cannot run yet:\n\n%1").arg(err));
            return;
        }
    }
    show_validation(QString());

    // F&O: refuse here, with the reason, anything the engine could not trade as the user
    // defined it (it would otherwise sit RUNNING and never enter).
    if (is_fno_type) {
        QString fno_err;
        const auto legs = leg_editor_ ? leg_editor_->legs() : QVector<fincept::algo::fno::AlgoFnoLeg>{};
        if (!underlying_combo_ || !underlying_combo_->isEnabled() || underlying_combo_->currentText().isEmpty())
            fno_err = tr("Select an underlying (connect a broker that lists F&O instruments).");
        else if (legs.isEmpty())
            fno_err = tr("Add at least one leg.");
        else if (expiry_mode() == QLatin1String("ABSOLUTE") && expiry_value().isEmpty())
            fno_err = tr("Pick the expiry to trade (Expiry: Absolute).");
        else {
            for (int i = 0; i < legs.size(); ++i) {
                if (legs[i].kind != QLatin1String("CE") && legs[i].kind != QLatin1String("PE")) {
                    fno_err = tr("Leg %1 is a %2 leg — only CE / PE legs can be traded by the algo engine.")
                                  .arg(i + 1)
                                  .arg(legs[i].kind);
                    break;
                }
            }
        }
        if (!fno_err.isEmpty()) {
            QMessageBox::warning(this, tr("Deploy"), fno_err);
            return;
        }
    }

    auto strat = build_strategy();
    // The deploy dialog (and the LIVE confirmation) states what the strategy did in its last
    // backtest: this session's run, else the one saved with the strategy.
    strat.last_backtest = report_panel_->has_result() ? sbp_report_summary(report_panel_) : loaded_last_backtest_;

    auto* dialog = new AlgoDeployDialog(strat, this);
    dialog->set_symbol(symbol_text()); // carry the builder's instrument in
    // Seed size/limits from the risk fields BEFORE exec(): what the user reviewed (and, for
    // live, confirmed) is what deploys.
    dialog->set_risk_defaults(risk_panel_->quantity(), risk_panel_->max_order_value());
    if (is_fno_type) {
        // The label names the rule the engine will actually apply (+ today's expiry), so the
        // LIVE confirmation states the contract month that will be ordered.
        dialog->set_fno_context(instrument_type_combo_->currentData().toString(),
                                underlying_combo_ ? underlying_combo_->currentText() : QString(), expiry_rule_label(),
                                fincept::algo::fno::fno_legs_from_json(strat.legs));
    }
    const bool accepted = (dialog->exec() == QDialog::Accepted);
    if (!accepted) {
        dialog->deleteLater();
        return;
    }
    auto deployment = dialog->deployment();
    dialog->deleteLater(); // nothing below reads the dialog again

    // Sanity check (non-blocking, bounded): warn if a rule can't fire at the current price.
    deploy_guard.armed = false; // the continuation owns the release from here

    struct ReleaseGate {
        QPointer<StrategyBuilderPanel> panel;
        ~ReleaseGate() {
            if (!panel)
                return;
            panel->deploying_ = false;
            panel->bar_->set_action_enabled(QStringLiteral("deploy"), true);
        }
    };

    QPointer<StrategyBuilderPanel> self = this;
    fetch_quote_ltp_async(
        deployment.broker_id, deployment.broker_account_id, deployment.symbol, 2500, this,
        [self, strat, deployment](double cur_price) {
            if (!self)
                return;
            ReleaseGate release_gate{self};

            if (cur_price > 0) {
                namespace fmt = fincept::ui::formatting;
                const fmt::InstrumentFmt inst = fmt::instrument_for_symbol(
                    deployment.symbol,
                    ui::algo::algo_currency_for(deployment.symbol, deployment.exchange, deployment.broker_id));
                QStringList warns;
                scan_unreachable(strat.entry_conditions, tr("Entry"), cur_price, inst, warns);
                scan_unreachable(strat.exit_conditions, tr("Exit"), cur_price, inst, warns);
                if (!warns.isEmpty()) {
                    const auto btn = QMessageBox::warning(
                        self.data(), tr("Deploy — check conditions"),
                        tr("%1 is at %2, but some rules may never trigger:\n\n%3\n\nDeploy anyway?")
                            .arg(deployment.symbol, fmt::price_with_symbol(cur_price, inst), warns.join("\n\n")),
                        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                    if (!self || btn != QMessageBox::Yes)
                        return;
                }
            }

            // Confirm before deploying an exact duplicate of an already-running setup.
            if (algo_ns::AlgoEngine::instance().has_active_duplicate(deployment.strategy_id, deployment.symbol,
                                                                     deployment.mode, deployment.entry_side)) {
                const auto btn =
                    QMessageBox::question(self.data(), tr("Already deployed"),
                                          tr("An identical deployment is already running:\n\n%1 · %2 · %3 · %4\n\n"
                                             "Deploy another copy anyway?")
                                              .arg(deployment.strategy_name, deployment.symbol,
                                                   deployment.mode.toUpper(), deployment.entry_side),
                                          QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (!self || btn != QMessageBox::Yes)
                    return;
            }

            services::algo::AlgoTradingService::instance().save_strategy(strat);

            LOG_INFO("AlgoTrading", QString("Deploy requested: id=%1 strategy='%2' symbol=%3 mode=%4 backend=%5 "
                                            "broker=%6 acct=%7 qty=%8")
                                        .arg(deployment.id, strat.name, deployment.symbol, deployment.mode,
                                             deployment.backend, deployment.broker_id, deployment.broker_account_id)
                                        .arg(deployment.quantity));

            // Persists the row, then starts the runner. Feedback is the jump to the Dashboard.
            algo_ns::AlgoEngine::instance().start_deployment(deployment, strat);
            emit self->deployed();
        });
}

// ── Templates / loading ──────────────────────────────────────────────────────

void StrategyBuilderPanel::load_template(int index) {
    const auto tpls = services::algo::algo_strategy_templates();
    if (index < 0 || index >= tpls.size())
        return;
    const auto& t = tpls[index];

    loaded_strategy_id_.clear(); // a template seeds a brand-new strategy
    loaded_last_backtest_ = QJsonObject();
    ++memory_token_; // a remembered result of the previous draft being parsed no longer applies
    remembered_loading_ = false;
    name_edit_->setText(t.name);
    desc_edit_->setText(t.description);
    {
        QSignalBlocker b(timeframe_combo_);
        timeframe_combo_->setCurrentText(t.timeframe);
    }
    entry_rules_->set_conditions(t.entry, t.entry_logic);
    exit_rules_->set_conditions(t.exit, t.exit_logic);
    risk_panel_->set_values(t.stop_loss, t.take_profit, 0.0, 1.0, 0.0);
    if (!run_in_flight_)
        report_panel_->clear();
    state_chip_->setText(tr("NEW DRAFT"));
    kit::set_tone(state_chip_, kit::Tone::Accent);
    on_symbol_changed();
    notify(tr("Template %1 loaded — RUN BACKTEST (Ctrl+Enter) to test it.").arg(t.name),
           static_cast<int>(SbpSB::Tone::Normal));
}

void StrategyBuilderPanel::load_strategy(const services::algo::AlgoStrategy& s) {
    loaded_strategy_id_ = s.id; // keep id → Save updates this strategy in place
    loaded_last_backtest_ = s.last_backtest;
    name_edit_->setText(s.name);
    desc_edit_->setText(s.description);
    {
        QSignalBlocker b(timeframe_combo_);
        timeframe_combo_->setCurrentText(s.timeframe.isEmpty() ? QStringLiteral("5m") : s.timeframe);
    }
    entry_rules_->set_conditions(s.entry_conditions, s.entry_logic.isEmpty() ? QStringLiteral("AND") : s.entry_logic);
    exit_rules_->set_conditions(s.exit_conditions, s.exit_logic.isEmpty() ? QStringLiteral("AND") : s.exit_logic);
    risk_panel_->set_values(s.stop_loss, s.take_profit, s.trailing_stop, 1.0, 0.0,
                            s.position_size_pct > 0 ? s.position_size_pct : 100.0);
    cost_panel_->set_entry_side(s.entry_side);
    cost_panel_->set_risk_free_pct(s.risk_free_pct);
    if (s.cost_model.isEmpty()) {
        // Saved before cost models existed: suggest the symbol's market preset.
        cost_panel_->set_user_chose(false);
        cost_panel_->apply_preset(QStringLiteral("none"));
        on_symbol_changed();
    } else {
        cost_panel_->set_cost_model(algo_ns::CostModel::from_json(s.cost_model));
        cost_panel_->set_user_chose(true);
    }
    if (instrument_type_combo_) {
        const int idx = instrument_type_combo_->findData(s.instrument_type.isEmpty() ? QStringLiteral("equity")
                                                                                     : s.instrument_type);
        QSignalBlocker blocker(instrument_type_combo_);
        instrument_type_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (leg_editor_) {
        const auto legs = fincept::algo::fno::fno_legs_from_json(s.legs);
        leg_editor_->set_legs(legs);
        if (!legs.isEmpty())
            apply_expiry_rule(legs.first().expiry_mode, legs.first().expiry_value);
    }
    on_instrument_type_changed();
    // Size the backtest range to the strategy's timeframe (daily needs years for long
    // indicators; intraday must stay under Yahoo's history cap).
    const QDate today = QDate::currentDate();
    bt_start_date_->setDate(today.addDays(-services::algo::algo_default_lookback_days(s.timeframe)));
    bt_end_date_->setDate(today);
    if (!run_in_flight_)
        report_panel_->clear();
    show_validation(QString());
    state_chip_->setText(tr("EDITING"));
    kit::set_tone(state_chip_, kit::Tone::Info);
    notify(tr("Editing %1").arg(s.name), static_cast<int>(SbpSB::Tone::Normal));
    refresh_definition_feedback();
    // Its last result from this or an earlier session, while rules and symbol still match.
    offer_remembered_backtest(false);
}

void StrategyBuilderPanel::load_and_backtest(const services::algo::AlgoStrategy& s, const QString& symbol,
                                             const QString& start_date, const QString& end_date) {
    load_strategy(s);
    if (!symbol.isEmpty()) {
        set_symbol_text(symbol);
        on_symbol_changed();
    }
    const QDate sd = QDate::fromString(start_date, QStringLiteral("yyyy-MM-dd"));
    const QDate ed = QDate::fromString(end_date, QStringLiteral("yyyy-MM-dd"));
    if (sd.isValid())
        bt_start_date_->setDate(sd);
    if (ed.isValid())
        bt_end_date_->setDate(ed);
    on_backtest(); // reuses validation + service call; the report renders in the result zone
}

void StrategyBuilderPanel::on_optimize() {
    if (bt_start_date_->date() >= bt_end_date_->date()) {
        notify(tr("Cannot optimise: the start date must be before the end date."), static_cast<int>(SbpSB::Tone::Danger));
        return;
    }
    if (const QString err = validate(); !err.isEmpty()) {
        show_validation(err);
        notify(tr("Cannot optimise: %1").arg(err), static_cast<int>(SbpSB::Tone::Danger));
        return;
    }
    const QString symbol = symbol_text();
    if (symbol.isEmpty()) {
        show_validation(tr("INSTRUMENT: pick a symbol to optimise on."));
        return;
    }
    show_validation(QString());
    auto* dlg = new StrategyOptimizeDialog(build_strategy(), symbol,
                                           bt_start_date_->date().toString(QStringLiteral("yyyy-MM-dd")),
                                           bt_end_date_->date().toString(QStringLiteral("yyyy-MM-dd")),
                                           bt_capital_->value(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &StrategyOptimizeDialog::apply_requested, this, [this](double sl, double tp) {
        risk_panel_->set_values(sl, tp, risk_panel_->trailing_stop(), risk_panel_->quantity(),
                                risk_panel_->max_order_value(), risk_panel_->capital_pct());
        notify(tr("Stop-loss %1 % / take-profit %2 % applied from the sweep — RUN BACKTEST for the full report.")
                   .arg(sl)
                   .arg(tp),
               static_cast<int>(SbpSB::Tone::Ok));
    });
    dlg->show();
}

QString StrategyBuilderPanel::default_backtest_symbol() {
    return sbp_default_backtest_symbol();
}

void StrategyBuilderPanel::set_backtest_symbol(const QString& symbol) {
    if (!symbol_box_ || symbol.trimmed().isEmpty())
        return;
    set_symbol_text(symbol.trimmed().toUpper());
    on_symbol_changed();
    notify(tr("Instrument set to %1.").arg(symbol.trimmed().toUpper()), static_cast<int>(SbpSB::Tone::Normal));
}

QString StrategyBuilderPanel::validate() const {
    // A problem a rule table found itself (a parameter that is not a number, …) first, located.
    for (const auto* t : {entry_rules_, exit_rules_}) {
        for (const auto& i : t->issues()) {
            if (!i.blocking)
                continue;
            const QString where = i.child >= 0 ? QStringLiteral("%1.%2").arg(i.index + 1).arg(i.child + 1)
                                               : QString::number(i.index + 1);
            return t == entry_rules_ ? tr("ENTRY rule %1: %2").arg(where, i.reason)
                                     : tr("EXIT rule %1: %2").arg(where, i.reason);
        }
    }
    return rules::validate_strategy(entry_rules_->conditions(), exit_rules_->conditions(), risk_panel_->stop_loss(),
                                    risk_panel_->take_profit(), risk_panel_->trailing_stop());
}

void StrategyBuilderPanel::on_backtest_result(const QJsonObject& payload) {
    if (!run_in_flight_)
        return; // another screen's run (the service is shared)
    run_in_flight_ = false;
    set_run_busy(false);
    release_auto_slot();
    show_validation(QString());
    shown_backtest_def_ = pending_backtest_def_;
    display_backtest_result(payload);
    status_->set_text(QStringLiteral("asof"), report_panel_->run_info_text());
    if (!payload.value(QStringLiteral("costs_applied")).toBool())
        notify(tr("Backtest complete — GROSS: no costs were applied."), static_cast<int>(SbpSB::Tone::Warn));
    remember_backtest(payload);
    LOG_DEBUG("AlgoTrading", "Backtest result displayed");
}

void StrategyBuilderPanel::on_error(const QString& context, const QString& msg) {
    if (context == QLatin1String("backtest")) {
        if (!run_in_flight_)
            return;
        run_in_flight_ = false;
        set_run_busy(false);
        pending_memory_entry_.clear(); // nothing to remember
        release_auto_slot();
        report_panel_->set_error(msg); // never leave the previous report on screen
        kit::set_state(status_, kit::RunState::Failed);
        return;
    }
    notify(QStringLiteral("[%1] %2").arg(context, msg), static_cast<int>(SbpSB::Tone::Danger));
}

void StrategyBuilderPanel::display_backtest_result(const QJsonObject& payload) {
    report_panel_->set_result(payload);
}

void StrategyBuilderPanel::clear_results() {
    report_panel_->clear();
}

// ── Draft persistence ────────────────────────────────────────────────────────

QVariantMap StrategyBuilderPanel::save_draft() const {
    QVariantMap draft;
    draft["id"] = loaded_strategy_id_;
    draft["name"] = name_edit_->text();
    draft["description"] = desc_edit_->text();
    draft["symbol"] = symbol_text();
    draft["timeframe"] = timeframe_combo_->currentText();
    draft["entry_conditions"] = QJsonDocument(entry_rules_->conditions()).toJson(QJsonDocument::Compact);
    draft["exit_conditions"] = QJsonDocument(exit_rules_->conditions()).toJson(QJsonDocument::Compact);
    draft["entry_logic"] = entry_rules_->combined_logic();
    draft["exit_logic"] = exit_rules_->combined_logic();
    draft["stop_loss"] = risk_panel_->stop_loss();
    draft["take_profit"] = risk_panel_->take_profit();
    draft["trailing_stop"] = risk_panel_->trailing_stop();
    draft["quantity"] = risk_panel_->quantity();
    draft["max_order_value"] = risk_panel_->max_order_value();
    draft["capital_pct"] = risk_panel_->capital_pct();
    draft["cost_model"] = QJsonDocument(cost_panel_->cost_model().to_json()).toJson(QJsonDocument::Compact);
    draft["cost_model_chosen"] = cost_panel_->user_chose();
    draft["entry_side"] = cost_panel_->entry_side();
    draft["risk_free_pct"] = cost_panel_->risk_free_pct();
    draft["capital"] = bt_capital_->value();
    draft["start"] = bt_start_date_->date().toString(Qt::ISODate);
    draft["end"] = bt_end_date_->date().toString(Qt::ISODate);
    if (instrument_type_combo_)
        draft["instrument_type"] = instrument_type_combo_->currentData().toString();
    if (leg_editor_) {
        auto legs = leg_editor_->legs();
        for (auto& l : legs) { // carry the header's expiry rule (restored by restore_draft)
            l.expiry_mode = expiry_mode();
            l.expiry_value = expiry_value();
        }
        draft["legs"] = QJsonDocument(fincept::algo::fno::fno_legs_to_json(legs)).toJson(QJsonDocument::Compact);
    }
    // The last result per draft (payloads kept as compact JSON text: cheap to copy here,
    // parsed off the GUI thread when shown again).
    if (!backtest_memory_.isEmpty())
        draft["backtest_memory"] = backtest_memory_;
    return draft;
}

void StrategyBuilderPanel::restore_draft(const QVariantMap& draft) {
    loaded_strategy_id_ = draft.value("id").toString();
    name_edit_->setText(draft.value("name").toString());
    desc_edit_->setText(draft.value("description").toString());
    set_symbol_text(draft.value("symbol").toString());
    {
        QSignalBlocker b(timeframe_combo_);
        timeframe_combo_->setCurrentText(draft.value("timeframe", "1d").toString());
    }

    auto entry_json = QJsonDocument::fromJson(draft.value("entry_conditions").toByteArray()).array();
    auto exit_json = QJsonDocument::fromJson(draft.value("exit_conditions").toByteArray()).array();
    // Migration: drafts saved by the old form carry its blank placeholder row ("Close > 0" on
    // both sides) — and usually an id, minted by the default run that backtested it. Nobody
    // wrote those rules (they traded every bar and lost to costs), so such a draft loads as a
    // new, empty one, which the default run opens on a template.
    if (rules::is_blank_rules(entry_json, exit_json)) {
        entry_json = QJsonArray();
        exit_json = QJsonArray();
        loaded_strategy_id_.clear();
    }
    entry_rules_->set_conditions(entry_json, draft.value("entry_logic", "AND").toString());
    exit_rules_->set_conditions(exit_json, draft.value("exit_logic", "AND").toString());

    risk_panel_->set_values(draft.value("stop_loss", 2.0).toDouble(), draft.value("take_profit", 5.0).toDouble(),
                            draft.value("trailing_stop", 0.0).toDouble(), draft.value("quantity", 1.0).toDouble(),
                            draft.value("max_order_value", 0.0).toDouble(),
                            draft.value("capital_pct", 100.0).toDouble());
    cost_panel_->set_entry_side(draft.value("entry_side", "BUY").toString());
    cost_panel_->set_risk_free_pct(draft.value("risk_free_pct", 0.0).toDouble());
    const QJsonObject cm = QJsonDocument::fromJson(draft.value("cost_model").toByteArray()).object();
    if (!cm.isEmpty())
        cost_panel_->set_cost_model(algo_ns::CostModel::from_json(cm));
    cost_panel_->set_user_chose(draft.value("cost_model_chosen", false).toBool());
    // Capital / period (absent in drafts saved before this layout: the defaults stay).
    if (draft.contains("capital"))
        bt_capital_->setValue(draft.value("capital").toDouble());
    const QDate ds = QDate::fromString(draft.value("start").toString(), Qt::ISODate);
    const QDate de = QDate::fromString(draft.value("end").toString(), Qt::ISODate);
    if (ds.isValid() && de.isValid() && ds < de) {
        bt_start_date_->setDate(ds);
        bt_end_date_->setDate(de);
    }
    on_symbol_changed();

    // Instrument type + F&O leg rules (absent in drafts saved earlier).
    if (instrument_type_combo_) {
        const int idx = instrument_type_combo_->findData(draft.value("instrument_type", "equity").toString());
        QSignalBlocker blocker(instrument_type_combo_);
        instrument_type_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (leg_editor_) {
        const auto legs =
            fincept::algo::fno::fno_legs_from_json(QJsonDocument::fromJson(draft.value("legs").toByteArray()).array());
        leg_editor_->set_legs(legs);
        if (!legs.isEmpty())
            apply_expiry_rule(legs.first().expiry_mode, legs.first().expiry_value);
    }
    on_instrument_type_changed();
    state_chip_->setText(loaded_strategy_id_.isEmpty() ? tr("NEW DRAFT") : tr("EDITING"));
    kit::set_tone(state_chip_, loaded_strategy_id_.isEmpty() ? kit::Tone::Accent : kit::Tone::Info);

    // Remembered results: the saved ones, unless this session already has a newer one per draft.
    const QVariantMap saved_memory = draft.value("backtest_memory").toMap();
    for (auto it = saved_memory.constBegin(); it != saved_memory.constEnd(); ++it) {
        const QVariantMap mine = backtest_memory_.value(it.key()).toMap();
        if (mine.isEmpty() || mine.value("ran_at").toString() < it.value().toMap().value("ran_at").toString())
            backtest_memory_.insert(it.key(), it.value());
    }
    sbp_trim_backtest_memory(backtest_memory_);
    refresh_definition_feedback();
    // Reopened: the draft's last result "as of" its run, instead of running it again.
    if (!run_in_flight_ && !report_panel_->has_result())
        offer_remembered_backtest(false);
}

// ── Language ─────────────────────────────────────────────────────────────────

void StrategyBuilderPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void StrategyBuilderPanel::retranslateUi() {
    if (bar_) {
        kit::retranslate_tab_bar(bar_);
        bar_->set_action_text(QStringLiteral("optimize"), tr("OPTIMISE SL / TP…"));
        bar_->set_action_tooltip(QStringLiteral("optimize"),
                                 tr("Sweep stop-loss × take-profit in-sample and test the best pair out-of-sample"));
        bar_->set_action_text(QStringLiteral("saveas"), tr("SAVE AS…"));
        bar_->set_action_tooltip(QStringLiteral("saveas"), tr("Save a copy under a new name (Ctrl+Shift+S)"));
        bar_->set_action_text(QStringLiteral("save"), tr("SAVE"));
        bar_->set_action_tooltip(QStringLiteral("save"), tr("Save this strategy to STRATEGIES (Ctrl+S)"));
        bar_->set_action_text(QStringLiteral("deploy"), tr("DEPLOY (PAPER)…"));
        bar_->set_action_tooltip(QStringLiteral("deploy"),
                                 tr("Open the deploy dialog — paper trading by default; live trading only after its own "
                                    "confirmation. Nothing starts until you confirm there."));
        set_run_busy(run_in_flight_);
    }
    if (name_edit_)
        name_edit_->setPlaceholderText(tr("Strategy name"));
    if (desc_edit_)
        desc_edit_->setPlaceholderText(tr("Description (optional)"));
    if (symbol_box_)
        symbol_box_->set_placeholder(tr("Symbol or name"));
    if (instrument_type_combo_) {
        instrument_type_combo_->setItemText(0, tr("Equity"));
        instrument_type_combo_->setItemText(1, tr("Option"));
        instrument_type_combo_->setItemText(2, tr("Future"));
        instrument_type_combo_->setItemData(2, tr("Futures strategies are not supported by the algo engine yet"),
                                            Qt::ToolTipRole);
    }
    if (templates_caption_) {
        templates_caption_->setText(tr("TEMPLATES"));
        templates_caption_->setToolTip(tr("One click fills the rules, timeframe and risk of a ready-made strategy"));
    }
    if (state_chip_ && state_chip_->text().isEmpty())
        state_chip_->setText(tr("NEW DRAFT"));
    if (entry_header_)
        entry_header_->set_title(tr("ENTRY RULES"));
    if (exit_header_)
        exit_header_->set_title(tr("EXIT RULES"));
    if (risk_header_)
        risk_header_->set_title(tr("RISK & SIZING"));
    if (exec_header_)
        exec_header_->set_title(tr("EXECUTION & COSTS"));
    if (exec_toggle_) {
        exec_toggle_->setText(exec_toggle_->isChecked() ? tr("HIDE ▴") : tr("EDIT ▾"));
        exec_toggle_->setAccessibleName(exec_toggle_->isChecked() ? tr("Hide the cost editor") : tr("Edit costs"));
        exec_toggle_->setToolTip(tr("Cost preset, commission, taxes, slippage, lot size, borrow fee, risk-free rate"));
    }
}

} // namespace fincept::screens
