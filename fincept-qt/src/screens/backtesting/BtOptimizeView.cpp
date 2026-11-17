// src/screens/backtesting/BtOptimizeView.cpp
//
// OPTIMISE view (BtAnalysisViews.h) and the helpers shared by the four analysis views.
//
// Thresholds of the plain-language readings and of the state colours (one place, used by the
// tiles, the verdict chips and the sentences):
//   Deflated Sharpe ratio (probability that the best set's true Sharpe is above zero once the
//   number of trials is accounted for; Bailey & Lopez de Prado 2014):
//       >= 95 % "very unlikely to be luck" (ok) · 50-95 % "inconclusive" (warn) · < 50 % "likely luck" (danger)
//   PBO (probability of backtest overfitting by combinatorially symmetric cross-validation; Bailey et al. 2015):
//       <= 20 % low (ok) · 20-50 % moderate (warn) · > 50 % the optimisation mostly picked noise (danger)
//   Plateau score (mean objective of the best set's grid neighbours / the best objective):
//       >= 0.8 broad plateau (ok) · 0.5-0.8 slope (warn) · < 0.5 isolated peak (danger)
//   Hold-out: out-of-sample objective below half of the in-sample one -> warn.
//   Parameter sensitivity |correlation with the objective|: >= 0.5 strong · >= 0.2 moderate · else weak.
#include "screens/backtesting/BtAnalysisViews.h"

#include "ui/charts/BarChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/SensitivityTable.h"
#include "ui/widgets/StatTile.h"

#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using services::backtest::BtOptimizePtr;
using services::backtest::BtOptimizeResult;
using services::backtest::BtOptRow;
using services::backtest::BtSurface;
using services::backtest::BtWarning;

// ════════════════════════════════════════════════════════════════════════════
// Shared helpers of the analysis views
// ════════════════════════════════════════════════════════════════════════════

namespace {

void bt_analysis_apply_table_fit(QTableView* view) {
    const int rows = view->property("btFitRows").toInt();
    if (rows <= 0)
        return;
    const int row_h = view->verticalHeader()->defaultSectionSize();
    const int head_h = view->horizontalHeader()->isHidden() ? 0 : view->horizontalHeader()->sizeHint().height();
    const int sb = view->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, view);
    const int h = head_h + rows * row_h + 2 * view->frameWidth() + sb + 2;
    if (view->minimumHeight() != h)
        view->setMinimumHeight(h);
}

void bt_analysis_apply_chart_fit(QWidget* chart) {
    const double lines = chart->property("btFitLines").toDouble();
    if (!(lines > 0))
        return;
    const QFontMetrics fm(ui::fonts::font(0));
    const int h = static_cast<int>(std::lround(lines * fm.height()));
    if (chart->minimumHeight() != h)
        chart->setMinimumHeight(h);
}

} // namespace

BtAnalysisReflow::BtAnalysisReflow(QWidget* parent, int wide_chars)
    : QWidget(parent), wide_chars_(std::max(20, wide_chars)) {
    box_ = new QBoxLayout(QBoxLayout::LeftToRight, this);
    box_->setContentsMargins(0, 0, 0, 0);
    box_->setSpacing(6);
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
                     [this]() { reflow(width(), true); });
}

void BtAnalysisReflow::add(QWidget* w, int stretch) {
    if (!w)
        return;
    box_->addWidget(w, wide_ == 0 ? 0 : stretch);
    stretch_.append(stretch);
}

void BtAnalysisReflow::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    reflow(e->size().width(), false);
}

