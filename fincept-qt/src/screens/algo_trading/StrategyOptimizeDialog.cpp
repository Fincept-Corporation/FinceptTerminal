// src/screens/algo_trading/StrategyOptimizeDialog.cpp
#include "screens/algo_trading/StrategyOptimizeDialog.h"

#include "services/algo_trading/AlgoTradingService.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
using fincept::services::algo::AlgoTradingService;

namespace {
// Cells a sweep may run (each is a full backtest of the in-sample period).
constexpr int kOptMaxCells = 225;
} // namespace

StrategyOptimizeDialog::StrategyOptimizeDialog(const services::algo::AlgoStrategy& strategy, const QString& symbol,
                                               const QString& start_date, const QString& end_date, double capital,
                                               QWidget* parent)
    : QDialog(parent), strategy_(strategy), symbol_(symbol), start_(start_date), end_(end_date), capital_(capital) {
    setWindowTitle(tr("Optimise stop-loss × take-profit — %1 on %2").arg(strategy.name, symbol));
    resize(ui::metrics::width(820), ui::metrics::height(640));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto* note = new QLabel(tr("Each cell is a full backtest of the IN-SAMPLE period with the same costs and fills as "
                               "RUN BACKTEST. The best cell by in-sample Sharpe is then run once on the untouched "
                               "OUT-OF-SAMPLE tail — judge it by that, not by the in-sample peak."),
                            this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:%1;font-size:%2px;").arg(ui::colors::TEXT_TERTIARY()).arg(ui::fonts::px(-2)));
    root->addWidget(note);

    auto spin = [this](double v, double max, double step) {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(0, max);
        s->setDecimals(2);
        s->setSingleStep(step);
        s->setValue(v);
        s->setSuffix(QStringLiteral(" %"));
        return s;
    };
    sl_from_ = spin(1, 50, 0.5);
    sl_to_ = spin(5, 50, 0.5);
    sl_step_ = spin(1, 25, 0.25);
    tp_from_ = spin(2, 100, 0.5);
    tp_to_ = spin(10, 100, 0.5);
    tp_step_ = spin(2, 50, 0.25);
    oos_ = spin(30, 60, 5);
    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Stop-loss"), this), 0, 0);
    grid->addWidget(sl_from_, 0, 1);
    grid->addWidget(new QLabel(tr("to"), this), 0, 2);
    grid->addWidget(sl_to_, 0, 3);
    grid->addWidget(new QLabel(tr("step"), this), 0, 4);
    grid->addWidget(sl_step_, 0, 5);
    grid->addWidget(new QLabel(tr("Take-profit"), this), 1, 0);
    grid->addWidget(tp_from_, 1, 1);
    grid->addWidget(new QLabel(tr("to"), this), 1, 2);
    grid->addWidget(tp_to_, 1, 3);
    grid->addWidget(new QLabel(tr("step"), this), 1, 4);
    grid->addWidget(tp_step_, 1, 5);
    grid->addWidget(new QLabel(tr("Out-of-sample"), this), 2, 0);
    grid->addWidget(oos_, 2, 1);
    oos_->setToolTip(tr("Share of the period (its END) kept out of the sweep and used only to test the best cell"));
    metric_ = new QComboBox(this);
    metric_->addItem(tr("Sharpe"), QStringLiteral("sharpe"));
    metric_->addItem(tr("Net return %"), QStringLiteral("net_return"));
    metric_->addItem(tr("Max drawdown %"), QStringLiteral("max_drawdown"));
    metric_->addItem(tr("Trades"), QStringLiteral("trades"));
    grid->addWidget(new QLabel(tr("Show"), this), 2, 2);
    grid->addWidget(metric_, 2, 3, 1, 3);
    root->addLayout(grid);

    auto* bar = new QHBoxLayout();
    run_btn_ = new QPushButton(tr("RUN SWEEP"), this);
    run_btn_->setCursor(Qt::PointingHandCursor);
    bar->addWidget(run_btn_);
    progress_ = new QProgressBar(this);
    progress_->setRange(0, 100);
    progress_->setVisible(false);
    bar->addWidget(progress_, 1);
    root->addLayout(bar);
    status_ = new QLabel(this);
    status_->setWordWrap(true);
    root->addWidget(status_);

    heat_ = new ui::MatrixHeatmap(this);
    heat_->setMinimumHeight(ui::metrics::height(260));
    root->addWidget(heat_, 1);

    best_ = new QLabel(this);
    best_->setWordWrap(true);
    best_->setTextFormat(Qt::RichText);
    root->addWidget(best_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    apply_btn_ = buttons->addButton(tr("Use best stop / target"), QDialogButtonBox::ActionRole);
    apply_btn_->setEnabled(false);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    auto& svc = AlgoTradingService::instance();
    connect(&svc, &AlgoTradingService::optimization_progress, this, [this](int pct, const QString& stage) {
        progress_->setValue(pct);
        status_->setText(stage);
    });
    connect(&svc, &AlgoTradingService::optimization_result, this, &StrategyOptimizeDialog::on_result);
    connect(&svc, &AlgoTradingService::optimization_failed, this, [this](const QString& msg) {
        running_ = false;
        run_btn_->setText(tr("RUN SWEEP"));
        progress_->setVisible(false);
        status_->setText(msg);
    });
    connect(run_btn_, &QPushButton::clicked, this, [this]() {
        if (running_) {
            AlgoTradingService::instance().cancel_optimization();
            running_ = false;
            run_btn_->setText(tr("RUN SWEEP"));
            progress_->setVisible(false);
            status_->setText(tr("Sweep cancelled."));
            return;
        }
        run();
    });
    connect(metric_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { show_metric(); });
    connect(apply_btn_, &QPushButton::clicked, this, [this]() {
        const QJsonObject b = result_.value(QStringLiteral("best")).toObject();
        if (!b.isEmpty())
            emit apply_requested(b.value(QStringLiteral("sl")).toDouble(), b.value(QStringLiteral("tp")).toDouble());
    });
}

void StrategyOptimizeDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (auto_started_)
        return;
    auto_started_ = true;
    // The heatmap is never shown empty: the default ranges are swept at once (one
    // candle download, then local backtests on a worker; CANCEL stops it).
    QTimer::singleShot(0, this, [this]() {
        if (!running_ && result_.isEmpty())
            run();
    });
}

StrategyOptimizeDialog::~StrategyOptimizeDialog() {
    if (running_)
        AlgoTradingService::instance().cancel_optimization();
}

QVector<double> StrategyOptimizeDialog::steps(double from, double to, double step) {
    QVector<double> out;
    if (step <= 0 || to < from)
        return {from};
    for (double v = from; v <= to + 1e-9 && out.size() < 100; v += step)
        out.append(std::round(v * 100.0) / 100.0);
    return out;
}

void StrategyOptimizeDialog::run() {
    const auto sl = steps(sl_from_->value(), sl_to_->value(), sl_step_->value());
    const auto tp = steps(tp_from_->value(), tp_to_->value(), tp_step_->value());
    if (sl.size() * tp.size() > kOptMaxCells) {
        status_->setText(tr("%1 × %2 = %3 cells — at most %4. Widen the steps.")
                             .arg(sl.size())
                             .arg(tp.size())
                             .arg(sl.size() * tp.size())
                             .arg(kOptMaxCells));
        return;
    }
    running_ = true;
    run_btn_->setText(tr("CANCEL"));
    progress_->setValue(0);
    progress_->setVisible(true);
    apply_btn_->setEnabled(false);
    best_->clear();
    heat_->clear();
    AlgoTradingService::instance().run_optimization(strategy_, symbol_, start_, end_, capital_, sl, tp,
                                                     oos_->value() / 100.0);
}

void StrategyOptimizeDialog::on_result(const QJsonObject& result) {
    running_ = false;
    run_btn_->setText(tr("RUN SWEEP"));
    progress_->setVisible(false);
    result_ = result;
    status_->setText(tr("%1 in-sample bars, %2 out-of-sample bars%3.")
                         .arg(result.value(QStringLiteral("bars_in_sample")).toInt())
                         .arg(result.value(QStringLiteral("bars_out_of_sample")).toInt())
                         .arg(result.value(QStringLiteral("costs_applied")).toBool() ? tr(", net of costs")
                                                                                     : tr(", GROSS (no costs)")));
    show_metric();

    const QJsonObject b = result.value(QStringLiteral("best")).toObject();
    if (b.isEmpty()) {
        best_->setText(tr("No cell produced a trade in the in-sample period."));
        return;
    }
    auto line = [](const QJsonObject& r) {
        if (r.isEmpty())
            return tr("—");
        return tr("return %1, Sharpe %2, max DD %3, %4 trades")
            .arg(fmt::pct(r.value(QStringLiteral("total_return")).toDouble(), 2, true),
                 fmt::number(r.value(QStringLiteral("sharpe_ratio")).toDouble(), 2),
                 fmt::pct(-std::abs(r.value(QStringLiteral("max_drawdown")).toDouble()), 2, true))
            .arg(r.value(QStringLiteral("total_trades")).toInt());
    };
    const QJsonObject is = b.value(QStringLiteral("in_sample")).toObject();
    const QJsonObject oos = b.value(QStringLiteral("out_of_sample")).toObject();
    QString verdict;
    if (!oos.isEmpty()) {
        const double is_s = is.value(QStringLiteral("sharpe_ratio")).toDouble();
        const double oos_s = oos.value(QStringLiteral("sharpe_ratio")).toDouble();
        verdict = oos_s < 0 || (is_s > 0 && oos_s < is_s * 0.5)
                      ? QStringLiteral("<br><b style='color:%1'>%2</b>")
                            .arg(ui::colors::WARNING(),
                                 tr("Out-of-sample is far weaker than in-sample — the in-sample peak is likely fitted "
                                    "noise."))
                      : QString();
    }
    best_->setText(tr("<b>Best by in-sample Sharpe:</b> stop-loss %1, take-profit %2<br>In-sample: %3<br>"
                      "Out-of-sample: %4%5")
                       .arg(fmt::pct(b.value(QStringLiteral("sl")).toDouble(), 2, false),
                            fmt::pct(b.value(QStringLiteral("tp")).toDouble(), 2, false), line(is),
                            oos.isEmpty() ? tr("too few bars to test") : line(oos), verdict));
    apply_btn_->setEnabled(true);
}

void StrategyOptimizeDialog::show_metric() {
    if (result_.isEmpty())
        return;
    const QString key = metric_->currentData().toString();
    QStringList rows, cols;
    for (const auto& v : result_.value(QStringLiteral("sl")).toArray())
        rows << tr("SL %1").arg(fmt::pct(v.toDouble(), 2, false));
    for (const auto& v : result_.value(QStringLiteral("tp")).toArray())
        cols << tr("TP %1").arg(fmt::pct(v.toDouble(), 2, false));
    QVector<QVector<double>> values;
    for (const auto& rv : result_.value(key).toArray()) {
        QVector<double> row;
        for (const auto& cv : rv.toArray())
            row.append(cv.isDouble() ? cv.toDouble() : std::numeric_limits<double>::quiet_NaN());
        values.append(row);
    }
    if (key == QLatin1String("trades") || key == QLatin1String("max_drawdown"))
        heat_->set_sequential_scale();
    else
        heat_->set_diverging_scale();
    if (key == QLatin1String("net_return") || key == QLatin1String("max_drawdown"))
        heat_->set_value_formatter([](double v) { return fmt::number(v, 1); });
    else if (key == QLatin1String("trades"))
        heat_->set_value_formatter([](double v) { return fmt::number(v, 0); });
    else
        heat_->set_value_formatter([](double v) { return fmt::number(v, 2); });
    heat_->set_matrix(rows, cols, values);
}

} // namespace fincept::screens
