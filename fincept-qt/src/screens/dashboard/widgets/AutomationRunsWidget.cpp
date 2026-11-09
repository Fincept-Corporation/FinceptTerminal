#include "screens/dashboard/widgets/AutomationRunsWidget.h"

#include "algo_engine/AlgoEngine.h"
#include "algo_engine/AlgoEngineTypes.h"
#include "core/currency/Currency.h"
#include "core/events/EventBus.h"
#include "datahub/DataHub.h"
#include "trading/BrokerRegistry.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {

constexpr int kAutoRunsWorkflowLimit = 25; // workflows asked from the audit log
constexpr int kAutoRunsMaxRows = 50;       // rows rendered (active rows sort first)
constexpr int kAutoRunsColName = 0;
constexpr int kAutoRunsColKind = 1;
constexpr int kAutoRunsColState = 2;
constexpr int kAutoRunsColLast = 3;
constexpr int kAutoRunsColResult = 4;

const QString kAutoRunsMetricsPrefix = QStringLiteral("algo:metrics:");
const QString kAutoRunsStatePrefix = QStringLiteral("algo:state:");

// algo_deployments.status (pending|starting|running|paused|stopped|error|crashed)
// → the widget's state vocabulary.
QString auto_runs_algo_state(const QString& status) {
    if (status == QLatin1String("running"))
        return QStringLiteral("running");
    if (status == QLatin1String("paused"))
        return QStringLiteral("paused");
    if (status == QLatin1String("starting") || status == QLatin1String("pending"))
        return QStringLiteral("starting");
    if (status == QLatin1String("error") || status == QLatin1String("crashed"))
        return QStringLiteral("failed");
    return QStringLiteral("stopped");
}

bool auto_runs_is_active(const QString& state) {
    return state == QLatin1String("running") || state == QLatin1String("starting") || state == QLatin1String("paused");
}

// SQLite datetime('now') / CURRENT_TIMESTAMP text is UTC "yyyy-MM-dd HH:mm:ss".
QDateTime auto_runs_parse_utc(const QString& text) {
    if (text.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(text, Qt::ISODate);
    if (!dt.isValid())
        return {};
    dt.setTimeZone(QTimeZone::utc());
    return dt.toLocalTime();
}

QString auto_runs_format_time(const QDateTime& local) {
    if (!local.isValid())
        return QStringLiteral("--");
    if (local.date() == QDate::currentDate())
        return local.toString(QStringLiteral("HH:mm:ss"));
    return local.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

// Deployment P&L is in the broker's own currency; no broker → global currency.
QString auto_runs_broker_currency(const QString& broker_id) {
    if (broker_id.isEmpty())
        return {};
    auto* broker = fincept::trading::BrokerRegistry::instance().get(broker_id);
    return broker ? broker->profile().currency : QString();
}

QString auto_runs_money(double v, const QString& currency) {
    return (v > 0 ? QStringLiteral("+") : QString()) + cur::money(v, false, currency);
}

} // namespace

AutomationRunsWidget::AutomationRunsWidget(const QJsonObject& /*cfg*/, QWidget* parent)
    : BaseWidget(tr("AUTOMATION RUNS"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // One stylesheet on this root styles everything below it (objectName
    // selectors) — set once in apply_styles(), never per row.
    root_ = new QWidget(this);
    root_->setObjectName(QStringLiteral("autoRunsRoot"));
    auto* rl = new QVBoxLayout(root_);
    rl->setContentsMargins(6, 4, 6, 4);
    rl->setSpacing(4);

    status_label_ = new QLabel(root_);
    status_label_->setObjectName(QStringLiteral("autoRunsStatus"));
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    status_label_->hide();
    rl->addWidget(status_label_);

    table_ = new QTableWidget(0, 5, root_);
    table_->setObjectName(QStringLiteral("autoRunsTable"));
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(22);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideRight);
    auto* hh = table_->horizontalHeader();
    hh->setSectionResizeMode(kAutoRunsColName, QHeaderView::Stretch);
    hh->setSectionResizeMode(kAutoRunsColKind, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(kAutoRunsColState, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(kAutoRunsColLast, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(kAutoRunsColResult, QHeaderView::ResizeToContents);
    hh->setHighlightSections(false);
    rl->addWidget(table_, 1);
    vl->addWidget(root_, 1);

    // Double-click → the screen that owns the automation (navigation only).
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        const auto* it = table_->item(row, kAutoRunsColName);
        const QString screen_id = it ? it->data(Qt::UserRole).toString() : QString();
        if (!screen_id.isEmpty())
            EventBus::instance().publish(QStringLiteral("nav.switch_screen"), QVariantMap{{"screen_id", screen_id}});
    });

    // ── Service signals (context `this` → auto-disconnect on destroy) ──
    auto& engine = algo::AlgoEngine::instance();
    connect(&engine, &algo::AlgoEngine::deployments_loaded, this, &AutomationRunsWidget::on_deployments_loaded);
    // A deployment appeared or went away: the list structure changed.
    connect(&engine, &algo::AlgoEngine::deployment_started, this, [this](const QString& /*id*/) {
        if (isVisible())
            algo::AlgoEngine::instance().list_deployments();
    });
    connect(&engine, &algo::AlgoEngine::deployment_stopped, this, [this](const QString& /*id*/) {
        if (isVisible())
            algo::AlgoEngine::instance().list_deployments();
    });

    auto& wf = workflow::WorkflowService::instance();
    connect(&wf, &workflow::WorkflowService::recent_runs_listed, this, &AutomationRunsWidget::on_workflow_runs);
    // The audit row is written before either signal is emitted, so a re-list
    // already sees the new state.
    connect(&wf, &workflow::WorkflowService::execution_started, this, [this](const QString& /*workflow_id*/) {
        if (isVisible())
            workflow::WorkflowService::instance().list_recent_runs(kAutoRunsWorkflowLimit);
    });
    connect(&wf, &workflow::WorkflowService::execution_finished, this,
            [this](const workflow::WorkflowExecutionResult& /*result*/) {
                if (isVisible())
                    workflow::WorkflowService::instance().list_recent_runs(kAutoRunsWorkflowLimit);
            });

    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        set_loading(true);
        request_all();
    });

    retranslateUi();
    apply_styles();
    set_loading(true);
}

