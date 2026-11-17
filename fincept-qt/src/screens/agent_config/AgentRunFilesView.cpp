// AgentRunFilesView.cpp — the FILES tab (see the header).

#include "screens/agent_config/AgentRunFilesView.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

namespace ca = services::coding_agents;

// ── Model ────────────────────────────────────────────────────────────────────

AgentRunFilesModel::AgentRunFilesModel(QObject* parent) : QAbstractTableModel(parent) {}

int AgentRunFilesModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int AgentRunFilesModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant AgentRunFilesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColChange:
            return tr("CHANGE");
        case ColPath:
            return tr("FILE");
        case ColTimes:
            return tr("TIMES");
        case ColWhen:
            return tr("LAST");
        default:
            return {};
    }
}

QVariant AgentRunFilesModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const Row& r = rows_[index.row()];
    if (role == kPathRole)
        return r.path;
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColChange:
                return r.kind.toUpper();
            case ColPath:
                return QDir::toNativeSeparators(r.path);
            case ColTimes:
                return r.times;
            case ColWhen:
                return r.last_ms > 0 ? QDateTime::fromMSecsSinceEpoch(r.last_ms).toString(QStringLiteral("HH:mm:ss"))
                                     : QString();
            default:
                return {};
        }
    }
    if (role == Qt::ForegroundRole) {
        if (index.column() == ColChange) {
            const QString k = r.kind.toLower();
            if (k == QLatin1String("delete") || k == QLatin1String("remove"))
                return QColor(ui::colors::DANGER());
            if (k == QLatin1String("add") || k == QLatin1String("create") || k == QLatin1String("write"))
                return QColor(ui::colors::SUCCESS());
            return QColor(ui::colors::INFO());
        }
        return QColor(index.column() == ColPath ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_SECONDARY());
    }
    if (role == Qt::ToolTipRole && index.column() == ColPath)
        return QDir::toNativeSeparators(r.path);
    if (role == Qt::TextAlignmentRole && index.column() == ColTimes)
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    return {};
}

void AgentRunFilesModel::take(const ca::AgentEvent& ev, bool notify) {
    if (ev.kind != ca::EventKind::FileChange || ev.partial || ev.is_error)
        return;
    for (const ca::FileChangeEntry& e : ca::parse_file_change_body(ev.body)) {
        const auto it = by_path_.constFind(e.path);
        if (it != by_path_.cend()) {
            Row& r = rows_[*it];
            r.kind = e.kind;
            ++r.times;
            r.last_ms = ev.ts_ms;
            if (notify)
                emit dataChanged(index(*it, 0), index(*it, ColCount - 1));
            continue;
        }
        Row r;
        r.path = e.path;
        r.kind = e.kind;
        r.times = 1;
        r.last_ms = ev.ts_ms;
        const int row = static_cast<int>(rows_.size());
        if (notify)
            beginInsertRows({}, row, row);
        rows_.append(r);
        by_path_.insert(e.path, row);
        if (notify)
            endInsertRows();
    }
}

void AgentRunFilesModel::reset_events(const QVector<ca::AgentEvent>& events) {
    beginResetModel();
    rows_.clear();
    by_path_.clear();
    for (const ca::AgentEvent& ev : events)
        take(ev, false);
    endResetModel();
}

void AgentRunFilesModel::add_events(const QVector<ca::AgentEvent>& events) {
    for (const ca::AgentEvent& ev : events)
        take(ev, true);
}

void AgentRunFilesModel::clear_rows() {
    beginResetModel();
    rows_.clear();
    by_path_.clear();
    endResetModel();
}

void AgentRunFilesModel::retranslate_headers() {
    emit headerDataChanged(Qt::Horizontal, 0, ColCount - 1);
}

// ── View ─────────────────────────────────────────────────────────────────────

AgentRunFilesView::AgentRunFilesView(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    stack_ = new QStackedWidget(this);
    empty_lbl_ = new QLabel(stack_);
    empty_lbl_->setObjectName(QStringLiteral("awbPlaceholder"));
    empty_lbl_->setAlignment(Qt::AlignCenter);
    empty_lbl_->setWordWrap(true);
    stack_->addWidget(empty_lbl_);

    model_ = new AgentRunFilesModel(this);
    table_ = new QTableView(stack_);
    table_->setModel(model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->verticalHeader()->hide();
    auto* hh = table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setSectionResizeMode(AgentRunFilesModel::ColPath, QHeaderView::Stretch);
    const auto fit = [this]() {
        table_->verticalHeader()->setDefaultSectionSize(qMax(24, QFontMetrics(ui::fonts::font()).height() + 8));
        const auto col_w = [](int w) { return qMax(w, w * ui::fonts::font_px(0) / ui::ThemeManager::kDefaultFontPx); };
        table_->horizontalHeader()->resizeSection(AgentRunFilesModel::ColChange, col_w(90));
        table_->horizontalHeader()->resizeSection(AgentRunFilesModel::ColTimes, col_w(60));
        table_->horizontalHeader()->resizeSection(AgentRunFilesModel::ColWhen, col_w(80));
    };
    fit();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, fit);
    connect(table_, &QTableView::doubleClicked, this, &AgentRunFilesView::open_row);
    stack_->addWidget(table_);
    v->addWidget(stack_);
    retranslate();
}

void AgentRunFilesView::retranslate() {
    model_->retranslate_headers();
    table_->setToolTip(tr("Double-click to open a file"));
    refresh_state();
}

void AgentRunFilesView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void AgentRunFilesView::set_run(const QString& run_id, const QString& workdir, bool read_only_run) {
    Q_UNUSED(run_id);
    workdir_ = workdir;
    read_only_run_ = read_only_run;
    model_->clear_rows();
    refresh_state();
}

void AgentRunFilesView::reset_events(const QVector<ca::AgentEvent>& events) {
    model_->reset_events(events);
    refresh_state();
}

void AgentRunFilesView::add_events(const QVector<ca::AgentEvent>& events) {
    model_->add_events(events);
    refresh_state();
}

int AgentRunFilesView::file_count() const {
    return model_->rowCount();
}

void AgentRunFilesView::refresh_state() {
    const int n = model_->rowCount();
    if (n == 0) {
        empty_lbl_->setText(read_only_run_
                                ? tr("No files changed.\n\nThis was a read-only run: the agent could read files but not "
                                     "write them. Its answer is saved as RESULT.md in the working folder.")
                                : tr("No files changed yet.\n\nEvery file the agent creates, edits or removes in this run "
                                     "is listed here — double-click one to open it."));
        stack_->setCurrentWidget(empty_lbl_);
    } else {
        stack_->setCurrentWidget(table_);
    }
    if (n != last_count_) {
        last_count_ = n;
        emit file_count_changed(n);
    }
}

void AgentRunFilesView::open_row(const QModelIndex& index) {
    if (!index.isValid())
        return;
    QString path = index.data(AgentRunFilesModel::kPathRole).toString();
    if (path.isEmpty())
        return;
    if (QDir::isRelativePath(path) && !workdir_.isEmpty())
        path = QDir(workdir_).filePath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

} // namespace fincept::screens
