// src/screens/backtesting/BtCompareView.cpp
//
// COMPARE view (BtAnalysisViews.h): the runs as chips (colour swatch = the run's chart colour,
// a menu with SET AS BASELINE / OPEN / REMOVE, a warning chip for another currency or period),
// EQUITY REBASED TO 100 over the common date range, the METRICS table (value and delta vs the
// baseline, best per row in bold) and WHAT DIFFERS (only the request inputs that differ).
// Fewer than two runs: an EmptyState whose action emits add_requested().
#include "screens/backtesting/BtAnalysisViews.h"

#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/PanelHeader.h"

#include <QAction>
#include <QColor>
#include <QDateTime>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableView>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::bt {

/// One metric row of METRICS (values per run, in run order).
struct BtCmpMetricRow {
    QString key;
    QString label;
    Unit unit = Unit::Ratio;
    bool direction = false; ///< a return: P&L direction colours
    int baseline = 0;
    QVector<double> values;
    QVector<bool> best;
    QStringList whys;
    QStringList ccys;
    QVector<bool> same_ccy; ///< same currency as the baseline (money deltas)
};

/// One input row of WHAT DIFFERS (text per run).
struct BtCmpDiffRow {
    QString key;
    QString label;
    int baseline = 0;
    QStringList values;
};

namespace {

constexpr int kCmpMaxRuns = 6;
constexpr double kCmpDayMs = 86400000.0;
using CmpMetricModel = ui::LiveTableModel<BtCmpMetricRow>;
using CmpDiffModel = ui::LiveTableModel<BtCmpDiffRow>;

QString cmp_value_text(double v, Unit unit, const QString& ccy) {
    switch (unit) {
        case Unit::SignedFraction:
            return pct(v, 2, true);
        case Unit::Fraction:
            return pct(v, 2);
        case Unit::Count:
            return count(v);
        case Unit::Money:
        case Unit::SignedMoney:
            return money_compact(v, ccy);
        case Unit::Ratio:
        case Unit::PctPoints:
        case Unit::Bars:
        case Unit::Plain:
            break;
    }
    return ratio(v, 2);
}

QString cmp_delta_text(double d, Unit unit, const QString& ccy) {
    if (!std::isfinite(d))
        return QString();
    QString arrow = d > 0 ? QStringLiteral("▲ ") : (d < 0 ? QStringLiteral("▼ ") : QStringLiteral("= "));
    switch (unit) {
        case Unit::SignedFraction:
        case Unit::Fraction:
        case Unit::PctPoints:
            return arrow + pp(d, 1);
        case Unit::Count:
            return arrow + ui::formatting::signed_number(std::round(d), 0);
        case Unit::Money:
        case Unit::SignedMoney:
            return arrow + money_compact(d, ccy, true);
        case Unit::Ratio:
        case Unit::Bars:
        case Unit::Plain:
            break;
    }
    return arrow + ui::formatting::signed_number(d, 2);
}

QDate cmp_date(double ms) {
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ms), QTimeZone::utc()).date();
}

QString cmp_period_text(double a, double b) {
    if (!std::isfinite(a) || !std::isfinite(b))
        return dash();
    const QDate da = cmp_date(a);
    const QDate db = cmp_date(b);
    return da.year() == db.year() ? QString::number(da.year())
                                  : QStringLiteral("%1–%2").arg(da.year()).arg(db.year());
}

QString cmp_iso(double ms) {
    return std::isfinite(ms) ? cmp_date(ms).toString(Qt::ISODate) : dash();
}

/// "fillTiming" -> "Fill timing".
QString cmp_humanize(const QString& key) {
    QString out;
    for (int i = 0; i < key.size(); ++i) {
        const QChar c = key.at(i);
        if (c == QLatin1Char('_')) {
            out += QLatin1Char(' ');
            continue;
        }
        if (c.isUpper() && i > 0 && !key.at(i - 1).isUpper())
            out += QLatin1Char(' ');
        out += out.isEmpty() ? c.toUpper() : c.toLower();
    }
    return out;
}

