// src/screens/backtesting/BtResearchTab.cpp
//
// RESEARCH tab (BtWorkbenchTabs.h): the eight non-strategy tools of the engines — indicator,
// indicator signals, signal generators, ML labels, labels -> signals, CV splits, returns analysis,
// indicator sweep — each with its settings in one wrapping strip and EXACTLY the request keys of the
// old screen (BacktestingScreen_Commands.cpp gather_args, defaults from BacktestingScreen_Layout.cpp).
// Common keys (symbols, startDate, endDate, interval, allowSyntheticData; returns also
// benchmarkSymbol / riskFreeRate) come from the setup through build_backtest_request(); the tools
// load ONE symbol (the engines' load_one), so the first symbol of the setup is sent.
//
// One ui::AutoLoadPane, one view per engine + tool: the seven light tools AutoRun, the sweep is
// Manual (preview + RUN); a tool the engine does not offer is a Manual view whose preview says so.
// Runs go through BtRunController (BtKind::Research). The result is a KPI row of the payload's
// top-level numbers, the tool's chart (btview::make_command_chart, rebuilt per result) and a
// model/view table of the first array of objects in the payload.
#include "screens/backtesting/BtWorkbenchTabs.h"

#include "screens/backtesting/BacktestResultData.h"
#include "screens/backtesting/BacktestViews.h"
#include "screens/backtesting/BtAnalysisTabs.h"
#include "screens/backtesting/BtAnalysisViews.h"
#include "screens/backtesting/BtRunController.h"
#include "services/backtesting/BacktestRequest.h"
#include "services/backtesting/BacktestRunProtocol.h"
#include "services/backtesting/BacktestingService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/StatTile.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace fincept::screens::bt {

using services::backtest::BtOutcome;
using services::backtest::BtProgress;
using services::backtest::BtSetup;

/// One row of the result table: an object of the payload's first array of objects.
struct BtResRow {
    int index = 0;
    QJsonObject obj;
};