void BtAnalysisReflow::reflow(int width, bool force) {
    const int avg = std::max(4, fontMetrics().averageCharWidth());
    const int threshold = wide_chars_ * avg;
    const int slack = 3 * avg; // hysteresis: a scroll bar appearing must not flip the layout back
    bool wide = width >= threshold;
    if (!force && wide_ == 1)
        wide = width >= threshold - slack;
    else if (!force && wide_ == 0)
        wide = width >= threshold + slack;
    const int state = wide ? 1 : 0;
    if (state == wide_)
        return;
    wide_ = state;
    box_->setDirection(wide ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    for (int i = 0; i < stretch_.size() && i < box_->count(); ++i)
        box_->setStretch(i, wide ? stretch_.at(i) : 0);
}

BtAnalysisNotices::BtAnalysisNotices(QWidget* parent) : QWidget(parent) {
    box_ = new QVBoxLayout(this);
    box_->setContentsMargins(2, 0, 2, 0);
    box_->setSpacing(2);
    setVisible(false);
}

void BtAnalysisNotices::set_notices(const QVector<BtAnalysisNotice>& notices) {
    while (labels_.size() < notices.size()) {
        auto* l = make_note(QString(), this);
        box_->addWidget(l);
        labels_.append(l);
    }
    for (int i = 0; i < labels_.size(); ++i) {
        const bool on = i < notices.size();
        if (on) {
            const BtAnalysisNotice& n = notices.at(i);
            const bool alert = n.tone == Tone::Warn || n.tone == Tone::Danger;
            const QString text = (alert ? QStringLiteral("⚠ ") : QStringLiteral("• ")) + n.text;
            if (labels_[i]->text() != text)
                labels_[i]->setText(text);
            set_tone(labels_[i], n.tone);
        }
        labels_[i]->setVisible(on);
    }
    setVisible(!notices.isEmpty());
}

QVector<BtAnalysisNotice> bt_analysis_warning_notices(const QVector<BtWarning>& warnings) {
    QVector<BtAnalysisNotice> out;
    for (const BtWarning& w : warnings) {
        if (w.message.trimmed().isEmpty())
            continue;
        BtAnalysisNotice n;
        n.tone = w.severity == QLatin1String("error")  ? Tone::Danger
                 : w.severity == QLatin1String("warn") ? Tone::Warn
                                                       : Tone::Dim;
        n.text = w.symbol.isEmpty() ? w.message : QStringLiteral("%1: %2").arg(w.symbol, w.message);
        out.append(n);
    }
    return out;
}

QScrollArea* bt_analysis_page(QWidget* parent, QVBoxLayout** body) {
    auto* scroll = new QScrollArea(parent);
    scroll->setObjectName(QStringLiteral("btScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    auto* v = new QVBoxLayout(content);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(6);
    scroll->setWidget(content);
    if (body)
        *body = v;
    return scroll;
}

void bt_analysis_chart_menu(QWidget* chart, const QString& export_name,
                            std::function<void(QStringList*, QVector<QStringList>*)> csv) {
    if (!chart)
        return;
    ui::ChartExport::MenuSpec spec;
    spec.suggested_name = export_name;
    if (csv) {
        spec.csv = [csv]() {
            ui::ChartExport::Table t;
            csv(&t.headers, &t.rows);
            return t;
        };
    }
    ui::ChartExport::install_context_menu(chart, spec);
}

void bt_analysis_fit_table(QTableView* view, int rows) {
    if (!view)
        return;
    const bool first = !view->property("btFitRows").isValid();
    view->setProperty("btFitRows", std::max(1, rows));
    bt_analysis_apply_table_fit(view);
    if (first)
        QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, view,
                         [view]() { bt_analysis_apply_table_fit(view); });
}

void bt_analysis_fit_chart(QWidget* chart, double lines) {
    if (!chart)
        return;
    const bool first = !chart->property("btFitLines").isValid();
    chart->setProperty("btFitLines", std::max(3.0, lines));
    bt_analysis_apply_chart_fit(chart);
    if (first)
        QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, chart,
                         [chart]() { bt_analysis_apply_chart_fit(chart); });
}

QString bt_analysis_param_text(double v) {
    if (!std::isfinite(v))
        return dash();
    int d = 0;
    double scaled = v;
    while (d < 4 && std::fabs(scaled - std::round(scaled)) > 1e-7 * std::max(1.0, std::fabs(scaled))) {
        ++d;
        scaled *= 10.0;
    }
    return num(v, d);
}

QString bt_analysis_json_text(const QJsonValue& v) {
    if (v.isDouble())
        return bt_analysis_param_text(v.toDouble());
    if (v.isBool())
        return v.toBool() ? BtOptimizeView::tr("on") : BtOptimizeView::tr("off");
    if (v.isString())
        return v.toString().isEmpty() ? dash() : v.toString();
    if (v.isArray()) {
        QStringList parts;
        for (const QJsonValue& x : v.toArray())
            parts << bt_analysis_json_text(x);
        return parts.isEmpty() ? dash() : parts.join(QStringLiteral(", "));
    }
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return dash();
}

QString bt_analysis_params_text(const QJsonObject& params, const QStringList& order, bool names) {
    QStringList keys;
    for (const QString& k : order)
        if (params.contains(k) && !keys.contains(k))
            keys << k;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        if (!keys.contains(it.key()))
            keys << it.key();
    QStringList parts;
    for (const QString& k : keys) {
        const QString val = bt_analysis_json_text(params.value(k));
        parts << (names ? QStringLiteral("%1 %2").arg(k, val) : val);
    }
    if (parts.isEmpty())
        return dash();
    return parts.join(names ? QStringLiteral(" · ") : QStringLiteral(" / "));
}

// ════════════════════════════════════════════════════════════════════════════
// OPTIMISE
// ════════════════════════════════════════════════════════════════════════════

/// One row of TOP SETS (values copied so the model diff stays correct across results).
struct BtOptTopRow {
    int rank = 0;    ///< 1 = best
    int index = -1;  ///< into BtOptimizeResult::rows
    bool pinned = false;
    Unit obj_unit = Unit::Ratio;
    QStringList ptext;
    QVector<double> pval;
    double objective = kBtNaN;
    double ret = kBtNaN;
    double sharpe = kBtNaN;
    double maxdd = kBtNaN;
    double trades = kBtNaN;
};

/// One metric of the HOLD-OUT comparison.
struct BtOptHoldRow {
    QString key;
    QString label;
    Unit unit = Unit::Ratio;
    double is = kBtNaN;
    double oos = kBtNaN;
};

namespace {

constexpr int kOptTopRows = 50;
using OptTopModel = ui::LiveTableModel<BtOptTopRow>;
using OptHoldModel = ui::LiveTableModel<BtOptHoldRow>;

struct BtOptObjective {
    QString key;      ///< normalised objective id (sharpe, return, ...)
    QString label;    ///< upper case, for tile labels
    QString name;     ///< sentence case
    Unit unit = Unit::Ratio;
    QString perf_key; ///< performance key of the same quantity
    bool negated = false; ///< objective = -performance (max_drawdown)
    bool ratio = true;    ///< a ratio (differences in ratio units) rather than a fraction
};

BtOptObjective opt_objective(const QString& raw) {
    QString k = raw.trimmed().toLower();
    k.replace(QLatin1Char(' '), QLatin1Char('_'));
    if (k == QLatin1String("sharpe_ratio") || k == QLatin1String("sharperatio"))
        k = QStringLiteral("sharpe");
    else if (k == QLatin1String("sortino_ratio"))
        k = QStringLiteral("sortino");
    else if (k == QLatin1String("calmar_ratio"))
        k = QStringLiteral("calmar");
    else if (k == QLatin1String("total_return") || k == QLatin1String("totalreturn") || k == QLatin1String("returns"))
        k = QStringLiteral("return");
    else if (k == QLatin1String("drawdown") || k == QLatin1String("maxdrawdown") || k == QLatin1String("min_drawdown"))
        k = QStringLiteral("max_drawdown");
    else if (k == QLatin1String("profitfactor"))
        k = QStringLiteral("profit_factor");
    else if (k == QLatin1String("winrate"))
        k = QStringLiteral("win_rate");
    else if (k == QLatin1String("omega_ratio"))
        k = QStringLiteral("omega");

    BtOptObjective o;
    o.key = k;
    if (k == QLatin1String("sharpe")) {
        o.label = BtOptimizeView::tr("SHARPE");
        o.name = BtOptimizeView::tr("Sharpe");
        o.perf_key = QStringLiteral("sharpeRatio");
    } else if (k == QLatin1String("sortino")) {
        o.label = BtOptimizeView::tr("SORTINO");
        o.name = BtOptimizeView::tr("Sortino");
        o.perf_key = QStringLiteral("sortinoRatio");
    } else if (k == QLatin1String("calmar")) {
        o.label = BtOptimizeView::tr("CALMAR");
        o.name = BtOptimizeView::tr("Calmar");
        o.perf_key = QStringLiteral("calmarRatio");
    } else if (k == QLatin1String("return")) {
        o.label = BtOptimizeView::tr("RETURN");
        o.name = BtOptimizeView::tr("Total return");
        o.unit = Unit::SignedFraction;
        o.perf_key = QStringLiteral("totalReturn");
        o.ratio = false;
    } else if (k == QLatin1String("cagr")) {
        o.label = BtOptimizeView::tr("CAGR");
        o.name = BtOptimizeView::tr("CAGR");
        o.unit = Unit::SignedFraction;
        o.perf_key = QStringLiteral("cagr");
        o.ratio = false;
    } else if (k == QLatin1String("max_drawdown")) {
        o.label = BtOptimizeView::tr("MAX DD");
        o.name = BtOptimizeView::tr("Max drawdown (negated)");
        o.unit = Unit::SignedFraction;
        o.perf_key = QStringLiteral("maxDrawdown");
        o.negated = true;
        o.ratio = false;
    } else if (k == QLatin1String("omega")) {
        o.label = BtOptimizeView::tr("OMEGA");
        o.name = BtOptimizeView::tr("Omega");
        o.perf_key = QStringLiteral("omegaRatio");
    } else if (k == QLatin1String("profit_factor")) {
        o.label = BtOptimizeView::tr("PROFIT FACTOR");
        o.name = BtOptimizeView::tr("Profit factor");
        o.perf_key = QStringLiteral("profitFactor");
    } else if (k == QLatin1String("win_rate")) {
        o.label = BtOptimizeView::tr("WIN RATE");
        o.name = BtOptimizeView::tr("Win rate");
        o.unit = Unit::Fraction;
        o.perf_key = QStringLiteral("winRate");
        o.ratio = false;
    } else if (k == QLatin1String("sqn")) {
        o.label = BtOptimizeView::tr("SQN");
        o.name = BtOptimizeView::tr("SQN");
    } else {
        o.label = k.isEmpty() ? BtOptimizeView::tr("OBJECTIVE") : raw.trimmed().toUpper();
        o.name = k.isEmpty() ? BtOptimizeView::tr("Objective") : raw.trimmed();
    }
    return o;
}

/// Tile tone rule of an objective: P&L direction colours only for returns; ratios warn below 0.
Rule opt_objective_rule(const BtOptObjective& o) {
    if (o.key == QLatin1String("return") || o.key == QLatin1String("cagr"))
        return Rule::Sign;
    return o.ratio ? Rule::Negative : Rule::None;
}

/// The objective's value in a performance map (hold-out halves).
double opt_objective_in(const QHash<QString, double>& perf, const BtOptObjective& o) {
    if (o.perf_key.isEmpty())
        return kBtNaN;
    const double v = perf.value(o.perf_key, kBtNaN);
    return o.negated ? -v : v;
}

struct BtOptMetric {
    QString key; ///< "objective" or a performance key
    QString label;
    Unit unit = Unit::Ratio;
    bool higher_better = true;
    bool diverging = true;
    bool direction = false; ///< a return (P&L direction colours allowed)
};

QVector<BtOptMetric> opt_metrics(const BtOptimizeResult& r, const BtOptObjective& obj) {
    QVector<BtOptMetric> out;
    out.append({QStringLiteral("objective"), BtOptimizeView::tr("Objective (%1)").arg(obj.name), obj.unit, true, true,
                obj.key == QLatin1String("return") || obj.key == QLatin1String("cagr")});
    const QVector<BtOptMetric> known = {
        {QStringLiteral("totalReturn"), BtOptimizeView::tr("Total return"), Unit::SignedFraction, true, true, true},
        {QStringLiteral("cagr"), BtOptimizeView::tr("CAGR"), Unit::SignedFraction, true, true, true},
        {QStringLiteral("sharpeRatio"), BtOptimizeView::tr("Sharpe"), Unit::Ratio, true, true, false},
        {QStringLiteral("sortinoRatio"), BtOptimizeView::tr("Sortino"), Unit::Ratio, true, true, false},
        {QStringLiteral("calmarRatio"), BtOptimizeView::tr("Calmar"), Unit::Ratio, true, true, false},
        {QStringLiteral("maxDrawdown"), BtOptimizeView::tr("Max drawdown"), Unit::Fraction, false, false, false},
        {QStringLiteral("volatility"), BtOptimizeView::tr("Volatility"), Unit::Fraction, false, false, false},
        {QStringLiteral("winRate"), BtOptimizeView::tr("Win rate"), Unit::Fraction, true, false, false},
        {QStringLiteral("profitFactor"), BtOptimizeView::tr("Profit factor"), Unit::Ratio, true, true, false},
        {QStringLiteral("totalTrades"), BtOptimizeView::tr("Trades"), Unit::Count, true, false, false},
    };
    for (const BtOptMetric& m : known) {
        bool any = false;
        for (const BtOptRow& row : r.rows) {
            if (std::isfinite(row.perf_value(m.key))) {
                any = true;
                break;
            }
        }
        if (any)
            out.append(m);
    }
    return out;
}

std::function<QString(double)> opt_formatter(Unit unit, const QString& ccy) {
    switch (unit) {
        case Unit::SignedFraction:
            return [](double v) { return pct(v, 1, true); };
        case Unit::Fraction:
            return [](double v) { return pct(v, 1, false); };
        case Unit::Count:
            return [](double v) { return count(v); };
        case Unit::Money:
        case Unit::SignedMoney:
            return [ccy](double v) { return money_compact(v, ccy, false); };
        case Unit::Ratio:
        case Unit::PctPoints:
        case Unit::Bars:
        case Unit::Plain:
            break;
    }
    return [](double v) { return ratio(v, 2); };
}

double opt_median(const QVector<QVector<double>>& m) {
    QVector<double> v;
    for (const auto& row : m)
        for (double x : row)
            if (std::isfinite(x))
                v.append(x);
    if (v.isEmpty())
        return kBtNaN;
    std::sort(v.begin(), v.end());
    const int n = static_cast<int>(v.size());
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

double opt_json_number(const QJsonValue& v) {
    if (v.isDouble())
        return v.toDouble();
    if (v.isBool())
        return v.toBool() ? 1.0 : 0.0;
    return kBtNaN;
}

bool opt_same(double a, double b) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= 1e-9 * std::max(1.0, std::fabs(b));
}

/// A signed difference in the unit's own terms ("+0.12", "+3.1 pp", "+4").
QString opt_delta_text(double d, Unit unit) {
    if (!std::isfinite(d))
        return dash();
    switch (unit) {
        case Unit::Fraction:
        case Unit::SignedFraction:
        case Unit::PctPoints:
            return pp(d, 1);
        case Unit::Count:
            return ui::formatting::signed_number(std::round(d), 0);
        case Unit::Ratio:
        case Unit::Money:
        case Unit::SignedMoney:
        case Unit::Bars:
        case Unit::Plain:
            break;
    }
    return ui::formatting::signed_number(d, 2);
}

QString opt_arrow(double d) {
    if (!std::isfinite(d) || d == 0)
        return QString();
    return d > 0 ? QStringLiteral("▲ ") : QStringLiteral("▼ ");
}

/// ok (>= good) / warn / danger (< bad) for "higher is safer" state values.
Tone opt_state_tone(double v, double good, double bad) {
    if (!std::isfinite(v))
        return Tone::Dim;
    return v >= good ? Tone::Ok : (v >= bad ? Tone::Warn : Tone::Danger);
}

Tone opt_pbo_tone(double pbo) {
    if (!std::isfinite(pbo))
        return Tone::Dim;
    return pbo <= 0.2 ? Tone::Ok : (pbo <= 0.5 ? Tone::Warn : Tone::Danger);
}

void opt_blank_tile(ui::StatTile* tile, const QString& reason) {
    set_tile(tile, kBtNaN, Unit::Plain, Rule::None, QString(), reason);
    tile->set_sub_label(reason);
}

void opt_text_tile(ui::StatTile* tile, const QString& value, ui::StatTile::Tone tone, const QString& sub,
                   const QString& tip = QString()) {
    tile->set_state(ui::StatTile::State::Normal);
    tile->set_value(value);
    tile->set_tone(tone);
    tile->set_sub_label(sub);
    if (!tip.isEmpty())
        tile->set_tooltip_text(tip);
}

QLabel* opt_field_label(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("btKvKey"));
    return l;
}

} // namespace

