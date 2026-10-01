#include "screens/devtools/DataHubInspector.h"

#include "datahub/DataHub.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

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
    return QStringLiteral("%1 m").arg(age / 60000);
}
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
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels(
        {tr("Topic"), tr("Subs"), tr("Publishes"), tr("Last Publish"), tr("Last Refresh"), tr("State")});
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
        table_->setHorizontalHeaderLabels(
            {tr("Topic"), tr("Subs"), tr("Publishes"), tr("Last Publish"), tr("Last Refresh"), tr("State")});
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

    auto set_cell = [this](int row, int col, const QString& text) {
        if (auto* it = table_->item(row, col)) {
            if (it->text() != text)
                it->setText(text);
        } else {
            table_->setItem(row, col, new QTableWidgetItem(text));
        }
    };

    const QString needle = filter_edit_ ? filter_edit_->text().trimmed() : QString();

    for (int row = 0; row < n; ++row) {
        const auto& s = stats[row];
        table_->setRowHidden(row, !needle.isEmpty() && !s.topic.contains(needle, Qt::CaseInsensitive));
        set_cell(row, 0, s.topic);
        set_cell(row, 1, QString::number(s.subscriber_count));
        set_cell(row, 2, QString::number(s.total_publishes));
        set_cell(row, 3, format_age(s.last_publish_ms));
        set_cell(row, 4, format_age(s.last_refresh_request_ms));
        QString state_label;
        if (s.push_only)
            state_label = tr("push");
        else if (s.in_flight)
            state_label = tr("in-flight");
        else
            state_label = tr("idle");
        set_cell(row, 5, state_label);
    }

    if (prev_sort)
        table_->setSortingEnabled(true);
    table_->setUpdatesEnabled(true);
}

} // namespace fincept::screens::devtools
