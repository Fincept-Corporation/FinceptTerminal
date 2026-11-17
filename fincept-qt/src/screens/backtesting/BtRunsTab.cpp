// src/screens/backtesting/BtRunsTab.cpp
//
// RUNS (history) tab (BtWorkbenchTabs.h): the saved runs of BacktestRunStore, read on a worker, in a
// model/view table — tick (max 6, for COMPARE), ★ pinned, label, when, engine, type, strategy,
// symbols, period, return, Sharpe, max DD, trades, equity sparkline — with a text filter, engine /
// type facets and "pinned only". Row actions: buttons for the selected row, a context menu and keys
// (Enter = OPEN, Delete = DELETE after confirm, Space = tick, Ctrl+F = filter).
#include "screens/backtesting/BtWorkbenchTabs.h"

#include "core/agent/AgentRiskMarkers.h"
#include "screens/backtesting/BtAnalysisViews.h"
#include "services/backtesting/BacktestRunStore.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using services::backtest::BacktestRunStore;
using services::backtest::SavedRun;

/// One row of the RUNS table (raw values; formatted in data() so a format change repaints).
struct BtRunsRow {
    SavedRun run;
    QString engine_name;
    double when_ms = kBtNaN;
    double ret = kBtNaN;
    double sharpe = kBtNaN;
    double dd = kBtNaN;
    double trades = kBtNaN;
    QVector<double> spark;
};

namespace {

using RunsModel = ui::LiveTableModel<BtRunsRow>;

constexpr int kRunsMaxTicked = 6;
constexpr int kRunsListLimit = 500;

enum RunsCol {
    kRcTick = 0,
    kRcPin,
    kRcLabel,
    kRcWhen,
    kRcEngine,
    kRcType,
    kRcStrategy,
    kRcSymbols,
    kRcPeriod,
    kRcReturn,
    kRcSharpe,
    kRcMaxDd,
    kRcTrades,
    kRcEquity,
    kRcCount
};

QString runs_type_text(const QString& command) {
    if (command == QLatin1String("backtest"))
        return BtRunsTab::tr("Backtest");
    if (command == QLatin1String("optimize"))
        return BtRunsTab::tr("Optimise");
    if (command == QLatin1String("walk_forward"))
        return BtRunsTab::tr("Walk-forward");
    if (command == QLatin1String("monte_carlo"))
        return BtRunsTab::tr("Monte Carlo");
    QString s = command;
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

double runs_summary_number(const QJsonObject& s, const char* key) {
    const QJsonValue v = s.value(QLatin1String(key));
    return v.isDouble() ? v.toDouble() : kBtNaN;
}

/// Built on the worker: raw values only (no locale formatting off the GUI thread).
BtRunsRow runs_make_row(const SavedRun& s, const QHash<QString, QString>& names) {
    BtRunsRow r;
    r.run = s;
    r.engine_name = names.value(s.provider, s.provider);
    QDateTime t = QDateTime::fromString(s.created_at, Qt::ISODate);
    if (t.isValid()) {
        // created_at is written as UTC ("…Z"); a stamp without a zone is read as UTC too.
        if (!s.created_at.contains(QLatin1Char('Z')) && !s.created_at.contains(QLatin1Char('+')))
            t.setTimeZone(QTimeZone::utc());
        r.when_ms = static_cast<double>(t.toMSecsSinceEpoch());
    }
    r.ret = runs_summary_number(s.summary, "totalReturn");
    if (!std::isfinite(r.ret))
        r.ret = runs_summary_number(s.summary, "compoundedOosReturn");
    r.sharpe = runs_summary_number(s.summary, "sharpeRatio");
    if (!std::isfinite(r.sharpe))
        r.sharpe = runs_summary_number(s.summary, "avgOosSharpe");
    r.dd = runs_summary_number(s.summary, "maxDrawdown");
    r.trades = runs_summary_number(s.summary, "trades");
    for (const QJsonValue& v : s.summary.value(QStringLiteral("spark")).toArray())
        if (v.isDouble())
            r.spark.append(v.toDouble());
    return r;
}

QString runs_period_text(const SavedRun& s) {
    if (s.start_date.isEmpty() && s.end_date.isEmpty())
        return dash();
    return QStringLiteral("%1 → %2").arg(date_text(s.start_date), date_text(s.end_date));
}

QString runs_name(const BtRunsRow& r) {
    if (!r.run.label.isEmpty())
        return r.run.label;
    QStringList parts;
    if (!r.run.strategy.isEmpty())
        parts << r.run.strategy;
    if (!r.run.symbols.isEmpty())
        parts << r.run.symbols;
    return parts.isEmpty() ? BtRunsTab::tr("Run %1").arg(r.run.id) : parts.join(QStringLiteral(" · "));
}

void runs_set_active(QPushButton* b, bool on) {
    if (b->property("active").toBool() == on)
        return;
    b->setProperty("active", on);
    b->style()->unpolish(b);
    b->style()->polish(b);
}

QComboBox* runs_combo(QWidget* parent, const QString& accessible) {
    auto* c = new QComboBox(parent);
    c->setObjectName(QStringLiteral("btCombo"));
    c->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    c->setAccessibleName(accessible);
    c->setFocusPolicy(Qt::StrongFocus);
    return c;
}

} // namespace

struct BtRunsTab::Impl {
    BtRunsTab* q = nullptr;
    BtHost* host = nullptr;