BtOptimizeView::BtOptimizeView(QWidget* parent) : QWidget(parent) {
    build_ui();
    ui::formatting::on_format_changed(this, [this]() {
        if (result_ && !result_->rows.isEmpty())
            render(false);
    });
}

BtOptimizeView::~BtOptimizeView() = default;

void BtOptimizeView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    stack_ = new QStackedWidget(this);
    root->addWidget(stack_);

    empty_ = new ui::EmptyState(stack_);
    empty_->apply(ui::EmptyState::no_data_spec(
        tr("No optimisation yet"),
        tr("Set the parameter ranges above and run the optimisation: the best sets, the objective surface and the "
           "overfitting checks appear here.")));
    stack_->addWidget(empty_);

    QVBoxLayout* page = nullptr;
    QScrollArea* scroll = bt_analysis_page(stack_, &page);
    stack_->addWidget(scroll);
    QWidget* body = scroll->widget();

    notices_ = new BtAnalysisNotices(body);
    page->addWidget(notices_);

    // ── KPI row ──
    kpis_ = new ui::KpiStrip(body);
    t_best_ = kpis_->add_tile(QStringLiteral("best"), tr("BEST OBJECTIVE"));
    t_set_ = kpis_->add_tile(QStringLiteral("set"), tr("BEST SET"));
    t_runs_ = kpis_->add_tile(QStringLiteral("runs"), tr("RUNS"));
    t_dsr_ = kpis_->add_tile(QStringLiteral("dsr"), tr("DEFLATED SHARPE"));
    t_emax_ = kpis_->add_tile(QStringLiteral("emax"), tr("EXP. MAX SHARPE (NULL)"));
    t_pbo_ = kpis_->add_tile(QStringLiteral("pbo"), tr("PBO"));
    t_hold_ = kpis_->add_tile(QStringLiteral("hold"), tr("HOLD-OUT OBJECTIVE"));
    t_drop_ = kpis_->add_tile(QStringLiteral("drop"), tr("IS → OOS DROP"));
    t_dsr_->set_tooltip_text(tr("Deflated Sharpe ratio: the probability that the best set's true Sharpe is above zero "
                                "once the number of trials is accounted for. 95 % or more = very unlikely to be luck."));
    t_emax_->set_tooltip_text(tr("The highest Sharpe ratio expected from the same number of strategies with no skill "
                                 "at all. The best set has to beat it to mean anything."));
    t_pbo_->set_tooltip_text(tr("Probability of backtest overfitting: the share of train / test splits in which the "
                                "best in-sample set ranked below the median out of sample. Above 50 % = mostly noise."));
    t_hold_->set_tooltip_text(tr("The objective of the in-sample best set on the hold-out period it never saw."));
    t_drop_->set_tooltip_text(tr("Hold-out (out-of-sample) objective minus the in-sample objective of the same set."));
    page->addWidget(kpis_);

    // ── Row 1: SURFACE | OVERFITTING + HOLD-OUT ──
    auto* row1 = new BtAnalysisReflow(body, 118);

    surface_panel_ = make_panel(tr("SURFACE"), row1);
    {
        auto* bar = new QWidget(surface_panel_.body_widget);
        auto* hl = new QHBoxLayout(bar);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);
        auto add_combo = [bar, hl](const QString& label, QComboBox*& combo) {
            hl->addWidget(opt_field_label(label, bar));
            combo = new QComboBox(bar);
            combo->setObjectName(QStringLiteral("btCombo"));
            combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            combo->setAccessibleName(label);
            hl->addWidget(combo);
        };
        add_combo(tr("X"), x_combo_);
        add_combo(tr("Y"), y_combo_);
        add_combo(tr("METRIC"), metric_combo_);
        hl->addStretch(1);
        surface_panel_.body->addWidget(bar);
        x_combo_->setToolTip(tr("Parameter across the columns"));
        y_combo_->setToolTip(tr("Parameter down the rows (the same as X: one bar per value)"));
        metric_combo_->setToolTip(tr("What each cell shows: the best value over the other parameters"));
    }
    surface_stack_ = new QStackedWidget(surface_panel_.body_widget);
    surface_table_ = new ui::SensitivityTable(surface_stack_);
    surface_table_->set_export_name(QStringLiteral("optimisation_surface"));
    surface_stack_->addWidget(surface_table_);
    surface_bars_ = new ui::BarChart(surface_stack_);
    bt_analysis_chart_menu(surface_bars_, QStringLiteral("optimisation_by_parameter"),
                           [this](QStringList* h, QVector<QStringList>* rows) {
                               *h << surface_.x_param << surface_metric_;
                               const auto& cats = surface_bars_->categories();
                               for (int c = 0; c < cats.size(); ++c) {
                                   const double v = surface_.values.isEmpty() || c >= surface_.values[0].size()
                                                        ? kBtNaN
                                                        : surface_.values[0][c];
                                   rows->append({cats.at(c), std::isfinite(v) ? QString::number(v, 'g', 10)
                                                                              : QString()});
                               }
                           });
    surface_stack_->addWidget(surface_bars_);
    surface_none_ = make_note(QString(), surface_stack_);
    surface_none_->setAlignment(Qt::AlignCenter);
    surface_stack_->addWidget(surface_none_);
    surface_panel_.body->addWidget(surface_stack_, 1);
    surface_note_ = make_note(QString(), surface_panel_.body_widget);
    surface_panel_.body->addWidget(surface_note_);
    bt_analysis_fit_chart(surface_bars_, 14);
    connect(surface_table_, &ui::SensitivityTable::cell_clicked, this,
            [this](int r, int c) { select_surface_cell(r, c); });
    connect(surface_bars_, &ui::BarChart::bar_clicked, this, [this](int, int c) { select_surface_cell(0, c); });
    auto on_axis = [this]() {
        if (!loading_ && result_)
            render_surface();
    };
    connect(x_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, on_axis);
    connect(y_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, on_axis);
    connect(metric_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, on_axis);
    row1->add(surface_panel_.frame, 3);

    auto* right = new QWidget(row1);
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(6);
    over_panel_ = make_panel(tr("OVERFITTING"), right);
    over_chip_ = make_chip(QString(), Tone::Dim, over_panel_.header);
    over_panel_.header->add_control(over_chip_);
    over_kv_ = new BtKvGrid(over_panel_.body_widget, 1);
    over_panel_.body->addWidget(over_kv_);
    over_reading_ = make_note(QString(), over_panel_.body_widget, Tone::None);
    over_panel_.body->addWidget(over_reading_);
    over_method_ = make_note(QString(), over_panel_.body_widget, Tone::Dim);
    over_panel_.body->addWidget(over_method_);
    over_panel_.body->addStretch(1);
    rv->addWidget(over_panel_.frame);

    hold_panel_ = make_panel(tr("HOLD-OUT"), right);
    hold_kv_ = new BtKvGrid(hold_panel_.body_widget, 1);
    hold_panel_.body->addWidget(hold_kv_);
    hold_view_ = new QTableView(hold_panel_.body_widget);
    hold_model_ = new OptHoldModel(this);
    hold_model_->set_headers({tr("METRIC"), tr("IN-SAMPLE"), tr("OUT-OF-SAMPLE"), tr("CHANGE")});
    hold_model_->set_key([](const BtOptHoldRow& r) { return r.key; });
    hold_model_->set_cell([](const BtOptHoldRow& r, int col, int role) -> QVariant {
        const bool ret = r.key == QLatin1String("totalReturn") || r.key == QLatin1String("cagr");
        const double d = r.oos - r.is;
        if (role == Qt::DisplayRole) {
            switch (col) {
                case 0:
                    return r.label;
                case 1:
                    return format_value(r.is, r.unit, QString());
                case 2:
                    return format_value(r.oos, r.unit, QString());
                case 3:
                    return std::isfinite(d) ? opt_arrow(d) + opt_delta_text(d, r.unit) : dash();
                default:
                    return {};
            }
        }
        if (role == Qt::TextAlignmentRole)
            return col == 0 ? QVariant(int(Qt::AlignLeft | Qt::AlignVCenter))
                            : QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
        if (role == Qt::ForegroundRole) {
            if (ret && (col == 1 || col == 2))
                return QVariant::fromValue(tone_color(sign_tone(col == 1 ? r.is : r.oos)));
            if (r.key == QLatin1String("maxDrawdown") && col == 2 && std::isfinite(r.oos) && r.oos >= 0.25)
                return QVariant::fromValue(tone_color(Tone::Warn));
            if (col == 0)
                return QVariant::fromValue(tone_color(Tone::Dim));
            return {};
        }
        if (role == OptHoldModel::SortRole) {
            switch (col) {
                case 0:
                    return r.label;
                case 1:
                    return r.is;
                case 2:
                    return r.oos;
                case 3:
                    return d;
                default:
                    return {};
            }
        }
        return {};
    });
    hold_view_->setModel(hold_model_);
    setup_table(hold_view_, QStringLiteral("optimisation_holdout"));
    hold_view_->setSortingEnabled(false);
    hold_view_->horizontalHeader()->setSortIndicatorShown(false);
    set_column_chars(hold_view_, {14, 11, 13, 11});
    ui::formatting::refresh_model_on_format_change(hold_model_);
    hold_panel_.body->addWidget(hold_view_);
    hold_note_ = make_note(QString(), hold_panel_.body_widget);
    hold_panel_.body->addWidget(hold_note_);
    hold_panel_.body->addStretch(1);
    rv->addWidget(hold_panel_.frame, 1);
    row1->add(right, 2);
    page->addWidget(row1);

    // ── TOP SETS ──
    top_panel_ = make_panel(tr("TOP SETS"), body);
    apply_btn_ = make_tool_button(tr("APPLY"), tr("Copy the selected parameter set into the strategy setup (Enter)"),
                                  top_panel_.header);
    run_btn_ = make_tool_button(tr("RUN"), tr("Backtest the selected parameter set; it becomes the active run"),
                                top_panel_.header);
    run_btn_->setProperty("active", true);
    top_panel_.header->add_control(apply_btn_);
    top_panel_.header->add_control(run_btn_);
    top_view_ = new QTableView(top_panel_.body_widget);
    top_model_ = new OptTopModel(this);
    top_model_->set_key([](const BtOptTopRow& r) { return QString::number(r.rank); });
    top_model_->set_cell([](const BtOptTopRow& r, int col, int role) -> QVariant {
        const int p = static_cast<int>(r.ptext.size());
        const int c_obj = p + 1;
        if (role == Qt::DisplayRole) {
            if (col == 0)
                return QString::number(r.rank);
            if (col >= 1 && col <= p)
                return r.ptext.at(col - 1);
            if (col == c_obj)
                return format_value(r.objective, r.obj_unit, QString());
            if (col == p + 2)
                return pct(r.ret, 1, true);
            if (col == p + 3)
                return ratio(r.sharpe, 2);
            if (col == p + 4)
                return pct(r.maxdd, 1);
            if (col == p + 5)
                return count(r.trades);
            return {};
        }
        if (role == Qt::EditRole && col == c_obj)
            return r.objective;
        if (role == OptTopModel::SortRole) {
            if (col == 0)
                return r.rank;
            if (col >= 1 && col <= p)
                return std::isfinite(r.pval.at(col - 1)) ? QVariant(r.pval.at(col - 1)) : QVariant(r.ptext.at(col - 1));
            const double v = col == c_obj ? r.objective
                             : col == p + 2 ? r.ret
                             : col == p + 3 ? r.sharpe
                             : col == p + 4 ? r.maxdd
                             : col == p + 5 ? r.trades
                                            : kBtNaN;
            return std::isfinite(v) ? v : -1e300;
        }
        if (role == Qt::TextAlignmentRole)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        if (role == Qt::ForegroundRole) {
            if (col == 0 && r.pinned)
                return QVariant::fromValue(tone_color(Tone::Accent));
            if (col == p + 2)
                return QVariant::fromValue(tone_color(sign_tone(r.ret)));
            if (col == p + 4 && std::isfinite(r.maxdd) && r.maxdd >= 0.25)
                return QVariant::fromValue(tone_color(Tone::Warn));
            return {};
        }
        if (role == Qt::ToolTipRole && col == 0 && r.pinned)
            return BtOptimizeView::tr("Outside the top %1: shown because it was picked on the surface").arg(kOptTopRows);
        return {};
    });
    top_proxy_ = make_sort_proxy(top_model_, this);
    top_view_->setModel(top_proxy_);
    setup_table(top_view_, QStringLiteral("optimisation_top_sets"));
    top_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    top_view_->sortByColumn(0, Qt::AscendingOrder);
    top_bar_ = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, top_view_);
    ui::formatting::refresh_model_on_format_change(top_model_);
    bt_analysis_fit_table(top_view_, 12);
    top_panel_.body->addWidget(top_view_, 1);
    top_note_ = make_note(tr("Select a set: APPLY (Enter) copies it into the strategy setup, RUN backtests it as the "
                             "active run. Clicking a surface cell selects its set here."),
                          top_panel_.body_widget);
    top_panel_.body->addWidget(top_note_);
    connect(top_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { update_actions(); });
    connect(apply_btn_, &QPushButton::clicked, this, [this]() { act_on_selection(false); });
    connect(run_btn_, &QPushButton::clicked, this, [this]() { act_on_selection(true); });
    for (const Qt::Key key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto* sc = new QShortcut(QKeySequence(QKeyCombination(key)), top_view_);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, [this]() { act_on_selection(false); });
    }
    page->addWidget(top_panel_.frame);

    // ── STABILITY ──
    stab_panel_ = make_panel(tr("STABILITY"), body);
    stab_chip_ = make_chip(QString(), Tone::Dim, stab_panel_.header);
    stab_panel_.header->add_control(stab_chip_);
    auto* srow = new BtAnalysisReflow(stab_panel_.body_widget, 100);
    auto* left = new QWidget(srow);
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(4);
    auto* spread_title = new QLabel(tr("PARAMETER SPREAD (OBJECTIVE RANGE ACROSS EACH PARAMETER'S VALUES)"), left);
    spread_title->setObjectName(QStringLiteral("btSectionTitle"));
    spread_title->setWordWrap(true);
    lv->addWidget(spread_title);
    spread_chart_ = new ui::BarChart(left);
    spread_chart_->set_orientation(Qt::Horizontal);
    spread_chart_->set_show_values(true);
    bt_analysis_chart_menu(spread_chart_, QStringLiteral("optimisation_parameter_spread"),
                           [this](QStringList* h, QVector<QStringList>* rows) {
                               *h << tr("Parameter") << tr("Objective spread") << tr("Correlation");
                               if (!result_)
                                   return;
                               for (const QString& p : spread_chart_->categories())
                                   rows->append({p, QString::number(result_->spread.value(p, kBtNaN), 'g', 10),
                                                 QString::number(result_->sensitivity.value(p, kBtNaN), 'g', 10)});
                           });
    bt_analysis_fit_chart(spread_chart_, 7);
    lv->addWidget(spread_chart_, 1);
    srow->add(left, 3);

    auto* rightc = new QWidget(srow);
    auto* rcv = new QVBoxLayout(rightc);
    rcv->setContentsMargins(0, 0, 0, 0);
    rcv->setSpacing(4);
    auto* plateau_title = new QLabel(tr("PLATEAU SCORE"), rightc);
    plateau_title->setObjectName(QStringLiteral("btSectionTitle"));
    rcv->addWidget(plateau_title);
    plateau_hero_ = new QLabel(rightc);
    plateau_hero_->setObjectName(QStringLiteral("btHero"));
    rcv->addWidget(plateau_hero_);
    plateau_text_ = make_note(QString(), rightc, Tone::None);
    rcv->addWidget(plateau_text_);
    auto* sens_title = new QLabel(tr("SENSITIVITY (CORRELATION WITH THE OBJECTIVE)"), rightc);
    sens_title->setObjectName(QStringLiteral("btSectionTitle"));
    sens_title->setWordWrap(true);
    rcv->addWidget(sens_title);
    sens_kv_ = new BtKvGrid(rightc, 1);
    rcv->addWidget(sens_kv_);
    rcv->addStretch(1);
    srow->add(rightc, 2);
    stab_panel_.body->addWidget(srow);
    page->addWidget(stab_panel_.frame);

    page->addStretch(1);
    stack_->setCurrentWidget(empty_);
    update_actions();
}