QString cmp_known_label(const QString& key) {
    struct L {
        const char* key;
        const char* label;
    };
    static const L labels[] = {
        {"engine", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Engine")},
        {"strategy", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Strategy")},
        {"symbols", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Symbols")},
        {"period", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Period")},
        {"interval", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Interval")},
        {"initialCapital", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Initial capital")},
        {"commission", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Commission")},
        {"slippage", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Slippage")},
        {"costModel", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Cost model")},
        {"fillTiming", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Fill timing")},
        {"positionSizing", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Position sizing")},
        {"positionSizeValue", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Position size")},
        {"sizing", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Sizing")},
        {"leverage", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Leverage")},
        {"allowShort", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Short selling")},
        {"benchmarkSymbol", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Benchmark")},
        {"benchmark", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Benchmark")},
        {"riskFreeRate", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Risk-free rate")},
        {"stopLoss", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Stop-loss")},
        {"takeProfit", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Take-profit")},
        {"trailingStop", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Trailing stop")},
        {"weights", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Weights")},
        {"allowSyntheticData", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Synthetic data allowed")},
        {"currency", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Currency")},
    };
    for (const L& l : labels)
        if (key == QLatin1String(l.key))
            return BtCompareView::tr(l.label);
    return cmp_humanize(key);
}

/// Readable label of a flattened input path.
QString cmp_label(const QString& path) {
    const QStringList parts = path.split(QLatin1Char('.'));
    if (parts.size() >= 2 && (parts.at(parts.size() - 2) == QLatin1String("params") ||
                              parts.at(parts.size() - 2) == QLatin1String("parameters")))
        return BtCompareView::tr("Param · %1").arg(parts.last());
    QStringList out;
    for (int i = 0; i < parts.size(); ++i)
        out << (i == 0 ? cmp_known_label(parts.at(i)) : cmp_humanize(parts.at(i)).toLower());
    return out.join(QStringLiteral(" · "));
}

/// Row order: engine, strategy and its parameters, universe and period, costs and fills, the rest.
int cmp_priority(const QString& path) {
    const QString head = path.section(QLatin1Char('.'), 0, 0);
    static const char* const order[] = {"engine",       "strategy",  "params",          "parameters",
                                        "symbols",      "period",    "interval",        "initialCapital",
                                        "costModel",    "commission", "slippage",       "fillTiming",
                                        "positionSizing", "positionSizeValue", "sizing", "leverage",
                                        "allowShort",   "benchmarkSymbol", "benchmark", "riskFreeRate",
                                        "stopLoss",     "takeProfit"};
    for (int i = 0; i < static_cast<int>(sizeof(order) / sizeof(order[0])); ++i)
        if (head == QLatin1String(order[i]))
            return i;
    return 100;
}

bool cmp_is_fraction_key(const QString& leaf) {
    return leaf == QLatin1String("commission") || leaf == QLatin1String("slippage") ||
           leaf == QLatin1String("riskFreeRate") || leaf == QLatin1String("stopLoss") ||
           leaf == QLatin1String("takeProfit") || leaf == QLatin1String("trailingStop");
}

QString cmp_fraction_text(double f) {
    int d = 0;
    double scaled = f * 100.0;
    while (d < 4 && std::fabs(scaled - std::round(scaled)) > 1e-7 * std::max(1.0, std::fabs(scaled))) {
        ++d;
        scaled *= 10.0;
    }
    return pct(f, d);
}

void cmp_flatten(const QJsonValue& v, const QString& path, const QString& ccy, QMap<QString, QString>* out) {
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        if (o.isEmpty()) {
            if (!path.isEmpty())
                out->insert(path, dash());
            return;
        }
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            if (it.key().startsWith(QLatin1Char('_')))
                continue;
            cmp_flatten(it.value(), path.isEmpty() ? it.key() : path + QLatin1Char('.') + it.key(), ccy, out);
        }
        return;
    }
    if (path.isEmpty())
        return;
    const QString leaf = path.section(QLatin1Char('.'), -1);
    if (v.isDouble() && cmp_is_fraction_key(leaf)) {
        out->insert(path, cmp_fraction_text(v.toDouble()));
        return;
    }
    if (v.isDouble() && leaf == QLatin1String("initialCapital")) {
        out->insert(path, money(v.toDouble(), ccy));
        return;
    }
    if (v.isArray()) {
        bool scalars = true;
        for (const QJsonValue& x : v.toArray())
            scalars = scalars && !x.isObject() && !x.isArray();
        out->insert(path, scalars ? bt_analysis_json_text(v)
                                  : QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact)));
        return;
    }
    out->insert(path, bt_analysis_json_text(v));
}

/// Every comparable input of one run as path -> text.
QMap<QString, QString> cmp_inputs(const BtCompareEntry& e) {
    QMap<QString, QString> out;
    const QString ccy = e.run ? e.run->currency : QString();
    QJsonObject in = e.inputs;
    // Folded / handled separately.
    static const char* const skip[] = {"startDate", "endDate", "requestId", "runId", "command", "label",
                                       "notes",     "asOf",    "provider",  "engine"};
    for (const char* k : skip)
        in.remove(QLatin1String(k));
    QJsonObject strategy = in.value(QStringLiteral("strategy")).toObject();
    if (!strategy.isEmpty()) {
        const QString name = strategy.value(QStringLiteral("name")).toString();
        const QString type = strategy.value(QStringLiteral("type")).toString();
        out.insert(QStringLiteral("strategy"),
                   name.isEmpty() ? (type.isEmpty() ? dash() : type)
                                  : (type.isEmpty() || type == name ? name : QStringLiteral("%1 (%2)").arg(name, type)));
        strategy.remove(QStringLiteral("name"));
        strategy.remove(QStringLiteral("type"));
        strategy.remove(QStringLiteral("category"));
        in.remove(QStringLiteral("strategy"));
        cmp_flatten(strategy, QStringLiteral("strategy"), ccy, &out);
    } else if (e.run && !e.run->provenance.strategy_type.isEmpty()) {
        out.insert(QStringLiteral("strategy"), e.run->provenance.strategy_name.isEmpty()
                                                   ? e.run->provenance.strategy_type
                                                   : QStringLiteral("%1 (%2)").arg(e.run->provenance.strategy_name,
                                                                                   e.run->provenance.strategy_type));
    }
    cmp_flatten(in, QString(), ccy, &out);

    // Engine and period always take part (entry / payload when the request lacks them).
    QString engine = e.engine;
    if (engine.isEmpty() && e.run)
        engine = e.run->engine;
    out.insert(QStringLiteral("engine"), engine.isEmpty() ? dash() : engine);
    const QString sd = e.inputs.value(QStringLiteral("startDate")).toString();
    const QString ed = e.inputs.value(QStringLiteral("endDate")).toString();
    if (!sd.isEmpty() || !ed.isEmpty())
        out.insert(QStringLiteral("period"), QStringLiteral("%1 → %2").arg(date_text(sd), date_text(ed)));
    else if (e.run && e.run->t.size() >= 2)
        out.insert(QStringLiteral("period"), QStringLiteral("%1 → %2").arg(cmp_iso(e.run->t.first()),
                                                                               cmp_iso(e.run->t.last())));
    if (!out.contains(QStringLiteral("symbols")) && e.run && !e.run->symbols().isEmpty())
        out.insert(QStringLiteral("symbols"), e.run->symbols().join(QStringLiteral(", ")));
    return out;
}

struct BtCmpMetricSpec {
    const char* key;
    const char* fallback;
    const char* label;
    Unit unit;
    int better; ///< +1 higher is better, -1 lower is better, 0 no "best"
    bool direction;
};

const BtCmpMetricSpec kCmpMetrics[] = {
    {"performance.totalReturn", nullptr, QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Total return"),
     Unit::SignedFraction, 1, true},
    {"performance.cagr", "performance.annualizedReturn", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "CAGR"),
     Unit::SignedFraction, 1, true},
    {"performance.sharpeRatio", nullptr, QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Sharpe"), Unit::Ratio,
     1, false},
    {"performance.sortinoRatio", nullptr, QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Sortino"),
     Unit::Ratio, 1, false},
    {"performance.calmarRatio", nullptr, QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Calmar"), Unit::Ratio,
     1, false},
    {"performance.maxDrawdown", nullptr, QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Max drawdown"),
     Unit::Fraction, -1, false},
    {"performance.volatility", nullptr, QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Volatility"),
     Unit::Fraction, -1, false},
    {"tradeStats.winRate", "performance.winRate", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Win rate"),
     Unit::Fraction, 1, false},
    {"tradeStats.profitFactor", "performance.profitFactor",
     QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Profit factor"), Unit::Ratio, 1, false},
    {"tradeStats.closedTrades", "performance.totalTrades",
     QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Trades"), Unit::Count, 0, false},
    {"exposure.timeInMarket", "performance.exposureTime",
     QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Exposure (time in market)"), Unit::Fraction, 0, false},
    {"costs.totalCosts", "statistics.totalFees", QT_TRANSLATE_NOOP("fincept::screens::bt::BtCompareView", "Total costs"),
     Unit::Money, -1, false},
};

} // namespace

BtCompareView::BtCompareView(QWidget* parent) : QWidget(parent) {
    build_ui();
    ui::formatting::on_format_changed(this, [this]() {
        if (runs_.size() >= 2)
            render();
    });
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refit(); });
}

BtCompareView::~BtCompareView() = default;

void BtCompareView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    stack_ = new QStackedWidget(this);
    root->addWidget(stack_);

    empty_ = new ui::EmptyState(stack_);
    empty_->apply(ui::EmptyState::no_data_spec(tr("Nothing to compare yet"),
                                               tr("Run a second backtest or tick runs in RUNS."), tr("ADD RUN")));
    connect(empty_, &ui::EmptyState::primary_clicked, this, &BtCompareView::add_requested);
    stack_->addWidget(empty_);

    QVBoxLayout* page = nullptr;
    QScrollArea* scroll = bt_analysis_page(stack_, &page);
    stack_->addWidget(scroll);
    QWidget* body = scroll->widget();

    notices_ = new BtAnalysisNotices(body);
    page->addWidget(notices_);

    // RUNS IN COMPARISON
    chips_panel_ = make_panel(tr("RUNS IN COMPARISON"), body);
    add_btn_ = make_tool_button(tr("+ ADD RUN"), tr("Add a run to the comparison (2 to 6 runs)"), chips_panel_.header);
    add_btn_->setProperty("active", true);
    connect(add_btn_, &QPushButton::clicked, this, &BtCompareView::add_requested);
    chips_panel_.header->add_control(add_btn_);
    chips_box_ = new QWidget(chips_panel_.body_widget);
    auto* grid = new QGridLayout(chips_box_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);
    for (int i = 0; i < kCmpMaxRuns; ++i) {
        ChipSlot s;
        s.unit = new QWidget(chips_box_);
        auto* h = new QHBoxLayout(s.unit);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        s.chip = make_tool_button(QString(), QString(), s.unit);
        s.menu = new QMenu(s.chip);
        QAction* base = s.menu->addAction(tr("Set as baseline"));
        QAction* open = s.menu->addAction(tr("Open (make it the active run)"));
        s.menu->addSeparator();
        QAction* remove = s.menu->addAction(tr("Remove from the comparison"));
        connect(base, &QAction::triggered, this, [this, i]() {
            if (i < runs_.size())
                emit baseline_requested(runs_.at(i).id);
        });
        connect(open, &QAction::triggered, this, [this, i]() {
            if (i < runs_.size())
                emit open_requested(runs_.at(i).id);
        });
        connect(remove, &QAction::triggered, this, [this, i]() {
            if (i < runs_.size())
                emit remove_requested(runs_.at(i).id);
        });
        s.chip->setMenu(s.menu);
        h->addWidget(s.chip);
        s.warn = make_chip(QString(), Tone::Warn, s.unit);
        s.warn->setVisible(false);
        h->addWidget(s.warn);
        s.unit->setVisible(false);
        slots_.append(s);
    }
    chips_box_->installEventFilter(this);
    chips_panel_.body->addWidget(chips_box_);
    chips_panel_.body->addWidget(make_note(tr("Click a run for SET AS BASELINE, OPEN or REMOVE. Deltas in METRICS are "
                                              "against the baseline (★)."),
                                           chips_panel_.body_widget));
    page->addWidget(chips_panel_.frame);

    // EQUITY, REBASED TO 100
    equity_panel_ = make_panel(tr("EQUITY, REBASED TO 100"), body);
    equity_ = make_time_chart(equity_panel_.body_widget, QStringLiteral("compare_equity"));
    equity_->set_value_mode(ui::TimeSeriesChart::ValueMode::Rebased100);
    equity_->set_show_mode_buttons(true);
    equity_->set_show_legend(true);
    bt_analysis_fit_chart(equity_, 17);
    equity_panel_.body->addWidget(equity_, 1);
    equity_note_ = make_note(QString(), equity_panel_.body_widget);
    equity_panel_.body->addWidget(equity_note_);
    page->addWidget(equity_panel_.frame);

    // METRICS
    metrics_panel_ = make_panel(tr("METRICS"), body);
    metrics_view_ = new QTableView(metrics_panel_.body_widget);
    metrics_model_ = new CmpMetricModel(this);
    metrics_model_->set_key([](const BtCmpMetricRow& r) { return r.key; });
    metrics_model_->set_cell([this](const BtCmpMetricRow& r, int col, int role) -> QVariant {
        const int i = col - 1;
        const bool cell = i >= 0 && i < r.values.size();
        const double v = cell ? r.values.at(i) : kBtNaN;
        const double base = r.baseline >= 0 && r.baseline < r.values.size() ? r.values.at(r.baseline) : kBtNaN;
        const bool delta = cell && i != r.baseline && std::isfinite(v) && std::isfinite(base) &&
                           (r.unit != Unit::Money || r.same_ccy.value(i, false));
        if (role == Qt::DisplayRole) {
            if (col == 0)
                return r.label;
            if (!cell)
                return {};
            const QString text = cmp_value_text(v, r.unit, r.ccys.value(i));
            return delta ? QStringLiteral("%1   %2").arg(text, cmp_delta_text(v - base, r.unit, r.ccys.value(i)))
                         : text;
        }
        if (role == Qt::TextAlignmentRole)
            return col == 0 ? QVariant(int(Qt::AlignLeft | Qt::AlignVCenter))
                            : QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
        if (role == Qt::ForegroundRole) {
            if (col == 0)
                return QVariant::fromValue(tone_color(Tone::Dim));
            if (r.direction && cell)
                return QVariant::fromValue(tone_color(sign_tone(v)));
            return {};
        }
        if (role == Qt::FontRole && cell && r.best.value(i, false)) {
            QFont f = metrics_view_->font();
            f.setBold(true);
            return QVariant::fromValue(f);
        }
        if (role == Qt::ToolTipRole && cell) {
            if (!std::isfinite(v))
                return r.whys.value(i).isEmpty() ? tr("Not reported for this run") : r.whys.value(i);
            if (delta)
                return tr("%1 vs the baseline: %2").arg(r.label, cmp_delta_text(v - base, r.unit, r.ccys.value(i)));
            if (i != r.baseline && r.unit == Unit::Money && !r.same_ccy.value(i, false))
                return tr("Another currency than the baseline: no delta");
            return r.best.value(i, false) ? QVariant(tr("Best of the compared runs")) : QVariant();
        }
        if (role == CmpMetricModel::SortRole)
            return col == 0 ? QVariant(r.label) : QVariant(std::isfinite(v) ? v : -1e300);
        return {};
    });
    metrics_view_->setModel(metrics_model_);
    setup_table(metrics_view_, QStringLiteral("compare_metrics"));
    metrics_view_->setSortingEnabled(false);
    metrics_view_->horizontalHeader()->setSortIndicatorShown(false);
    ui::formatting::refresh_model_on_format_change(metrics_model_);
    bt_analysis_fit_table(metrics_view_, static_cast<int>(sizeof(kCmpMetrics) / sizeof(kCmpMetrics[0])));
    metrics_panel_.body->addWidget(metrics_view_, 1);
    metrics_panel_.body->addWidget(make_note(tr("Each run's own period; deltas (▲ / ▼) are against the "
                                                "baseline; bold = best of the runs for that metric."),
                                             metrics_panel_.body_widget));
    page->addWidget(metrics_panel_.frame);

    // WHAT DIFFERS
    diff_panel_ = make_panel(tr("WHAT DIFFERS"), body);
    diff_view_ = new QTableView(diff_panel_.body_widget);
    diff_model_ = new CmpDiffModel(this);
    diff_model_->set_key([](const BtCmpDiffRow& r) { return r.key; });
    diff_model_->set_cell([](const BtCmpDiffRow& r, int col, int role) -> QVariant {
        const int i = col - 1;
        const bool cell = i >= 0 && i < r.values.size();
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole || role == CmpDiffModel::SortRole)
            return col == 0 ? r.label : (cell ? r.values.at(i) : QString());
        if (role == Qt::ForegroundRole) {
            if (col == 0)
                return QVariant::fromValue(tone_color(Tone::Dim));
            if (cell && i != r.baseline && r.values.at(i) != r.values.value(r.baseline))
                return QVariant::fromValue(tone_color(Tone::Accent));
            return {};
        }
        return {};
    });
    diff_view_->setModel(diff_model_);
    setup_table(diff_view_, QStringLiteral("compare_what_differs"));
    diff_view_->setSortingEnabled(false);
    diff_view_->horizontalHeader()->setSortIndicatorShown(false);
    bt_analysis_fit_table(diff_view_, 4);
    diff_panel_.body->addWidget(diff_view_, 1);
    diff_note_ = make_note(QString(), diff_panel_.body_widget);
    diff_panel_.body->addWidget(diff_note_);
    page->addWidget(diff_panel_.frame);

    page->addStretch(1);
    stack_->setCurrentWidget(empty_);
}

int BtCompareView::baseline_index() const {
    for (int i = 0; i < runs_.size(); ++i)
        if (runs_.at(i).baseline)
            return i;
    return 0;
}

void BtCompareView::show_runs(const QVector<BtCompareEntry>& runs) {
    runs_ = runs.mid(0, kCmpMaxRuns);
    baseline_ = baseline_index();
    if (runs_.size() < 2) {
        metrics_model_->clear();
        diff_model_->clear();
        equity_->clear();
        stack_->setCurrentWidget(empty_);
        return;
    }
    QVector<BtAnalysisNotice> n;
    if (runs.size() > kCmpMaxRuns)
        n.append({tr("Only the first %1 of the %2 runs are compared.").arg(kCmpMaxRuns).arg(runs.size()), Tone::Warn});
    int missing = 0;
    for (const BtCompareEntry& e : runs_)
        if (!e.run)
            ++missing;
    if (missing > 0)
        n.append({tr("%1 run(s) could not be loaded and show no data.").arg(missing), Tone::Warn});
    // Currency / period differences against the baseline.
    const BtRunPtr& base = runs_.at(baseline_).run;
    bool other_ccy = false;
    bool other_period = false;
    for (int i = 0; i < runs_.size(); ++i) {
        const BtRunPtr& r = runs_.at(i).run;
        if (i == baseline_ || !r || !base)
            continue;
        if (!r->currency.isEmpty() && !base->currency.isEmpty() && r->currency != base->currency)
            other_ccy = true;
        if (r->t.size() >= 2 && base->t.size() >= 2 &&
            (std::fabs(r->t.first() - base->t.first()) > 3 * kCmpDayMs ||
             std::fabs(r->t.last() - base->t.last()) > 3 * kCmpDayMs))
            other_period = true;
    }
    if (other_ccy)
        n.append({tr("The runs are in different currencies: money values (costs) are not comparable; returns and the "
                     "rebased equity are."),
                  Tone::Warn});
    if (other_period)
        n.append({tr("The runs cover different periods: the chart is cut to the common range, but each run's metrics "
                     "cover its own period."),
                  Tone::Warn});
    notices_->set_notices(n);
    render();
}

void BtCompareView::render() {
    if (runs_.size() < 2) {
        stack_->setCurrentWidget(empty_);
        return;
    }
    if (stack_->currentIndex() != 1)
        stack_->setCurrentIndex(1);
    render_chips();
    render_equity();
    render_metrics();
    render_differs();
}

void BtCompareView::render_chips() {
    const BtRunPtr& base = runs_.at(baseline_).run;
    for (int i = 0; i < slots_.size(); ++i) {
        ChipSlot& s = slots_[i];
        const bool on = i < runs_.size();
        s.unit->setVisible(on);
        if (!on)
            continue;
        const BtCompareEntry& e = runs_.at(i);
        const bool is_base = i == baseline_;
        QString engine = e.engine.isEmpty() && e.run ? e.run->engine : e.engine;
        QString text = e.label.isEmpty() ? tr("Run %1").arg(e.id) : e.label;
        if (!engine.isEmpty())
            text += QStringLiteral(" · ") + engine;
        if (is_base)
            text = QStringLiteral("★ ") + text + QStringLiteral("  ") + tr("(baseline)");
        text.replace(QLatin1Char('&'), QStringLiteral("&&"));
        if (s.chip->text() != text)
            s.chip->setText(text);
        s.chip->setAccessibleName(text);
        QStringList tip;
        tip << (e.label.isEmpty() ? tr("Run %1").arg(e.id) : e.label);
        if (!engine.isEmpty())
            tip << tr("Engine: %1").arg(engine);
        if (!e.as_of.isEmpty())
            tip << tr("As of: %1").arg(e.as_of);
        if (e.run) {
            if (!e.run->currency.isEmpty())
                tip << tr("Currency: %1").arg(e.run->currency);
            if (e.run->t.size() >= 2)
                tip << tr("Period: %1 → %2").arg(cmp_iso(e.run->t.first()), cmp_iso(e.run->t.last()));
        } else {
            tip << tr("Could not be loaded");
        }
        s.chip->setToolTip(tip.join(QLatin1Char('\n')));
        const QList<QAction*> acts = s.menu->actions();
        if (!acts.isEmpty())
            acts.first()->setEnabled(!is_base);

        QStringList warn;
        if (!is_base && e.run && base) {
            if (!e.run->currency.isEmpty() && !base->currency.isEmpty() && e.run->currency != base->currency)
                warn << tr("currency %1").arg(e.run->currency);
            if (e.run->t.size() >= 2 && base->t.size() >= 2 &&
                (std::fabs(e.run->t.first() - base->t.first()) > 3 * kCmpDayMs ||
                 std::fabs(e.run->t.last() - base->t.last()) > 3 * kCmpDayMs)) {
                QString span = cmp_period_text(e.run->t.first(), e.run->t.last());
                if (span == cmp_period_text(base->t.first(), base->t.last()))
                    span = QStringLiteral("%1 – %2").arg(cmp_iso(e.run->t.first()), cmp_iso(e.run->t.last()));
                warn << tr("different period: %1").arg(span);
            }
        }
        if (!e.run)
            warn << tr("not loaded");
        s.warn->setText(QStringLiteral("⚠ ") + warn.join(QStringLiteral(" · ")));
        s.warn->setVisible(!warn.isEmpty());
    }
    chips_panel_.header->set_badge(tr("%1 runs").arg(runs_.size()));
    add_btn_->setEnabled(runs_.size() < kCmpMaxRuns);
    refresh_swatches();
    reflow_chips(true);
}

void BtCompareView::refresh_swatches() {
    for (int i = 0; i < slots_.size() && i < runs_.size(); ++i) {
        QPushButton* chip = slots_.at(i).chip;
        const int sz = std::max(8, QFontMetrics(chip->font()).height() * 2 / 3);
        const qreal dpr = chip->devicePixelRatioF();
        QPixmap pm(QSize(sz, sz) * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(ui::colors::chart_color(i));
        p.drawEllipse(QRectF(1, 1, sz - 2, sz - 2));
        p.end();
        chip->setIcon(QIcon(pm));
        chip->setIconSize(QSize(sz, sz));
    }
}

void BtCompareView::reflow_chips(bool force) {
    auto* grid = qobject_cast<QGridLayout*>(chips_box_->layout());
    if (!grid)
        return;
    const int shown = std::min(static_cast<int>(runs_.size()), static_cast<int>(slots_.size()));
    int unit_w = 1;
    for (int i = 0; i < shown; ++i)
        unit_w = std::max(unit_w, slots_.at(i).unit->sizeHint().width());
    const int sp = grid->horizontalSpacing();
    const int w = std::max(1, chips_box_->width());
    const int cols = std::clamp((w + sp) / (unit_w + sp), 1, std::max(1, shown));
    if (!force && cols == chip_cols_)
        return;
    chip_cols_ = cols;
    for (const ChipSlot& s : slots_)
        grid->removeWidget(s.unit);
    for (int c = 0; c <= kCmpMaxRuns; ++c)
        grid->setColumnStretch(c, 0);
    for (int i = 0; i < shown; ++i)
        grid->addWidget(slots_.at(i).unit, i / cols, i % cols, Qt::AlignLeft | Qt::AlignVCenter);
    grid->setColumnStretch(cols, 1);
}

bool BtCompareView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == chips_box_ && event->type() == QEvent::Resize)
        reflow_chips(false);
    return QWidget::eventFilter(watched, event);
}

void BtCompareView::render_equity() {
    equity_->clear();
    double from = -std::numeric_limits<double>::infinity();
    double to = std::numeric_limits<double>::infinity();
    int added = 0;
    for (int i = 0; i < runs_.size(); ++i) {
        const BtCompareEntry& e = runs_.at(i);
        if (!e.run || e.run->t.size() < 2 || e.run->equity.size() < 2)
            continue;
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = (i == baseline_ ? QStringLiteral("★ ") : QString()) +
                 (e.label.isEmpty() ? tr("Run %1").arg(e.id) : e.label);
        s.points = to_points(e.run->t, e.run->equity);
        s.color = ui::ChartColorRole::Palette;
        s.palette_index = i;
        s.line_width = i == baseline_ ? 2.2 : 1.5;
        s.id = QString::number(e.id);
        if (s.points.size() < 2)
            continue;
        equity_->add_series(s);
        ++added;
        from = std::max(from, s.points.first().x());
        to = std::min(to, s.points.last().x());
    }
    if (added == 0) {
        equity_->set_state(ui::TimeSeriesChart::State::Empty, tr("None of the runs has an equity curve"));
        equity_note_->clear();
        return;
    }
    equity_->set_state(ui::TimeSeriesChart::State::Ready);
    if (std::isfinite(from) && std::isfinite(to) && to > from) {
        equity_->set_x_range(from, to);
        equity_note_->setText(tr("Each curve is rebased to 100 at the start of the common range %1 → %2 (zoom out "
                                 "to see each run's full history). LOG in the chart header for long histories.")
                                  .arg(cmp_iso(from), cmp_iso(to)));
    } else {
        equity_->reset_x_range();
        equity_note_->setText(tr("The runs share no common date range: each curve is rebased at its own start."));
    }
}

void BtCompareView::render_metrics() {
    const int n = static_cast<int>(runs_.size());
    QStringList headers;
    headers << tr("METRIC");
    for (int i = 0; i < n; ++i) {
        const BtCompareEntry& e = runs_.at(i);
        const QString label = e.label.isEmpty() ? tr("Run %1").arg(e.id) : e.label;
        headers << (i == baseline_ ? QStringLiteral("★ ") + label : label);
    }
    const QString base_ccy = runs_.at(baseline_).run ? runs_.at(baseline_).run->currency : QString();
    QVector<BtCmpMetricRow> rows;
    for (const BtCmpMetricSpec& spec : kCmpMetrics) {
        BtCmpMetricRow row;
        row.key = QString::fromLatin1(spec.key);
        row.label = tr(spec.label);
        row.unit = spec.unit;
        row.direction = spec.direction;
        row.baseline = baseline_;
        bool all_same_ccy = true;
        for (int i = 0; i < n; ++i) {
            const BtRunPtr& r = runs_.at(i).run;
            double v = kBtNaN;
            QString why;
            QString ccy;
            if (r) {
                v = r->metric(row.key);
                if (!std::isfinite(v) && spec.fallback)
                    v = r->metric(QString::fromLatin1(spec.fallback));
                why = r->why_blank(row.key);
                ccy = r->currency;
            } else {
                why = tr("This run could not be loaded");
            }
            row.values << v;
            row.whys << why;
            row.ccys << ccy;
            const bool same = ccy == base_ccy;
            row.same_ccy << same;
            all_same_ccy = all_same_ccy && same;
        }
        // Best per row (ties all bold); only when the values are comparable and not all equal.
        row.best = QVector<bool>(n, false);
        if (spec.better != 0 && (spec.unit != Unit::Money || all_same_ccy)) {
            double best = kBtNaN;
            int finite = 0;
            for (double v : row.values) {
                if (!std::isfinite(v))
                    continue;
                ++finite;
                if (!std::isfinite(best) || (spec.better > 0 ? v > best : v < best))
                    best = v;
            }
            bool all_equal = true;
            for (double v : row.values)
                if (std::isfinite(v) && std::fabs(v - best) > 1e-12 * std::max(1.0, std::fabs(best)))
                    all_equal = false;
            if (finite >= 2 && !all_equal)
                for (int i = 0; i < n; ++i)
                    row.best[i] = std::isfinite(row.values.at(i)) &&
                                  std::fabs(row.values.at(i) - best) <= 1e-12 * std::max(1.0, std::fabs(best));
        }
        rows.append(row);
    }
    metrics_model_->clear();
    metrics_model_->set_headers(headers);
    metrics_model_->upsert(rows);
    QVector<int> chars;
    chars << 22;
    for (int i = 0; i < n; ++i)
        chars << 22;
    set_column_chars(metrics_view_, chars);
    metrics_panel_.header->set_badge(tr("%1 runs · baseline %2")
                                         .arg(n)
                                         .arg(runs_.at(baseline_).label.isEmpty()
                                                  ? tr("Run %1").arg(runs_.at(baseline_).id)
                                                  : runs_.at(baseline_).label));
}

void BtCompareView::render_differs() {
    const int n = static_cast<int>(runs_.size());
    QVector<QMap<QString, QString>> inputs;
    QStringList keys;
    bool any_inputs = false;
    for (const BtCompareEntry& e : runs_) {
        any_inputs = any_inputs || !e.inputs.isEmpty();
        inputs.append(cmp_inputs(e));
        for (auto it = inputs.last().constBegin(); it != inputs.last().constEnd(); ++it)
            if (!keys.contains(it.key()))
                keys << it.key();
    }
    QVector<BtCmpDiffRow> rows;
    for (const QString& k : keys) {
        BtCmpDiffRow row;
        row.key = k;
        row.label = cmp_label(k);
        row.baseline = baseline_;
        bool differs = false;
        for (int i = 0; i < n; ++i) {
            row.values << inputs.at(i).value(k, dash());
            if (i > 0 && row.values.at(i) != row.values.first())
                differs = true;
        }
        if (differs)
            rows.append(row);
    }
    std::stable_sort(rows.begin(), rows.end(), [](const BtCmpDiffRow& a, const BtCmpDiffRow& b) {
        const int pa = cmp_priority(a.key);
        const int pb = cmp_priority(b.key);
        if (pa != pb)
            return pa < pb;
        return a.label.localeAwareCompare(b.label) < 0;
    });

    QStringList headers;
    headers << tr("INPUT");
    for (int i = 0; i < n; ++i) {
        const BtCompareEntry& e = runs_.at(i);
        const QString label = e.label.isEmpty() ? tr("Run %1").arg(e.id) : e.label;
        headers << (i == baseline_ ? QStringLiteral("★ ") + label : label);
    }
    diff_model_->clear();
    diff_model_->set_headers(headers);
    diff_model_->upsert(rows);
    QVector<int> chars;
    chars << 22;
    for (int i = 0; i < n; ++i)
        chars << 24;
    set_column_chars(diff_view_, chars);
    bt_analysis_fit_table(diff_view_, std::clamp(static_cast<int>(rows.size()), 1, 14));
    diff_view_->setVisible(!rows.isEmpty());
    diff_panel_.header->set_badge(rows.isEmpty() ? QString() : tr("%1 inputs differ").arg(rows.size()));
    if (rows.isEmpty())
        diff_note_->setText(any_inputs ? tr("No recorded input differs between these runs: the results differ only "
                                            "through data updates or the engine itself.")
                                       : tr("The inputs of these runs were not recorded, so they cannot be compared."));
    else
        diff_note_->setText(tr("Only the inputs that differ are listed; values that differ from the baseline (★) "
                               "are highlighted."));
}

void BtCompareView::refit() {
    // The swatch size and the chip columns follow the font (after the appearance has settled).
    if (runs_.size() < 2)
        return;
    refresh_swatches();
    reflow_chips(true);
}

} // namespace fincept::screens::bt