    QStackedWidget* stack = nullptr;
    ui::LoadingSkeleton* loading = nullptr;
    ui::EmptyState* empty = nullptr;
    bool empty_is_error = false;
    QWidget* main = nullptr;

    QLineEdit* filter = nullptr;
    QComboBox* engine = nullptr;
    QComboBox* type = nullptr;
    QPushButton* pinned_only = nullptr;
    QLabel* count_label = nullptr;
    QPushButton* compare_ticked = nullptr;

    BtPanel panel;
    QPushButton* b_open = nullptr;
    QPushButton* b_rerun = nullptr;
    QPushButton* b_inputs = nullptr;
    QPushButton* b_rename = nullptr;
    QPushButton* b_pin = nullptr;
    QPushButton* b_compare = nullptr;
    QPushButton* b_delete = nullptr;
    QTableView* view = nullptr;
    RunsModel* model = nullptr;
    QSortFilterProxyModel* proxy = nullptr;
    QWidget* note_row = nullptr;
    QLabel* filter_note = nullptr;
    QPushButton* clear_filters = nullptr;

    QVector<BtRunsRow> all;
    QVector<qint64> ticked; ///< in tick order
    bool loaded = false;
    quint64 generation = 0;
    qint64 pending_select = 0;

    void build();
    void apply(qint64 keep_selected);
    void loaded_rows(const QVector<BtRunsRow>& rows, const QString& error);
    qint64 selected_id() const;
    const BtRunsRow* selected_row() const;
    void select_id(qint64 id);
    void update_actions();
    void toggle_tick(qint64 id);
    void act_open();
    void act_rerun(bool run);
    void act_rename(bool pin_toggle);
    void act_compare();
    void act_delete();
    void context_menu(const QPoint& pos);
};

BtRunsTab::BtRunsTab(BtHost* host, QWidget* parent) : QWidget(parent), d(std::make_unique<Impl>()) {
    d->q = this;
    d->host = host;
    d->build();
    refresh();
}

BtRunsTab::~BtRunsTab() {
    // The model's cell function reads d (ticked ids): detach the view before d goes.
    if (d && d->view)
        d->view->setModel(nullptr);
}

void BtRunsTab::Impl::build() {
    auto* root = new QVBoxLayout(q);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Filter strip ──
    auto* strip = new QWidget(q);
    strip->setObjectName(QStringLiteral("btSettingsStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    auto* sh = new QHBoxLayout(strip);
    sh->setContentsMargins(8, 5, 8, 5);
    sh->setSpacing(8);
    filter = new QLineEdit(strip);
    filter->setObjectName(QStringLiteral("btFilter"));
    filter->setPlaceholderText(BtRunsTab::tr("Filter label, strategy, symbol, engine… (Ctrl+F)"));
    filter->setClearButtonEnabled(true);
    filter->setAccessibleName(BtRunsTab::tr("Filter runs"));
    sh->addWidget(filter, 2);
    engine = runs_combo(strip, BtRunsTab::tr("Engine"));
    engine->addItem(BtRunsTab::tr("All engines"), QString());
    for (const services::backtest::Provider& p : services::backtest::all_providers())
        engine->addItem(p.display_name, p.slug);
    sh->addWidget(engine);
    type = runs_combo(strip, BtRunsTab::tr("Type"));
    type->addItem(BtRunsTab::tr("All types"), QString());
    for (const char* c : {"backtest", "optimize", "walk_forward", "monte_carlo"})
        type->addItem(runs_type_text(QLatin1String(c)), QString::fromLatin1(c));
    sh->addWidget(type);
    pinned_only = make_tool_button(BtRunsTab::tr("★ PINNED ONLY"),
                                   BtRunsTab::tr("Show only pinned (labelled) runs"), strip);
    pinned_only->setCheckable(true);
    sh->addWidget(pinned_only);
    sh->addStretch(1);
    count_label = new QLabel(strip);
    count_label->setObjectName(QStringLiteral("btNote"));
    sh->addWidget(count_label);
    compare_ticked = new QPushButton(strip);
    compare_ticked->setObjectName(QStringLiteral("btPrimary"));
    compare_ticked->setCursor(Qt::PointingHandCursor);
    compare_ticked->setToolTip(BtRunsTab::tr("Compare the ticked runs (2 to %1) in COMPARE").arg(kRunsMaxTicked));
    sh->addWidget(compare_ticked);
    root->addWidget(strip);

    // ── Pages: loading / empty / list ──
    stack = new QStackedWidget(q);
    root->addWidget(stack, 1);
    loading = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, stack);
    stack->addWidget(loading);
    empty = new ui::EmptyState(stack);
    stack->addWidget(empty);
    QObject::connect(empty, &ui::EmptyState::primary_clicked, q, [this]() {
        if (empty_is_error)
            q->refresh();
        else if (host)
            host->run_backtest_with(QJsonObject());
    });

    main = new QWidget(stack);
    auto* mv = new QVBoxLayout(main);
    mv->setContentsMargins(6, 6, 6, 6);
    mv->setSpacing(6);
    panel = make_panel(BtRunsTab::tr("SAVED RUNS"), main);
    auto add_action = [this](const QString& text, const QString& tip) {
        QPushButton* b = make_tool_button(text, tip, panel.header);
        panel.header->add_control(b);
        return b;
    };
    b_open = add_action(BtRunsTab::tr("OPEN"), BtRunsTab::tr("Make this run the active run (Enter); the setup is not "
                                                             "changed"));
    b_open->setProperty("active", true);
    b_rerun = add_action(BtRunsTab::tr("RE-RUN"), BtRunsTab::tr("Load this run's inputs into the setup and run it"));
    b_inputs = add_action(BtRunsTab::tr("LOAD INPUTS"), BtRunsTab::tr("Load this run's inputs into the setup"));
    b_rename = add_action(BtRunsTab::tr("RENAME"), BtRunsTab::tr("Give the run a label (a labelled run is pinned)"));
    b_pin = add_action(BtRunsTab::tr("PIN"), BtRunsTab::tr("Pinned runs are never dropped by the %1-run cap")
                                                 .arg(BacktestRunStore::kMaxRuns));
    b_compare = add_action(BtRunsTab::tr("COMPARE"), BtRunsTab::tr("Compare the ticked runs, or this run"));
    b_delete = add_action(BtRunsTab::tr("DELETE"), BtRunsTab::tr("Delete this saved run (Delete)"));
    b_delete->setProperty("fincept_agent_risk", "delete");
    set_tone(b_delete, Tone::Danger);

    view = new QTableView(panel.body_widget);
    model = new RunsModel(q);
    model->set_headers({QString(), QStringLiteral("★"), BtRunsTab::tr("LABEL"), BtRunsTab::tr("WHEN"),
                        BtRunsTab::tr("ENGINE"), BtRunsTab::tr("TYPE"), BtRunsTab::tr("STRATEGY"),
                        BtRunsTab::tr("SYMBOLS"), BtRunsTab::tr("PERIOD"), BtRunsTab::tr("RETURN"),
                        BtRunsTab::tr("SHARPE"), BtRunsTab::tr("MAX DD"), BtRunsTab::tr("TRADES"),
                        BtRunsTab::tr("EQUITY")});
    model->set_key([](const BtRunsRow& r) { return QString::number(r.run.id); });
    model->set_cell([this](const BtRunsRow& r, int col, int role) -> QVariant {
        const auto num_or_low = [](double v) { return std::isfinite(v) ? v : -1e300; };
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kRcPin:
                    return r.run.pinned() ? QStringLiteral("★") : QString();
                case kRcLabel:
                    return r.run.label.isEmpty() ? dash() : r.run.label;
                case kRcWhen:
                    return std::isfinite(r.when_ms)
                               ? ui::formatting::date_time(
                                     QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(r.when_ms), QTimeZone::utc()),
                                     ui::formatting::DateStyle::Short)
                               : r.run.created_at;
                case kRcEngine:
                    return r.engine_name;
                case kRcType:
                    return runs_type_text(r.run.command);
                case kRcStrategy:
                    return r.run.strategy.isEmpty() ? dash() : r.run.strategy;
                case kRcSymbols:
                    return r.run.symbols.isEmpty() ? dash() : r.run.symbols;
                case kRcPeriod:
                    return runs_period_text(r.run);
                case kRcReturn:
                    return pct(r.ret, 1, true);
                case kRcSharpe:
                    return ratio(r.sharpe, 2);
                case kRcMaxDd:
                    return pct(r.dd, 1);
                case kRcTrades:
                    return count(r.trades);
                default:
                    return {};
            }
        }
        if (role == Qt::CheckStateRole && col == kRcTick)
            return static_cast<int>(ticked.contains(r.run.id) ? Qt::Checked : Qt::Unchecked);
        if (role == ui::SparklineDelegate::kDefaultRole && col == kRcEquity && r.spark.size() >= 2)
            return QVariant::fromValue(r.spark);
        if (role == RunsModel::SortRole) {
            switch (col) {
                case kRcTick:
                    return ticked.contains(r.run.id) ? 1 : 0;
                case kRcPin:
                    return r.run.pinned() ? 1 : 0;
                case kRcLabel:
                    return r.run.label;
                case kRcWhen:
                    return num_or_low(r.when_ms);
                case kRcEngine:
                    return r.engine_name;
                case kRcType:
                    return r.run.command;
                case kRcStrategy:
                    return r.run.strategy;
                case kRcSymbols:
                    return r.run.symbols;
                case kRcPeriod:
                    return r.run.start_date;
                case kRcReturn:
                    return num_or_low(r.ret);
                case kRcSharpe:
                    return num_or_low(r.sharpe);
                case kRcMaxDd:
                    return num_or_low(r.dd);
                case kRcTrades:
                    return num_or_low(r.trades);
                case kRcEquity:
                    return r.spark.size() >= 2 && r.spark.first() != 0 ? r.spark.last() / r.spark.first() : -1e300;
                default:
                    return {};
            }
        }
        if (role == Qt::TextAlignmentRole) {
            if (col >= kRcReturn && col <= kRcTrades)
                return int(Qt::AlignRight | Qt::AlignVCenter);
            if (col == kRcPin || col == kRcTick)
                return int(Qt::AlignCenter);
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (role == Qt::ForegroundRole) {
            if (col == kRcPin)
                return QVariant::fromValue(tone_color(Tone::Accent));
            if (col == kRcReturn)
                return QVariant::fromValue(tone_color(sign_tone(r.ret)));
            if (col == kRcMaxDd && std::isfinite(r.dd) && r.dd >= 0.25)
                return QVariant::fromValue(tone_color(Tone::Warn));
            if (col == kRcWhen || col == kRcPeriod)
                return QVariant::fromValue(tone_color(Tone::Dim));
            return {};
        }
        if (role == Qt::ToolTipRole) {
            switch (col) {
                case kRcTick:
                    return BtRunsTab::tr("Tick up to %1 runs, then COMPARE TICKED (Space toggles)").arg(kRunsMaxTicked);
                case kRcPin:
                    return r.run.pinned() ? BtRunsTab::tr("Pinned: never dropped by the run cap") : QString();
                case kRcWhen:
                    return std::isfinite(r.when_ms)
                               ? QStringLiteral("%1 · %2").arg(
                                     ui::formatting::relative(QDateTime::fromMSecsSinceEpoch(
                                         static_cast<qint64>(r.when_ms), QTimeZone::utc())),
                                     r.run.created_at)
                               : QString();
                case kRcEngine: {
                    const QString ver = r.run.summary.value(QStringLiteral("engineVersion")).toString();
                    const QString impl = r.run.summary.value(QStringLiteral("engineImplementation")).toString();
                    QStringList t{r.engine_name};
                    if (!impl.isEmpty())
                        t << impl;
                    if (!ver.isEmpty())
                        t << BtRunsTab::tr("version %1").arg(ver);
                    return t.join(QLatin1Char('\n'));
                }
                case kRcLabel:
                case kRcStrategy:
                case kRcSymbols:
                    return runs_name(r);
                case kRcPeriod:
                    return r.run.synthetic ? BtRunsTab::tr("Synthetic data") : r.run.interval;
                default:
                    return {};
            }
        }
        return {};
    });
    proxy = make_sort_proxy(model, q);
    view->setModel(proxy);
    setup_table(view, QStringLiteral("backtest_runs"));
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->sortByColumn(kRcWhen, Qt::DescendingOrder);
    auto* spark = new ui::SparklineDelegate(view);
    spark->set_width_chars(12);
    view->setItemDelegateForColumn(kRcEquity, spark);
    set_column_chars(view, {3, 3, 18, 15, 12, 11, 18, 14, 23, 9, 8, 8, 7, 14});
    ui::formatting::refresh_model_on_format_change(model);
    panel.body->addWidget(view, 1);
    note_row = new QWidget(panel.body_widget);
    auto* nh = new QHBoxLayout(note_row);
    nh->setContentsMargins(0, 0, 0, 0);
    nh->setSpacing(6);
    filter_note = make_note(BtRunsTab::tr("No run matches the filters."), note_row, Tone::Warn);
    nh->addWidget(filter_note, 1);
    clear_filters = make_tool_button(BtRunsTab::tr("CLEAR FILTERS"), QString(), note_row);
    nh->addWidget(clear_filters);
    note_row->setVisible(false);
    panel.body->addWidget(note_row);
    panel.body->addWidget(make_note(BtRunsTab::tr("OPEN makes a run the active run without changing the setup; RE-RUN "
                                                 "loads its inputs and runs them. Tick runs for COMPARE. Pinned (★) "
                                                 "runs are kept beyond the %1-run cap.")
                                       .arg(BacktestRunStore::kMaxRuns),
                                   panel.body_widget));
    mv->addWidget(panel.frame, 1);
    stack->addWidget(main);