void BtOptimizeView::show_result(const BtOptimizePtr& result, const QString& ccy, const QString& engine_label) {
    const bool fresh = result.get() != result_.get();
    result_ = result;
    ccy_ = ccy;
    engine_ = engine_label.trimmed().isEmpty() ? tr("this engine") : engine_label.trimmed();
    if (!result_) {
        top_model_->clear();
        hold_model_->clear();
        empty_->apply(ui::EmptyState::no_data_spec(
            tr("No optimisation yet"),
            tr("Set the parameter ranges above and run the optimisation: the best sets, the objective surface and "
               "the overfitting checks appear here.")));
        stack_->setCurrentWidget(empty_);
        update_actions();
        return;
    }
    if (result_->rows.isEmpty()) {
        top_model_->clear();
        hold_model_->clear();
        QString why = tr("The engine returned no evaluated parameter set.");
        if (!result_->warnings.isEmpty())
            why += QLatin1Char(' ') + result_->warnings.first().message;
        empty_->apply(ui::EmptyState::no_data_spec(tr("No parameter set was evaluated"), why));
        stack_->setCurrentWidget(empty_);
        update_actions();
        return;
    }
    if (fresh)
        load_axes();
    render(fresh || top_model_->rowCount() == 0);
    if (fresh)
        select_result_row(0);
}

