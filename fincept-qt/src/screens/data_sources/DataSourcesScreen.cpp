// DataSourcesScreen.cpp — Fincept Data Sources, Obsidian design system v2.
//
// Core lifecycle: ctor, show/hide/resize events, eventFilter, the connection
// refresh loop (refresh_connections + on_poll_timer's update_clock partner),
// and IStatefulScreen save/restore. Other concerns live in sibling
// translation units:
//   - DataSourcesScreen_Layout.cpp     — setup_ui + build_* page methods
//   - DataSourcesScreen_Tables.cpp     — table/ladder rebuilds + updates
//   - DataSourcesScreen_Handlers.cpp   — all on_* slots + show_config_dialog
//   - DataSourcesStyles.h              — full QSS string
//   - DataSourcesHelpers.h/cpp         — pure ConnectorConfig / DataSource utils
//   - ConnectionConfigDialog.*         — modal add/edit/clone dialog
//   - ConnectionTester.*               — async TCP probe + result dialog
//   - ImportExportConnections.*        — JSON round-trip and template dump

#include "screens/data_sources/DataSourcesScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/data_sources/ConnectionConfigDialog.h"
#include "screens/data_sources/ConnectionTester.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourcesHelpers.h"
#include "screens/data_sources/DataSourcesStyles.h"
#include "screens/data_sources/ImportExportConnections.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidgetItem>
#include <QMap>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::datasources {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

namespace {

static const QString TAG = "DataSources";

static const QStringList kCategoryLabels = {
    "All Connectors", "Databases",   "APIs",   "Files",      "Streaming",        "Cloud",
    "Time Series",    "Market Data", "Search", "Warehouses", "Alternative Data", "Open Banking"};

} // anonymous namespace


// ─────────────────────────────────────────────────────────────────────────────
// Constructor & lifecycle
// ─────────────────────────────────────────────────────────────────────────────


DataSourcesScreen::DataSourcesScreen(QWidget* parent) : QWidget(parent) {
    register_all_connectors();
    setup_ui();
    refresh_connections();
    LOG_INFO(TAG, QString("Loaded %1 connector definitions").arg(ConnectorRegistry::instance().count()));
}

void DataSourcesScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (clock_timer_) {
        update_clock();
        clock_timer_->start();
    }
    if (poll_timer_)
        poll_timer_->start();
    refresh_connections();
}

void DataSourcesScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (clock_timer_)
        clock_timer_->stop();
    if (poll_timer_)
        poll_timer_->stop();
}

void DataSourcesScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update_stats_strip();
}

bool DataSourcesScreen::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        const QVariant idx_var = obj->property("statIndex");
        if (idx_var.isValid()) {
            on_stat_box_clicked(idx_var.toInt());
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ─────────────────────────────────────────────────────────────────────────────
// setup_ui — assembles the complete screen
// ─────────────────────────────────────────────────────────────────────────────


void DataSourcesScreen::refresh_connections() {
    const auto result = DataSourceRepository::instance().list_all();
    connections_cache_ = result.is_ok() ? result.value() : QVector<DataSource>{};
    build_category_ladder();
    build_connector_table();
    build_connections_table();
    update_provider_ladder();
    update_stats_strip();
}

void DataSourcesScreen::update_clock() {
    if (clock_label_)
        clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap DataSourcesScreen::save_state() const {
    return {
        {"connector_id", selected_connector_id_},
        {"category", static_cast<int>(active_category_)},
    };
}

void DataSourcesScreen::restore_state(const QVariantMap& state) {
    const QString id = state.value("connector_id").toString();
    if (!id.isEmpty())
        select_connector_by_id(id);
}

} // namespace fincept::screens::datasources
