// src/screens/backtesting/BtAnalysisTabs.cpp
#include "screens/backtesting/BtAnalysisTabs.h"

#include "screens/backtesting/BtAnalysisViews.h"
#include "screens/backtesting/BtTabKit.h"
#include "services/backtesting/BacktestRequest.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::bt {

using namespace fincept::services::backtest;

namespace {

int bt_at_decimals(double step) {
    if (!(step > 0) || step >= 1)
        return 0;
    int d = 0;
    double s = step;
    while (d < 6 && std::fabs(s - std::round(s)) > 1e-9) {
        s *= 10;
        ++d;
    }
    return d;
}

QString bt_at_seconds(double s) {
    if (s < 60)
        return QCoreApplication::translate("fincept::screens::bt::BtAnalysisTab", "about %1 s").arg(std::max(1, static_cast<int>(std::lround(s))));
    return QCoreApplication::translate("fincept::screens::bt::BtAnalysisTab", "about %1 min").arg(std::max(1, static_cast<int>(std::lround(s / 60.0))));
}

QWidget* bt_at_spacer(QWidget* parent) {
    auto* w = new QWidget(parent);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return w;
}

} // namespace

QString bt_failure_title(const QString& code) {
    auto t = [](const char* s) { return QCoreApplication::translate("fincept::screens::bt::BtAnalysisTab", s); };
    if (code == QLatin1String("DATA"))
        return t("No market data");
    if (code == QLatin1String("INPUT"))
        return t("Check the inputs");
    if (code == QLatin1String("ENGINE"))
        return t("The engine failed");
    if (code == QLatin1String("MISSING_PACKAGE"))
        return t("Library not installed");
    if (code == QLatin1String("TIMEOUT"))
        return t("Timed out");
    if (code == QLatin1String("NO_ENVELOPE") || code == QLatin1String("BAD_OUTPUT") || code == QLatin1String("PROCESS"))
        return t("The engine stopped without a result");
    return t("The run failed");
}

// ═════════════════════════════════════════════════════════════════════════════
// BtRangeEditor
// ═════════════════════════════════════════════════════════════════════════════

BtRangeEditor::BtRangeEditor(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    auto* grid_box = new QWidget(this);
    grid_ = new QGridLayout(grid_box);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(8);
    grid_->setVerticalSpacing(2);
    v->addWidget(grid_box);
    empty_note_ = make_note(tr("This strategy has no numeric parameters to search."), this, Tone::Warn);
    v->addWidget(empty_note_);
}