void BtOptimizeView::load_axes() {
    const BtOptimizeResult& r = *result_;
    axis_params_.clear();
    for (const QString& p : r.param_names)
        if (r.param_grid.value(p).size() >= 2 && !axis_params_.contains(p))
            axis_params_ << p;
    table_params_ = r.param_names;
    if (table_params_.isEmpty() && !r.rows.isEmpty())
        table_params_ = r.rows.first().params.keys();

    loading_ = true;
    const QString prev_x = x_combo_->currentData().toString();
    const QString prev_y = y_combo_->currentData().toString();
    const QString prev_m = metric_combo_->currentData().toString();
    x_combo_->clear();
    y_combo_->clear();
    for (const QString& p : axis_params_) {
        x_combo_->addItem(p, p);
        y_combo_->addItem(p, p);
    }
    int xi = std::max(0, static_cast<int>(axis_params_.indexOf(prev_x)));
    int yi = static_cast<int>(axis_params_.indexOf(prev_y));
    if (yi < 0 || yi == xi)
        yi = axis_params_.size() > 1 ? (xi == 0 ? 1 : 0) : 0;
    x_combo_->setCurrentIndex(axis_params_.isEmpty() ? -1 : xi);
    y_combo_->setCurrentIndex(axis_params_.isEmpty() ? -1 : yi);
    x_combo_->setEnabled(!axis_params_.isEmpty());
    y_combo_->setEnabled(axis_params_.size() > 1);

    metric_combo_->clear();
    const QVector<BtOptMetric> metrics = opt_metrics(r, opt_objective(r.objective));
    int mi = 0;
    for (int i = 0; i < metrics.size(); ++i) {
        metric_combo_->addItem(metrics.at(i).label, metrics.at(i).key);
        if (metrics.at(i).key == prev_m)
            mi = i;
    }
    metric_combo_->setCurrentIndex(mi);
    metric_combo_->setEnabled(!axis_params_.isEmpty());
    loading_ = false;
}

void BtOptimizeView::render(bool fresh) {
    if (!result_ || result_->rows.isEmpty())
        return;
    if (stack_->currentIndex() != 1)
        stack_->setCurrentIndex(1);
    render_notices();
    render_kpis();
    render_surface();
    if (fresh)
        render_top_sets(); // the model formats in data(): a format change needs no rebuild
    render_overfitting();
    render_holdout();
    render_stability();
}

void BtOptimizeView::render_notices() {
    const BtOptimizeResult& r = *result_;
    QVector<BtAnalysisNotice> n;
    const int trials = r.n_trials > 0 ? r.n_trials : r.iterations;
    if (!r.has_overfitting || (std::isfinite(r.pbo) && r.pbo > 0.5))
        n.append({tr("Best of %1 trials: expect an optimistic bias; confirm with WALK-FORWARD.").arg(count(trials)),
                  Tone::Warn});
    if (r.truncated)
        n.append({tr("Searched %1 of %2 combinations (%3): the surface has untested cells.")
                      .arg(count(r.iterations), count(r.total_combinations),
                           r.sampling.isEmpty() ? tr("sampled") : r.sampling),
                  Tone::Dim});
    n += bt_analysis_warning_notices(r.warnings);
    notices_->set_notices(n);
}