namespace {

using ResRowModel = ui::LiveTableModel<BtResRow>;

constexpr int kResMaxRows = 20000;
constexpr int kResMaxColumns = 16;
constexpr int kResMaxKpis = 10;
constexpr int kResTimeoutSec = 120;
constexpr int kResPersistBytes = 1024 * 1024;

const char* const kResTools[] = {"indicator", "indicator_signals", "signals", "labels",
                                 "labels_to_signals", "splits", "returns", "indicator_sweep"};

QString res_tool_label(const QString& tool) {
    if (tool == QLatin1String("indicator"))
        return BtResearchTab::tr("Indicator");
    if (tool == QLatin1String("indicator_signals"))
        return BtResearchTab::tr("Indicator signals");
    if (tool == QLatin1String("signals"))
        return BtResearchTab::tr("Signal generators");
    if (tool == QLatin1String("labels"))
        return BtResearchTab::tr("ML labels");
    if (tool == QLatin1String("labels_to_signals"))
        return BtResearchTab::tr("Labels → signals");
    if (tool == QLatin1String("splits"))
        return BtResearchTab::tr("CV splits");
    if (tool == QLatin1String("returns"))
        return BtResearchTab::tr("Returns analysis");
    if (tool == QLatin1String("indicator_sweep"))
        return BtResearchTab::tr("Indicator sweep");
    return tool;
}

QString res_tool_text(const QString& tool) {
    if (tool == QLatin1String("indicator"))
        return BtResearchTab::tr("Computes the indicator on the symbol and charts it with its statistics.");
    if (tool == QLatin1String("indicator_signals"))
        return BtResearchTab::tr("Entry and exit signals from an indicator rule, over the price.");
    if (tool == QLatin1String("signals"))
        return BtResearchTab::tr("Random entry / exit generators: a no-skill benchmark for a strategy's signals.");
    if (tool == QLatin1String("labels"))
        return BtResearchTab::tr("Machine-learning labels (future-move classes) for every bar.");
    if (tool == QLatin1String("labels_to_signals"))
        return BtResearchTab::tr("Turns ML labels into entry / exit signals.");
    if (tool == QLatin1String("splits"))
        return BtResearchTab::tr("Cross-validation splits (train / test spans) of the period.");
    if (tool == QLatin1String("returns"))
        return BtResearchTab::tr("Return statistics, drawdowns, ranges or a rolling metric of the symbol.");
    if (tool == QLatin1String("indicator_sweep"))
        return BtResearchTab::tr("Runs the indicator over a range of periods and compares the results.");
    return QString();
}

/// Engine indicator lists (get_indicators), process-wide, GUI thread only.
QHash<QString, QVector<QPair<QString, QString>>>& res_indicator_cache() {
    static QHash<QString, QVector<QPair<QString, QString>>> cache;
    return cache;
}

/// Used until the engine's own list arrives (ids of the engines' catalogues).
QVector<QPair<QString, QString>> res_fallback_indicators() {
    return {{QStringLiteral("sma"), QStringLiteral("SMA")},
            {QStringLiteral("ema"), QStringLiteral("EMA")},
            {QStringLiteral("rsi"), QStringLiteral("RSI")},
            {QStringLiteral("macd"), QStringLiteral("MACD")},
            {QStringLiteral("bbands"), QStringLiteral("Bollinger Bands")},
            {QStringLiteral("atr"), QStringLiteral("ATR")},
            {QStringLiteral("stochastic"), QStringLiteral("Stochastic")},
            {QStringLiteral("adx"), QStringLiteral("ADX")},
            {QStringLiteral("obv"), QStringLiteral("OBV")},
            {QStringLiteral("zscore"), QStringLiteral("Z-Score")}};
}

/// {indicators: {Category: [{id, name}]}} (or an array of {id, name}) -> (id, name).
QVector<QPair<QString, QString>> res_parse_indicators(const QJsonObject& data) {
    QJsonValue v = data.value(QStringLiteral("indicators"));
    if (v.isUndefined())
        v = data.value(QStringLiteral("data")).toObject().value(QStringLiteral("indicators"));
    QVector<QPair<QString, QString>> out;
    auto take = [&out](const QJsonArray& arr) {
        for (const QJsonValue& x : arr) {
            const QJsonObject o = x.toObject();
            const QString id = o.value(QStringLiteral("id")).toString();
            if (!id.isEmpty())
                out.append({id, o.value(QStringLiteral("name")).toString(id)});
        }
    };
    if (v.isArray()) {
        take(v.toArray());
    } else if (v.isObject()) {
        const QJsonObject cats = v.toObject();
        for (auto it = cats.constBegin(); it != cats.constEnd(); ++it)
            take(it.value().toArray());
    }
    return out;
}

/// Unit of a payload key by the shared key lists (BacktestingTypes.h).
Unit res_unit_of(const QString& key) {
    using namespace services::backtest;
    static const QStringList pcts = pct_metric_keys();
    static const QStringList ratios = ratio_metric_keys();
    static const QStringList counts = count_metric_keys();
    static const QStringList money = currency_metric_keys();
    if (pcts.contains(key))
        return Unit::Fraction;
    if (ratios.contains(key))
        return Unit::Ratio;
    if (counts.contains(key))
        return Unit::Count;
    if (money.contains(key))
        return Unit::Money;
    return Unit::Plain;
}

QString res_number_text(double v, const QString& key, const QString& ccy) {
    const Unit u = res_unit_of(key);
    if (u == Unit::Fraction)
        return pct(v, 2);
    if (u == Unit::Plain) {
        const double a = std::fabs(v);
        return num(v, a >= 1000 ? 0 : (a >= 1 ? 2 : 4));
    }
    return format_value(v, u, ccy);
}

QString res_cell_text(const QJsonValue& v, const QString& key, const QString& ccy) {
    if (v.isDouble())
        return res_number_text(v.toDouble(), key, ccy);
    if (v.isBool())
        return v.toBool() ? BtResearchTab::tr("yes") : BtResearchTab::tr("no");
    if (v.isString())
        return v.toString();
    if (v.isNull() || v.isUndefined())
        return dash();
    const QString json = QString::fromUtf8(v.isArray() ? QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact)
                                                       : QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return json.size() > 120 ? json.left(117) + QStringLiteral("…") : json;
}

/// Wheel over an unfocused spin box / combo does not change it.
class BtResWheelGuard : public QObject {
  public:
    using QObject::QObject;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Wheel) {
            auto* w = qobject_cast<QWidget*>(watched);
            if (w && !w->hasFocus()) {
                event->ignore();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

struct BtResearchTab::Impl {
    BtResearchTab* q = nullptr;
    BtHost* host = nullptr;
    ui::InputMemory* memory = nullptr;
    BtResWheelGuard* wheel = nullptr;

    // Strip
    QWidget* flow = nullptr;
    QComboBox* tool = nullptr;
    QLabel* note = nullptr;
    QLabel* as_of = nullptr;
    QPushButton* run_btn = nullptr;
    struct Field {
        QWidget* box = nullptr;
        std::function<bool(const QString& tool)> visible;
    };
    QVector<Field> fields;

    // Tool inputs (names follow the old screen's widgets).
    QComboBox* ind = nullptr;
    QSpinBox* ind_period = nullptr;
    QComboBox* is_ind = nullptr;
    QComboBox* is_mode = nullptr;
    QSpinBox* is_fast = nullptr;
    QSpinBox* is_slow = nullptr;
    QComboBox* is_ma = nullptr;
    QSpinBox* is_period = nullptr;
    QDoubleSpinBox* is_lower = nullptr;
    QDoubleSpinBox* is_upper = nullptr;
    QComboBox* is_channel = nullptr;
    QDoubleSpinBox* is_z_entry = nullptr;
    QDoubleSpinBox* is_z_exit = nullptr;
    QComboBox* is_filt_ind = nullptr;
    QSpinBox* is_filt_period = nullptr;
    QDoubleSpinBox* is_filt_thr = nullptr;
    QComboBox* is_filt_type = nullptr;
    QComboBox* sig_gen = nullptr;
    QSpinBox* sig_n = nullptr;
    QDoubleSpinBox* sig_entry = nullptr;
    QDoubleSpinBox* sig_exit = nullptr;
    QSpinBox* sig_min_hold = nullptr;
    QSpinBox* sig_max_hold = nullptr;
    QComboBox* lab_type = nullptr;
    QComboBox* l2s_type = nullptr;
    QSpinBox* l2s_entry = nullptr;
    QSpinBox* l2s_exit = nullptr;
    QSpinBox* lab_horizon = nullptr;
    QSpinBox* lab_window = nullptr;
    QDoubleSpinBox* lab_threshold = nullptr;
    QDoubleSpinBox* lab_alpha = nullptr;
    QComboBox* spl_type = nullptr;
    QSpinBox* spl_window = nullptr;
    QSpinBox* spl_min = nullptr;
    QSpinBox* spl_test = nullptr;
    QSpinBox* spl_step = nullptr;
    QSpinBox* spl_n = nullptr;
    QSpinBox* spl_purge = nullptr;
    QSpinBox* spl_embargo = nullptr;
    QComboBox* ret_type = nullptr;
    QSpinBox* ret_window = nullptr;
    QComboBox* ret_metric = nullptr;
    QDoubleSpinBox* ret_threshold = nullptr;
    QComboBox* sweep_ind = nullptr;
    QSpinBox* sweep_min = nullptr;
    QSpinBox* sweep_max = nullptr;
    QSpinBox* sweep_step = nullptr;

    // Result
    ui::AutoLoadPane* pane = nullptr;
    ui::KpiStrip* kpis = nullptr;
    BtPanel chart_panel;
    QPointer<QWidget> chart;
    QLabel* chart_note = nullptr;
    BtPanel rows_panel;
    QTableView* rows_view = nullptr;
    ResRowModel* rows_model = nullptr;
    QSortFilterProxyModel* rows_proxy = nullptr;
    QStringList columns;
    QString result_ccy;
    QLabel* rows_note = nullptr;

    // Runs
    quint64 token = 0;
    quint64 pane_request = 0;
    QString run_tool;
    QString run_engine;
    QString view_key;
    quint64 indicator_run = 0;
    QString indicator_engine;

    void build();
    QWidget* add_field(const QString& caption, QWidget* editor, std::function<bool(const QString&)> visible,
                       const QString& tip = QString());
    QSpinBox* ispin(int lo, int hi, int value);
    QDoubleSpinBox* dspin(double lo, double hi, double value, int decimals, double step = 0.0);
    QComboBox* combo(const QStringList& items);
    QString current_tool() const;
    QJsonObject request(const QString& tool) const;
    QString signature() const;
    void update_fields();
    void update_tools();
    void apply_view();
    void inputs_changed();
    void fill_indicators(const QVector<QPair<QString, QString>>& list);
    void request_indicators();
    void start(quint64 pane_request);
    void render(const QJsonObject& payload);
};

BtResearchTab::BtResearchTab(BtHost* host, QWidget* parent) : QWidget(parent), d(std::make_unique<Impl>()) {
    d->q = this;
    d->host = host;
    d->build();
    refresh();
}

BtResearchTab::~BtResearchTab() {
    // The pane outlives d (children go after the members): its callbacks must not reach d.
    if (d && d->pane) {
        d->pane->set_start({});
        d->pane->set_cancel({});
        d->pane->set_render({});
    }
    if (d && d->token && d->host && d->host->controller())
        d->host->controller()->cancel(d->token);
}

QWidget* BtResearchTab::Impl::add_field(const QString& caption, QWidget* editor,
                                        std::function<bool(const QString&)> visible, const QString& tip) {
    auto* box = new QWidget(flow);
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    auto* l = new QLabel(caption, box);
    l->setObjectName(QStringLiteral("btFieldLabel"));
    editor->setParent(box);
    editor->setAccessibleName(caption);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->installEventFilter(wheel);
    if (!tip.isEmpty()) {
        editor->setToolTip(tip);
        l->setToolTip(tip);
    }
    h->addWidget(l);
    h->addWidget(editor);
    flow->layout()->addWidget(box);
    fields.append({box, std::move(visible)});
    return box;
}

QSpinBox* BtResearchTab::Impl::ispin(int lo, int hi, int value) {
    auto* s = new QSpinBox(flow);
    s->setRange(lo, hi);
    s->setValue(value);
    s->setKeyboardTracking(false);
    return s;
}

QDoubleSpinBox* BtResearchTab::Impl::dspin(double lo, double hi, double value, int decimals, double step) {
    auto* s = new QDoubleSpinBox(flow);
    s->setDecimals(decimals);
    s->setRange(lo, hi);
    s->setValue(value);
    if (step > 0)
        s->setSingleStep(step);
    s->setKeyboardTracking(false);
    return s;
}

QComboBox* BtResearchTab::Impl::combo(const QStringList& items) {
    auto* c = new QComboBox(flow);
    c->setObjectName(QStringLiteral("btCombo"));
    c->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    for (const QString& it : items)
        c->addItem(it, it);
    return c;
}

void BtResearchTab::Impl::build() {
    using namespace services::backtest;
    wheel = new BtResWheelGuard(q);
    auto* root = new QVBoxLayout(q);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Settings strip ──
    auto* strip = new QWidget(q);
    strip->setObjectName(QStringLiteral("btSettingsStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    auto* sv = new QVBoxLayout(strip);
    sv->setContentsMargins(8, 5, 8, 5);
    sv->setSpacing(3);
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    flow = new QWidget(strip);
    (new ui::FlowLayout(flow, 12, 4))->set_compact_hint(true); // shared wrapping row; hint as the old copy
    row->addWidget(flow, 1);
    as_of = new QLabel(strip);
    as_of->setObjectName(QStringLiteral("btNote"));
    row->addWidget(as_of, 0, Qt::AlignTop);
    run_btn = new QPushButton(BtResearchTab::tr("RUN"), strip);
    run_btn->setObjectName(QStringLiteral("btPrimary"));
    run_btn->setCursor(Qt::PointingHandCursor);
    row->addWidget(run_btn, 0, Qt::AlignTop);
    sv->addLayout(row);
    note = new QLabel(strip);
    note->setObjectName(QStringLiteral("btNote"));
    note->setWordWrap(true);
    sv->addWidget(note);
    root->addWidget(strip);

    const auto always = [](const QString&) { return true; };
    const auto on = [](const char* t) { return [t](const QString& tool) { return tool == QLatin1String(t); }; };

    tool = combo({});
    for (const char* t : kResTools)
        tool->addItem(res_tool_label(QLatin1String(t)), QString::fromLatin1(t));
    add_field(BtResearchTab::tr("TOOL"), tool, always);

    // indicator
    ind = combo({});
    add_field(BtResearchTab::tr("INDICATOR TYPE"), ind, on("indicator"));
    ind_period = ispin(2, 500, 14);
    add_field(BtResearchTab::tr("PERIOD"), ind_period, on("indicator"));

    // indicator_signals
    is_ind = combo({});
    add_field(BtResearchTab::tr("INDICATOR"), is_ind, on("indicator_signals"));
    is_mode = combo(indicator_signal_modes());
    add_field(BtResearchTab::tr("SIGNAL MODE"), is_mode, on("indicator_signals"));
    const auto is_m = [this](std::initializer_list<const char*> modes) {
        QStringList ms;
        for (const char* m : modes)
            ms << QString::fromLatin1(m);
        return [this, ms](const QString& t) {
            return t == QLatin1String("indicator_signals") && ms.contains(is_mode->currentText());
        };
    };
    is_fast = ispin(2, 200, 10);
    add_field(BtResearchTab::tr("FAST PERIOD"), is_fast, is_m({"crossover_signals", "crossover"}));
    is_slow = ispin(2, 500, 20);
    add_field(BtResearchTab::tr("SLOW PERIOD"), is_slow, is_m({"crossover_signals", "crossover"}));
    is_ma = combo({QStringLiteral("ma"), QStringLiteral("ema")});
    add_field(BtResearchTab::tr("MA TYPE"), is_ma, is_m({"crossover_signals", "crossover"}));
    is_period = ispin(2, 200, 14);
    add_field(BtResearchTab::tr("PERIOD"), is_period,
              is_m({"threshold_signals", "threshold", "breakout_signals", "breakout", "mean_reversion_signals",
                    "mean_reversion", "signal_filter", "filter"}),
              BtResearchTab::tr("Indicator period (the base period in signal_filter mode)"));
    is_lower = dspin(-200, 200, 30, 2);
    add_field(BtResearchTab::tr("LOWER THRESHOLD"), is_lower, is_m({"threshold_signals", "threshold"}));
    is_upper = dspin(-200, 200, 70, 2);
    add_field(BtResearchTab::tr("UPPER THRESHOLD"), is_upper, is_m({"threshold_signals", "threshold"}));
    is_channel = combo({QStringLiteral("donchian"), QStringLiteral("bbands"), QStringLiteral("keltner")});
    add_field(BtResearchTab::tr("CHANNEL"), is_channel, is_m({"breakout_signals", "breakout"}));
    is_z_entry = dspin(0.1, 5.0, 2.0, 2);
    add_field(BtResearchTab::tr("Z ENTRY (σ)"), is_z_entry, is_m({"mean_reversion_signals", "mean_reversion"}));
    is_z_exit = dspin(0.0, 5.0, 0.0, 2);
    add_field(BtResearchTab::tr("Z EXIT (σ)"), is_z_exit, is_m({"mean_reversion_signals", "mean_reversion"}));
    is_filt_ind = combo({QStringLiteral("adx"), QStringLiteral("atr"), QStringLiteral("mstd"), QStringLiteral("zscore"),
                         QStringLiteral("rsi")});
    add_field(BtResearchTab::tr("FILTER INDICATOR"), is_filt_ind, is_m({"signal_filter", "filter"}));
    is_filt_period = ispin(2, 200, 14);
    add_field(BtResearchTab::tr("FILTER PERIOD"), is_filt_period, is_m({"signal_filter", "filter"}));
    is_filt_thr = dspin(-100, 100, 25, 3);
    add_field(BtResearchTab::tr("FILTER THRESHOLD"), is_filt_thr, is_m({"signal_filter", "filter"}));
    is_filt_type = combo({QStringLiteral("above"), QStringLiteral("below")});
    add_field(BtResearchTab::tr("FILTER TYPE"), is_filt_type, is_m({"signal_filter", "filter"}));

    // signals
    sig_gen = combo(signal_generators());
    add_field(BtResearchTab::tr("GENERATOR TYPE"), sig_gen, on("signals"));
    const auto sig_g = [this](std::initializer_list<const char*> gens) {
        QStringList gs;
        for (const char* g : gens)
            gs << QString::fromLatin1(g);
        return [this, gs](const QString& t) { return t == QLatin1String("signals") && gs.contains(sig_gen->currentText()); };
    };
    sig_n = ispin(1, 1000, 10);
    add_field(BtResearchTab::tr("COUNT (n)"), sig_n, sig_g({"RAND", "RANDX", "RANDNX"}));
    sig_entry = dspin(0.001, 1.0, 0.05, 3, 0.01);
    add_field(BtResearchTab::tr("ENTRY PROB"), sig_entry, sig_g({"RPROB", "RPROBX"}));
    sig_exit = dspin(0.001, 1.0, 0.10, 3, 0.01);
    add_field(BtResearchTab::tr("EXIT PROB"), sig_exit, sig_g({"RPROBX"}));
    sig_min_hold = ispin(1, 500, 5);
    add_field(BtResearchTab::tr("MIN HOLD (bars)"), sig_min_hold, sig_g({"RANDNX"}));
    sig_max_hold = ispin(1, 500, 20);
    add_field(BtResearchTab::tr("MAX HOLD (bars)"), sig_max_hold, sig_g({"RANDNX"}));

    // labels / labels_to_signals (the label parameters are shared, as on the old screen)
    lab_type = combo(label_types());
    add_field(BtResearchTab::tr("LABEL TYPE"), lab_type, on("labels"));
    l2s_type = combo(label_types());
    add_field(BtResearchTab::tr("LABEL TYPE"), l2s_type, on("labels_to_signals"));
    l2s_entry = ispin(-2, 2, 1);
    add_field(BtResearchTab::tr("ENTRY LABEL"), l2s_entry, on("labels_to_signals"),
              BtResearchTab::tr("Label value that opens a position (1 = up move)"));
    l2s_exit = ispin(-2, 2, -1);
    add_field(BtResearchTab::tr("EXIT LABEL"), l2s_exit, on("labels_to_signals"),
              BtResearchTab::tr("Label value that closes it (-1 = down move)"));
    const auto lab_t = [this](std::initializer_list<const char*> types) {
        QStringList ts;
        for (const char* t : types)
            ts << QString::fromLatin1(t);
        return [this, ts](const QString& t) {
            if (t == QLatin1String("labels"))
                return ts.contains(lab_type->currentText());
            if (t == QLatin1String("labels_to_signals"))
                return ts.contains(l2s_type->currentText());
            return false;
        };
    };
    lab_horizon = ispin(1, 100, 5);
    add_field(BtResearchTab::tr("HORIZON"), lab_horizon, lab_t({"FIXLB"}));
    lab_window = ispin(2, 500, 20);
    add_field(BtResearchTab::tr("WINDOW"), lab_window, lab_t({"MEANLB", "LEXLB", "TRENDLB", "BOLB"}));
    lab_threshold = dspin(0.0, 5.0, 0.02, 3, 0.005);
    add_field(BtResearchTab::tr("THRESHOLD"), lab_threshold, lab_t({"FIXLB", "MEANLB", "TRENDLB"}));
    lab_alpha = dspin(0.1, 5.0, 2.0, 2, 0.1);
    add_field(BtResearchTab::tr("ALPHA"), lab_alpha, lab_t({"BOLB"}));

    // splits
    spl_type = combo(splitter_types());
    add_field(BtResearchTab::tr("SPLITTER TYPE"), spl_type, on("splits"));
    const auto spl_t = [this](std::initializer_list<const char*> types) {
        QStringList ts;
        for (const char* t : types)
            ts << QString::fromLatin1(t);
        return [this, ts](const QString& t) { return t == QLatin1String("splits") && ts.contains(spl_type->currentText()); };
    };
    spl_window = ispin(10, 2000, 252);
    add_field(BtResearchTab::tr("WINDOW LENGTH"), spl_window, spl_t({"RollingSplitter"}));
    spl_min = ispin(10, 2000, 252);
    add_field(BtResearchTab::tr("MIN TRAIN LENGTH"), spl_min, spl_t({"ExpandingSplitter"}));
    spl_test = ispin(5, 1000, 63);
    add_field(BtResearchTab::tr("TEST LENGTH"), spl_test, spl_t({"RollingSplitter", "ExpandingSplitter"}));
    spl_step = ispin(1, 500, 21);
    add_field(BtResearchTab::tr("STEP SIZE"), spl_step, spl_t({"RollingSplitter", "ExpandingSplitter"}));
    spl_n = ispin(2, 20, 5);
    add_field(BtResearchTab::tr("N SPLITS"), spl_n, spl_t({"PurgedKFoldSplitter", "PurgedKFold"}));
    spl_purge = ispin(0, 100, 5);
    add_field(BtResearchTab::tr("PURGE LENGTH"), spl_purge, spl_t({"PurgedKFoldSplitter", "PurgedKFold"}));
    spl_embargo = ispin(0, 100, 5);
    add_field(BtResearchTab::tr("EMBARGO LENGTH"), spl_embargo, spl_t({"PurgedKFoldSplitter", "PurgedKFold"}));

    // returns
    ret_type = combo(returns_analysis_types());
    add_field(BtResearchTab::tr("ANALYSIS TYPE"), ret_type, on("returns"));
    const auto ret_t = [this](const char* at) {
        return [this, at](const QString& t) {
            return t == QLatin1String("returns") && ret_type->currentText() == QLatin1String(at);
        };
    };
    ret_window = ispin(5, 504, 63);
    add_field(BtResearchTab::tr("WINDOW"), ret_window, ret_t("rolling"));
    ret_metric = combo({QStringLiteral("sharpe"), QStringLiteral("sortino"), QStringLiteral("calmar"),
                        QStringLiteral("omega"), QStringLiteral("info_ratio"), QStringLiteral("downside_risk"),
                        QStringLiteral("total"), QStringLiteral("annualized"), QStringLiteral("volatility")});
    add_field(BtResearchTab::tr("METRIC"), ret_metric, ret_t("rolling"));
    ret_threshold = dspin(-1.0, 1.0, 0.0, 4, 0.001);
    add_field(BtResearchTab::tr("THRESHOLD"), ret_threshold, ret_t("ranges"));

    // indicator_sweep
    sweep_ind = combo({});
    add_field(BtResearchTab::tr("INDICATOR"), sweep_ind, on("indicator_sweep"));
    sweep_min = ispin(1, 1000, 5);
    add_field(BtResearchTab::tr("MIN"), sweep_min, on("indicator_sweep"));
    sweep_max = ispin(1, 1000, 50);
    add_field(BtResearchTab::tr("MAX"), sweep_max, on("indicator_sweep"));
    sweep_step = ispin(1, 100, 1);
    add_field(BtResearchTab::tr("STEP"), sweep_step, on("indicator_sweep"));

    fill_indicators(res_fallback_indicators());

    // ── Remembered inputs ──
    memory = new ui::InputMemory(QStringLiteral("backtesting"), QStringLiteral("research"), q);
    const QVector<QPair<const char*, QWidget*>> tracked = {
        {"tool", tool},
        {"indicator.indicator", ind},
        {"indicator.period", ind_period},
        {"indicator_signals.indicator", is_ind},
        {"indicator_signals.mode", is_mode},
        {"indicator_signals.fast", is_fast},
        {"indicator_signals.slow", is_slow},
        {"indicator_signals.ma", is_ma},
        {"indicator_signals.period", is_period},
        {"indicator_signals.lower", is_lower},
        {"indicator_signals.upper", is_upper},
        {"indicator_signals.channel", is_channel},
        {"indicator_signals.z_entry", is_z_entry},
        {"indicator_signals.z_exit", is_z_exit},
        {"indicator_signals.filter_indicator", is_filt_ind},
        {"indicator_signals.filter_period", is_filt_period},
        {"indicator_signals.filter_threshold", is_filt_thr},
        {"indicator_signals.filter_type", is_filt_type},
        {"signals.generator", sig_gen},
        {"signals.n", sig_n},
        {"signals.entry_prob", sig_entry},
        {"signals.exit_prob", sig_exit},
        {"signals.min_hold", sig_min_hold},
        {"signals.max_hold", sig_max_hold},
        {"labels.type", lab_type},
        {"labels_to_signals.type", l2s_type},
        {"labels_to_signals.entry", l2s_entry},
        {"labels_to_signals.exit", l2s_exit},
        {"labels.horizon", lab_horizon},
        {"labels.window", lab_window},
        {"labels.threshold", lab_threshold},
        {"labels.alpha", lab_alpha},
        {"splits.type", spl_type},
        {"splits.window", spl_window},
        {"splits.min", spl_min},
        {"splits.test", spl_test},
        {"splits.step", spl_step},
        {"splits.n", spl_n},
        {"splits.purge", spl_purge},
        {"splits.embargo", spl_embargo},
        {"returns.type", ret_type},
        {"returns.window", ret_window},
        {"returns.metric", ret_metric},
        {"returns.threshold", ret_threshold},
        {"indicator_sweep.indicator", sweep_ind},
        {"indicator_sweep.min", sweep_min},
        {"indicator_sweep.max", sweep_max},
        {"indicator_sweep.step", sweep_step},
    };
    for (const auto& t : tracked)
        memory->track(QString::fromLatin1(t.first), t.second);
    memory->restore();
    QObject::connect(memory, &ui::InputMemory::changed, q, [this]() { inputs_changed(); });
    QObject::connect(tool, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this]() {
        update_fields();
        apply_view();
    });
    for (QComboBox* c : {is_mode, sig_gen, lab_type, l2s_type, spl_type, ret_type})
        QObject::connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this]() { update_fields(); });

    // ── Result pane ──
    pane = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, q);
    pane->set_requirements({ui::Requirement::python()});
    pane->set_cache_key(QStringLiteral("backtesting/research"));
    pane->set_persist_limit(kResPersistBytes);
    pane->set_max_age(-1);
    pane->set_timeout(kResTimeoutSec);
    pane->set_auto_delay(300);
    pane->set_status_label(as_of);
    pane->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);

