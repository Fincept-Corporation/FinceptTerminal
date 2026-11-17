// AgentRunsPanel.cpp — RUNS section: the one run list of every engine.

#include "screens/agent_config/AgentRunsPanel.h"

#include "core/logging/Logger.h"
#include "screens/agent_config/AgentRunCommon.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/agents/AgentService.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/harness/HarnessRegistry.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace ca = services::coding_agents;
namespace ar = agent_runs;

namespace agent_runs_detail {

constexpr int kAgenticListLimit = 50;
constexpr int kAgenticRefreshMs = 3000;

QColor status_color(const QString& status) {
    if (status == QLatin1String("queued"))
        return QColor(ui::colors::INFO());
    if (status == QLatin1String("running"))
        return QColor(ui::colors::AMBER());
    if (status == QLatin1String("waiting"))
        return QColor(ui::colors::WARNING());
    if (status == QLatin1String("succeeded"))
        return QColor(ui::colors::SUCCESS());
    if (status == QLatin1String("failed") || status == QLatin1String("interrupted"))
        return QColor(ui::colors::DANGER());
    if (status == QLatin1String("paused"))
        return QColor(ui::colors::TEXT_SECONDARY());
    return QColor(ui::colors::TEXT_TERTIARY());
}

QColor engine_color(ar::Engine e) {
    switch (e) {
        case ar::Engine::Harness:
            return QColor(ui::colors::CYAN());
        case ar::Engine::Agentic:
            return QColor(ui::colors::AMBER());
        case ar::Engine::Chat:
            break;
    }
    return QColor(ui::colors::INFO());
}

QString tr_runs(const char* text) {
    return QCoreApplication::translate("fincept::screens::AgentRunsTableModel", text);
}

/// The status word of a common status (non-harness rows; harness rows carry the
/// failure class from awb_run_pill_text()).
QString pill_for(const QString& status, const QString& raw) {
    if (raw == QLatin1String("failed_budget") || raw == QLatin1String("budget_stopped"))
        return tr_runs("BUDGET STOP");
    if (raw == QLatin1String("waiting_approval"))
        return tr_runs("NEEDS APPROVAL");
    if (status == QLatin1String("waiting"))
        return tr_runs("NEEDS INPUT");
    if (status == QLatin1String("queued"))
        return tr_runs("QUEUED");
    if (status == QLatin1String("running"))
        return tr_runs("RUNNING");
    if (status == QLatin1String("paused"))
        return tr_runs("PAUSED");
    if (status == QLatin1String("succeeded"))
        return tr_runs("SUCCEEDED");
    if (status == QLatin1String("cancelled"))
        return tr_runs("CANCELLED");
    if (status == QLatin1String("interrupted"))
        return tr_runs("INTERRUPTED");
    return tr_runs("FAILED");
}

} // namespace agent_runs_detail

using namespace agent_runs_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Model
// ═════════════════════════════════════════════════════════════════════════════

AgentRunsTableModel::AgentRunsTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int AgentRunsTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int AgentRunsTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant AgentRunsTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColStatus:
            return tr("STATUS");
        case ColEngine:
            return tr("ENGINE");
        case ColTitle:
            return tr("TITLE");
        case ColModel:
            return tr("MODEL");
        case ColStarted:
            return tr("STARTED");
        case ColDuration:
            return tr("DURATION");
        case ColTokens:
            return tr("TOKENS");
        case ColCost:
            return tr("COST");
        case ColSchedule:
            return tr("SCHEDULE / ORIGIN");
        default:
            return {};
    }
}

