// AgentRunsPanel.cpp — RUNS tab: run history table.

#include "screens/agent_workbench/AgentRunsPanel.h"

#include "screens/agent_workbench/AgentWorkbenchCommon.h"
#include "services/coding_agents/CodingAgentService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace {

QColor status_color(ca::RunStatus s) {
    switch (s) {
        case ca::RunStatus::Queued:
            return QColor(ui::colors::INFO());
        case ca::RunStatus::Running:
            return QColor(ui::colors::AMBER());
        case ca::RunStatus::Succeeded:
            return QColor(ui::colors::POSITIVE());
        case ca::RunStatus::Failed:
        case ca::RunStatus::Interrupted:
            return QColor(ui::colors::NEGATIVE());
        case ca::RunStatus::Cancelled:
            break;
    }
    return QColor(ui::colors::TEXT_TERTIARY());
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// Model
// ═════════════════════════════════════════════════════════════════════════════

AgentRunsTableModel::AgentRunsTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int AgentRunsTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(ids_.size());
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
        case ColTitle:
            return tr("TITLE");
        case ColAgent:
            return tr("AGENT");
        case ColStarted:
            return tr("STARTED");
        case ColDuration:
            return tr("DURATION");
        case ColCost:
            return tr("COST");
        case ColSchedule:
            return tr("SCHEDULE / ORIGIN");
        default:
            return {};
    }
}

QVariant AgentRunsTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= ids_.size())
        return {};
    auto& svc = ca::CodingAgentService::instance();
    const QString& id = ids_[index.row()];
    const ca::CodingAgentRun* r = svc.find_run(id);
    if (!r)
        return {};
    const qint64 started = r->started_at_ms > 0 ? r->started_at_ms : r->created_at_ms;

    if (role == kRunIdRole)
        return id;
    if (role == kStatusIdRole)
        return ca::status_id(r->status);
    if (role == kAgentIdRole)
        return ca::agent_id(r->agent);
    if (role == kSearchRole)
        return r->title + QLatin1Char(' ') + r->prompt.left(2000);

    const int col = index.column();
    if (role == Qt::DisplayRole) {
        switch (col) {
            case ColStatus:
                return awb_status_text(r->status);
            case ColTitle:
                return r->title;
            case ColAgent:
                return r->model.isEmpty() ? awb_agent_text(r->agent)
                                          : QStringLiteral("%1 · %2").arg(awb_agent_text(r->agent), r->model);
            case ColStarted:
                return QDateTime::fromMSecsSinceEpoch(started).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
            case ColDuration:
                return r->started_at_ms > 0 ? awb_duration(awb_run_duration_ms(*r)) : QStringLiteral("—");
            case ColCost:
                return r->cost_usd.has_value() ? QStringLiteral("$%1").arg(*r->cost_usd, 0, 'f', 4) : QStringLiteral("—");
            case ColSchedule: {
                if (!r->schedule_id.isEmpty()) {
                    const ca::CodingAgentSchedule* s = svc.find_schedule(r->schedule_id);
                    return s ? s->name : tr("(deleted schedule)");
                }
                if (r->origin == QLatin1String("mcp"))
                    return tr("AI assistant");
                if (r->origin == QLatin1String("follow_up"))
                    return tr("follow-up");
                return QStringLiteral("—");
            }
            default:
                return {};
        }
    }
    if (role == kSortRole) {
        switch (col) {
            case ColStatus:
                return static_cast<int>(r->status);
            case ColTitle:
                return r->title.toLower();
            case ColAgent:
                return ca::agent_id(r->agent);
            case ColStarted:
                return started;
            case ColDuration:
                return awb_run_duration_ms(*r);
            case ColCost:
                return r->cost_usd.value_or(-1.0);
            case ColSchedule:
                return data(index, Qt::DisplayRole).toString().toLower();
            default:
                return {};
        }
    }
    if (role == Qt::ForegroundRole) {
        if (col == ColStatus)
            return status_color(r->status);
        if (col == ColTitle)
            return QColor(ui::colors::TEXT_PRIMARY());
        return QColor(ui::colors::TEXT_SECONDARY());
    }
    if (role == Qt::TextAlignmentRole && (col == ColDuration || col == ColCost))
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ToolTipRole) {
        if (col == ColTitle)
            return r->prompt.left(800);
        if (col == ColStatus && !r->error.isEmpty())
            return r->error.left(800);
    }
    return {};
}

void AgentRunsTableModel::reload() {
    beginResetModel();
    ids_ = ca::CodingAgentService::instance().run_ids();
    endResetModel();
}

void AgentRunsTableModel::add_run(const QString& run_id) {
    if (ids_.contains(run_id))
        return;
    beginInsertRows({}, 0, 0);
    ids_.prepend(run_id);
    endInsertRows();
}

void AgentRunsTableModel::refresh_runs(const QSet<QString>& ids) {
    for (const auto& id : ids) {
        const qsizetype row = ids_.indexOf(id);
        if (row >= 0)
            emit dataChanged(index(static_cast<int>(row), 0), index(static_cast<int>(row), ColCount - 1));
    }
}