    auto* content = new QWidget(pane->content_parent());
    auto* cv = new QVBoxLayout(content);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(0);
    QVBoxLayout* page = nullptr;
    QScrollArea* scroll = bt_analysis_page(content, &page);
    cv->addWidget(scroll);
    QWidget* body = scroll->widget();
    kpis = new ui::KpiStrip(body);
    page->addWidget(kpis);
    chart_panel = make_panel(BtResearchTab::tr("CHART"), body);
    chart_note = make_note(QString(), chart_panel.body_widget);
    chart_panel.body->addWidget(chart_note);
    page->addWidget(chart_panel.frame);
    rows_panel = make_panel(BtResearchTab::tr("ROWS"), body);
    rows_view = new QTableView(rows_panel.body_widget);
    rows_model = new ResRowModel(q);
    rows_model->set_key([](const BtResRow& r) { return QString::number(r.index); });
    rows_model->set_cell([this](const BtResRow& r, int col, int role) -> QVariant {
        if (col < 0 || col >= columns.size())
            return {};
        const QString& key = columns.at(col);
        const QJsonValue v = r.obj.value(key);
        if (role == Qt::DisplayRole)
            return res_cell_text(v, key, result_ccy);
        if (role == ResRowModel::SortRole) {
            if (v.isDouble())
                return v.toDouble();
            if (v.isBool())
                return v.toBool() ? 1 : 0;
            return v.isString() ? QVariant(v.toString()) : QVariant(r.index);
        }
        if (role == Qt::TextAlignmentRole)
            return v.isDouble() ? QVariant(int(Qt::AlignRight | Qt::AlignVCenter))
                                : QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));
        if (role == Qt::ToolTipRole && (v.isObject() || v.isArray()))
            return QString::fromUtf8(v.isArray() ? QJsonDocument(v.toArray()).toJson(QJsonDocument::Indented)
                                                 : QJsonDocument(v.toObject()).toJson(QJsonDocument::Indented));
        return {};
    });
    rows_proxy = make_sort_proxy(rows_model, q);
    rows_view->setModel(rows_proxy);
    setup_table(rows_view, QStringLiteral("research_rows"));
    ui::formatting::refresh_model_on_format_change(rows_model);
    bt_analysis_fit_table(rows_view, 12);
    rows_panel.body->addWidget(rows_view, 1);
    rows_note = make_note(QString(), rows_panel.body_widget);
    rows_panel.body->addWidget(rows_note);
    page->addWidget(rows_panel.frame);
    page->addStretch(1);
    pane->set_content(content);
    root->addWidget(pane, 1);

    pane->set_start([this](quint64 id) { start(id); });
    pane->set_cancel([this](quint64) {
        const quint64 tok = token;
        token = 0; // first: the controller's cancelled() for it is then not ours any more
        if (tok && host && host->controller())
            host->controller()->cancel(tok);
    });
    pane->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p.toObject()); });
    QObject::connect(pane, &ui::AutoLoadPane::running_changed, q, [this](bool running) {
        run_btn->setText(running ? BtResearchTab::tr("CANCEL") : BtResearchTab::tr("RUN"));
        set_tone(run_btn, running ? Tone::Danger : Tone::None);
        update_tools();
        emit q->running_changed(running);
    });
    QObject::connect(pane, &ui::AutoLoadPane::notice_action, q, [this]() { q->run(); });
    QObject::connect(run_btn, &QPushButton::clicked, q, [this]() {
        if (q->is_running())
            q->cancel();
        else
            q->run();
    });

    if (host && host->controller()) {
        BtRunController* ctl = host->controller();
        QObject::connect(ctl, &BtRunController::progress, q, [this](quint64 t, const BtProgress& p) {
            if (t == token) // NaN fraction (no counts yet) = text only
                pane->set_progress(pane_request, p.fraction(), services::backtest::progress_text(p));
        });
        QObject::connect(ctl, &BtRunController::finished, q, [this](quint64 t, const BtBundlePtr& b) {
            if (t != token || !b)
                return;
            token = 0;
            QJsonObject out;
            out.insert(QStringLiteral("tool"), run_tool);
            out.insert(QStringLiteral("engine"), run_engine);
            out.insert(QStringLiteral("currency"), b->currency);
            out.insert(QStringLiteral("data"), b->payload);
            pane->deliver(pane_request, out);
        });
        QObject::connect(ctl, &BtRunController::failed, q, [this](quint64 t, const BtOutcome& o) {
            if (t != token)
                return;
            token = 0;
            pane->fail(pane_request,
                       o.message.isEmpty() ? BtResearchTab::tr("The %1 tool failed.").arg(res_tool_label(run_tool))
                                           : o.message,
                       bt_failure_title(o.error_code), o.technical);
        });
        QObject::connect(ctl, &BtRunController::cancelled, q, [this](quint64 t) {
            if (t != token)
                return;
            token = 0;
            pane->cancel();
        });
    }

    // The engine's own indicator list (get_indicators); the fallback list stays until it arrives.
    auto& svc = services::backtest::BacktestingService::instance();
    QObject::connect(&svc, &services::backtest::BacktestingService::run_completed, q,
                     [this](quint64 run_id, QString provider, QString, QJsonObject data) {
                         if (run_id == 0 || run_id != indicator_run)
                             return;
                         indicator_run = 0;
                         const auto list = res_parse_indicators(data);
                         if (list.isEmpty())
                             return;
                         res_indicator_cache().insert(provider, list);
                         if (host && host->engine_slug() == provider)
                             fill_indicators(list);
                     });
    QObject::connect(&svc, &services::backtest::BacktestingService::run_failed, q,
                     [this](quint64 run_id, QString, QString) {
                         if (run_id != 0 && run_id == indicator_run)
                             indicator_run = 0; // keep the fallback list
                     });
    QObject::connect(&svc, &services::backtest::BacktestingService::run_cancelled, q,
                     [this](quint64 run_id, QString, QString) {
                         if (run_id != 0 && run_id == indicator_run)
                             indicator_run = 0;
                     });
    update_fields();
}