QVariant AgentRunsTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const ar::RunRow& r = rows_[index.row()];
    if (role == kRunKeyRole)
        return r.key();
    if (role == kRowIndexRole)
        return index.row();

    const int col = index.column();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (role == Qt::DisplayRole) {
        switch (col) {
            case ColStatus:
                return r.pill;
            case ColEngine:
                return r.engine_label;
            case ColTitle:
                return r.title;
            case ColModel:
                return r.model.isEmpty() ? QStringLiteral("—") : r.model;
            case ColStarted:
                return r.when_ms() > 0
                           ? QDateTime::fromMSecsSinceEpoch(r.when_ms()).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                           : QStringLiteral("—");
            case ColDuration:
                return r.started_ms > 0 ? awb_duration(r.duration_ms(now)) : QStringLiteral("—");
            case ColTokens:
                if (r.tokens_in > 0 || r.tokens_out > 0) {
                    if (r.tokens_out < 0)
                        return awb_tokens(r.tokens_in);
                    return QStringLiteral("%1 / %2").arg(awb_tokens(qMax<qint64>(0, r.tokens_in)),
                                                         awb_tokens(r.tokens_out));
                }
                return QStringLiteral("—");
            case ColCost:
                if (r.cost < 0)
                    return QStringLiteral("—");
                return r.cost_in_credits ? tr("%1 cr").arg(r.cost, 0, 'f', 2)
                                         : QStringLiteral("$%1").arg(r.cost, 0, 'f', 4);
            case ColSchedule:
                return r.origin.isEmpty() ? QStringLiteral("—") : r.origin;
            default:
                return {};
        }
    }
    if (role == kSortRole) {
        switch (col) {
            case ColStatus:
                return (ar::is_active_status(r.status) ? QStringLiteral("0") : QStringLiteral("1")) + r.status;
            case ColEngine:
                return r.engine_label.toLower();
            case ColTitle:
                return r.title.toLower();
            case ColModel:
                return r.model.toLower();
            case ColStarted:
                return r.when_ms();
            case ColDuration:
                return r.duration_ms(now);
            case ColTokens:
                return qMax<qint64>(0, r.tokens_in) + qMax<qint64>(0, r.tokens_out);
            case ColCost:
                return r.cost;
            case ColSchedule:
                return r.origin.toLower();
            default:
                return {};
        }
    }
    if (role == Qt::ForegroundRole) {
        if (col == ColStatus)
            return status_color(r.status);
        if (col == ColEngine) {
            if (r.engine == ar::Engine::Harness && !harness::HarnessRegistry::instance().is_known(r.engine_detail))
                return QColor(ui::colors::WARNING());
            return engine_color(r.engine);
        }
        if (col == ColTitle)
            return QColor(ui::colors::TEXT_PRIMARY());
        return QColor(ui::colors::TEXT_SECONDARY());
    }
    if (role == Qt::TextAlignmentRole && (col == ColDuration || col == ColCost || col == ColTokens))
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ToolTipRole) {
        if (col == ColTitle)
            return r.search.left(800);
        if (col == ColStatus && !r.error.isEmpty())
            return r.error.left(800);
        if (col == ColEngine) {
            switch (r.engine) {
                case ar::Engine::Harness:
                    return tr("Coding-agent harness run (Claude Code, Codex …) — transcript in the inspector");
                case ar::Engine::Agentic:
                    return tr("Local agentic task — plan, steps, budget and replies in the inspector");
                case ar::Engine::Chat:
                    return tr("Chat Mode run — opened in Chat Mode");
            }
        }
    }
    return {};
}

ar::RunRow AgentRunsTableModel::harness_row(const QString& id, bool* ok) const {
    auto& svc = ca::CodingAgentService::instance();
    ar::RunRow row;
    const ca::CodingAgentRun* r = svc.find_run(id);
    *ok = r != nullptr;
    if (!r)
        return row;
    row.engine = ar::Engine::Harness;
    row.id = id;
    row.title = r->title;
    row.status = ca::status_id(r->status);
    row.pill = awb_run_pill_text(*r);
    row.engine_detail = r->harness;
    row.engine_label = r->harness_version.isEmpty()
                           ? awb_harness_name(r->harness)
                           : QStringLiteral("%1 %2").arg(awb_harness_name(r->harness), r->harness_version);
    QString m = r->model.isEmpty() ? tr("CLI default") : r->model;
    if (!r->effort.isEmpty())
        m += QStringLiteral(" · ") + r->effort;
    row.model = m;
    if (!r->schedule_id.isEmpty()) {
        const ca::CodingAgentSchedule* s = svc.find_schedule(r->schedule_id);
        row.origin = s ? s->name : tr("(deleted schedule)");
    } else if (r->origin == QLatin1String("mcp")) {
        row.origin = tr("AI assistant");
    } else if (r->origin == QLatin1String("follow_up")) {
        row.origin = tr("follow-up");
    }
    row.search = r->title + QLatin1Char(' ') + r->prompt.left(2000) + QLatin1Char(' ') + r->model;
    row.error = r->error;
    row.created_ms = r->created_at_ms;
    row.started_ms = r->started_at_ms;
    row.finished_ms = r->finished_at_ms;
    if (r->input_tokens > 0 || r->output_tokens > 0) {
        row.tokens_in = r->input_tokens;
        row.tokens_out = r->output_tokens;
    }
    row.cost = r->cost_usd.value_or(-1.0);
    return row;
}

ar::RunRow AgentRunsTableModel::chat_row(const QString& id, bool* ok) const {
    ar::RunRow row;
    const agent_tasks::TaskRun* r = agent_tasks::TaskRunService::instance().find_run(id);
    *ok = r != nullptr;
    if (!r)
        return row;
    row.engine = ar::Engine::Chat;
    row.id = id;
    row.title = r->title.isEmpty() ? r->prompt.left(120) : r->title;
    const QString raw = agent_tasks::run_status_id(r->status);
    row.status = ar::normalize_chat_status(raw);
    row.pill = pill_for(row.status, raw);
    row.engine_detail = r->engine;
    row.engine_label = r->engine == QLatin1String(agent_tasks::kEngineLocal) ? tr("CHAT · LOCAL") : tr("CHAT · CLOUD");
    row.model = r->mode.isEmpty() ? QString() : r->mode;
    row.origin = r->origin == QLatin1String(agent_tasks::kOriginSchedule) ? tr("schedule") : tr("Chat Mode");
    row.search = row.title + QLatin1Char(' ') + r->prompt.left(2000);
    row.error = r->error;
    row.created_ms = r->created_at_ms;
    row.started_ms = r->started_at_ms;
    row.finished_ms = r->finished_at_ms;
    if (r->tokens_used > 0)
        row.tokens_in = r->tokens_used;
    if (r->credits_spent > 0) {
        row.cost = r->credits_spent;
        row.cost_in_credits = true;
    }
    return row;
}