    // ── Wiring ──
    QObject::connect(filter, &QLineEdit::textChanged, q, [this]() { apply(selected_id()); });
    QObject::connect(engine, QOverload<int>::of(&QComboBox::currentIndexChanged), q,
                     [this]() { apply(selected_id()); });
    QObject::connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this]() { apply(selected_id()); });
    QObject::connect(pinned_only, &QPushButton::toggled, q, [this](bool on) {
        runs_set_active(pinned_only, on);
        apply(selected_id());
    });
    QObject::connect(clear_filters, &QPushButton::clicked, q, [this]() {
        const qint64 keep = selected_id();
        filter->blockSignals(true);
        engine->blockSignals(true);
        type->blockSignals(true);
        pinned_only->blockSignals(true);
        filter->clear();
        engine->setCurrentIndex(0);
        type->setCurrentIndex(0);
        pinned_only->setChecked(false);
        runs_set_active(pinned_only, false);
        filter->blockSignals(false);
        engine->blockSignals(false);
        type->blockSignals(false);
        pinned_only->blockSignals(false);
        apply(keep);
    });
    QObject::connect(compare_ticked, &QPushButton::clicked, q, [this]() {
        if (ticked.size() >= 2)
            emit q->compare_requested(ticked);
    });
    QObject::connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, q, [this]() { update_actions(); });
    QObject::connect(view, &QTableView::clicked, q, [this](const QModelIndex& idx) {
        if (idx.column() != kRcTick)
            return;
        if (const BtRunsRow* r = model->row_at(proxy->mapToSource(idx)))
            toggle_tick(r->run.id);
    });
    QObject::connect(view, &QTableView::doubleClicked, q, [this](const QModelIndex& idx) {
        if (idx.column() != kRcTick)
            act_open();
    });
    QObject::connect(b_open, &QPushButton::clicked, q, [this]() { act_open(); });
    QObject::connect(b_rerun, &QPushButton::clicked, q, [this]() { act_rerun(true); });
    QObject::connect(b_inputs, &QPushButton::clicked, q, [this]() { act_rerun(false); });
    QObject::connect(b_rename, &QPushButton::clicked, q, [this]() { act_rename(false); });
    QObject::connect(b_pin, &QPushButton::clicked, q, [this]() { act_rename(true); });
    QObject::connect(b_compare, &QPushButton::clicked, q, [this]() { act_compare(); });
    QObject::connect(b_delete, &QPushButton::clicked, q, [this]() { act_delete(); });

    // Keys on the table: Enter = OPEN, Delete = DELETE, Space = tick. Ctrl+F = the filter box.
    auto add_key = [this](const QKeySequence& seq, std::function<void()> fn, QWidget* on, Qt::ShortcutContext ctx) {
        auto* sc = new QShortcut(seq, on);
        sc->setContext(ctx);
        QObject::connect(sc, &QShortcut::activated, q, [fn]() { fn(); });
    };
    add_key(QKeySequence(QKeyCombination(Qt::Key_Return)), [this]() { act_open(); }, view, Qt::WidgetShortcut);
    add_key(QKeySequence(QKeyCombination(Qt::Key_Enter)), [this]() { act_open(); }, view, Qt::WidgetShortcut);
    add_key(QKeySequence(QKeySequence::Delete), [this]() { act_delete(); }, view, Qt::WidgetShortcut);
    // Agents operating the UI: Delete on the table deletes the saved run, like the DELETE button.
    view->setProperty(fincept::core::agent::kAgentRiskKeysProperty, QStringLiteral("Delete=delete"));
    add_key(QKeySequence(QKeyCombination(Qt::Key_Space)), [this]() {
        if (const qint64 id = selected_id())
            toggle_tick(id);
    }, view, Qt::WidgetShortcut);
    add_key(QKeySequence(QKeySequence::Find), [this]() {
        filter->setFocus(Qt::ShortcutFocusReason);
        filter->selectAll();
    }, q, Qt::WidgetWithChildrenShortcut);

    // Context menu: the run actions, then the table export (replaces setup_table's export-only menu).
    QObject::disconnect(view, &QWidget::customContextMenuRequested, nullptr, nullptr);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(view, &QWidget::customContextMenuRequested, q, [this](const QPoint& pos) { context_menu(pos); });

    stack->setCurrentWidget(loading);
    update_actions();
}