QString BtResearchTab::Impl::current_tool() const {
    const QString t = tool->currentData().toString();
    return t.isEmpty() ? QStringLiteral("indicator") : t;
}

void BtResearchTab::Impl::fill_indicators(const QVector<QPair<QString, QString>>& list) {
    for (QComboBox* c : {ind, is_ind, sweep_ind}) {
        const QString keep = c->currentData().toString();
        const bool blocked = c->blockSignals(true);
        c->clear();
        for (const auto& it : list)
            c->addItem(it.second, it.first);
        int idx = c->findData(keep);
        if (idx < 0 && c == is_ind)
            idx = c->findData(QStringLiteral("rsi"));
        c->setCurrentIndex(std::max(0, idx));
        c->blockSignals(blocked);
    }
    inputs_changed();
}

void BtResearchTab::Impl::request_indicators() {
    if (!host)
        return;
    const QString engine = host->engine_slug();
    if (engine.isEmpty())
        return;
    const auto& cache = res_indicator_cache();
    if (cache.contains(engine)) {
        fill_indicators(cache.value(engine));
        return;
    }
    fill_indicators(res_fallback_indicators());
    if (indicator_run && indicator_engine == engine)
        return;
    if (!host->engine_offers(QStringLiteral("indicator")))
        return;
    indicator_engine = engine;
    indicator_run = services::backtest::BacktestingService::instance().execute(engine, QStringLiteral("get_indicators"),
                                                                              QJsonObject());
}