ar::RunRow AgentRunsTableModel::agentic_row(const QJsonObject& t) const {
    ar::RunRow row;
    row.engine = ar::Engine::Agentic;
    row.id = t.value(QStringLiteral("id")).toString();
    const QString query = t.value(QStringLiteral("query")).toString();
    row.title = query.left(160);
    const QString raw = t.value(QStringLiteral("status")).toString();
    row.status = ar::normalize_agentic_status(raw);
    row.pill = pill_for(row.status, raw);
    row.engine_detail = QStringLiteral("local");
    row.engine_label = tr("AGENTIC");
    const int steps = static_cast<int>(t.value(QStringLiteral("steps")).toArray().size());
    const int current = t.value(QStringLiteral("current_step")).toInt();
    row.model = tr("step %1 / %2").arg(current).arg(qMax(steps, current));
    const QJsonObject cfg = t.value(QStringLiteral("config")).toObject();
    const QString agent = cfg.value(QStringLiteral("agent_id")).toString();
    row.origin = agent.isEmpty() ? tr("agentic task") : agent;
    row.search = query.left(2000) + QLatin1Char(' ') + agent;
    row.error = t.value(QStringLiteral("error")).toString();
    row.created_ms = ar::parse_task_time_ms(t.value(QStringLiteral("created_at")).toVariant().toString());
    row.started_ms = row.created_ms;
    if (ar::is_finished_status(row.status))
        row.finished_ms = ar::parse_task_time_ms(t.value(QStringLiteral("updated_at")).toVariant().toString());
    const QJsonObject budget = t.value(QStringLiteral("budget")).toObject();
    const double tokens = budget.value(QStringLiteral("tokens_used")).toDouble(-1);
    if (tokens > 0)
        row.tokens_in = static_cast<qint64>(tokens);
    const double cost = budget.value(QStringLiteral("cost_used")).toDouble(-1);
    if (cost >= 0 && budget.contains(QStringLiteral("cost_used")))
        row.cost = cost;
    return row;
}

void AgentRunsTableModel::reindex() {
    index_.clear();
    index_.reserve(rows_.size());
    for (int i = 0; i < rows_.size(); ++i)
        index_.insert(rows_[i].key(), i);
}

void AgentRunsTableModel::reload() {
    QVector<ar::RunRow> rows;
    auto& svc = ca::CodingAgentService::instance();
    for (const QString& id : svc.run_ids()) {
        bool ok = false;
        ar::RunRow r = harness_row(id, &ok);
        if (ok)
            rows.append(std::move(r));
    }
    auto& tasks = agent_tasks::TaskRunService::instance();
    for (const QString& id : tasks.run_ids()) {
        bool ok = false;
        ar::RunRow r = chat_row(id, &ok);
        if (ok)
            rows.append(std::move(r));
    }
    if (agentic_enabled_) {
        for (auto it = agentic_.cbegin(); it != agentic_.cend(); ++it)
            rows.append(agentic_row(it.value()));
    }
    std::stable_sort(rows.begin(), rows.end(), ar::run_before);
    beginResetModel();
    rows_ = std::move(rows);
    reindex();
    endResetModel();
}

void AgentRunsTableModel::upsert(const ar::RunRow& row) {
    const QString key = row.key();
    const auto it = index_.constFind(key);
    if (it != index_.cend()) {
        rows_[*it] = row;
        emit dataChanged(index(*it, 0), index(*it, ColCount - 1));
        return;
    }
    // New rows go first (the proxy sorts the view anyway).
    beginInsertRows({}, 0, 0);
    rows_.prepend(row);
    reindex();
    endInsertRows();
}

void AgentRunsTableModel::remove_key(const QString& key) {
    const auto it = index_.constFind(key);
    if (it == index_.cend())
        return;
    const int row = *it;
    beginRemoveRows({}, row, row);
    rows_.remove(row);
    reindex();
    endRemoveRows();
}

void AgentRunsTableModel::upsert_harness(const QString& id) {
    bool ok = false;
    const ar::RunRow r = harness_row(id, &ok);
    if (ok)
        upsert(r);
    else
        remove_key(ar::run_key(ar::Engine::Harness, id));
}

void AgentRunsTableModel::upsert_chat(const QString& id) {
    bool ok = false;
    const ar::RunRow r = chat_row(id, &ok);
    if (ok)
        upsert(r);
    else
        remove_key(ar::run_key(ar::Engine::Chat, id));
}

void AgentRunsTableModel::set_agentic(const QJsonArray& tasks, bool replace) {
    QSet<QString> seen;
    for (const auto& v : tasks) {
        const QJsonObject t = v.toObject();
        const QString id = t.value(QStringLiteral("id")).toString();
        if (id.isEmpty())
            continue;
        seen.insert(id);
        agentic_.insert(id, t);
        if (agentic_enabled_)
            upsert(agentic_row(t));
    }
    if (!replace)
        return;
    const QStringList known = agentic_.keys();
    for (const QString& id : known) {
        if (seen.contains(id))
            continue;
        agentic_.remove(id);
        remove_key(ar::run_key(ar::Engine::Agentic, id));
    }
}

void AgentRunsTableModel::set_agentic_enabled(bool enabled) {
    if (agentic_enabled_ == enabled)
        return;
    agentic_enabled_ = enabled;
    reload();
}