void AutomationRunsWidget::apply_styles() {
    root_->setStyleSheet(
        QString("QWidget#autoRunsRoot{background:transparent;}"
                "QLabel#autoRunsStatus{color:%1;font-size:10px;padding:16px;background:transparent;}"
                "QTableWidget#autoRunsTable{background:transparent;alternate-background-color:%2;color:%3;"
                "border:none;font-size:10px;}"
                "QTableWidget#autoRunsTable::item{padding:2px 4px;border-bottom:1px solid %4;}"
                "QHeaderView::section{background:%5;color:%1;border:none;border-bottom:1px solid %4;"
                "padding:2px 4px;font-size:9px;font-weight:bold;}"
                "QScrollBar:vertical{width:5px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%6;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::ROW_ALT(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
}

void AutomationRunsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    // Cached rows first (P11), then a fresh read of both sources.
    if (have_algo_ || have_workflows_)
        render();
    request_all();
    if (!hub_active_)
        hub_subscribe();
}

void AutomationRunsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e); // drops every hub subscription this tile owns
    hub_active_ = false;
}

void AutomationRunsWidget::hub_subscribe() {
    auto& hub = datahub::DataHub::instance();
    // Push-only topics published by AlgoEngine for each running deployment.
    hub.subscribe_pattern(this, kAutoRunsMetricsPrefix + QLatin1Char('*'),
                          [this](const QString& topic, const QVariant& v) { on_algo_metrics(topic, v); });
    hub.subscribe_pattern(this, kAutoRunsStatePrefix + QLatin1Char('*'),
                          [this](const QString& topic, const QVariant& v) { on_algo_state(topic, v); });
    hub_active_ = true;
}

void AutomationRunsWidget::request_all() {
    algo_pending_ = true;
    workflow_pending_ = true;
    algo::AlgoEngine::instance().list_deployments();
    workflow::WorkflowService::instance().list_recent_runs(kAutoRunsWorkflowLimit);
}

void AutomationRunsWidget::finish_loading_if_done() {
    if (!algo_pending_ && !workflow_pending_)
        set_loading(false);
}

void AutomationRunsWidget::on_deployments_loaded(const QVector<services::algo::AlgoDeployment>& deployments) {
    algo_pending_ = false;
    have_algo_ = true;
    algo_rows_.clear();
    algo_index_.clear();
    algo_rows_.reserve(deployments.size());
    for (const auto& d : deployments) {
        RunRow r;
        r.kind = Kind::Algo;
        r.id = d.id;
        r.symbol = d.symbol.isEmpty() ? d.underlying : d.symbol;
        r.name = d.strategy_name.isEmpty() ? d.id.left(8) : d.strategy_name;
        r.state = auto_runs_algo_state(d.status);
        r.live = d.is_live();
        r.last_run = auto_runs_parse_utc(d.updated_at.isEmpty() ? d.created_at : d.updated_at);
        r.timeframe = d.timeframe;
        r.has_pnl = true;
        r.realized = d.total_pnl;
        r.unrealized = d.unrealized_pnl;
        r.trades = d.total_trades;
        r.win_rate = d.win_rate;
        r.currency = auto_runs_broker_currency(d.broker_id);
        r.note = d.error_message;
        algo_index_.insert(r.id, static_cast<int>(algo_rows_.size()));
        algo_rows_.append(r);
    }
    if (isVisible())
        schedule_render([this]() { render(); });
    finish_loading_if_done();
}