void BtResearchTab::Impl::update_fields() {
    const QString t = current_tool();
    for (const Field& f : fields) {
        const bool show = f.visible(t);
        if (f.box->isVisibleTo(flow) != show)
            f.box->setVisible(show);
    }
    flow->layout()->invalidate();
    flow->updateGeometry();
}

QJsonObject BtResearchTab::Impl::request(const QString& t) const {
    QJsonObject a;
    if (!host)
        return a;
    const BtSetup setup = host->current_setup();
    const QJsonObject base = services::backtest::build_backtest_request(setup);
    // The tools load one symbol (the engines' load_one): the first of the setup.
    QJsonArray symbols;
    const QJsonArray all = base.value(QStringLiteral("symbols")).toArray();
    if (!all.isEmpty())
        symbols.append(all.first());
    a.insert(QStringLiteral("symbols"), symbols);
    for (const char* k : {"startDate", "endDate", "interval", "allowSyntheticData"}) {
        const QJsonValue v = base.value(QLatin1String(k));
        if (!v.isUndefined())
            a.insert(QLatin1String(k), v);
    }

    if (t == QLatin1String("indicator")) {
        a.insert(QStringLiteral("indicator"), ind->currentData().toString());
        QJsonObject p;
        p.insert(QStringLiteral("period"), ind_period->value());
        a.insert(QStringLiteral("params"), p);
    } else if (t == QLatin1String("indicator_signals")) {
        a.insert(QStringLiteral("indicator"), is_ind->currentData().toString());
        const QString m = is_mode->currentText();
        a.insert(QStringLiteral("mode"), m);
        QJsonObject p;
        if (m == QLatin1String("crossover_signals") || m == QLatin1String("crossover")) {
            p.insert(QStringLiteral("fast_period"), is_fast->value());
            p.insert(QStringLiteral("slow_period"), is_slow->value());
            p.insert(QStringLiteral("fastPeriod"), is_fast->value()); // zipline alias
            p.insert(QStringLiteral("slowPeriod"), is_slow->value());
            p.insert(QStringLiteral("ma_type"), is_ma->currentText());
            p.insert(QStringLiteral("maType"), is_ma->currentText());
            p.insert(QStringLiteral("fast_indicator"), is_ma->currentText()); // vectorbt key
            p.insert(QStringLiteral("slow_indicator"), is_ma->currentText());
        } else if (m == QLatin1String("threshold_signals") || m == QLatin1String("threshold")) {
            p.insert(QStringLiteral("period"), is_period->value());
            p.insert(QStringLiteral("lower"), is_lower->value());
            p.insert(QStringLiteral("upper"), is_upper->value());
        } else if (m == QLatin1String("breakout_signals") || m == QLatin1String("breakout")) {
            p.insert(QStringLiteral("period"), is_period->value());
            p.insert(QStringLiteral("channel"), is_channel->currentText());
        } else if (m == QLatin1String("mean_reversion_signals") || m == QLatin1String("mean_reversion")) {
            p.insert(QStringLiteral("period"), is_period->value());
            p.insert(QStringLiteral("z_entry"), is_z_entry->value());
            p.insert(QStringLiteral("z_exit"), is_z_exit->value());
        } else if (m == QLatin1String("signal_filter") || m == QLatin1String("filter")) {
            p.insert(QStringLiteral("base_indicator"), is_ind->currentData().toString());
            p.insert(QStringLiteral("base_period"), is_period->value());
            p.insert(QStringLiteral("filter_indicator"), is_filt_ind->currentText());
            p.insert(QStringLiteral("filter_period"), is_filt_period->value());
            p.insert(QStringLiteral("filter_threshold"), is_filt_thr->value());
            p.insert(QStringLiteral("filter_type"), is_filt_type->currentText());
        }
        a.insert(QStringLiteral("params"), p);
    } else if (t == QLatin1String("labels") || t == QLatin1String("labels_to_signals")) {
        const bool l2s = t == QLatin1String("labels_to_signals");
        const QString lt = l2s ? l2s_type->currentText() : lab_type->currentText();
        a.insert(QStringLiteral("labelType"), lt);
        if (l2s) {
            a.insert(QStringLiteral("entryLabel"), l2s_entry->value());
            a.insert(QStringLiteral("exitLabel"), l2s_exit->value());
        }
        QJsonObject p;
        if (lt == QLatin1String("FIXLB")) {
            p.insert(QStringLiteral("horizon"), lab_horizon->value());
            p.insert(QStringLiteral("threshold"), lab_threshold->value());
        } else if (lt == QLatin1String("MEANLB") || lt == QLatin1String("TRENDLB")) {
            p.insert(QStringLiteral("window"), lab_window->value());
            p.insert(QStringLiteral("threshold"), lab_threshold->value());
        } else if (lt == QLatin1String("LEXLB")) {
            p.insert(QStringLiteral("window"), lab_window->value());
        } else if (lt == QLatin1String("BOLB")) {
            p.insert(QStringLiteral("window"), lab_window->value());
            p.insert(QStringLiteral("alpha"), lab_alpha->value());
        }
        a.insert(QStringLiteral("params"), p);
    } else if (t == QLatin1String("splits")) {
        const QString st = spl_type->currentText();
        a.insert(QStringLiteral("splitterType"), st);
        QJsonObject p;
        // vectorbt reads snake_case, zipline camelCase: both are sent.
        if (st == QLatin1String("RollingSplitter")) {
            p.insert(QStringLiteral("window_len"), spl_window->value());
            p.insert(QStringLiteral("windowLen"), spl_window->value());
            p.insert(QStringLiteral("test_len"), spl_test->value());
            p.insert(QStringLiteral("testLen"), spl_test->value());
            p.insert(QStringLiteral("step"), spl_step->value());
        } else if (st == QLatin1String("ExpandingSplitter")) {
            p.insert(QStringLiteral("min_len"), spl_min->value());
            p.insert(QStringLiteral("minLen"), spl_min->value());
            p.insert(QStringLiteral("test_len"), spl_test->value());
            p.insert(QStringLiteral("testLen"), spl_test->value());
            p.insert(QStringLiteral("step"), spl_step->value());
        } else if (st == QLatin1String("PurgedKFoldSplitter") || st == QLatin1String("PurgedKFold")) {
            p.insert(QStringLiteral("n_splits"), spl_n->value());
            p.insert(QStringLiteral("nSplits"), spl_n->value());
            p.insert(QStringLiteral("purge_len"), spl_purge->value());
            p.insert(QStringLiteral("purgeLen"), spl_purge->value());
            p.insert(QStringLiteral("embargo_len"), spl_embargo->value());
            p.insert(QStringLiteral("embargoLen"), spl_embargo->value());
        }
        a.insert(QStringLiteral("params"), p);
    } else if (t == QLatin1String("returns")) {
        const QString at = ret_type->currentText();
        a.insert(QStringLiteral("analysisType"), at);
        const QString bench = base.value(QStringLiteral("benchmarkSymbol")).toString().trimmed();
        if (!bench.isEmpty())
            a.insert(QStringLiteral("benchmarkSymbol"), bench);
        if (base.contains(QStringLiteral("riskFreeRate")))
            a.insert(QStringLiteral("riskFreeRate"), base.value(QStringLiteral("riskFreeRate")));
        QJsonObject p;
        if (at == QLatin1String("rolling")) {
            p.insert(QStringLiteral("window"), ret_window->value());
            // vectorbt reads `metric`, zipline `metrics` (a list): both are sent.
            const QString m = ret_metric->currentText();
            p.insert(QStringLiteral("metric"), m);
            p.insert(QStringLiteral("metrics"), QJsonArray{m});
        } else if (at == QLatin1String("ranges")) {
            p.insert(QStringLiteral("threshold"), ret_threshold->value());
        }
        if (!p.isEmpty())
            a.insert(QStringLiteral("params"), p);
    } else if (t == QLatin1String("signals")) {
        const QString g = sig_gen->currentText();
        a.insert(QStringLiteral("generatorType"), g);
        QJsonObject p;
        if (g == QLatin1String("RAND") || g == QLatin1String("RANDX") || g == QLatin1String("RANDNX"))
            p.insert(QStringLiteral("n"), sig_n->value());
        if (g == QLatin1String("RANDNX")) {
            p.insert(QStringLiteral("min_hold"), sig_min_hold->value());
            p.insert(QStringLiteral("max_hold"), sig_max_hold->value());
            p.insert(QStringLiteral("minHold"), sig_min_hold->value());
            p.insert(QStringLiteral("maxHold"), sig_max_hold->value());
        }
        if (g == QLatin1String("RPROB") || g == QLatin1String("RPROBX")) {
            p.insert(QStringLiteral("entry_prob"), sig_entry->value());
            p.insert(QStringLiteral("entryProb"), sig_entry->value());
        }
        if (g == QLatin1String("RPROBX")) {
            p.insert(QStringLiteral("exit_prob"), sig_exit->value());
            p.insert(QStringLiteral("exitProb"), sig_exit->value());
        }
        a.insert(QStringLiteral("params"), p);
    } else if (t == QLatin1String("indicator_sweep")) {
        a.insert(QStringLiteral("indicator"), sweep_ind->currentData().toString());
        QJsonObject range;
        range.insert(QStringLiteral("min"), sweep_min->value());
        range.insert(QStringLiteral("max"), sweep_max->value());
        range.insert(QStringLiteral("step"), sweep_step->value());
        a.insert(QStringLiteral("paramRange"), range);
    }
    return a;
}