void BtOptimizeView::render_kpis() {
    using T = ui::StatTile::Tone;
    const BtOptimizeResult& r = *result_;
    const BtOptObjective obj = opt_objective(r.objective);
    const QString not_computed = tr("Not computed by %1").arg(engine_);

    // BEST <OBJECTIVE>
    t_best_->set_label(tr("BEST %1").arg(obj.label));
    set_tile(t_best_, r.best_objective, obj.unit, opt_objective_rule(obj), ccy_, tr("No objective value was reported"));
    {
        QStringList sub;
        if (obj.key != QLatin1String("sharpe"))
            sub << tr("Sharpe %1").arg(ratio(r.best_perf.value(QStringLiteral("sharpeRatio"), kBtNaN)));
        if (obj.key != QLatin1String("return"))
            sub << tr("return %1").arg(pct(r.best_perf.value(QStringLiteral("totalReturn"), kBtNaN), 1, true));
        sub << tr("max DD %1").arg(pct(r.best_perf.value(QStringLiteral("maxDrawdown"), kBtNaN), 1));
        t_best_->set_sub_label(sub.join(QStringLiteral(" · ")));
    }

    // BEST SET
    {
        QStringList vals, names;
        const QStringList keys = table_params_.isEmpty() ? r.best_params.keys() : table_params_;
        for (const QString& k : keys) {
            if (!r.best_params.contains(k))
                continue;
            vals << bt_analysis_json_text(r.best_params.value(k));
            names << k;
        }
        if (vals.isEmpty())
            opt_blank_tile(t_set_, tr("No best parameter set was reported"));
        else
            opt_text_tile(t_set_, vals.join(QStringLiteral(" / ")), T::Neutral, names.join(QStringLiteral(" / ")),
                          bt_analysis_params_text(r.best_params, table_params_));
    }

    // RUNS
    {
        const QString value = r.total_combinations > r.iterations
                                  ? tr("%1 / %2").arg(count(r.iterations), count(r.total_combinations))
                                  : count(r.iterations);
        QStringList parts;
        if (r.truncated)
            parts << tr("truncated");
        if (!r.method.isEmpty())
            parts << r.method;
        if (!r.sampling.isEmpty())
            parts << r.sampling;
        const QString sub = parts.join(QStringLiteral(" · "));
        QString tip;
        if (std::isfinite(r.elapsed_sec))
            tip = tr("%1 sets evaluated in %2 s").arg(count(r.iterations), num(r.elapsed_sec, 1));
        opt_text_tile(t_runs_, value, r.truncated ? T::Warning : T::Neutral, sub, tip);
    }

    const int trials = r.n_trials > 0 ? r.n_trials : r.iterations;
    if (!r.has_overfitting) {
        opt_blank_tile(t_dsr_, not_computed);
        opt_blank_tile(t_emax_, not_computed);
        opt_blank_tile(t_pbo_, not_computed);
    } else {
        // DEFLATED SHARPE
        if (!std::isfinite(r.deflated_sharpe)) {
            opt_blank_tile(t_dsr_, tr("Not reported by %1").arg(engine_));
        } else {
            const double d = r.deflated_sharpe;
            // A significance test: passed (Success) / failed (Danger) states, inconclusive in between.
            opt_text_tile(t_dsr_, pct(d, 1), d >= 0.95 ? T::Success : (d >= 0.5 ? T::Warning : T::Danger),
                          d >= 0.95 ? tr("very unlikely to be luck")
                                    : (d >= 0.5 ? tr("inconclusive (below 95 %)") : tr("likely luck")));
        }
        // EXPECTED MAX SHARPE UNDER THE NULL
        if (!std::isfinite(r.expected_max_sharpe)) {
            opt_blank_tile(t_emax_, tr("Not reported by %1").arg(engine_));
        } else {
            const bool beaten = std::isfinite(r.best_sharpe) && r.best_sharpe > r.expected_max_sharpe;
            QString sub = std::isfinite(r.effective_trials)
                              ? tr("best of %1 trials (%2 effective)").arg(count(trials), num(r.effective_trials, 1))
                              : tr("best of %1 trials").arg(count(trials));
            if (std::isfinite(r.best_sharpe) && !beaten)
                sub = tr("best Sharpe %1 does not beat it").arg(ratio(r.best_sharpe));
            opt_text_tile(t_emax_, ratio(r.expected_max_sharpe), beaten ? T::Neutral : T::Warning, sub);
        }
        // PBO
        if (!std::isfinite(r.pbo)) {
            opt_blank_tile(t_pbo_, r.pbo_reason.isEmpty() ? tr("Not reported by %1").arg(engine_) : r.pbo_reason);
        } else {
            const double p = r.pbo;
            opt_text_tile(t_pbo_, pct(p, 0), p <= 0.2 ? T::Success : (p <= 0.5 ? T::Warning : T::Danger),
                          p <= 0.2 ? tr("low overfitting risk")
                                   : (p <= 0.5 ? tr("moderate") : tr("high: mostly noise")));
        }
    }

    // HOLD-OUT <OBJECTIVE> and the IS -> OOS drop
    t_hold_->set_label(tr("HOLD-OUT %1").arg(obj.label));
    if (!r.has_holdout) {
        const QString why = tr("No hold-out in this run");
        opt_blank_tile(t_hold_, why);
        opt_blank_tile(t_drop_, why);
    } else {
        const double is = opt_objective_in(r.holdout_in_sample, obj);
        const double oos = opt_objective_in(r.holdout_out_sample, obj);
        set_tile(t_hold_, oos, obj.unit, opt_objective_rule(obj), ccy_,
                 tr("The hold-out did not report %1").arg(obj.name));
        t_hold_->set_sub_label(std::isfinite(oos) ? tr("in-sample %1 · split %2")
                                                        .arg(format_value(is, obj.unit, ccy_),
                                                             date_text(r.holdout_split_date))
                                                  : QString());
        const double d = oos - is;
        if (!std::isfinite(d)) {
            opt_blank_tile(t_drop_, tr("Needs the in-sample and out-of-sample %1").arg(obj.name));
        } else {
            const double rel = std::fabs(is) > 1e-12 ? d / std::fabs(is) : kBtNaN;
            const bool bad = std::isfinite(rel) ? rel < -0.5 : d < 0;
            opt_text_tile(t_drop_, opt_arrow(d) + opt_delta_text(d, obj.unit), bad ? T::Warning : T::Neutral,
                          std::isfinite(rel) ? tr("%1 of in-sample").arg(pct(rel, 0, true)) : QString());
        }
    }
}

void BtOptimizeView::render_surface() {
    const BtOptimizeResult& r = *result_;
    const BtOptObjective obj = opt_objective(r.objective);
    const QVector<BtOptMetric> metrics = opt_metrics(r, obj);
    BtOptMetric m = metrics.first();
    const QString mkey = metric_combo_->currentData().toString();
    for (const BtOptMetric& x : metrics)
        if (x.key == mkey)
            m = x;
    surface_metric_ = m.label;
    surface_higher_better_ = m.higher_better;
    const auto f = opt_formatter(m.unit, ccy_);

    if (axis_params_.isEmpty()) {
        surface_ = BtSurface{};
        surface_none_->setText(tr("No numeric parameter varied in this optimisation, so there is no surface. "
                                  "%1 parameter set(s) were evaluated: see TOP SETS.")
                                   .arg(count(static_cast<double>(r.rows.size()))));
        surface_stack_->setCurrentWidget(surface_none_);
        surface_note_->clear();
        surface_panel_.header->set_badge(QString());
        return;
    }
    const QString xp = x_combo_->currentData().toString();
    const QString yp = y_combo_->currentData().toString();
    const bool one_d = axis_params_.size() < 2 || yp.isEmpty() || yp == xp;
    surface_ = services::backtest::optimize_surface(r, xp, one_d ? xp : yp, m.key);

    if (one_d) {
        QStringList cats;
        QVector<double> vals;
        for (int c = 0; c < surface_.x_values.size(); ++c) {
            cats << bt_analysis_param_text(surface_.x_values.at(c));
            vals << (surface_.values.isEmpty() ? kBtNaN : surface_.values.at(0).value(c, kBtNaN));
        }
        surface_bars_->set_data(cats, vals, m.label);
        surface_bars_->set_value_formatter(f);
        surface_bars_->set_show_values(cats.size() <= 24);
        surface_bars_->set_signed_colors(m.direction);
        surface_stack_->setCurrentWidget(surface_bars_);
        surface_panel_.header->set_badge(tr("%1 values").arg(cats.size()));
        surface_note_->setText(tr("%1 by %2: each bar is the best value over the other parameters. Click a bar to "
                                  "select its set in TOP SETS.")
                                   .arg(m.label, xp));
        return;
    }

    surface_table_->clear();
    surface_table_->set_axes(
        yp, surface_.y_values, xp, surface_.x_values, [](double v) { return bt_analysis_param_text(v); },
        [](double v) { return bt_analysis_param_text(v); });
    ui::SensitivityMetric sm;
    sm.name = m.label;
    sm.values = surface_.values;
    sm.format = f;
    sm.higher_is_better = surface_.higher_is_better;
    sm.diverging = m.diverging;
    if (m.diverging)
        sm.midpoint = opt_median(surface_.values);
    surface_table_->add_metric(sm);
    surface_table_->set_base_cell(surface_.best_row, surface_.best_col);
    surface_stack_->setCurrentWidget(surface_table_);
    int filled = 0;
    for (const auto& row : surface_.values)
        for (double v : row)
            if (std::isfinite(v))
                ++filled;
    const int cells = static_cast<int>(surface_.x_values.size() * surface_.y_values.size());
    surface_panel_.header->set_badge(tr("%1 × %2").arg(surface_.y_values.size()).arg(surface_.x_values.size()));
    QString note = tr("Rows %1, columns %2 · %3 of %4 cells evaluated · colour centred on the median.")
                       .arg(yp, xp, count(filled), count(cells));
    if (axis_params_.size() > 2 || r.param_names.size() > 2)
        note += QLatin1Char(' ') + tr("Each cell is the best %1 over the other parameters.").arg(m.label);
    note += QLatin1Char(' ') + tr("Outlined: the best set. Click a cell to select its set in TOP SETS.");
    surface_note_->setText(note);
}

void BtOptimizeView::render_top_sets(int pinned_index) {
    const BtOptimizeResult& r = *result_;
    const BtOptObjective obj = opt_objective(r.objective);
    const int p = static_cast<int>(table_params_.size());

    QStringList headers;
    headers << tr("#");
    for (const QString& name : table_params_)
        headers << name.toUpper();
    headers << tr("%1 (OBJ.)").arg(obj.label) << tr("RETURN") << tr("SHARPE") << tr("MAX DD") << tr("TRADES");

    auto make_row = [&](int k, bool pinned) {
        const BtOptRow& src = r.rows.at(k);
        BtOptTopRow row;
        row.rank = k + 1;
        row.index = k;
        row.pinned = pinned;
        row.obj_unit = obj.unit;
        for (const QString& name : table_params_) {
            const QJsonValue v = src.params.value(name);
            row.ptext << bt_analysis_json_text(v);
            row.pval << opt_json_number(v);
        }
        row.objective = src.objective;
        row.ret = src.perf_value(QStringLiteral("totalReturn"));
        row.sharpe = src.perf_value(QStringLiteral("sharpeRatio"));
        row.maxdd = src.perf_value(QStringLiteral("maxDrawdown"));
        row.trades = src.perf_value(QStringLiteral("totalTrades"));
        return row;
    };
    const int n = std::min(kOptTopRows, static_cast<int>(r.rows.size()));
    QVector<BtOptTopRow> rows;
    rows.reserve(n + 1);
    for (int k = 0; k < n; ++k)
        rows.append(make_row(k, false));
    if (pinned_index >= n && pinned_index < r.rows.size())
        rows.append(make_row(pinned_index, true));

    top_model_->clear();
    top_model_->set_headers(headers);
    top_model_->upsert(rows);

    const int c_obj = p + 1;
    if (top_bar_column_ != c_obj) {
        if (top_bar_column_ >= 0)
            top_view_->setItemDelegateForColumn(top_bar_column_, nullptr);
        top_view_->setItemDelegateForColumn(c_obj, top_bar_);
        top_bar_column_ = c_obj;
    }
    top_bar_->invalidate_range_cache();
    QVector<int> chars;
    chars << 5;
    for (const QString& name : table_params_)
        chars << std::clamp(static_cast<int>(name.size()) + 2, 7, 16);
    chars << 15 << 10 << 8 << 9 << 8;
    set_column_chars(top_view_, chars);
    top_panel_.header->set_badge(tr("top %1 of %2").arg(count(n), count(static_cast<double>(r.rows.size()))));
    update_actions();
}