bool AgentRunsTableModel::refresh_active_durations() {
    auto& svc = ca::CodingAgentService::instance();
    bool any = false;
    for (int row = 0; row < ids_.size(); ++row) {
        const ca::CodingAgentRun* r = svc.find_run(ids_[row]);
        if (r && r->status == ca::RunStatus::Running) {
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

void AgentRunsFilterProxy::set_filters(const QString& status, const QString& agent, const QString& text) {
    status_ = status;
    agent_ = agent;
    text_ = text.trimmed();
    invalidateFilter();
}

bool AgentRunsFilterProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    const QString status = idx.data(AgentRunsTableModel::kStatusIdRole).toString();
    if (status_ == QLatin1String("active")) {
        if (status != QLatin1String("queued") && status != QLatin1String("running"))
            return false;
    } else if (!status_.isEmpty() && status != status_) {
        return false;
    }
    if (!agent_.isEmpty() && idx.data(AgentRunsTableModel::kAgentIdRole).toString() != agent_)
        return false;
    if (!text_.isEmpty() &&
        !idx.data(AgentRunsTableModel::kSearchRole).toString().contains(text_, Qt::CaseInsensitive))
        return false;
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Status pill delegate
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
    f.setPixelSize(qMax(9, ui::fonts::font_px(-4)));
    painter->setFont(f);
    const QFontMetrics fm(f);
    const int w = fm.horizontalAdvance(text) + 14;
    const int h = fm.height() + 4;
    const QRect pill(opt.rect.left() + 6, opt.rect.center().y() - h / 2, w, h);
    painter->setPen(color);
    painter->drawRect(pill.adjusted(0, 0, -1, -1));
    painter->drawText(pill, Qt::AlignCenter, text);
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

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(8);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(6);
    status_combo_ = new QComboBox(this);
    for (const char* id : {"", "active", "succeeded", "failed", "cancelled", "interrupted"})
        status_combo_->addItem(QString(), QString::fromLatin1(id));
    agent_combo_ = new QComboBox(this);
    for (const char* id : {"", "claude", "codex"})
        agent_combo_->addItem(QString(), QString::fromLatin1(id));
    search_edit_ = new QLineEdit(this);
    search_edit_->setClearButtonEnabled(true);
    connect(status_combo_, &QComboBox::currentIndexChanged, this, [this](int) { apply_filters(); });
    connect(agent_combo_, &QComboBox::currentIndexChanged, this, [this](int) { apply_filters(); });
    connect(search_edit_, &QLineEdit::textChanged, this, [this](const QString&) { apply_filters(); });
    bar->addWidget(status_combo_);
    bar->addWidget(agent_combo_);
    bar->addWidget(search_edit_, 1);
    count_lbl_ = new QLabel(this);
    count_lbl_->setObjectName(QStringLiteral("awbHint"));
    bar->addWidget(count_lbl_);
    open_btn_ = new QPushButton(this);
    open_btn_->setObjectName(QStringLiteral("awbBtn"));
    connect(open_btn_, &QPushButton::clicked, this, &AgentRunsPanel::on_open);
    delete_btn_ = new QPushButton(this);
    delete_btn_->setObjectName(QStringLiteral("awbDangerBtn"));
    connect(delete_btn_, &QPushButton::clicked, this, &AgentRunsPanel::on_delete);
    clear_btn_ = new QPushButton(this);
    clear_btn_->setObjectName(QStringLiteral("awbBtn"));
    connect(clear_btn_, &QPushButton::clicked, this, &AgentRunsPanel::on_clear_finished);
    bar->addWidget(open_btn_);
    bar->addWidget(delete_btn_);
    bar->addWidget(clear_btn_);
    v->addLayout(bar);

    table_ = new QTableView(this);
    table_->setObjectName(QStringLiteral("awbRunsTable"));
    table_->setModel(proxy_);
    table_->setItemDelegateForColumn(AgentRunsTableModel::ColStatus, new AgentRunStatusDelegate(table_));
    table_->setSortingEnabled(true);
    table_->sortByColumn(AgentRunsTableModel::ColStarted, Qt::DescendingOrder);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(26);
    auto* hh = table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setSectionResizeMode(AgentRunsTableModel::ColTitle, QHeaderView::Stretch);
    hh->resizeSection(AgentRunsTableModel::ColStatus, 120);
    hh->resizeSection(AgentRunsTableModel::ColAgent, 170);
    hh->resizeSection(AgentRunsTableModel::ColStarted, 140);
    hh->resizeSection(AgentRunsTableModel::ColDuration, 90);
    hh->resizeSection(AgentRunsTableModel::ColCost, 90);
    hh->resizeSection(AgentRunsTableModel::ColSchedule, 170);
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        const QString id = idx.data(AgentRunsTableModel::kRunIdRole).toString();
        if (!id.isEmpty())
            emit open_run_requested(id);
    });
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const bool any = !selected_ids().isEmpty();
        open_btn_->setEnabled(any);
        delete_btn_->setEnabled(any);
    });
    v->addWidget(table_, 1);

    hint_lbl_ = new QLabel(this);
    hint_lbl_->setObjectName(QStringLiteral("awbHint"));
    v->addWidget(hint_lbl_);

    open_btn_->setEnabled(false);
    delete_btn_->setEnabled(false);
    retranslate();
}