void BtRangeEditor::set_params(const QString& engine, const QString& strategy_id, const QVector<StrategyParam>& params) {
    const QString key = engine + QLatin1Char('/') + strategy_id;
    if (key == key_)
        return;
    key_ = key;
    while (QLayoutItem* item = grid_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    rows_.clear();
    delete memory_;
    memory_ = new ui::InputMemory(QStringLiteral("backtesting"), QStringLiteral("ranges/") + key, this);
    memory_->restore();
    int r = 0;
    if (!params.isEmpty()) {
        const QStringList heads = {tr("PARAMETER"), tr("MIN"), tr("MAX"), tr("STEP"), tr("VALUES")};
        for (int c = 0; c < heads.size(); ++c) {
            auto* h = new QLabel(heads[c], this);
            h->setObjectName(QStringLiteral("btSectionTitle"));
            grid_->addWidget(h, 0, c);
        }
        r = 1;
    }
    for (const StrategyParam& p : params) {
        if (p.type == QLatin1String("bool") || p.type == QLatin1String("choice"))
            continue;
        Row row;
        row.name = p.name;
        const bool is_int = p.type == QLatin1String("int");
        const int dec = is_int ? 0 : std::max(2, bt_at_decimals(p.step));
        auto make = [this, &p, dec](double value) {
            auto* s = new QDoubleSpinBox(this);
            s->setDecimals(dec);
            s->setRange(p.min_val, p.max_val);
            s->setSingleStep(p.step > 0 ? p.step : 1);
            s->setValue(value);
            s->setFocusPolicy(Qt::StrongFocus);
            return s;
        };
        // Default search band: half to one-and-a-half times the default, about five values.
        double lo = std::max(p.min_val, p.default_val * 0.5);
        double hi = std::min(p.max_val, std::max(lo, p.default_val * 1.5));
        double step = p.step > 0 ? p.step : 1;
        if (hi > lo) {
            step = std::max(step, (hi - lo) / 4.0);
            if (is_int)
                step = std::max(1.0, std::round(step));
        }
        row.min = make(lo);
        row.max = make(hi);
        row.step = make(step);
        row.step->setRange(is_int ? 1 : std::pow(10.0, -dec), std::max(1.0, p.max_val - p.min_val));
        row.step->setValue(step);
        row.count = new QLabel(this);
        row.count->setObjectName(QStringLiteral("btKvValue"));
        auto* label = new QLabel(p.label.isEmpty() ? p.name : p.label, this);
        label->setObjectName(QStringLiteral("btFieldLabel"));
        label->setToolTip(p.description);
        row.min->setAccessibleName(tr("%1 minimum").arg(label->text()));
        row.max->setAccessibleName(tr("%1 maximum").arg(label->text()));
        row.step->setAccessibleName(tr("%1 step").arg(label->text()));
        grid_->addWidget(label, r, 0);
        grid_->addWidget(row.min, r, 1);
        grid_->addWidget(row.max, r, 2);
        grid_->addWidget(row.step, r, 3);
        grid_->addWidget(row.count, r, 4);
        memory_->track(p.name + QStringLiteral("_min"), row.min);
        memory_->track(p.name + QStringLiteral("_max"), row.max);
        memory_->track(p.name + QStringLiteral("_step"), row.step);
        for (QDoubleSpinBox* s : {row.min, row.max, row.step})
            connect(s, &QDoubleSpinBox::valueChanged, this, [this]() {
                update_counts();
                emit changed();
            });
        rows_.append(row);
        ++r;
    }
    grid_->setColumnStretch(5, 1);
    empty_note_->setVisible(rows_.isEmpty());
    update_counts();
    emit changed();
}

QJsonObject BtRangeEditor::ranges() const {
    QJsonObject out;
    for (const Row& r : rows_) {
        double mn = r.min->value(), mx = r.max->value();
        if (mx < mn)
            std::swap(mn, mx);
        out.insert(r.name, QJsonObject{{QStringLiteral("min"), mn},
                                       {QStringLiteral("max"), mx},
                                       {QStringLiteral("step"), std::max(1e-6, r.step->value())}});
    }
    return out;
}

qint64 BtRangeEditor::combinations() const {
    qint64 n = rows_.isEmpty() ? 0 : 1;
    for (const Row& r : rows_) {
        const double span = std::fabs(r.max->value() - r.min->value());
        const double st = std::max(1e-9, r.step->value());
        n *= static_cast<qint64>(std::floor(span / st + 1e-9)) + 1;
        if (n > 10'000'000)
            return n;
    }
    return n;
}

void BtRangeEditor::update_counts() {
    for (const Row& r : rows_) {
        const double span = std::fabs(r.max->value() - r.min->value());
        const qint64 c = static_cast<qint64>(std::floor(span / std::max(1e-9, r.step->value()) + 1e-9)) + 1;
        r.count->setText(QLocale().toString(c));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// BtAnalysisTab
// ═════════════════════════════════════════════════════════════════════════════

BtAnalysisTab::BtAnalysisTab(BtHost* host, BtKind kind, QWidget* parent) : QWidget(parent), host_(host), kind_(kind) {
    setObjectName(QStringLiteral("btAnalysisTab"));
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    strip_ = new QWidget(this);
    strip_->setObjectName(QStringLiteral("btSettingsStrip"));
    strip_->setAttribute(Qt::WA_StyledBackground, true);
    auto* sh = new QHBoxLayout(strip_);
    sh->setContentsMargins(8, 5, 8, 5);
    sh->setSpacing(10);
    run_btn_ = new QPushButton(strip_);
    run_btn_->setObjectName(QStringLiteral("btPrimary"));
    run_btn_->setCursor(Qt::PointingHandCursor);
    note_ = new QLabel(strip_);
    note_->setObjectName(QStringLiteral("btNote"));
    as_of_ = new QLabel(strip_);
    as_of_->setObjectName(QStringLiteral("btNote"));
    sh->addWidget(bt_at_spacer(strip_), 1);
    sh->addWidget(note_);
    sh->addWidget(as_of_);
    sh->addWidget(run_btn_);
    v->addWidget(strip_);

    extra_ = new QWidget(this);
    extra_->setObjectName(QStringLiteral("btSettingsExtra"));
    auto* ev = new QVBoxLayout(extra_);
    ev->setContentsMargins(8, 2, 8, 6);
    ev->setSpacing(2);
    extra_->hide();
    v->addWidget(extra_);

    const QString view_name = bt_command_of(kind);
    memory_ = new ui::InputMemory(QStringLiteral("backtesting"), view_name, this);

    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::Manual, this);
    pane_->set_requirements({ui::Requirement::python()});
    pane_->set_max_age(ui::autoload::kNeverRefresh);
    pane_->set_persist_limit(0); // the run store remembers the results
    pane_->set_status_label(as_of_);
   pane_->skeleton()->set_shape(kind == BtKind::WalkForward ? ui::LoadingSkeleton::Shape::Table
                                                             : ui::LoadingSkeleton::Shape::Chart);
    pane_->set_remembered_signed_source(
        [this](const QString& view, QJsonValue* payload, QDateTime* as_of, QString* signature) {
            if (!host_ || view.isEmpty())
                return false;
            const QString strategy = view.section(QLatin1Char('/'), 1);
            const SavedRun s =
                host_->controller()->latest_saved(bt_command_of(kind_), host_->engine_slug(), strategy);
            if (s.id <= 0)
                return false;
            *payload = QJsonObject{{QStringLiteral("store"), QString::number(s.id)}};
            QDateTime t = QDateTime::fromString(s.created_at, Qt::ISODate);
            t.setTimeZone(QTimeZone::UTC);
            *as_of = t.toLocalTime();
            *signature = s.signature();
            return true;
        });
    pane_->set_start([this](quint64 id) { start(id); });
    pane_->set_cancel([this](quint64) {
        const quint64 tok = token_;
        token_ = 0; // first: the controller's cancelled() for it is then not ours any more
        if (tok && host_)
            host_->controller()->cancel(tok);
        emit running_changed(false);
    });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) {
        const QJsonObject o = p.toObject();
        const QString key = o.value(QStringLiteral("key")).toString();
        if (!key.isEmpty()) {
            if (const BtBundlePtr b = host_->controller()->bundle(key)) {
                show_bundle(b);
                return;
            }
        }
        const qint64 store = o.value(QStringLiteral("store")).toString().toLongLong();
        if (store > 0) {
            const QString want_view = pane_->view();
            QPointer<BtAnalysisTab> self = this;
            host_->controller()->load_saved(store, [self, want_view](BtBundlePtr b, QString err) {
                if (!self || self->pane_->view() != want_view)
                    return;
                if (b)
                    self->show_bundle(b);
                else if (!err.isEmpty())
                    self->pane_->set_notice(err, ui::EmptyState::Tone::Danger);
            });
        }
    });
    v->addWidget(pane_, 1);

    auto* ctl = host_->controller();
    connect(ctl, &BtRunController::progress, this, [this](quint64 token, const BtProgress& p) {
        if (token == token_)
            pane_->set_progress(pane_request_, p.fraction(), progress_text(p)); // NaN fraction = text only
    });
    connect(ctl, &BtRunController::finished, this, [this](quint64 token, const BtBundlePtr& b) {
        if (token != token_ || !b)
            return;
        token_ = 0;
        pane_->deliver(pane_request_, QJsonObject{{QStringLiteral("key"), b->key}}, b->as_of.toLocalTime());
        emit running_changed(false);
    });
    connect(ctl, &BtRunController::failed, this, [this](quint64 token, const BtOutcome& o) {
        if (token != token_)
            return;
        token_ = 0;
        pane_->fail(pane_request_, o.message, bt_failure_title(o.error_code), o.technical);
        emit running_changed(false);
    });
    connect(ctl, &BtRunController::cancelled, this, [this](quint64 token) {
        if (token != token_)
            return;
        token_ = 0;
        pane_->cancel();
        emit running_changed(false);
    });
    connect(run_btn_, &QPushButton::clicked, this, &BtAnalysisTab::run);
    connect(pane_, &ui::AutoLoadPane::notice_action, this, &BtAnalysisTab::run);
    connect(this, &BtAnalysisTab::running_changed, this, [this](bool running) {
        run_btn_->setText(running ? tr("CANCEL") : run_text());
        set_tone(run_btn_, running ? Tone::Danger : Tone::None);
        update_note();
    });
    run_btn_->setText(run_text());
}