const ar::RunRow* AgentRunsTableModel::row_by_key(const QString& key) const {
    const auto it = index_.constFind(key);
    return it == index_.cend() ? nullptr : &rows_[*it];
}

QStringList AgentRunsTableModel::harness_ids_in_rows() const {
    QStringList out;
    for (const ar::RunRow& r : rows_) {
        if (r.engine == ar::Engine::Harness && !out.contains(r.engine_detail))
            out << r.engine_detail;
    }
    return out;
}

int AgentRunsTableModel::count_with_status(const QString& status) const {
    return static_cast<int>(
        std::count_if(rows_.cbegin(), rows_.cend(), [&status](const ar::RunRow& r) { return r.status == status; }));
}

int AgentRunsTableModel::count_active() const {
    return static_cast<int>(
        std::count_if(rows_.cbegin(), rows_.cend(), [](const ar::RunRow& r) { return ar::is_active_status(r.status); }));
}

bool AgentRunsTableModel::refresh_active_durations() {
    bool any = false;
    for (int row = 0; row < rows_.size(); ++row) {
        if (rows_[row].status == QLatin1String("running") && rows_[row].started_ms > 0) {
            any = true;
            emit dataChanged(index(row, ColDuration), index(row, ColDuration), {Qt::DisplayRole});
        }
    }
    return any;
}

void AgentRunsTableModel::retranslate_headers() {
    emit headerDataChanged(Qt::Horizontal, 0, ColCount - 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// Filter proxy
// ═════════════════════════════════════════════════════════════════════════════

AgentRunsFilterProxy::AgentRunsFilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(AgentRunsTableModel::kSortRole);
}

void AgentRunsFilterProxy::set_filter(const ar::RunFilter& filter) {
    filter_ = filter;
    invalidateFilter();
}

bool AgentRunsFilterProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    const auto* model = qobject_cast<const AgentRunsTableModel*>(sourceModel());
    if (!model)
        return true;
    const QString key = model->index(source_row, 0, source_parent).data(AgentRunsTableModel::kRunKeyRole).toString();
    const ar::RunRow* r = model->row_by_key(key);
    return r && filter_.accepts(*r);
}

// ═════════════════════════════════════════════════════════════════════════════
// Chip delegate
// ═════════════════════════════════════════════════════════════════════════════

AgentRunStatusDelegate::AgentRunStatusDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void AgentRunStatusDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QString text = opt.text;
    opt.text.clear();
    QStyledItemDelegate::paint(painter, opt, index); // background / selection only

    const QColor color = index.data(Qt::ForegroundRole).value<QColor>();
    painter->save();
    QFont f = opt.font;
    f.setBold(true);
    f.setPixelSize(ui::fonts::font_px(-4));
    painter->setFont(f);
    const QFontMetrics fm(f);
    const int w = qMin(fm.horizontalAdvance(text) + 14, opt.rect.width() - 8);
    const int h = fm.height() + 4;
    const QRect pill(opt.rect.left() + 6, opt.rect.center().y() - h / 2, w, h);
    painter->setPen(color);
    painter->drawRect(pill.adjusted(0, 0, -1, -1));
    painter->drawText(pill, Qt::AlignCenter, fm.elidedText(text, Qt::ElideRight, w - 6));
    painter->restore();
}

// ═════════════════════════════════════════════════════════════════════════════
// Panel
// ═════════════════════════════════════════════════════════════════════════════