void AgentRunsPanel::retranslate() {
    const QStringList status_labels = {tr("All statuses"), tr("Active"),    tr("Succeeded"),
                                       tr("Failed"),       tr("Cancelled"), tr("Interrupted")};
    for (int i = 0; i < status_labels.size() && i < status_combo_->count(); ++i)
        status_combo_->setItemText(i, status_labels[i]);
    const QStringList agent_labels = {tr("All agents"), tr("Claude Code"), tr("Codex")};
    for (int i = 0; i < agent_labels.size() && i < agent_combo_->count(); ++i)
        agent_combo_->setItemText(i, agent_labels[i]);
    search_edit_->setPlaceholderText(tr("Search title or prompt…"));
    open_btn_->setText(tr("OPEN"));
    delete_btn_->setText(tr("DELETE"));
    clear_btn_->setText(tr("CLEAR FINISHED"));
    hint_lbl_->setText(tr("Double-click a run to open its transcript. Deleting a run removes its transcript; the "
                          "working folder stays on disk."));
    model_->retranslate_headers();
    update_count();
}

void AgentRunsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void AgentRunsPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    subscribe();
    model_->reload(); // updates were not tracked while hidden
    update_count();
    if (model_->refresh_active_durations())
        tick_timer_->start();
}

void AgentRunsPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    unsubscribe();
    dirty_timer_->stop();
    tick_timer_->stop();
    dirty_.clear();
}

void AgentRunsPanel::subscribe() {
    if (!conns_.isEmpty())
        return;
    auto& svc = ca::CodingAgentService::instance();
    conns_ << connect(&svc, &ca::CodingAgentService::run_added, this, [this](const QString& id) {
        model_->add_run(id);
        update_count();
        if (!tick_timer_->isActive())
            tick_timer_->start();
    });
    auto mark = [this](const QString& id) {
        dirty_.insert(id);
        if (!dirty_timer_->isActive())
            dirty_timer_->start();
    };
    conns_ << connect(&svc, &ca::CodingAgentService::run_updated, this, mark);
    conns_ << connect(&svc, &ca::CodingAgentService::run_finished, this, mark);
    conns_ << connect(&svc, &ca::CodingAgentService::runs_reset, this, [this]() {
        model_->reload();
        update_count();
    });
    conns_ << connect(&svc, &ca::CodingAgentService::schedules_changed, this, [this]() {
        // schedule names appear in the SCHEDULE column
        const QStringList ids = ca::CodingAgentService::instance().run_ids();
        model_->refresh_runs(QSet<QString>(ids.cbegin(), ids.cend()));
    });
}

void AgentRunsPanel::unsubscribe() {
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

void AgentRunsPanel::flush_dirty() {
    if (dirty_.isEmpty())
        return;
    model_->refresh_runs(dirty_);
    dirty_.clear();
    proxy_->invalidate(); // status changes move rows between filters / sort order
    update_count();
}

void AgentRunsPanel::on_tick() {
    if (!model_->refresh_active_durations())
        tick_timer_->stop();
}

void AgentRunsPanel::apply_filters() {
    proxy_->set_filters(status_combo_->currentData().toString(), agent_combo_->currentData().toString(),
                        search_edit_->text());
    update_count();
}

void AgentRunsPanel::update_count() {
    if (!count_lbl_ || !proxy_)
        return;
    count_lbl_->setText(tr("%1 of %2 runs").arg(proxy_->rowCount()).arg(model_->rowCount()));
}

QStringList AgentRunsPanel::selected_ids() const {
    QStringList ids;
    if (!table_->selectionModel())
        return ids;
    for (const auto& idx : table_->selectionModel()->selectedRows()) {
        const QString id = idx.data(AgentRunsTableModel::kRunIdRole).toString();
        if (!id.isEmpty())
            ids << id;
    }
    return ids;
}

void AgentRunsPanel::on_open() {
    const QStringList ids = selected_ids();
    if (!ids.isEmpty())
        emit open_run_requested(ids.first());
}

void AgentRunsPanel::on_delete() {
    const QStringList ids = selected_ids();
    if (ids.isEmpty())
        return;
    const auto answer = QMessageBox::question(
        this, tr("Delete runs"),
        tr("Delete %1 run(s) and their transcripts?\n\nRunning or queued runs are skipped. Working folders and the "
           "files the agents produced stay on disk.")
            .arg(ids.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        ca::CodingAgentService::instance().delete_runs(ids);
}

void AgentRunsPanel::on_clear_finished() {
    const auto answer = QMessageBox::question(
        this, tr("Clear finished runs"),
        tr("Delete every finished run (succeeded, failed, cancelled, interrupted) and its transcript?\n\nWorking "
           "folders stay on disk."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        ca::CodingAgentService::instance().clear_finished();
}

} // namespace fincept::screens