void BtRunsTab::refresh() {
    const quint64 gen = ++d->generation;
    if (!d->loaded)
        d->stack->setCurrentWidget(d->loading);
    QPointer<BtRunsTab> self(this);
    (void)QtConcurrent::run([self, gen]() {
        QVector<BtRunsRow> rows;
        QString error;
        auto& store = BacktestRunStore::instance();
        if (!store.available()) {
            error = BtRunsTab::tr("The run history is not available yet: the database has not been upgraded.");
        } else {
            auto listed = store.list(kRunsListLimit);
            if (listed.is_err()) {
                error = BtRunsTab::tr("The run history could not be read: %1")
                            .arg(QString::fromStdString(listed.error()));
            } else {
                QHash<QString, QString> names;
                for (const services::backtest::Provider& p : services::backtest::all_providers())
                    names.insert(p.slug, p.display_name);
                rows.reserve(listed.value().size());
                for (const SavedRun& s : listed.value())
                    rows.append(runs_make_row(s, names));
            }
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, gen, rows, error]() {
                if (!self || self->d->generation != gen)
                    return;
                self->d->loaded_rows(rows, error);
            },
            Qt::QueuedConnection);
    });
}

void BtRunsTab::Impl::loaded_rows(const QVector<BtRunsRow>& rows, const QString& error) {
    if (!error.isEmpty()) {
        empty_is_error = true;
        empty->apply(ui::EmptyState::error_spec(error, BtRunsTab::tr("Run history unavailable"), BtRunsTab::tr("RETRY")));
        stack->setCurrentWidget(empty);
        return;
    }
    const qint64 keep = pending_select ? pending_select : selected_id();
    loaded = true;
    all = rows;
    // Ticks of runs that no longer exist go.
    QVector<qint64> still;
    for (qint64 id : ticked)
        for (const BtRunsRow& r : all)
            if (r.run.id == id) {
                still.append(id);
                break;
            }
    ticked = still;
    if (all.isEmpty()) {
        empty_is_error = false;
        empty->apply(ui::EmptyState::no_data_spec(BtRunsTab::tr("No saved runs yet"),
                                                  BtRunsTab::tr("Every completed run is saved here."),
                                                  BtRunsTab::tr("RUN BACKTEST")));
        stack->setCurrentWidget(empty);
        model->clear();
        update_actions();
        return;
    }
    stack->setCurrentWidget(main);
    apply(keep);
    pending_select = 0;
}