void BtOptimizeView::render_overfitting() {
    const BtOptimizeResult& r = *result_;
    const int trials = r.n_trials > 0 ? r.n_trials : r.iterations;
    QVector<BtKv> kv;
    QStringList reading;

    if (!r.has_overfitting) {
        over_chip_->setText(tr("NOT COMPUTED"));
        set_tone(over_chip_, Tone::Dim);
        const QString nc = tr("Not computed by %1").arg(engine_);
        kv.append({tr("Deflated Sharpe"), dash(), Tone::Dim, nc});
        kv.append({tr("PBO"), dash(), Tone::Dim, nc});
        kv.append({tr("Trials"), count(trials), Tone::None, {}});
        if (r.has_significance) {
            const QString sig = std::isfinite(r.p_value)
                                    ? tr("p = %1 · %2").arg(num(r.p_value, 3),
                                                                r.significant ? tr("significant at 5 %")
                                                                              : tr("not significant"))
                                    : (r.significant ? tr("significant") : tr("not significant"));
            kv.append({tr("Significance (naive z-test)"), sig, Tone::None, r.significance_note});
        }
        reading << tr("Overfitting statistics were not computed by %1. The best of %2 trials is an optimistic "
                      "estimate: the more sets are tried, the higher the best one scores by chance alone. Confirm "
                      "the set with WALK-FORWARD or a HOLD-OUT.")
                       .arg(engine_, count(trials));
        if (r.has_significance)
            reading << tr("The naive z-test compares the best set with the average set; it does not correct for the "
                          "number of trials, so it overstates significance.");
        over_kv_->set_rows(kv);
        over_reading_->setText(reading.join(QStringLiteral("\n\n")));
        over_method_->clear();
        over_method_->setVisible(false);
        return;
    }

    const double dsr = r.deflated_sharpe;
    const double pbo = r.pbo;
    const Tone dsr_tone = opt_state_tone(dsr, 0.95, 0.5);
    const Tone pbo_tone = opt_pbo_tone(pbo);
    kv.append({tr("Deflated Sharpe"), pct(dsr, 1), dsr_tone,
               tr("Probability that the best set's true Sharpe is above zero after deflating for %1 trials")
                   .arg(count(trials))});
    kv.append({tr("Probabilistic Sharpe"), pct(r.probabilistic_sharpe, 1), Tone::None,
               tr("The same test without the multiple-testing deflation")});
    kv.append({tr("PBO"), pct(pbo, 1), pbo_tone, std::isfinite(pbo) ? QString() : r.pbo_reason});
    kv.append({tr("Expected max Sharpe (null)"), ratio(r.expected_max_sharpe),
               (std::isfinite(r.best_sharpe) && std::isfinite(r.expected_max_sharpe) &&
                r.best_sharpe <= r.expected_max_sharpe)
                   ? Tone::Warn
                   : Tone::None,
               tr("The best Sharpe that %1 strategies with no skill would show by chance").arg(count(trials))});
    kv.append({tr("Best Sharpe"), ratio(r.best_sharpe), Tone::None, {}});
    kv.append({tr("Trials · effective"),
               tr("%1 · %2").arg(count(trials), std::isfinite(r.effective_trials) ? num(r.effective_trials, 1)
                                                                                       : dash()),
               Tone::None, tr("Correlated parameter sets count as fewer independent trials")});
    kv.append({tr("Prob. out-of-sample loss"), pct(r.prob_oos_loss, 1),
               std::isfinite(r.prob_oos_loss) && r.prob_oos_loss > 0.5 ? Tone::Warn : Tone::None,
               tr("Share of the train / test splits in which the in-sample best set lost money out of sample")});
    kv.append({tr("Sharpe dispersion"), ratio(r.sharpe_std, 3), Tone::None,
               tr("Standard deviation of the Sharpe ratio across the trials")});
    over_kv_->set_rows(kv);

    // Verdict chip.
    if ((std::isfinite(pbo) && pbo > 0.5) || (std::isfinite(dsr) && dsr < 0.5)) {
        over_chip_->setText(tr("LIKELY OVERFIT"));
        set_tone(over_chip_, Tone::Danger);
    } else if ((std::isfinite(pbo) && pbo > 0.2) || (std::isfinite(dsr) && dsr < 0.95)) {
        over_chip_->setText(tr("CHECK"));
        set_tone(over_chip_, Tone::Warn);
    } else if (std::isfinite(pbo) || std::isfinite(dsr)) {
        over_chip_->setText(tr("LOW RISK"));
        set_tone(over_chip_, Tone::Ok);
    } else {
        over_chip_->setText(tr("INCOMPLETE"));
        set_tone(over_chip_, Tone::Dim);
    }

    // Plain-language reading.
    if (std::isfinite(pbo)) {
        const QString p = pct(pbo, 0);
        if (pbo > 0.5)
            reading << tr("PBO %1: in %1 of the train / test splits the best in-sample set ranked below the median "
                          "out of sample — the optimisation mostly picked noise. Confirm with WALK-FORWARD.")
                           .arg(p);
        else if (pbo > 0.2)
            reading << tr("PBO %1: in %1 of the train / test splits the best in-sample set ranked below the median "
                          "out of sample — part of the edge is selection luck.")
                           .arg(p);
        else
            reading << tr("PBO %1: the best in-sample set ranked below the median out of sample in only %1 of the "
                          "train / test splits — the ranking carries over.")
                           .arg(p);
    } else {
        reading << (r.pbo_reason.isEmpty() ? tr("PBO was not computed.")
                                           : tr("PBO was not computed: %1").arg(r.pbo_reason));
    }
    if (std::isfinite(dsr)) {
        const QString d = pct(dsr, 0);
        if (dsr >= 0.95)
            reading << tr("Deflated Sharpe %1: after accounting for %2 trials the best Sharpe is very unlikely to "
                          "be luck.")
                           .arg(d, count(trials));
        else if (dsr >= 0.5)
            reading << tr("Deflated Sharpe %1: after accounting for %2 trials the best Sharpe could still be luck "
                          "(below the 95 % bar).")
                           .arg(d, count(trials));
        else
            reading << tr("Deflated Sharpe %1: after accounting for %2 trials the best Sharpe (%3) is no better than "
                          "the best of %2 strategies with no skill would show (%4).")
                           .arg(d, count(trials), ratio(r.best_sharpe), ratio(r.expected_max_sharpe));
    }
    if (std::isfinite(dsr) && std::isfinite(pbo) && dsr >= 0.95 && pbo > 0.5)
        reading << tr("The two disagree: the best Sharpe is significant, but which set comes out best is not stable "
                      "across periods. Prefer a set from a broad plateau of the surface.");
    if (std::isfinite(r.effective_trials) && trials > 0 && r.effective_trials < 0.8 * trials)
        reading << tr("The %1 trials behave like %2 independent ones (neighbouring sets are correlated).")
                       .arg(count(trials), num(r.effective_trials, 1));
    over_reading_->setText(reading.join(QStringLiteral("\n\n")));
    over_method_->setText(r.overfitting_method.isEmpty() ? QString() : tr("Method: %1").arg(r.overfitting_method));
    over_method_->setVisible(!r.overfitting_method.isEmpty());
}

