#include "screens/dashboard/widgets/ScanAlertsWidget.h"

#include "algo_engine/ScanMonitor.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/ScanEventRepository.h"
#include "storage/repositories/ScanWatchRepository.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

namespace {

constexpr const char* kScanAlertsTag = "ScanAlertsWidget";
constexpr int kScanAlertsMaxRows = 50;
constexpr int kScanAlertsColTime = 0;
constexpr int kScanAlertsColSymbol = 1;
constexpr int kScanAlertsColCondition = 2;
constexpr int kScanAlertsRowHeight = 22;
// ScanMonitor::test_fire() records this detail, and this symbol when none is given.
constexpr const char* kScanAlertsTestDetail = "test fire";
constexpr const char* kScanAlertsTestSymbol = "TEST";

QString scanalerts_time_text(qint64 ms) {
    const QDateTime when = QDateTime::fromMSecsSinceEpoch(ms);
    return when.date() == QDate::currentDate() ? when.toString(QStringLiteral("HH:mm:ss"))
                                               : when.toString(QStringLiteral("dd MMM HH:mm"));
}

/// A row to open Algo Trading for instead of a symbol: a test fire, or no symbol at all.
bool scanalerts_opens_screen(const QString& symbol, const QString& detail) {
    return symbol.trimmed().isEmpty() ||
           (detail == QLatin1String(kScanAlertsTestDetail) && symbol == QLatin1String(kScanAlertsTestSymbol));
}

QTableWidgetItem* scanalerts_cell(QTableWidget* table, int row, int col) {
    QTableWidgetItem* item = table->item(row, col);
    if (!item) {
        item = new QTableWidgetItem;
        table->setItem(row, col, item);
    }
    return item;
}

} // namespace

ScanAlertsWidget::ScanAlertsWidget(const QJsonObject& /*cfg*/, QWidget* parent)
    : BaseWidget(tr("SCAN ALERTS"), parent) {
    set_title(tr("SCAN ALERTS"));

    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // One body widget carries the tile's only stylesheet (objectName selectors).
    body_ = new QWidget(this);
    body_->setObjectName(QStringLiteral("scanAlertsBody"));
    auto* bl = new QVBoxLayout(body_);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);

    table_ = new QTableWidget(0, 3, body_);
    table_->setObjectName(QStringLiteral("scanAlertsTable"));
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(kScanAlertsRowHeight);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideRight);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(kScanAlertsColTime, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kScanAlertsColSymbol, QHeaderView::ResizeToContents);
    install_symbol_menu(table_, kScanAlertsColSymbol);
    connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int /*col*/) { on_row_double_clicked(row); });
    bl->addWidget(table_, 1);

    empty_label_ = new QLabel(body_);
    empty_label_->setObjectName(QStringLiteral("scanAlertsEmpty"));
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->setWordWrap(true);
    empty_label_->setVisible(false);
    bl->addWidget(empty_label_, 1);

    footer_ = new QLabel(body_);
    footer_->setObjectName(QStringLiteral("scanAlertsFooter"));
    bl->addWidget(footer_);

    vl->addWidget(body_, 1);

    // Title-bar refresh re-reads the persisted history.
    connect(this, &BaseWidget::refresh_requested, this, [this]() { reload_history(); });

    ScanAlertsWidget::retranslateUi(); // qualified: no dynamic dispatch inside a constructor
    apply_styles();
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void ScanAlertsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    // History first (covers alerts that fired while hidden), then follow live.
    // Both run on the GUI thread, where ScanMonitor fires, so nothing slips in
    // between and nothing is counted twice: the event row is written before
    // watch_fired is emitted.
    reload_history();
    if (!fired_conn_)
        fired_conn_ = connect(&fincept::algo::ScanMonitor::instance(), &fincept::algo::ScanMonitor::watch_fired, this,
                              &ScanAlertsWidget::on_watch_fired);
}

void ScanAlertsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (fired_conn_) {
        disconnect(fired_conn_);
        fired_conn_ = QMetaObject::Connection();
    }
}

// ── Data ────────────────────────────────────────────────────────────────────

void ScanAlertsWidget::reload_history() {
    const auto events = fincept::ScanEventRepository::instance().recent(kScanAlertsMaxRows);
    if (events.is_err()) {
        LOG_WARN(kScanAlertsTag,
                 QString("Reading scan alert history failed: %1").arg(QString::fromStdString(events.error())));
        set_error(tr("Could not read the scan alert history — click refresh to retry"));
        return;
    }
    alerts_.clear();
    alerts_.reserve(events.value().size());
    for (const auto& ev : events.value()) {
        Alert a;
        a.watch_id = ev.watch_id;
        a.symbol = ev.symbol;
        a.detail = ev.detail;
        a.fired_at = ev.fired_at;
        alerts_.append(a);
    }
    // Names of the watches that still exist; fired poll watches are gone, and
    // keep whatever name this tile cached while they were alive.
    const auto watches = fincept::ScanWatchRepository::instance().list_all();
    if (watches.is_ok()) {
        for (const auto& w : watches.value())
            watch_names_.insert(w.id, w.name);
    }
    if (is_showing_error())
        set_loading(false); // clears the overlay left by a failed read
    render();
}