QString BtAnalysisTab::run_text() const {
    switch (kind_) {
        case BtKind::Optimize:
            return tr("RUN OPTIMISATION");
        case BtKind::WalkForward:
            return tr("RUN WALK-FORWARD");
        case BtKind::MonteCarlo:
            return tr("RUN MONTE CARLO");
        default:
            break;
    }
    return tr("RUN");
}

BtAnalysisTab::~BtAnalysisTab() {
    if (token_ && host_)
        host_->controller()->cancel(token_);
}

void BtAnalysisTab::add_field(const QString& caption, QWidget* editor, const QString& tooltip) {
    auto* box = new QWidget(strip_);
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    auto* l = new QLabel(caption, box);
    l->setObjectName(QStringLiteral("btFieldLabel"));
    editor->setParent(box);
    editor->setAccessibleName(caption);
    editor->setFocusPolicy(Qt::StrongFocus);
    if (!tooltip.isEmpty()) {
        editor->setToolTip(tooltip);
        l->setToolTip(tooltip);
    }
    h->addWidget(l);
    h->addWidget(editor);
    auto* lay = static_cast<QHBoxLayout*>(strip_->layout());
    lay->insertWidget(lay->count() - 4, box); // before the spacer, note, as-of and RUN
}

void BtAnalysisTab::refresh() {
    if (!host_)
        return;
    pane_->set_timeout(timeout_seconds());
    const BtSetup s = host_->current_setup();
    const QString view = s.engine + QLatin1Char('/') + s.strategy_id;
    if (view != view_key_) {
        view_key_ = view;
        pane_->set_view_preview(view, preview_spec(s));
        pane_->set_view(view);
    } else {
        pane_->set_view_preview(view, preview_spec(s));
    }
    settings_changed();
}