void BtRunsTab::Impl::apply(qint64 keep_selected) {
    const QString text = filter->text().trimmed();
    const QString eng = engine->currentData().toString();
    const QString typ = type->currentData().toString();
    const bool pinned = pinned_only->isChecked();
    QVector<BtRunsRow> rows;
    rows.reserve(all.size());
    for (const BtRunsRow& r : all) {
        if (!eng.isEmpty() && r.run.provider != eng)
            continue;
        if (!typ.isEmpty() && r.run.command != typ)
            continue;
        if (pinned && !r.run.pinned())
            continue;
        if (!text.isEmpty()) {
            const QString hay = QStringList{r.run.label, r.run.strategy, r.run.symbols, r.engine_name,
                                            runs_type_text(r.run.command), r.run.created_at, r.run.start_date,
                                            r.run.end_date}
                                    .join(QLatin1Char(' '));
            if (!hay.contains(text, Qt::CaseInsensitive))
                continue;
        }
        rows.append(r);
    }
    model->upsert(rows);
    const int n = static_cast<int>(rows.size());
    const int total = static_cast<int>(all.size());
    count_label->setText(n == total ? BtRunsTab::tr("%1 runs").arg(count(n))
                                    : BtRunsTab::tr("%1 of %2 runs").arg(count(n), count(total)));
    panel.header->set_badge(count(n));
    note_row->setVisible(n == 0 && total > 0);
    if (keep_selected && model->index_of_key(QString::number(keep_selected)) >= 0)
        select_id(keep_selected);
    else if (n > 0 && view->selectionModel()->selectedRows().isEmpty())
        view->selectRow(0);
    update_actions();
}

