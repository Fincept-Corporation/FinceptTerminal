#include "screens/devtools/DataHubInspector.h"

#include "datahub/DataHub.h"

#include <QDateTime>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace fincept::screens::devtools {

namespace {
QString format_age(qint64 ms_since_epoch) {
    if (ms_since_epoch <= 0) return QStringLiteral("—");
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - ms_since_epoch;
    if (age < 1000) return QStringLiteral("%1 ms").arg(age);
    if (age < 60000) return QStringLiteral("%1 s").arg(age / 1000);
    return QStringLiteral("%1 m").arg(age / 60000);
}
} // namespace

DataHubInspector::DataHubInspector(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels(
        {"Topic", "Subs", "Publishes", "Last Publish", "Last Refresh", "State"});
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
    if (!initial_sized_) {
        // Size once based on initial content; user can drag thereafter.
        table_->resizeColumnsToContents();
        initial_sized_ = true;
    }
    refresh_timer_.start();
}

void DataHubInspector::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_.stop();
}

void DataHubInspector::refresh() {
    const auto stats = datahub::DataHub::instance().stats();
    const int n = static_cast<int>(stats.size());

    // Suppress paint + layout events during bulk mutation. Without this each
    // setItem / setText would trigger a viewport repaint, dominating cost.
    table_->setUpdatesEnabled(false);
    const bool prev_sort = table_->isSortingEnabled();
    if (prev_sort) table_->setSortingEnabled(false);

    if (table_->rowCount() != n)
        table_->setRowCount(n);

    auto set_cell = [this](int row, int col, const QString& text) {
        if (auto* it = table_->item(row, col)) {
            if (it->text() != text) it->setText(text);
        } else {
            table_->setItem(row, col, new QTableWidgetItem(text));
        }
    };

    for (int row = 0; row < n; ++row) {
        const auto& s = stats[row];
        set_cell(row, 0, s.topic);
        set_cell(row, 1, QString::number(s.subscriber_count));
        set_cell(row, 2, QString::number(s.total_publishes));
        set_cell(row, 3, format_age(s.last_publish_ms));
        set_cell(row, 4, format_age(s.last_refresh_request_ms));
        QString state_label;
        if (s.push_only) state_label = QStringLiteral("push");
        else if (s.in_flight) state_label = QStringLiteral("in-flight");
        else state_label = QStringLiteral("idle");
        set_cell(row, 5, state_label);
    }

    if (prev_sort) table_->setSortingEnabled(true);
    table_->setUpdatesEnabled(true);
}

} // namespace fincept::screens::devtools