void BtAnalysisTab::settings_changed() {
    if (!host_)
        return;
    const BtSetup s = host_->current_setup();
    pane_->set_signature(run_signature(s.engine, bt_command_of(kind_), build_request(s)));
    update_note();
}

void BtAnalysisTab::update_note() {
    const BtSetup s = host_->current_setup();
    QString problem = host_->setup_problem();
    if (problem.isEmpty())
        problem = prerequisite_problem();
    if (!host_->engine_offers(bt_command_of(kind_)))
        problem = tr("%1 does not offer this analysis.").arg(host_->engine_label());
    note_->setText(problem.isEmpty() ? work_note(s) : problem);
    set_tone(note_, problem.isEmpty() ? Tone::Dim : Tone::Warn);
    run_btn_->setEnabled(problem.isEmpty() || is_running());
    run_btn_->setToolTip(problem);
    pane_->preview()->set_primary_enabled(problem.isEmpty());
}

void BtAnalysisTab::run() {
    if (is_running()) {
        pane_->cancel();
        return;
    }
    QString problem = host_->setup_problem();
    if (problem.isEmpty())
        problem = prerequisite_problem();
    if (!problem.isEmpty()) {
        host_->notify(problem);
        return;
    }
    pane_->run();
}

void BtAnalysisTab::cancel() {
    if (is_running())
        pane_->cancel();
}

bool BtAnalysisTab::is_running() const {
    return token_ != 0;
}

void BtAnalysisTab::start(quint64 pane_request) {
    pane_request_ = pane_request;
    if (token_)
        host_->controller()->cancel(token_);
    token_ = 0;
    QString problem = host_->setup_problem();
    if (problem.isEmpty())
        problem = prerequisite_problem();
    if (!problem.isEmpty()) {
        pane_->fail(pane_request, problem, bt_failure_title(QStringLiteral("INPUT")));
        return;
    }
    const BtSetup s = host_->current_setup();
    const QJsonObject req = build_request(s);
    const QString sig = run_signature(s.engine, bt_command_of(kind_), req);
    QString why;
    token_ = host_->controller()->start(kind_, s.engine, bt_command_of(kind_), req, sig, host_->setup_currency(), &why);
    if (!token_) {
        pane_->fail(pane_request, why, tr("Queue full"));
        return;
    }
    const int pos = host_->controller()->queue_position(token_);
    pane_->set_loading_text(pos > 0 ? tr("Queued (%1) — waiting for the run in progress").arg(pos)
                                    : tr("Starting %1…").arg(host_->engine_label()));
    emit running_changed(true);
}