void AutomationRunsWidget::on_workflow_runs(const QVector<workflow::WorkflowService::RunSummary>& runs) {
    workflow_pending_ = false;
    have_workflows_ = true;
    // Only one top-level run executes at a time; a latest event of
    // "workflow_started" with nothing executing means the run never finished
    // (e.g. the app closed mid-run).
    const bool executing = workflow::WorkflowService::instance().is_executing();
    workflow_rows_.clear();
    workflow_rows_.reserve(runs.size());
    for (const auto& run : runs) {
        RunRow r;
        r.kind = Kind::Workflow;
        r.id = run.workflow_id;
        r.name = run.workflow_name;
        r.live = !run.paper;
        r.last_run = run.timestamp.toLocalTime();
        r.note = run.details;
        if (run.last_action == QLatin1String("workflow_completed")) {
            r.state = QStringLiteral("succeeded");
        } else if (run.last_action == QLatin1String("workflow_failed")) {
            r.state = run.details.endsWith(QLatin1String(": Execution stopped")) ? QStringLiteral("stopped")
                                                                                 : QStringLiteral("failed");
        } else {
            r.state = executing ? QStringLiteral("running") : QStringLiteral("stopped");
            r.interrupted = !executing;
        }
        workflow_rows_.append(r);
    }
    if (isVisible())
        schedule_render([this]() { render(); });
    finish_loading_if_done();
}

void AutomationRunsWidget::on_algo_metrics(const QString& topic, const QVariant& value) {
    if (!value.canConvert<algo::AlgoMetrics>())
        return;
    const auto it = algo_index_.constFind(topic.mid(kAutoRunsMetricsPrefix.size()));
    if (it == algo_index_.constEnd())
        return; // not listed yet — deployment_started triggers a re-list
    const auto m = value.value<algo::AlgoMetrics>();
    RunRow& r = algo_rows_[it.value()];
    r.has_pnl = true;
    r.realized = m.total_pnl;
    r.unrealized = m.unrealized_pnl;
    r.trades = m.total_trades;
    r.win_rate = m.win_rate;
    schedule_render([this]() { render(); });
}

void AutomationRunsWidget::on_algo_state(const QString& topic, const QVariant& value) {
    const auto it = algo_index_.constFind(topic.mid(kAutoRunsStatePrefix.size()));
    if (it == algo_index_.constEnd())
        return;
    algo_rows_[it.value()].state = auto_runs_algo_state(value.toString());
    schedule_render([this]() { render(); });
}

QString AutomationRunsWidget::state_label(const QString& state) const {
    if (state == QLatin1String("running"))
        return tr("RUNNING");
    if (state == QLatin1String("starting"))
        return tr("STARTING");
    if (state == QLatin1String("paused"))
        return tr("PAUSED");
    if (state == QLatin1String("succeeded"))
        return tr("SUCCEEDED");
    if (state == QLatin1String("failed"))
        return tr("FAILED");
    return tr("STOPPED");
}

QString AutomationRunsWidget::state_color(const QString& state) const {
    if (state == QLatin1String("running") || state == QLatin1String("succeeded"))
        return ui::colors::POSITIVE();
    if (state == QLatin1String("starting") || state == QLatin1String("paused"))
        return ui::colors::WARNING();
    if (state == QLatin1String("failed"))
        return ui::colors::NEGATIVE();
    return ui::colors::TEXT_TERTIARY();
}