void ScanAlertsWidget::on_watch_fired(const QString& watch_id, const QString& symbol, const QString& detail) {
    Alert a;
    a.watch_id = watch_id;
    a.symbol = symbol;
    a.detail = detail;
    a.fired_at = QDateTime::currentMSecsSinceEpoch();
    // Cache the name now: a one-shot poll watch deletes its row right after firing.
    watch_name(watch_id);
    alerts_.prepend(a);
    if (alerts_.size() > kScanAlertsMaxRows)
        alerts_.resize(kScanAlertsMaxRows);
    // A realtime universe scan can fire in bursts — render once per burst.
    schedule_render([this]() { render(); });
}

QString ScanAlertsWidget::watch_name(const QString& watch_id) {
    const auto it = watch_names_.constFind(watch_id);
    if (it != watch_names_.constEnd())
        return it.value();
    QString name;
    if (!watch_id.isEmpty()) {
        const auto w = fincept::ScanWatchRepository::instance().get(watch_id);
        if (w.is_ok())
            name = w.value().name;
    }
    watch_names_.insert(watch_id, name); // misses too: one lookup per watch
    return name;
}

// ── Render ──────────────────────────────────────────────────────────────────

void ScanAlertsWidget::render() {
    const int n = static_cast<int>(alerts_.size());
    const bool empty = n == 0;
    table_->setVisible(!empty);
    empty_label_->setVisible(empty);
    footer_->setVisible(!empty);
    table_->setRowCount(n);

    const QColor primary = ui::colors::TEXT_PRIMARY();
    const QColor secondary = ui::colors::TEXT_SECONDARY();
    const QColor tertiary = ui::colors::TEXT_TERTIARY();
    const QColor warning = ui::colors::WARNING();

    for (int i = 0; i < n; ++i) {
        const Alert& a = alerts_[i];
        const bool opens_screen = scanalerts_opens_screen(a.symbol, a.detail);
        const QString name = watch_name(a.watch_id);
        const QString fired =
            QDateTime::fromMSecsSinceEpoch(a.fired_at).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

        QStringList tip{tr("%1 — %2").arg(a.symbol, a.detail)};
        if (!name.isEmpty())
            tip << tr("Watch: %1").arg(name);
        tip << tr("Fired %1").arg(fired);
        tip << (opens_screen ? tr("Double-click to open Algo Trading")
                             : tr("Double-click to open %1 in Equity Research · right-click for more").arg(a.symbol));
        const QString tooltip = tip.join(QLatin1Char('\n'));

        QTableWidgetItem* time = scanalerts_cell(table_, i, kScanAlertsColTime);
        time->setText(scanalerts_time_text(a.fired_at));
        time->setForeground(secondary);
        time->setToolTip(tooltip);

        QTableWidgetItem* sym = scanalerts_cell(table_, i, kScanAlertsColSymbol);
        sym->setText(a.symbol);
        sym->setForeground(primary);
        sym->setToolTip(tooltip);

        QTableWidgetItem* cond = scanalerts_cell(table_, i, kScanAlertsColCondition);
        cond->setText(a.detail);
        cond->setForeground(opens_screen ? tertiary : warning);
        cond->setToolTip(tooltip);
    }

    footer_->setText(tr("%1 recent alerts · double-click here for Algo Trading").arg(n));
}

void ScanAlertsWidget::on_row_double_clicked(int row) {
    // Read the row back from the table, not alerts_: a fire may have been
    // prepended since the last render.
    const QTableWidgetItem* sym = table_->item(row, kScanAlertsColSymbol);
    const QTableWidgetItem* cond = table_->item(row, kScanAlertsColCondition);
    if (!sym)
        return;
    const QString symbol = sym->text();
    if (scanalerts_opens_screen(symbol, cond ? cond->text() : QString())) {
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                     QVariantMap{{"screen_id", QStringLiteral("algo_trading")}});
        return;
    }
    open_symbol(symbol);
}

// ── Theme / language ────────────────────────────────────────────────────────

void ScanAlertsWidget::apply_styles() {
    body_->setStyleSheet(
        QString("#scanAlertsBody{background:transparent;}"
                "QTableWidget#scanAlertsTable{background:transparent;color:%1;border:none;font-size:10px;}"
                "QTableWidget#scanAlertsTable QHeaderView::section{background:%2;color:%3;border:none;"
                "border-bottom:1px solid %4;padding:2px 4px;font-size:9px;font-weight:bold;}"
                "QTableWidget#scanAlertsTable::item{padding:0px 4px;border-bottom:1px solid %4;}"
                "#scanAlertsEmpty{color:%3;font-size:10px;padding:16px;}"
                "#scanAlertsFooter{color:%3;background:%2;border-top:1px solid %4;padding:3px 8px;font-size:9px;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::BORDER_DIM()));
}

void ScanAlertsWidget::on_theme_changed() {
    apply_styles();
    render(); // row colours come from the theme tokens
}

void ScanAlertsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("SCAN ALERTS"));
    table_->setHorizontalHeaderLabels({tr("TIME"), tr("SYMBOL"), tr("CONDITION")});
    table_->setAccessibleName(tr("Scan alerts"));
    empty_label_->setText(
        tr("No scan alerts yet.\nCreate a watch in Algo Trading › Alerts — double-click here to open it."));
    link_screen(empty_label_, QStringLiteral("algo_trading"), tr("Double-click to open Algo Trading"));
    link_screen(footer_, QStringLiteral("algo_trading"), tr("Double-click to open Algo Trading alerts"));
    render(); // tooltips and the footer are translated text
}

} // namespace fincept::screens::widgets