void BtAnalysisTab::show_bundle(const BtBundlePtr& b) {
    current_ = b;
    render(b);
}

// ═════════════════════════════════════════════════════════════════════════════
// OPTIMISE
// ═════════════════════════════════════════════════════════════════════════════

BtOptimizeTab::BtOptimizeTab(BtHost* host, QWidget* parent) : BtAnalysisTab(host, BtKind::Optimize, parent) {
    objective_ = new QComboBox(this);
    objective_->setObjectName(QStringLiteral("btCombo"));
    for (const BtChoice& c : optimize_objective_choices())
        objective_->addItem(c.label, c.id);
    method_ = new QComboBox(this);
    method_->setObjectName(QStringLiteral("btCombo"));
    for (const BtChoice& c : optimize_method_choices())
        method_->addItem(c.label, c.id);
    max_runs_ = new QSpinBox(this);
    max_runs_->setRange(5, 20000);
    max_runs_->setValue(500);
    max_runs_->setGroupSeparatorShown(true);
    holdout_ = new QComboBox(this);
    holdout_->setObjectName(QStringLiteral("btCombo"));
    holdout_->addItem(tr("none"), 0.0);
    holdout_->addItem(tr("last 20 %"), 0.2);
    holdout_->addItem(tr("last 30 %"), 0.3);
    holdout_->setCurrentIndex(2);
    seed_ = new QSpinBox(this);
    seed_->setRange(0, 999999);
    seed_->setValue(7);
    add_field(tr("OBJECTIVE"), objective_, tr("What the search ranks the parameter sets by"));
    add_field(tr("METHOD"), method_);
    add_field(tr("MAX RUNS"), max_runs_, tr("Cap on evaluated sets (random sampling beyond it)"));
    add_field(tr("HOLD-OUT"), holdout_,
              tr("Keep the end of the period out of the search and report the best set on it"));
    add_field(tr("SEED"), seed_, tr("Random sampling and the overfitting statistics are repeatable with the same seed"));
    memory_->track(QStringLiteral("objective"), objective_);
    memory_->track(QStringLiteral("method"), method_);
    memory_->track(QStringLiteral("max_runs"), max_runs_);
    memory_->track(QStringLiteral("holdout"), holdout_);
    memory_->track(QStringLiteral("seed"), seed_);
    memory_->restore();

    ranges_ = new BtRangeEditor(extra_);
    extra_->layout()->addWidget(ranges_);
    extra_->show();

    view_ = new BtOptimizeView(pane_->content_parent());
    pane_->set_content(view_);
    connect(view_, &BtOptimizeView::apply_requested, this, [this](const QJsonObject& p) { host_->apply_params(p); });
    connect(view_, &BtOptimizeView::run_requested, this, [this](const QJsonObject& p) { host_->run_backtest_with(p); });
    for (QComboBox* c : {objective_, method_, holdout_})
        connect(c, &QComboBox::currentIndexChanged, this, [this]() { settings_changed(); });
    for (QSpinBox* s : {max_runs_, seed_})
        connect(s, &QSpinBox::valueChanged, this, [this]() { settings_changed(); });
    connect(ranges_, &BtRangeEditor::changed, this, [this]() { settings_changed(); });
}

QJsonObject BtOptimizeTab::build_request(const BtSetup& s) const {
    if (ranges_)
        ranges_->set_params(s.engine, s.strategy_id, host_->strategy_params());
    BtOptimizeSettings o;
    o.objective = objective_->currentData().toString();
    o.method = method_->currentData().toString();
    o.max_iterations = max_runs_->value();
    o.holdout_fraction = holdout_->currentData().toDouble();
    o.seed = seed_->value();
    o.param_ranges = ranges_ ? ranges_->ranges() : QJsonObject();
    return build_optimize_request(s, o);
}

void BtOptimizeTab::render(const BtBundlePtr& b) {
    if (b && b->optimize)
        view_->show_result(b->optimize, b->currency, host_->engine_label());
}