void AutomationRunsWidget::render() {
    QVector<const RunRow*> rows;
    rows.reserve(algo_rows_.size() + workflow_rows_.size());
    for (const auto& r : algo_rows_)
        rows.append(&r);
    for (const auto& r : workflow_rows_)
        rows.append(&r);
    // Active automations first, then most recent activity.
    std::stable_sort(rows.begin(), rows.end(), [](const RunRow* a, const RunRow* b) {
        const bool aa = auto_runs_is_active(a->state);
        const bool ba = auto_runs_is_active(b->state);
        if (aa != ba)
            return aa;
        return a->last_run > b->last_run;
    });
    if (rows.size() > kAutoRunsMaxRows)
        rows.resize(kAutoRunsMaxRows);

    if (rows.isEmpty()) {
        table_->setRowCount(0);
        table_->hide();
        status_label_->setText(
            tr("No automations yet. Deploy a strategy in Algo Trading or run a workflow in the Node Editor."));
        status_label_->show();
        return;
    }
    status_label_->hide();
    table_->show();

    const QString algo_screen = QStringLiteral("algo_trading");
    const QString workflow_screen = QStringLiteral("node_editor");
    const QString open_algo = tr("Double-click to open Algo Trading");
    const QString open_workflow = tr("Double-click to open the Node Editor");
    const QColor dim(ui::colors::TEXT_SECONDARY());
    const QColor muted(ui::colors::TEXT_TERTIARY());
    const QColor warn(ui::colors::WARNING());
    const QColor neg(ui::colors::NEGATIVE());
    const QColor pos(ui::colors::POSITIVE());

    table_->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < rows.size(); ++i) {
        const RunRow& r = *rows[i];
        const bool algo = r.kind == Kind::Algo;

        QString name = r.name;
        if (name.isEmpty())
            name = r.id.isEmpty() ? tr("Unsaved workflow") : tr("Workflow %1").arg(r.id.left(8));
        if (algo && !r.symbol.isEmpty())
            name += QStringLiteral(" · ") + r.symbol;

        // Tooltip: built here (not at data arrival) so a language switch
        // re-translates it on the next render.
        QStringList tip;
        tip << name;
        tip << tr("Mode: %1").arg(r.live ? tr("LIVE") : tr("PAPER"));
        if (algo) {
            if (!r.timeframe.isEmpty())
                tip << tr("Timeframe: %1").arg(r.timeframe);
            tip << tr("Trades: %1 · Win rate: %2%").arg(r.trades).arg(r.win_rate, 0, 'f', 1);
            tip << tr("Realized: %1 · Unrealized: %2")
                       .arg(auto_runs_money(r.realized, r.currency), auto_runs_money(r.unrealized, r.currency));
            if (!r.note.isEmpty())
                tip << tr("Error: %1").arg(r.note);
        } else {
            if (r.interrupted)
                tip << tr("The run did not finish (the app may have closed during it).");
            if (!r.note.isEmpty())
                tip << r.note;
        }
        if (r.last_run.isValid())
            tip << tr("Last activity: %1").arg(r.last_run.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        tip << (algo ? open_algo : open_workflow);
        const QString tooltip = tip.join(QLatin1Char('\n'));

        auto* name_it = new QTableWidgetItem(name);
        name_it->setData(Qt::UserRole, algo ? algo_screen : workflow_screen);

        QString kind_text = algo ? tr("ALGO") : tr("WORKFLOW");
        if (r.live)
            kind_text += QStringLiteral(" · ") + tr("LIVE");
        auto* kind_it = new QTableWidgetItem(kind_text);
        kind_it->setForeground(r.live ? warn : dim);

        auto* state_it = new QTableWidgetItem(state_label(r.state));
        state_it->setForeground(QColor(state_color(r.state)));
        QFont bold = state_it->font();
        bold.setBold(true);
        state_it->setFont(bold);

        auto* last_it = new QTableWidgetItem(auto_runs_format_time(r.last_run));
        last_it->setForeground(dim);

        QTableWidgetItem* result_it = nullptr;
        if (algo) {
            const double pnl = r.realized + r.unrealized;
            result_it = new QTableWidgetItem(r.has_pnl ? auto_runs_money(pnl, r.currency) : QStringLiteral("--"));
            result_it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            result_it->setForeground(!r.has_pnl ? muted : pnl > 0 ? pos : pnl < 0 ? neg : dim);
        } else {
            QString text;
            if (r.state == QLatin1String("running"))
                text = QStringLiteral("--");
            else if (r.interrupted)
                text = tr("Did not finish");
            else
                text = r.note.isEmpty() ? QStringLiteral("--") : r.note;
            result_it = new QTableWidgetItem(text);
            result_it->setForeground(r.state == QLatin1String("failed") ? neg : dim);
        }

        QTableWidgetItem* cells[] = {name_it, kind_it, state_it, last_it, result_it};
        for (int c = 0; c < 5; ++c) {
            cells[c]->setToolTip(tooltip);
            table_->setItem(i, c, cells[c]);
        }
    }
}

void AutomationRunsWidget::on_theme_changed() {
    apply_styles();
    if (have_algo_ || have_workflows_)
        render(); // per-cell colours come from the theme tokens
}

void AutomationRunsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("AUTOMATION RUNS"));
    table_->setHorizontalHeaderLabels({tr("NAME"), tr("KIND"), tr("STATE"), tr("LAST RUN"), tr("P&L / RESULT")});
    table_->setAccessibleName(tr("Automation runs"));
    if (have_algo_ || have_workflows_)
        render();
}

} // namespace fincept::screens::widgets