void BtOptimizeView::render_holdout() {
    const BtOptimizeResult& r = *result_;
    if (!r.has_holdout) {
        hold_kv_->set_rows({});
        hold_model_->clear();
        hold_view_->setVisible(false);
        hold_panel_.header->set_badge(QString());
        hold_note_->setText(tr("No hold-out in this run. Set HOLD-OUT in the settings above to keep the last part of "
                               "the period unseen by the optimiser and test the best set on it."));
        return;
    }
    QVector<BtKv> kv;
    kv.append({tr("Split date"), date_text(r.holdout_split_date), Tone::None, {}});
    if (std::isfinite(r.holdout_fraction))
        kv.append({tr("Hold-out"), tr("last %1 of the bars").arg(pct(r.holdout_fraction, 0)), Tone::None, {}});
    const QString best_is = bt_analysis_params_text(r.holdout_best_params, table_params_);
    kv.append({tr("Best set (in-sample part)"), best_is, Tone::None, best_is});
    if (!r.holdout_best_params.isEmpty()) {
        const bool same = r.holdout_best_params == r.best_params;
        kv.append({tr("Same as the full-period best"), same ? tr("yes") : tr("no"), same ? Tone::None : Tone::Warn,
                   same ? QString() : tr("The best set changes when the last part of the data is left out")});
    }
    hold_kv_->set_rows(kv);

    struct Spec {
        const char* key;
        const char* label;
        Unit unit;
    };
    static const Spec specs[] = {
        {"totalReturn", QT_TR_NOOP("Total return"), Unit::SignedFraction},
        {"cagr", QT_TR_NOOP("CAGR"), Unit::SignedFraction},
        {"sharpeRatio", QT_TR_NOOP("Sharpe"), Unit::Ratio},
        {"sortinoRatio", QT_TR_NOOP("Sortino"), Unit::Ratio},
        {"calmarRatio", QT_TR_NOOP("Calmar"), Unit::Ratio},
        {"maxDrawdown", QT_TR_NOOP("Max drawdown"), Unit::Fraction},
        {"volatility", QT_TR_NOOP("Volatility"), Unit::Fraction},
        {"totalTrades", QT_TR_NOOP("Trades"), Unit::Count},
        {"winRate", QT_TR_NOOP("Win rate"), Unit::Fraction},
        {"profitFactor", QT_TR_NOOP("Profit factor"), Unit::Ratio},
    };
    QVector<BtOptHoldRow> rows;
    for (const Spec& s : specs) {
        const QString key = QString::fromLatin1(s.key);
        BtOptHoldRow row;
        row.key = key;
        row.label = tr(s.label);
        row.unit = s.unit;
        row.is = r.holdout_in_sample.value(key, kBtNaN);
        row.oos = r.holdout_out_sample.value(key, kBtNaN);
        if (std::isfinite(row.is) || std::isfinite(row.oos))
            rows.append(row);
    }
    hold_model_->upsert(rows);
    hold_view_->setVisible(!rows.isEmpty());
    bt_analysis_fit_table(hold_view_, std::max(1, static_cast<int>(rows.size())));
    hold_panel_.header->set_badge(date_text(r.holdout_split_date));
    hold_note_->setText(tr("The optimiser saw only the bars before the split; OUT-OF-SAMPLE is the same set on the "
                           "bars after it."));
}

void BtOptimizeView::render_stability() {
    const BtOptimizeResult& r = *result_;
    const BtOptObjective obj = opt_objective(r.objective);

    QStringList cats;
    QVector<double> vals;
    QStringList order = table_params_;
    QStringList spread_keys = r.spread.keys();
    std::sort(spread_keys.begin(), spread_keys.end());
    for (const QString& k : spread_keys)
        if (!order.contains(k))
            order << k;
    for (const QString& k : order) {
        if (!r.spread.contains(k))
            continue;
        cats << k;
        vals << r.spread.value(k);
    }
    spread_chart_->set_data(cats, vals, tr("Objective spread"));
    spread_chart_->set_value_formatter(obj.ratio ? std::function<QString(double)>([](double v) { return ratio(v, 2); })
                                                 : std::function<QString(double)>([](double v) { return pct(v, 1); }));
    if (cats.isEmpty())
        spread_chart_->set_state(ui::PaintedChart::Empty, tr("Parameter spread not reported by %1").arg(engine_));
    else
        spread_chart_->set_state(ui::PaintedChart::Ready);
    bt_analysis_fit_chart(spread_chart_, std::max(6.0, 2.2 * static_cast<double>(cats.size()) + 3.0));

    // Plateau.
    const double ps = r.plateau_score;
    if (!std::isfinite(ps)) {
        plateau_hero_->setText(dash());
        set_tone(plateau_hero_, Tone::Dim);
        plateau_text_->setText(r.neighbours == 0
                                   ? tr("The best set has no evaluated neighbour on the grid, so the plateau score is "
                                        "undefined.")
                                   : tr("Plateau score not reported by %1.").arg(engine_));
        stab_chip_->setText(tr("NO PLATEAU SCORE"));
        set_tone(stab_chip_, Tone::Dim);
    } else {
        plateau_hero_->setText(num(ps, 2));
        const Tone t = opt_state_tone(ps, 0.8, 0.5);
        set_tone(plateau_hero_, t);
        const QString shape = ps >= 0.8 ? tr("a broad plateau: nearby settings work almost as well")
                              : ps >= 0.5
                                  ? tr("a slope: performance falls away around the best set")
                                  : tr("an isolated peak: small changes lose most of the edge (a sign of overfitting)");
        plateau_text_->setText(tr("Plateau score %1: the best set's %2 neighbours average %3 of its objective — %4.")
                                   .arg(num(ps, 2), count(r.neighbours), pct(ps, 0), shape));
        stab_chip_->setText(ps >= 0.8 ? tr("BROAD PLATEAU") : (ps >= 0.5 ? tr("SLOPE") : tr("ISOLATED PEAK")));
        set_tone(stab_chip_, t);
    }

    // Sensitivity.
    QVector<BtKv> kv;
    for (const QString& k : order) {
        if (!r.sensitivity.contains(k))
            continue;
        const double c = r.sensitivity.value(k);
        if (!std::isfinite(c)) {
            kv.append({k, dash(), Tone::Dim, {}});
            continue;
        }
        const double a = std::fabs(c);
        const QString strength = a >= 0.5 ? tr("strong") : (a >= 0.2 ? tr("moderate") : tr("weak"));
        const QString dir = a < 0.05 ? tr("no clear direction")
                            : c > 0  ? tr("higher value → higher objective")
                                     : tr("higher value → lower objective");
        kv.append({k, tr("%1 · %2, %3").arg(ui::formatting::signed_number(c, 2), strength, dir), Tone::None,
                   tr("Correlation of %1 with the objective across the evaluated sets").arg(k)});
    }
    if (kv.isEmpty())
        kv.append({tr("Sensitivity"), tr("not reported by %1").arg(engine_), Tone::Dim, {}});
    sens_kv_->set_rows(kv);
}

void BtOptimizeView::select_surface_cell(int row, int col) {
    if (!result_ || surface_.x_values.isEmpty() || col < 0 || col >= surface_.x_values.size())
        return;
    const bool one_d = surface_.y_param.isEmpty() || surface_.y_param == surface_.x_param;
    if (!one_d && (row < 0 || row >= surface_.y_values.size()))
        return;
    const double xv = surface_.x_values.at(col);
    const double yv = one_d ? kBtNaN : surface_.y_values.at(row);
    const QString mkey = metric_combo_->currentData().toString();
    const bool by_objective = mkey.isEmpty() || mkey == QLatin1String("objective");
    int best = -1;
    double best_v = kBtNaN;
    const auto& rows = result_->rows;
    for (int k = 0; k < rows.size(); ++k) {
        const BtOptRow& row_k = rows.at(k);
        if (!opt_same(opt_json_number(row_k.params.value(surface_.x_param)), xv))
            continue;
        if (!one_d && !opt_same(opt_json_number(row_k.params.value(surface_.y_param)), yv))
            continue;
        const double v = by_objective ? row_k.objective : row_k.perf_value(mkey);
        if (!std::isfinite(v))
            continue;
        if (best < 0 || (surface_higher_better_ ? v > best_v : v < best_v)) {
            best = k;
            best_v = v;
        }
    }
    if (best >= 0)
        select_result_row(best);
}

void BtOptimizeView::select_result_row(int index) {
    if (!result_ || index < 0 || index >= result_->rows.size())
        return;
    int src_row = top_model_->index_of_key(QString::number(index + 1));
    if (src_row < 0) {
        render_top_sets(index);
        src_row = top_model_->index_of_key(QString::number(index + 1));
    }
    if (src_row < 0)
        return;
    const QModelIndex proxy = top_proxy_->mapFromSource(top_model_->index(src_row, 0));
    if (!proxy.isValid())
        return;
    top_view_->selectionModel()->select(proxy, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    top_view_->selectionModel()->setCurrentIndex(proxy, QItemSelectionModel::NoUpdate);
    top_view_->scrollTo(proxy, QAbstractItemView::EnsureVisible);
    update_actions();
}

int BtOptimizeView::selected_index() const {
    if (!result_ || !top_view_->selectionModel())
        return -1;
    const QModelIndexList sel = top_view_->selectionModel()->selectedRows();
    if (sel.isEmpty())
        return -1;
    const BtOptTopRow* row = top_model_->row_at(top_proxy_->mapToSource(sel.first()));
    if (!row || row->index < 0 || row->index >= result_->rows.size())
        return -1;
    return row->index;
}

void BtOptimizeView::update_actions() {
    const bool on = selected_index() >= 0;
    apply_btn_->setEnabled(on);
    run_btn_->setEnabled(on);
}

void BtOptimizeView::act_on_selection(bool run) {
    const int idx = selected_index();
    if (idx < 0)
        return;
    const QJsonObject params = result_->rows.at(idx).params;
    if (run)
        emit run_requested(params);
    else
        emit apply_requested(params);
}

} // namespace fincept::screens::bt