QString BtOptimizeTab::work_note(const BtSetup& s) const {
    if (ranges_)
        ranges_->set_params(s.engine, s.strategy_id, host_->strategy_params());
    const qint64 combos = ranges_ ? ranges_->combinations() : 0;
    const qint64 runs = method_->currentData().toString() == QLatin1String("random")
                            ? std::min<qint64>(combos, max_runs_->value())
                            : std::min<qint64>(combos, max_runs_->value());
    const int workers = default_max_workers();
    const double secs = 4.0 + static_cast<double>(runs) * 0.35 / workers;
    return tr("%1 combinations · %2 runs · %3 · up to %n worker(s)", nullptr, workers)
        .arg(QLocale().toString(combos), QLocale().toString(runs), bt_at_seconds(secs));
}

ui::EmptyState::Spec BtOptimizeTab::preview_spec(const BtSetup& s) const {
    auto spec = ui::EmptyState::preview_spec(
        tr("Parameter optimisation"),
        tr("Runs %1 over a grid of its parameters and ranks the sets by the objective — with the deflated Sharpe "
           "ratio, the probability of backtest overfitting and a hold-out test next to the best set.")
            .arg(s.strategy_name.isEmpty() ? tr("the strategy") : s.strategy_name),
        tr("RUN OPTIMISATION"), work_note(s), ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::Chart);
    QList<QPair<QString, QString>> details;
    const QJsonObject r = ranges_ ? ranges_->ranges() : QJsonObject();
    for (auto it = r.constBegin(); it != r.constEnd() && details.size() < 6; ++it) {
        const QJsonObject o = it.value().toObject();
        details.append({it.key(), tr("%1 … %2 step %3")
                                      .arg(o.value(QStringLiteral("min")).toDouble())
                                      .arg(o.value(QStringLiteral("max")).toDouble())
                                      .arg(o.value(QStringLiteral("step")).toDouble())});
    }
    details.append({tr("Engine"), host_->engine_label()});
    spec.details = details;
    spec.details_heading = tr("WHAT WILL BE SEARCHED");
    return spec;
}

QString BtOptimizeTab::prerequisite_problem() const {
    if (!ranges_ || ranges_->empty())
        return tr("The strategy has no numeric parameter to optimise.");
    if (ranges_->combinations() > 200000)
        return tr("%1 combinations — narrow the ranges or raise the steps (max 200,000).")
            .arg(QLocale().toString(ranges_->combinations()));
    return {};
}

// ═════════════════════════════════════════════════════════════════════════════
// WALK-FORWARD
// ═════════════════════════════════════════════════════════════════════════════

BtWalkForwardTab::BtWalkForwardTab(BtHost* host, QWidget* parent) : BtAnalysisTab(host, BtKind::WalkForward, parent) {
    windows_ = new QSpinBox(this);
    windows_->setRange(2, 20);
    windows_->setValue(5);
    train_ = new QSpinBox(this);
    train_->setRange(30, 90);
    train_->setValue(70);
    train_->setSuffix(QStringLiteral(" %"));
    mode_ = new QComboBox(this);
    mode_->setObjectName(QStringLiteral("btCombo"));
    mode_->addItem(tr("rolling"), QStringLiteral("rolling"));
    mode_->addItem(tr("anchored"), QStringLiteral("anchored"));
    purge_ = new QSpinBox(this);
    purge_->setRange(0, 500);
    embargo_ = new QSpinBox(this);
    embargo_->setRange(0, 500);
    objective_ = new QComboBox(this);
    objective_->setObjectName(QStringLiteral("btCombo"));
    for (const BtChoice& c : optimize_objective_choices())
        objective_->addItem(c.label, c.id);
    add_field(tr("WINDOWS"), windows_);
    add_field(tr("TRAIN"), train_, tr("Share of each window used to pick the parameters"));
    add_field(tr("MODE"), mode_, tr("Rolling = fixed-length training windows; anchored = training grows from the start"));
    add_field(tr("PURGE"), purge_, tr("Bars dropped between each training and test window (no leakage)"));
    add_field(tr("EMBARGO"), embargo_, tr("Bars after each test window left out of later training windows"));
    add_field(tr("OBJECTIVE"), objective_);
    memory_->track(QStringLiteral("windows"), windows_);
    memory_->track(QStringLiteral("train"), train_);
    memory_->track(QStringLiteral("mode"), mode_);
    memory_->track(QStringLiteral("purge"), purge_);
    memory_->track(QStringLiteral("embargo"), embargo_);
    memory_->track(QStringLiteral("objective"), objective_);
    memory_->restore();

    ranges_ = new BtRangeEditor(extra_);
    extra_->layout()->addWidget(ranges_);
    extra_->show();

    view_ = new BtWalkForwardView(pane_->content_parent());
    pane_->set_content(view_);
    for (QComboBox* c : {mode_, objective_})
        connect(c, &QComboBox::currentIndexChanged, this, [this]() { settings_changed(); });
    for (QSpinBox* s : {windows_, train_, purge_, embargo_})
        connect(s, &QSpinBox::valueChanged, this, [this]() { settings_changed(); });
    connect(ranges_, &BtRangeEditor::changed, this, [this]() { settings_changed(); });
}