QString BtResearchTab::Impl::signature() const {
    if (!host)
        return QString();
    const QString t = current_tool();
    return services::backtest::run_signature(host->engine_slug(), t, request(t));
}

void BtResearchTab::Impl::update_tools() {
    if (!host)
        return;
    auto* m = qobject_cast<QStandardItemModel*>(tool->model());
    const QString engine = host->engine_label();
    for (int i = 0; i < tool->count(); ++i) {
        const QString t = tool->itemData(i).toString();
        const bool offered = host->engine_offers(t);
        if (m && m->item(i))
            m->item(i)->setEnabled(offered);
        tool->setItemData(i, offered ? res_tool_text(t) : BtResearchTab::tr("%1 does not offer this tool").arg(engine),
                          Qt::ToolTipRole);
    }
    const QString t = current_tool();
    const bool offered = host->engine_offers(t);
    const BtSetup s = host->current_setup();
    QString problem;
    if (!offered)
        problem = BtResearchTab::tr("%1 does not offer this tool.").arg(engine);
    else if (s.symbols.isEmpty())
        problem = BtResearchTab::tr("Enter a symbol in the setup first.");
    else if (t == QLatin1String("indicator_sweep") && sweep_min->value() > sweep_max->value())
        problem = BtResearchTab::tr("The sweep's MIN is above its MAX.");
    if (problem.isEmpty()) {
        QString on = s.symbols.first();
        if (s.start.isValid() && s.end.isValid())
            on += QStringLiteral(" · %1 → %2")
                      .arg(s.start.toString(Qt::ISODate), s.end.toString(Qt::ISODate));
        note->setText(BtResearchTab::tr("%1 On %2 (the first symbol of the setup), with %3.")
                          .arg(res_tool_text(t), on, engine));
        set_tone(note, Tone::Dim);
    } else {
        note->setText(problem);
        set_tone(note, Tone::Warn);
    }
    const bool running = pane && pane->is_running();
    run_btn->setEnabled(running || problem.isEmpty());
    run_btn->setToolTip(problem);
    if (pane)
        pane->preview()->set_primary_enabled(problem.isEmpty());
}