qint64 BtRunsTab::Impl::selected_id() const {
    const BtRunsRow* r = selected_row();
    return r ? r->run.id : 0;
}

const BtRunsRow* BtRunsTab::Impl::selected_row() const {
    if (!view->selectionModel())
        return nullptr;
    const QModelIndexList sel = view->selectionModel()->selectedRows();
    if (sel.isEmpty())
        return nullptr;
    return model->row_at(proxy->mapToSource(sel.first()));
}

void BtRunsTab::Impl::select_id(qint64 id) {
    const int src = model->index_of_key(QString::number(id));
    if (src < 0)
        return;
    const QModelIndex idx = proxy->mapFromSource(model->index(src, 0));
    if (!idx.isValid())
        return;
    view->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    view->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::NoUpdate);
    view->scrollTo(idx, QAbstractItemView::EnsureVisible);
}

void BtRunsTab::Impl::update_actions() {
    const BtRunsRow* r = selected_row();
    const bool on = r != nullptr;
    for (QPushButton* b : {b_open, b_rerun, b_inputs, b_rename, b_pin, b_delete})
        b->setEnabled(on);
    const bool backtest = on && r->run.command == QLatin1String("backtest");
    b_compare->setEnabled(ticked.size() >= 2 || backtest);
    b_pin->setText(on && r->run.pinned() ? BtRunsTab::tr("UNPIN") : BtRunsTab::tr("PIN"));
    compare_ticked->setText(BtRunsTab::tr("COMPARE TICKED (%1)").arg(ticked.size()));
    compare_ticked->setEnabled(ticked.size() >= 2);
}

