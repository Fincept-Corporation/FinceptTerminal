#include "screens/devtools/DataHubInspector.h"

#include "datahub/DataHub.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <limits>

namespace fincept::screens::devtools {

namespace {
QString format_age(qint64 ms_since_epoch) {
    if (ms_since_epoch <= 0)
        return QStringLiteral("—");
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - ms_since_epoch;
    if (age < 1000)
        return QStringLiteral("%1 ms").arg(age);
    if (age < 60000)
        return QStringLiteral("%1 s").arg(age / 1000);
    if (age < 3600000)
        return QStringLiteral("%1 m").arg(age / 60000);
    return QStringLiteral("%1 h").arg(age / 3600000);
}

/// Sorts by a numeric key (Qt::UserRole) instead of the display text, so "10"
/// sorts after "9" and "2 m" after "30 s".
class InspectorNumericItem : public QTableWidgetItem {
  public:
    bool operator<(const QTableWidgetItem& other) const override {
        return data(Qt::UserRole).toLongLong() < other.data(Qt::UserRole).toLongLong();
    }
};

enum InspectorColumn { kColTopic, kColSubs, kColPublishes, kColLastPublish, kColLastRefresh, kColErrors, kColState };
} // namespace

DataHubInspector::DataHubInspector(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    // The hub easily holds hundreds of topics (one per symbol / series); without a
    // filter the table is unusable for finding a specific one.
    filter_edit_ = new QLineEdit(this);
    filter_edit_->setPlaceholderText(tr("Filter topics..."));
    filter_edit_->setClearButtonEnabled(true);
    filter_edit_->setAccessibleName(tr("Filter DataHub topics"));
    connect(filter_edit_, &QLineEdit::textChanged, this, [this](const QString&) { refresh(); });
    layout->addWidget(filter_edit_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({tr("Topic"), tr("Subs"), tr("Publishes"), tr("Last Publish"),
                                       tr("Last Refresh"), tr("Errors"), tr("State")});
    // Click a header to sort (e.g. most errors / stalest topics first).
    table_->setSortingEnabled(true);
    // Interactive (not ResizeToContents) — the latter re-measures every row on
    // every cell write, turning each refresh into O(rows × cols × rows) of
    // layout work. We size columns once after the first populate.
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    // verticalHeader visible by default; row heights driven by the default
    // size hint avoid per-row measurement.
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    layout->addWidget(table_);

    refresh_timer_.setInterval(1000);
    connect(&refresh_timer_, &QTimer::timeout, this, &DataHubInspector::refresh);
    // Per CLAUDE.md P3: start/stop the timer in show/hide, not the ctor.
}

void DataHubInspector::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh();
    // Size once, from the first populate that actually has rows (sizing against an
    // empty table just fits the headers); the user can drag thereafter.
    if (!initial_sized_ && table_->rowCount() > 0) {
        table_->resizeColumnsToContents();
        initial_sized_ = true;
    }
    refresh_timer_.start();
}

void DataHubInspector::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_.stop();
}

void DataHubInspector::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DataHubInspector::retranslateUi() {
    if (table_) {
        table_->setHorizontalHeaderLabels({tr("Topic"), tr("Subs"), tr("Publishes"), tr("Last Publish"),
                                           tr("Last Refresh"), tr("Errors"), tr("State")});
    }
    if (filter_edit_) {
        filter_edit_->setPlaceholderText(tr("Filter topics..."));
        filter_edit_->setAccessibleName(tr("Filter DataHub topics"));
    }
    // State-column cells are re-rendered (translated) on the next refresh().
}

void DataHubInspector::refresh() {
    const auto stats = datahub::DataHub::instance().stats();
    const int n = static_cast<int>(stats.size());

    // Suppress paint + layout events during bulk mutation. Without this each
    // setItem / setText would trigger a viewport repaint, dominating cost.
    table_->setUpdatesEnabled(false);
    const bool prev_sort = table_->isSortingEnabled();
    if (prev_sort)
        table_->setSortingEnabled(false);

    if (table_->rowCount() != n)
        table_->setRowCount(n);

    // Numeric columns carry their sort key in Qt::UserRole.
    auto set_cell = [this](int row, int col, const QString& text, qint64 sort_key = 0, bool numeric = false,
                           const QString& tip = QString()) {
        QTableWidgetItem* it = table_->item(row, col);
        if (!it) {
            it = numeric ? new InspectorNumericItem : new QTableWidgetItem;
            table_->setItem(row, col, it);
        }
        if (it->text() != text)
            it->setText(text);
        if (numeric)
            it->setData(Qt::UserRole, sort_key);
        if (it->toolTip() != tip)
            it->setToolTip(tip);
    };
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto age_key = [now](qint64 ms) { return ms > 0 ? now - ms : std::numeric_limits<qint64>::max(); };

    const QString needle = filter_edit_ ? filter_edit_->text().trimmed() : QString();

    for (int row = 0; row < n; ++row) {
        const auto& s = stats[row];
        table_->setRowHidden(row, !needle.isEmpty() && !s.topic.contains(needle, Qt::CaseInsensitive));
        set_cell(row, kColTopic, s.topic);
        set_cell(row, kColSubs, QString::number(s.subscriber_count), s.subscriber_count, true);
        set_cell(row, kColPublishes, QString::number(s.total_publishes), s.total_publishes, true);
        set_cell(row, kColLastPublish, format_age(s.last_publish_ms), age_key(s.last_publish_ms), true);
        set_cell(row, kColLastRefresh, format_age(s.last_refresh_request_ms), age_key(s.last_refresh_request_ms),
                 true);
        // The hub records refresh failures; a diagnostic view that hid them could
        // not show why a topic went stale.
        const QString err_tip =
            s.last_error.isEmpty() ? QString() : tr("Last error (%1 ago): %2").arg(format_age(s.last_error_ms), s.last_error);
        set_cell(row, kColErrors, s.total_errors > 0 ? QString::number(s.total_errors) : QString(), s.total_errors,
                 true, err_tip);
        QString state_label;
        if (s.push_only)
            state_label = tr("push");
        else if (s.in_flight)
            state_label = tr("in-flight");
        else if (!s.last_error.isEmpty())
            state_label = tr("error");
        else
            state_label = tr("idle");
        set_cell(row, kColState, state_label, 0, false, err_tip);
    }

    if (prev_sort)
        table_->setSortingEnabled(true);
    table_->setUpdatesEnabled(true);
}

} // namespace fincept::screens::devtools