void BtResearchTab::Impl::apply_view() {
    if (!host)
        return;
    const QString t = current_tool();
    const QString key = host->engine_slug() + QLatin1Char('/') + t;
    const bool offered = host->engine_offers(t);
    const QString sig = signature();
    if (key != view_key) {
        view_key = key;
        if (!offered) {
            pane->set_view_preview(key, ui::EmptyState::no_data_spec(
                                            res_tool_label(t),
                                            BtResearchTab::tr("%1 does not offer this tool. Pick another engine in the "
                                                              "setup (VectorBT offers all eight).")
                                                .arg(host->engine_label())));
        } else if (t == QLatin1String("indicator_sweep")) {
            pane->set_view_preview(
                key, ui::EmptyState::preview_spec(
                         res_tool_label(t),
                         BtResearchTab::tr("Runs the indicator for every period from MIN to MAX and compares the "
                                           "results of each period in one chart and table."),
                         BtResearchTab::tr("RUN SWEEP"), BtResearchTab::tr("One pass per period: up to a minute"),
                         ui::EmptyState::Tone::Info, ui::LoadingSkeleton::Shape::Chart));
        }
        const bool manual = !offered || t == QLatin1String("indicator_sweep");
        pane->set_view(key, manual ? ui::AutoLoadPane::Policy::Manual : ui::AutoLoadPane::Policy::AutoRun, sig);
    } else {
        pane->set_signature(sig);
    }
    update_tools();
}