void BtRunsTab::Impl::toggle_tick(qint64 id) {
    if (ticked.contains(id)) {
        ticked.removeAll(id);
    } else {
        if (ticked.size() >= kRunsMaxTicked) {
            if (host)
                host->notify(BtRunsTab::tr("At most %1 runs can be compared; untick one first.").arg(kRunsMaxTicked));
            return;
        }
        ticked.append(id);
    }
    const int src = model->index_of_key(QString::number(id));
    if (src >= 0)
        emit model->dataChanged(model->index(src, kRcTick), model->index(src, kRcTick), {Qt::CheckStateRole});
    update_actions();
}

void BtRunsTab::Impl::act_open() {
    if (const qint64 id = selected_id(); id && host)
        host->open_saved_run(id);
}

void BtRunsTab::Impl::act_rerun(bool run) {
    if (const qint64 id = selected_id(); id && host)
        host->load_saved_inputs(id, run);
}

void BtRunsTab::Impl::act_rename(bool pin_toggle) {
    const BtRunsRow* r = selected_row();
    if (!r)
        return;
    const qint64 id = r->run.id;
    QString label;
    if (pin_toggle && r->run.pinned()) {
        label.clear(); // UNPIN: back under the run cap
    } else {
        QStringList parts;
        if (!r->run.strategy.isEmpty())
            parts << r->run.strategy;
        if (!r->run.symbols.isEmpty())
            parts << r->run.symbols;
        const QString suggestion = r->run.label.isEmpty() ? parts.join(QStringLiteral(" · ")) : r->run.label;
        bool ok = false;
        label = QInputDialog::getText(q, pin_toggle ? BtRunsTab::tr("Pin run") : BtRunsTab::tr("Rename run"),
                                      BtRunsTab::tr("Label (a labelled run is pinned and kept beyond the %1-run cap; "
                                                    "an empty label unpins it):")
                                          .arg(BacktestRunStore::kMaxRuns),
                                      QLineEdit::Normal, suggestion, &ok)
                    .trimmed();
        if (!ok)
            return;
        if (pin_toggle && label.isEmpty())
            return;
    }
    auto res = BacktestRunStore::instance().rename(id, label);
    if (res.is_err()) {
        if (host)
            host->notify(BtRunsTab::tr("Could not rename the run: %1").arg(QString::fromStdString(res.error())));
        return;
    }
    pending_select = id;
    q->refresh();
    emit q->runs_changed();
}