QJsonObject BtWalkForwardTab::build_request(const BtSetup& s) const {
    if (ranges_)
        ranges_->set_params(s.engine, s.strategy_id, host_->strategy_params());
    BtWalkForwardSettings w;
    w.windows = windows_->value();
    w.train_ratio = train_->value() / 100.0;
    w.mode = mode_->currentData().toString();
    w.purge_bars = purge_->value();
    w.embargo_bars = embargo_->value();
    w.objective = objective_->currentData().toString();
    w.param_ranges = ranges_ ? ranges_->ranges() : QJsonObject();
    return build_walk_forward_request(s, w);
}

void BtWalkForwardTab::render(const BtBundlePtr& b) {
    if (b && b->walk_forward)
        view_->show_result(b->walk_forward, b->currency, host_->engine_label());
}

QString BtWalkForwardTab::work_note(const BtSetup& s) const {
    if (ranges_)
        ranges_->set_params(s.engine, s.strategy_id, host_->strategy_params());
    const qint64 combos = ranges_ ? std::max<qint64>(1, ranges_->combinations()) : 1;
    const qint64 runs = combos * windows_->value() + windows_->value();
    const int workers = default_max_workers();
    return tr("%1 windows × %2 combinations = %3 runs · %4")
        .arg(windows_->value())
        .arg(QLocale().toString(combos), QLocale().toString(runs),
             bt_at_seconds(5.0 + static_cast<double>(runs) * 0.25 / workers));
}

ui::EmptyState::Spec BtWalkForwardTab::preview_spec(const BtSetup& s) const {
    auto spec = ui::EmptyState::preview_spec(
        tr("Walk-forward analysis"),
        tr("Re-optimises %1 on each training window and tests the best set on the next, unseen window — the "
           "stitched out-of-sample curve is what the strategy would have earned without hindsight.")
            .arg(s.strategy_name.isEmpty() ? tr("the strategy") : s.strategy_name),
        tr("RUN WALK-FORWARD"), work_note(s), ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::Table);
    spec.details = {{tr("Windows"), QString::number(windows_->value())},
                    {tr("Training share"), QStringLiteral("%1 %").arg(train_->value())},
                    {tr("Mode"), mode_->currentText()},
                    {tr("Purge / embargo"), tr("%1 / %2 bars").arg(purge_->value()).arg(embargo_->value())},
                    {tr("Engine"), host_->engine_label()}};
    spec.details_heading = tr("HOW IT WILL RUN");
    return spec;
}

QString BtWalkForwardTab::prerequisite_problem() const {
    if (!ranges_ || ranges_->empty())
        return tr("The strategy has no numeric parameter to re-optimise.");
    return {};
}

// ═════════════════════════════════════════════════════════════════════════════
// MONTE CARLO
// ═════════════════════════════════════════════════════════════════════════════