AgentRunsPanel::AgentRunsPanel(QWidget* parent) : QWidget(parent) {
    model_ = new AgentRunsTableModel(this);
    proxy_ = new AgentRunsFilterProxy(this);
    proxy_->setSourceModel(model_);

    dirty_timer_ = new QTimer(this);
    dirty_timer_->setSingleShot(true);
    dirty_timer_->setInterval(250);
    connect(dirty_timer_, &QTimer::timeout, this, &AgentRunsPanel::flush_dirty);
    tick_timer_ = new QTimer(this);
    tick_timer_->setInterval(1000);
    connect(tick_timer_, &QTimer::timeout, this, &AgentRunsPanel::on_tick);
    agentic_timer_ = new QTimer(this);
    agentic_timer_->setSingleShot(true);
    agentic_timer_->setInterval(kAgenticRefreshMs);
    connect(agentic_timer_, &QTimer::timeout, this, [this]() {
        if (!agentic_enabled_ || !isVisible())
            return;
        agentic_requested_ = true;
        agentic_stale_ = false;
        services::AgentService::instance().list_tasks(QString(), kAgenticListLimit);
    });

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(6);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(6);
    status_combo_ = new QComboBox(this);
    for (const char* id : {"", "active", "waiting", "paused", "succeeded", "failed", "cancelled", "interrupted"})
        status_combo_->addItem(QString(), QString::fromLatin1(id));
    engine_combo_ = new QComboBox(this);
    engine_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    engine_combo_->setMinimumContentsLength(12);
    search_edit_ = new QLineEdit(this);
    search_edit_->setClearButtonEnabled(true);
    connect(status_combo_, &QComboBox::currentIndexChanged, this, [this](int) { apply_filters(); });
    connect(engine_combo_, &QComboBox::currentIndexChanged, this, [this](int) { apply_filters(); });
    connect(search_edit_, &QLineEdit::textChanged, this, [this](const QString&) { apply_filters(); });
    bar->addWidget(status_combo_);
    bar->addWidget(engine_combo_);
    bar->addWidget(search_edit_, 1);
    count_lbl_ = new QLabel(this);
    count_lbl_->setObjectName(QStringLiteral("awbHint"));
    bar->addWidget(count_lbl_);
    v->addLayout(bar);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    open_btn_ = new QPushButton(this);
    open_btn_->setObjectName(QStringLiteral("awbBtn"));
    connect(open_btn_, &QPushButton::clicked, this, &AgentRunsPanel::on_open);
    delete_btn_ = new QPushButton(this);
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the selected runs
    delete_btn_->setObjectName(QStringLiteral("awbDangerBtn"));
    connect(delete_btn_, &QPushButton::clicked, this, &AgentRunsPanel::on_delete);
    clear_btn_ = new QPushButton(this);
    clear_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes every finished harness run
    clear_btn_->setObjectName(QStringLiteral("awbBtn"));
    connect(clear_btn_, &QPushButton::clicked, this, &AgentRunsPanel::on_clear_finished);
    refresh_btn_ = new QPushButton(this);
    refresh_btn_->setObjectName(QStringLiteral("awbBtn"));
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        model_->reload();
        agentic_timer_->stop();
        if (agentic_enabled_) {
            agentic_requested_ = true;
            services::AgentService::instance().list_tasks(QString(), kAgenticListLimit);
        }
    });
    actions->addWidget(open_btn_);
    actions->addWidget(delete_btn_);
    actions->addWidget(clear_btn_);
    actions->addWidget(refresh_btn_);
    actions->addStretch(1);
    // Agentic Mode's own tools (were on the AGENTIC tab).
    schedule_task_btn_ = new QPushButton(this);
    schedule_task_btn_->setObjectName(QStringLiteral("awbBtn"));
    schedule_task_btn_->setProperty("fincept_agent_risk", "spend"); // agents: arms a recurring agentic task (model quota)
    connect(schedule_task_btn_, &QPushButton::clicked, this,
            [this]() { emit agentic_action_requested(QStringLiteral("schedule")); });
    task_schedules_btn_ = new QPushButton(this);
    task_schedules_btn_->setObjectName(QStringLiteral("awbBtn"));
    connect(task_schedules_btn_, &QPushButton::clicked, this,
            [this]() { emit agentic_action_requested(QStringLiteral("schedules")); });
    libraries_btn_ = new QPushButton(this);
    libraries_btn_->setObjectName(QStringLiteral("awbBtn"));
    connect(libraries_btn_, &QPushButton::clicked, this,
            [this]() { emit agentic_action_requested(QStringLiteral("libraries")); });
    actions->addWidget(schedule_task_btn_);
    actions->addWidget(task_schedules_btn_);
    actions->addWidget(libraries_btn_);
    v->addLayout(actions);

    stack_ = new QStackedWidget(this);
    table_ = new QTableView(stack_);
    table_->setObjectName(QStringLiteral("awbRunsTable"));
    table_->setModel(proxy_);
    auto* chips = new AgentRunStatusDelegate(table_);
    table_->setItemDelegateForColumn(AgentRunsTableModel::ColStatus, chips);
    table_->setItemDelegateForColumn(AgentRunsTableModel::ColEngine, chips);
    table_->setSortingEnabled(true);
    table_->sortByColumn(AgentRunsTableModel::ColStarted, Qt::DescendingOrder);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->verticalHeader()->hide();
    table_->installEventFilter(this); // Enter opens the run
    auto* hh = table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setSectionResizeMode(AgentRunsTableModel::ColTitle, QHeaderView::Stretch);
    fit_rows();
    // Row height and starting widths follow the user's font — re-fit once an
    // appearance change has settled (never inside a theme_changed handler).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, &AgentRunsPanel::fit_rows);
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        const QString key = idx.data(AgentRunsTableModel::kRunKeyRole).toString();
        if (!key.isEmpty())
            emit open_run_requested(key);
    });
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const bool any = !selected_keys().isEmpty();
        open_btn_->setEnabled(any);
        delete_btn_->setEnabled(any);
    });
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& cur, const QModelIndex&) {
                const QString key = cur.data(AgentRunsTableModel::kRunKeyRole).toString();
                if (!key.isEmpty())
                    emit run_selected(key);
            });
    stack_->addWidget(table_);

    // Empty state: never a blank table.
    auto* empty = new QWidget(stack_);
    empty->setObjectName(QStringLiteral("awbPage"));
    auto* ev = new QVBoxLayout(empty);
    ev->setContentsMargins(24, 24, 24, 24);
    ev->setSpacing(10);
    ev->addStretch(1);
    empty_title_ = new QLabel(empty);
    empty_title_->setObjectName(QStringLiteral("awbSection"));
    empty_title_->setAlignment(Qt::AlignCenter);
    ev->addWidget(empty_title_);
    empty_text_ = new QLabel(empty);
    empty_text_->setObjectName(QStringLiteral("awbHint"));
    empty_text_->setAlignment(Qt::AlignCenter);
    empty_text_->setWordWrap(true);
    ev->addWidget(empty_text_);
    empty_btn_ = new QPushButton(empty);
    empty_btn_->setObjectName(QStringLiteral("awbPrimaryBtn"));
    empty_btn_->setCursor(Qt::PointingHandCursor);
    connect(empty_btn_, &QPushButton::clicked, this, [this]() {
        if (proxy_->filtering() && model_->rowCount() > 0) {
            const QSignalBlocker b1(status_combo_);
            const QSignalBlocker b2(engine_combo_);
            const QSignalBlocker b3(search_edit_);
            status_combo_->setCurrentIndex(0);
            engine_combo_->setCurrentIndex(0);
            search_edit_->clear();
            apply_filters();
            return;
        }
        emit new_run_requested(); // opens the composer — nothing runs from here
    });
    ev->addWidget(empty_btn_, 0, Qt::AlignHCenter);
    ev->addStretch(2);
    stack_->addWidget(empty);
    v->addWidget(stack_, 1);

    hint_lbl_ = new QLabel(this);
    hint_lbl_->setObjectName(QStringLiteral("awbHint"));
    hint_lbl_->setWordWrap(true);
    v->addWidget(hint_lbl_);

    open_btn_->setEnabled(false);
    delete_btn_->setEnabled(false);

    // The agentic list answers every list_tasks() caller: take what arrives.
    auto& agents = services::AgentService::instance();
    connect(&agents, &services::AgentService::tasks_listed, this, [this](const QJsonArray& tasks) {
        const bool replace = agentic_requested_;
        agentic_requested_ = false;
        model_->set_agentic(tasks, replace);
        rebuild_engine_filter();
        update_count();
        if (!pending_select_.isEmpty())
            select_run(pending_select_);
        emit counts_changed();
    });
    connect(&agents, &services::AgentService::task_event, this, [this](const QString&, const QJsonObject&) {
        agentic_stale_ = true;
        if (isVisible() && agentic_enabled_ && !agentic_timer_->isActive())
            agentic_timer_->start(); // trailing refresh, at most one per kAgenticRefreshMs
    });

    model_->reload();
    rebuild_engine_filter();
    retranslate();
}