void BtRunsTab::Impl::act_compare() {
    if (ticked.size() >= 2) {
        emit q->compare_requested(ticked);
        return;
    }
    // Fewer than two ticked: the selected run (with the ticked one, if any); the screen adds the active run.
    QVector<qint64> ids = ticked;
    if (const qint64 id = selected_id(); id && !ids.contains(id))
        ids.prepend(id);
    if (!ids.isEmpty())
        emit q->compare_requested(ids);
}

void BtRunsTab::Impl::act_delete() {
    const BtRunsRow* r = selected_row();
    if (!r)
        return;
    const qint64 id = r->run.id;
    const QString name = runs_name(*r);
    const auto answer = QMessageBox::question(
        q, BtRunsTab::tr("Delete run"),
        BtRunsTab::tr("Delete the saved run “%1”? This cannot be undone.").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    auto res = BacktestRunStore::instance().remove(id);
    if (res.is_err()) {
        if (host)
            host->notify(BtRunsTab::tr("Could not delete the run: %1").arg(QString::fromStdString(res.error())));
        return;
    }
    ticked.removeAll(id);
    q->refresh();
    emit q->runs_changed();
}

void BtRunsTab::Impl::context_menu(const QPoint& pos) {
    const QModelIndex at = view->indexAt(pos);
    if (at.isValid() && !view->selectionModel()->isRowSelected(at.row(), QModelIndex()))
        view->selectRow(at.row());
    const BtRunsRow* r = selected_row();
    QMenu menu(view);
    if (r) {
        const qint64 id = r->run.id;
        menu.addAction(BtRunsTab::tr("Open"), q, [this]() { act_open(); });
        menu.addAction(BtRunsTab::tr("Re-run"), q, [this]() { act_rerun(true); });
        menu.addAction(BtRunsTab::tr("Load inputs"), q, [this]() { act_rerun(false); });
        menu.addSeparator();
        menu.addAction(BtRunsTab::tr("Rename…"), q, [this]() { act_rename(false); });
        menu.addAction(r->run.pinned() ? BtRunsTab::tr("Unpin") : BtRunsTab::tr("Pin…"), q,
                       [this]() { act_rename(true); });
        menu.addAction(ticked.contains(id) ? BtRunsTab::tr("Untick") : BtRunsTab::tr("Tick for compare"), q,
                       [this, id]() { toggle_tick(id); });
        QAction* cmp = menu.addAction(BtRunsTab::tr("Compare"), q, [this]() { act_compare(); });
        cmp->setEnabled(b_compare->isEnabled());
        menu.addSeparator();
        menu.addAction(BtRunsTab::tr("Delete…"), q, [this]() { act_delete(); });
        menu.actions().constLast()->setProperty("fincept_agent_risk", "delete");
        menu.addSeparator();
    }
    ui::ChartExport::MenuSpec spec;
    spec.suggested_name = QStringLiteral("backtest_runs");
    const QPointer<QTableView> guard(view);
    spec.csv = [guard]() -> ui::ChartExport::Table {
        if (!guard || !guard->model())
            return {};
        return ui::ChartExport::table_from_model(guard->model(), {kRcTick, kRcEquity});
    };
    ui::ChartExport::populate_menu(&menu, view, spec);
    if (!menu.isEmpty())
        menu.exec(view->viewport()->mapToGlobal(pos));
}

void BtRunsTab::select_run(qint64 store_id) {
    if (store_id <= 0)
        return;
    if (!d->loaded) {
        d->pending_select = store_id;
        return;
    }
    if (d->model->index_of_key(QString::number(store_id)) >= 0) {
        d->select_id(store_id);
        return;
    }
    bool known = false;
    for (const BtRunsRow& r : d->all)
        if (r.run.id == store_id) {
            known = true;
            break;
        }
    if (known) {
        // Hidden by the filters: clear them, then select.
        d->clear_filters->click();
        d->select_id(store_id);
    } else {
        // Saved after the last read: re-read and select it then.
        d->pending_select = store_id;
        refresh();
    }
}

} // namespace fincept::screens::bt