void BtResearchTab::Impl::inputs_changed() {
    if (pane && !view_key.isEmpty())
        pane->set_signature(signature());
    update_tools();
}

void BtResearchTab::Impl::start(quint64 request_id) {
    pane_request = request_id;
    if (token && host && host->controller())
        host->controller()->cancel(token);
    token = 0;
    if (!host || !host->controller()) {
        pane->fail(request_id, BtResearchTab::tr("The run controller is not available."));
        return;
    }
    const QString t = current_tool();
    const QString engine = host->engine_slug();
    if (!host->engine_offers(t)) {
        pane->fail(request_id, BtResearchTab::tr("%1 does not offer this tool.").arg(host->engine_label()),
                   BtResearchTab::tr("Not available"));
        return;
    }
    const QJsonObject req = request(t);
    if (req.value(QStringLiteral("symbols")).toArray().isEmpty()) {
        pane->fail(request_id, BtResearchTab::tr("Enter a symbol in the setup first."),
                   bt_failure_title(QStringLiteral("INPUT")));
        return;
    }
    const QString sig = services::backtest::run_signature(engine, t, req);
    pane->set_signature(sig);
    QString why;
    token = host->controller()->start(BtKind::Research, engine, t, req, sig, host->setup_currency(), &why);
    if (!token) {
        pane->fail(request_id, why.isEmpty() ? BtResearchTab::tr("The run queue is full.") : why,
                   BtResearchTab::tr("Queue full"));
        return;
    }
    run_tool = t;
    run_engine = engine;
    pane->set_loading_text(BtResearchTab::tr("Running %1 on %2…").arg(res_tool_label(t), host->engine_label()));
}

void BtResearchTab::Impl::render(const QJsonObject& payload) {
    const QString t = payload.value(QStringLiteral("tool")).toString(current_tool());
    const QJsonObject data = payload.value(QStringLiteral("data")).toObject();
    result_ccy = payload.value(QStringLiteral("currency")).toString();
    if (result_ccy.isEmpty() && host)
        result_ccy = host->setup_currency();

    // KPI row: the payload's top-level numbers.
    kpis->clear();
    int shown = 0;
    static const QStringList skip = {QStringLiteral("success"), QStringLiteral("schemaVersion"),
                                     QStringLiteral("durationMs"), QStringLiteral("elapsedSec")};
    for (auto it = data.constBegin(); it != data.constEnd() && shown < kResMaxKpis; ++it) {
        if (!it.value().isDouble() || skip.contains(it.key()))
            continue;
        ui::StatTile* tile = kpis->add_tile(it.key(), btview::humanize_key(it.key()).toUpper());
        tile->set_value(res_number_text(it.value().toDouble(), it.key(), result_ccy));
        tile->set_tooltip_text(it.key());
        ++shown;
    }
    kpis->setVisible(shown > 0);

    // The tool's chart (built per result by the shared builder).
    if (chart) {
        chart->hide();
        chart->deleteLater();
    }
    chart = btview::make_command_chart(chart_panel.body_widget, t, data, btview::FmtCtx{result_ccy});
    if (chart) {
        chart_panel.body->insertWidget(0, chart, 1);
        chart_note->clear();
        chart_note->setVisible(false);
    } else {
        chart_note->setText(BtResearchTab::tr("%1 has no chart: its result is the table below.").arg(res_tool_label(t)));
        chart_note->setVisible(true);
    }
    chart_panel.header->set_title(res_tool_label(t).toUpper());
    chart_panel.header->set_badge(payload.value(QStringLiteral("engine")).toString());

    // Rows: the first array of objects in the payload.
    QJsonArray arr;
    QString arr_key;
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        if (!it.value().isArray())
            continue;
        const QJsonArray a = it.value().toArray();
        if (!a.isEmpty() && a.first().isObject()) {
            arr = a;
            arr_key = it.key();
            break;
        }
    }
    columns.clear();
    static const QStringList first_keys = {QStringLiteral("date"),   QStringLiteral("datetime"), QStringLiteral("timestamp"),
                                           QStringLiteral("time"),   QStringLiteral("index"),    QStringLiteral("window"),
                                           QStringLiteral("fold"),   QStringLiteral("split"),    QStringLiteral("period")};
    QStringList seen;
    const int probe = std::min(50, static_cast<int>(arr.size()));
    for (int i = 0; i < probe; ++i) {
        const QJsonObject o = arr.at(i).toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            if (!seen.contains(it.key()))
                seen << it.key();
    }
    for (const QString& k : first_keys)
        if (seen.contains(k))
            columns << k;
    for (const QString& k : seen)
        if (!columns.contains(k))
            columns << k;
    if (columns.size() > kResMaxColumns)
        columns = columns.mid(0, kResMaxColumns);
    QStringList headers;
    for (const QString& k : columns)
        headers << btview::humanize_key(k).toUpper();
    QVector<BtResRow> rows;
    const int n = std::min(kResMaxRows, static_cast<int>(arr.size()));
    rows.reserve(n);
    for (int i = 0; i < n; ++i)
        rows.append({i, arr.at(i).toObject()});
    rows_model->clear();
    rows_model->set_headers(headers);
    rows_model->upsert(rows);
    QVector<int> chars;
    for (const QString& k : columns)
        chars << std::clamp(static_cast<int>(k.size()) + 3, 9, 18);
    set_column_chars(rows_view, chars);
    rows_panel.header->set_title(arr_key.isEmpty() ? BtResearchTab::tr("ROWS")
                                                   : btview::humanize_key(arr_key).toUpper());
    rows_panel.header->set_badge(arr.isEmpty() ? QString() : count(static_cast<double>(arr.size())));
    rows_view->setVisible(!arr.isEmpty());
    if (arr.isEmpty())
        rows_note->setText(BtResearchTab::tr("This result has no row list."));
    else if (arr.size() > kResMaxRows)
        rows_note->setText(BtResearchTab::tr("Showing the first %1 of %2 rows (right-click: CSV of the shown rows).")
                               .arg(count(kResMaxRows), count(static_cast<double>(arr.size()))));
    else if (static_cast<int>(seen.size()) > kResMaxColumns)
        rows_note->setText(BtResearchTab::tr("Showing %1 of %2 fields.").arg(kResMaxColumns).arg(seen.size()));
    else
        rows_note->clear();
    rows_note->setVisible(!rows_note->text().isEmpty());
}

void BtResearchTab::refresh() {
    if (!d->host)
        return;
    d->request_indicators();
    d->update_fields();
    d->apply_view();
}

void BtResearchTab::run() {
    if (!d->host)
        return;
    if (is_running())
        return;
    const QString t = d->current_tool();
    if (!d->host->engine_offers(t)) {
        d->host->notify(tr("%1 does not offer this tool.").arg(d->host->engine_label()));
        return;
    }
    d->pane->run();
}

void BtResearchTab::cancel() {
    if (is_running())
        d->pane->cancel();
}

bool BtResearchTab::is_running() const {
    return d->pane && d->pane->is_running();
}

} // namespace fincept::screens::bt