void AgentRunsPanel::fit_rows() {
    table_->verticalHeader()->setDefaultSectionSize(qMax(26, QFontMetrics(ui::fonts::font()).height() + 8));
    const auto col_w = [](int w) { return qMax(w, w * ui::fonts::font_px(0) / ui::ThemeManager::kDefaultFontPx); };
    auto* hh = table_->horizontalHeader();
    hh->resizeSection(AgentRunsTableModel::ColStatus, col_w(120));
    hh->resizeSection(AgentRunsTableModel::ColEngine, col_w(150));
    hh->resizeSection(AgentRunsTableModel::ColModel, col_w(120));
    hh->resizeSection(AgentRunsTableModel::ColStarted, col_w(130));
    hh->resizeSection(AgentRunsTableModel::ColDuration, col_w(80));
    hh->resizeSection(AgentRunsTableModel::ColTokens, col_w(100));
    hh->resizeSection(AgentRunsTableModel::ColCost, col_w(80));
    hh->resizeSection(AgentRunsTableModel::ColSchedule, col_w(140));
}

void AgentRunsPanel::rebuild_engine_filter() {
    auto& registry = harness::HarnessRegistry::instance();
    const QString keep = engine_combo_->currentData().toString();
    QStringList harness_ids = registry.installed_ids();
    // Harnesses only stored runs name (an older or newer build's) stay filterable.
    for (const QString& id : model_->harness_ids_in_rows()) {
        if (!harness_ids.contains(id))
            harness_ids << id;
    }
    const QSignalBlocker b(engine_combo_);
    engine_combo_->clear();
    engine_combo_->addItem(tr("All engines"), QString());
    engine_combo_->addItem(tr("Harness runs"), QStringLiteral("harness"));
    for (const QString& id : std::as_const(harness_ids))
        engine_combo_->addItem(QStringLiteral("   ") + registry.display_name(id), QStringLiteral("harness:") + id);
    if (agentic_enabled_)
        engine_combo_->addItem(tr("Agentic tasks"), QStringLiteral("agentic"));
    engine_combo_->addItem(tr("Chat Mode runs"), QStringLiteral("chat"));
    const int idx = engine_combo_->findData(keep);
    engine_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void AgentRunsPanel::retranslate() {
    const QStringList status_labels = {tr("All statuses"), tr("Active"),    tr("Needs input"), tr("Paused"),
                                       tr("Succeeded"),    tr("Failed"),    tr("Cancelled"),   tr("Interrupted")};
    for (int i = 0; i < status_labels.size() && i < status_combo_->count(); ++i)
        status_combo_->setItemText(i, status_labels[i]);
    rebuild_engine_filter();
    search_edit_->setPlaceholderText(tr("Search title, prompt or model…"));
    open_btn_->setText(tr("OPEN"));
    delete_btn_->setText(tr("DELETE"));
    clear_btn_->setText(tr("CLEAR FINISHED"));
    clear_btn_->setToolTip(tr("Delete every finished harness run and its transcript (working folders stay on disk)"));
    refresh_btn_->setText(tr("REFRESH"));
    schedule_task_btn_->setText(tr("SCHEDULE TASK…"));
    schedule_task_btn_->setToolTip(tr("A recurring agentic task (\"every 30m\", \"daily 09:30\" …)"));
    task_schedules_btn_->setText(tr("TASK SCHEDULES…"));
    task_schedules_btn_->setToolTip(tr("View, pause and delete recurring scheduled tasks"));
    libraries_btn_->setText(tr("LIBRARIES…"));
    libraries_btn_->setToolTip(tr("Skills, archival memory and reflexions the agentic runner keeps"));
    for (QPushButton* b : {schedule_task_btn_, task_schedules_btn_, libraries_btn_})
        b->setVisible(agentic_enabled_);
    hint_lbl_->setText(tr("Every engine in one list: HARNESS runs (Claude Code, Codex …), AGENTIC tasks and CHAT Mode "
                          "runs. Select a run to see it in the inspector; Enter or double-click moves the focus there. "
                          "Deleting a harness run removes its transcript; the working folder stays on disk."));
    model_->retranslate_headers();
    update_count();
}

void AgentRunsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

bool AgentRunsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == table_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            on_open();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AgentRunsPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    subscribe();
    model_->reload(); // updates were not tracked while hidden
    rebuild_engine_filter();
    apply_filters();
    if (agentic_enabled_ && agentic_stale_ && !agentic_timer_->isActive()) {
        // The task list is a local read (Python), never a model call; once per show.
        agentic_requested_ = true;
        agentic_stale_ = false;
        services::AgentService::instance().list_tasks(QString(), kAgenticListLimit);
    }
    if (model_->refresh_active_durations())
        tick_timer_->start();
    if (!pending_select_.isEmpty())
        select_run(pending_select_);
}

void AgentRunsPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    unsubscribe();
    dirty_timer_->stop();
    tick_timer_->stop();
    agentic_timer_->stop();
    dirty_harness_.clear();
    dirty_chat_.clear();
    agentic_stale_ = true;
}

void AgentRunsPanel::subscribe() {
    if (!conns_.isEmpty())
        return;
    auto& svc = ca::CodingAgentService::instance();
    auto mark_harness = [this](const QString& id) {
        dirty_harness_.insert(id);
        if (!dirty_timer_->isActive())
            dirty_timer_->start();
    };
    conns_ << connect(&svc, &ca::CodingAgentService::run_added, this, [this](const QString& id) {
        model_->upsert_harness(id);
        rebuild_engine_filter();
        update_count();
        emit counts_changed();
        if (!tick_timer_->isActive())
            tick_timer_->start();
        if (pending_select_ == agent_runs::run_key(agent_runs::Engine::Harness, id))
            select_run(pending_select_);
    });
    conns_ << connect(&svc, &ca::CodingAgentService::run_updated, this, mark_harness);
    conns_ << connect(&svc, &ca::CodingAgentService::run_finished, this, mark_harness);
    conns_ << connect(&svc, &ca::CodingAgentService::runs_reset, this, [this]() {
        model_->reload();
        rebuild_engine_filter();
        update_count();
        emit counts_changed();
    });
    conns_ << connect(&svc, &ca::CodingAgentService::schedules_changed, this, [this]() {
        // schedule names appear in the ORIGIN column
        for (const QString& id : ca::CodingAgentService::instance().run_ids())
            dirty_harness_.insert(id);
        if (!dirty_timer_->isActive())
            dirty_timer_->start();
    });
    auto& tasks = agent_tasks::TaskRunService::instance();
    auto mark_chat = [this](const QString& id) {
        dirty_chat_.insert(id);
        if (!dirty_timer_->isActive())
            dirty_timer_->start();
    };
    conns_ << connect(&tasks, &agent_tasks::TaskRunService::run_added, this, mark_chat);
    conns_ << connect(&tasks, &agent_tasks::TaskRunService::run_updated, this, mark_chat);
    conns_ << connect(&tasks, &agent_tasks::TaskRunService::run_finished, this, mark_chat);
    conns_ << connect(&tasks, &agent_tasks::TaskRunService::runs_reset, this, [this]() {
        model_->reload();
        update_count();
        emit counts_changed();
    });
}