BtMonteCarloTab::BtMonteCarloTab(BtHost* host, QWidget* parent) : BtAnalysisTab(host, BtKind::MonteCarlo, parent) {
    method_ = new QComboBox(this);
    method_->setObjectName(QStringLiteral("btCombo"));
    for (const BtChoice& c : monte_carlo_methods()) {
        method_->addItem(c.label, c.id);
        method_->setItemData(method_->count() - 1, c.detail, Qt::ToolTipRole);
    }
    paths_ = new QSpinBox(this);
    paths_->setRange(100, 20000);
    paths_->setSingleStep(500);
    paths_->setValue(1000);
    paths_->setGroupSeparatorShown(true);
    block_ = new QSpinBox(this);
    block_->setRange(2, 250);
    block_->setValue(20);
    block_->setSuffix(tr(" bars"));
    ruin_ = new QSpinBox(this);
    ruin_->setRange(5, 95);
    ruin_->setValue(50);
    ruin_->setPrefix(QStringLiteral("−"));
    ruin_->setSuffix(QStringLiteral(" %"));
    seed_ = new QSpinBox(this);
    seed_->setRange(0, 999999);
    seed_->setValue(42);
    add_field(tr("METHOD"), method_);
    add_field(tr("PATHS"), paths_);
    add_field(tr("BLOCK"), block_, tr("Block length of the block bootstrap"));
    add_field(tr("RUIN"), ruin_, tr("A path that ever falls this far below the initial capital counts as ruined"));
    add_field(tr("SEED"), seed_, tr("The same seed gives the same paths"));
    memory_->track(QStringLiteral("method"), method_);
    memory_->track(QStringLiteral("paths"), paths_);
    memory_->track(QStringLiteral("block"), block_);
    memory_->track(QStringLiteral("ruin"), ruin_);
    memory_->track(QStringLiteral("seed"), seed_);
    memory_->restore();
    block_->setEnabled(method_->currentData().toString() == QLatin1String("block_bootstrap"));

    view_ = new BtMonteCarloView(pane_->content_parent());
    pane_->set_content(view_);
    connect(method_, &QComboBox::currentIndexChanged, this, [this]() {
        block_->setEnabled(method_->currentData().toString() == QLatin1String("block_bootstrap"));
        settings_changed();
    });
    for (QSpinBox* s : {paths_, block_, ruin_, seed_})
        connect(s, &QSpinBox::valueChanged, this, [this]() { settings_changed(); });
}

QJsonObject BtMonteCarloTab::build_request(const BtSetup& s) const {
    BtMonteCarloSettings m;
    m.method = method_->currentData().toString();
    m.simulations = paths_->value();
    m.block_size = block_->value();
    m.ruin_level = ruin_->value() / 100.0;
    m.seed = seed_->value();
    return build_monte_carlo_request(s, m);
}

void BtMonteCarloTab::render(const BtBundlePtr& b) {
    if (b && b->monte_carlo)
        view_->show_result(b->monte_carlo, b->currency, host_->engine_label());
}

QString BtMonteCarloTab::work_note(const BtSetup&) const {
    const QString m = method_->currentData().toString();
    const BtRunPtr run = host_->active_run();
    QString what;
    if (m.startsWith(QLatin1String("trade")))
        what = run ? tr("resamples the %n closed trade(s) of the backtest", nullptr, run->closed_trades())
                   : tr("resamples the backtest's closed trades");
    else
        what = run ? tr("resamples the %n bar return(s) of the backtest", nullptr, static_cast<int>(run->returns.size()))
                   : tr("resamples the backtest's bar returns");
    return tr("%1 paths · %2 · %3").arg(QLocale().toString(paths_->value()), what,
                                        bt_at_seconds(3.0 + paths_->value() / 1500.0));
}

ui::EmptyState::Spec BtMonteCarloTab::preview_spec(const BtSetup& s) const {
    auto spec = ui::EmptyState::preview_spec(
        tr("Monte Carlo"),
        tr("Re-runs the backtest of %1 and resamples it into %2 alternative equity paths: the spread of outcomes, "
           "the probability of a loss and of ruin, and where the actual run falls among them.")
            .arg(s.strategy_name.isEmpty() ? tr("the strategy") : s.strategy_name, QLocale().toString(paths_->value())),
        tr("RUN MONTE CARLO"), work_note(s), ui::EmptyState::Tone::Info, ui::LoadingSkeleton::Shape::Chart);
    spec.details = {{tr("Method"), method_->currentText()},
                    {tr("Paths"), QLocale().toString(paths_->value())},
                    {tr("Ruin level"), QStringLiteral("−%1 %").arg(ruin_->value())},
                    {tr("Seed"), QString::number(seed_->value())}};
    spec.details_heading = tr("HOW IT WILL RUN");
    return spec;
}

QString BtMonteCarloTab::prerequisite_problem() const {
    const QString m = method_->currentData().toString();
    const BtRunPtr run = host_->active_run();
    if (m.startsWith(QLatin1String("trade")) && run && run->closed_trades() < 10)
        return tr("Needs at least 10 closed trades; this run has %1 — use RETURN BOOTSTRAP or BLOCK BOOTSTRAP.")
            .arg(run->closed_trades());
    return {};
}

} // namespace fincept::screens::bt
