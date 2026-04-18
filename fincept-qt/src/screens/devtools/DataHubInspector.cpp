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
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table_);

    refresh_timer_.setInterval(1000);
    connect(&refresh_timer_, &QTimer::timeout, this, &DataHubInspector::refresh);
    // Per CLAUDE.md P3: start/stop the timer in show/hide, not the ctor.
}

void DataHubInspector::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh();
    refresh_timer_.start();
}

void DataHubInspector::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    refresh_timer_.stop();
}

void DataHubInspector::refresh() {
    const auto stats = datahub::DataHub::instance().stats();
    table_->setRowCount(stats.size());
    for (int row = 0; row < stats.size(); ++row) {
        const auto& s = stats[row];
        table_->setItem(row, 0, new QTableWidgetItem(s.topic));
        table_->setItem(row, 1, new QTableWidgetItem(QString::number(s.subscriber_count)));
        table_->setItem(row, 2, new QTableWidgetItem(QString::number(s.total_publishes)));
        table_->setItem(row, 3, new QTableWidgetItem(format_age(s.last_publish_ms)));
        table_->setItem(row, 4, new QTableWidgetItem(format_age(s.last_refresh_request_ms)));
        QString state_label;
        if (s.push_only) state_label = QStringLiteral("push");
        else if (s.in_flight) state_label = QStringLiteral("in-flight");
        else state_label = QStringLiteral("idle");
        table_->setItem(row, 5, new QTableWidgetItem(state_label));
    }
}

} // namespace fincept::screens::devtools