void AgentRunsPanel::unsubscribe() {
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

void AgentRunsPanel::flush_dirty() {
    if (dirty_harness_.isEmpty() && dirty_chat_.isEmpty())
        return;
    for (const QString& id : std::as_const(dirty_harness_))
        model_->upsert_harness(id);
    for (const QString& id : std::as_const(dirty_chat_))
        model_->upsert_chat(id);
    dirty_harness_.clear();
    dirty_chat_.clear();
    proxy_->invalidate(); // status changes move rows between filters / sort order
    update_count();
    emit counts_changed();
    if (!tick_timer_->isActive() && model_->refresh_active_durations())
        tick_timer_->start();
}

void AgentRunsPanel::on_tick() {
    if (!model_->refresh_active_durations())
        tick_timer_->stop();
}

void AgentRunsPanel::apply_filters() {
    agent_runs::RunFilter f;
    f.status = status_combo_->currentData().toString();
    f.engine = engine_combo_->currentData().toString();
    f.text = search_edit_->text();
    proxy_->set_filter(f);
    update_count();
}

void AgentRunsPanel::set_engine_filter(const QString& engine) {
    rebuild_engine_filter();
    const int idx = engine_combo_->findData(engine);
    engine_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void AgentRunsPanel::set_text_filter(const QString& text) {
    if (search_edit_->text() != text)
        search_edit_->setText(text);
}

void AgentRunsPanel::set_agentic_enabled(bool enabled) {
    if (agentic_enabled_ == enabled)
        return;
    agentic_enabled_ = enabled;
    model_->set_agentic_enabled(enabled);
    rebuild_engine_filter();
    for (QPushButton* b : {schedule_task_btn_, task_schedules_btn_, libraries_btn_})
        b->setVisible(enabled);
    update_count();
    emit counts_changed();
}

void AgentRunsPanel::request_agentic_refresh() {
    if (agentic_enabled_ && !agentic_timer_->isActive())
        agentic_timer_->start();
}

void AgentRunsPanel::update_count() {
    if (!count_lbl_ || !proxy_)
        return;
    const int shown = proxy_->rowCount();
    const int total = model_->rowCount();
    count_lbl_->setText(tr("%1 of %2 runs").arg(shown).arg(total));
    if (shown > 0) {
        stack_->setCurrentWidget(table_);
        return;
    }
    if (total == 0) {
        empty_title_->setText(tr("NO RUNS YET"));
        empty_text_->setText(tr("Every agent run — a coding agent started here, by a schedule or as a follow-up, an "
                                "agentic task, a Chat Mode run — is listed with its engine, model, duration, tokens "
                                "and cost. NEW RUN starts a coding agent; nothing runs until you press RUN."));
        empty_btn_->setText(tr("NEW RUN"));
    } else {
        empty_title_->setText(tr("NO RUN MATCHES"));
        empty_text_->setText(tr("%1 runs are hidden by the filters above.").arg(total));
        empty_btn_->setText(tr("CLEAR FILTERS"));
    }
    stack_->setCurrentIndex(1);
}

QStringList AgentRunsPanel::selected_keys() const {
    QStringList keys;
    if (!table_->selectionModel())
        return keys;
    for (const auto& idx : table_->selectionModel()->selectedRows()) {
        const QString key = idx.data(AgentRunsTableModel::kRunKeyRole).toString();
        if (!key.isEmpty())
            keys << key;
    }
    return keys;
}

QString AgentRunsPanel::selected_key() const {
    const QModelIndex cur = table_->currentIndex();
    return cur.isValid() ? cur.data(AgentRunsTableModel::kRunKeyRole).toString() : QString();
}

bool AgentRunsPanel::select_run(const QString& key) {
    for (int row = 0; row < proxy_->rowCount(); ++row) {
        const QModelIndex idx = proxy_->index(row, 0);
        if (idx.data(AgentRunsTableModel::kRunKeyRole).toString() != key)
            continue;
        pending_select_.clear();
        table_->setCurrentIndex(idx);
        table_->selectRow(row);
        table_->scrollTo(idx);
        return true;
    }
    pending_select_ = key; // selected when it arrives (a run just started, a list still loading)
    return false;
}

void AgentRunsPanel::on_open() {
    const QStringList keys = selected_keys();
    if (!keys.isEmpty())
        emit open_run_requested(keys.first());
}

void AgentRunsPanel::on_delete() {
    const QStringList keys = selected_keys();
    if (keys.isEmpty())
        return;
    QStringList harness_ids;
    QStringList agentic_ids;
    int chat = 0;
    for (const QString& key : keys) {
        agent_runs::Engine e;
        QString id;
        if (!agent_runs::split_run_key(key, &e, &id))
            continue;
        if (e == agent_runs::Engine::Harness)
            harness_ids << id;
        else if (e == agent_runs::Engine::Agentic)
            agentic_ids << id;
        else
            ++chat;
    }
    if (harness_ids.isEmpty() && agentic_ids.isEmpty()) {
        QMessageBox::information(this, tr("Delete runs"),
                                 tr("Chat Mode runs are kept and deleted in Chat Mode, with their conversation."));
        return;
    }
    QString text = tr("Delete %1 run(s) and their transcripts?\n\nRunning or queued harness runs are skipped; a running "
                      "agentic task is cancelled first. Working folders and the files the agents produced stay on disk.")
                       .arg(harness_ids.size() + agentic_ids.size());
    if (chat > 0)
        text += QStringLiteral("\n\n") + tr("%1 Chat Mode run(s) in the selection are left as they are.").arg(chat);
    const auto answer = QMessageBox::question(this, tr("Delete runs"), text, QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    if (!harness_ids.isEmpty())
        ca::CodingAgentService::instance().delete_runs(harness_ids);
    if (!agentic_ids.isEmpty()) {
        auto& svc = services::AgentService::instance();
        for (const QString& id : std::as_const(agentic_ids)) {
            svc.cancel_task(id);
            svc.delete_task(id);
        }
        request_agentic_refresh();
    }
    LOG_INFO("AgentConfig", QString("Runs deleted from the run list harness=%1 agentic=%2")
                                .arg(harness_ids.size())
                                .arg(agentic_ids.size()));
}

void AgentRunsPanel::on_clear_finished() {
    const auto answer = QMessageBox::question(
        this, tr("Clear finished runs"),
        tr("Delete every finished harness run (succeeded, failed, cancelled, interrupted) and its transcript?\n\nWorking "
           "folders stay on disk. Agentic tasks and Chat Mode runs are not touched."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        ca::CodingAgentService::instance().clear_finished();
}

} // namespace fincept::screens
