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

// ─────────────────────────────────────────────────────────────────────────────
// Live language switch
// ─────────────────────────────────────────────────────────────────────────────
void DataSourcesScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DataSourcesScreen::retranslateUi() {
    // Header
    if (header_title_)    header_title_->setText(tr("DATA SOURCES"));
    if (header_subtitle_) header_subtitle_->setText(tr("%1 CONNECTORS").arg(ConnectorRegistry::instance().count()));
    if (search_edit_)     search_edit_->setPlaceholderText(tr("search connectors..."));
    if (import_btn_)      import_btn_->setText(tr("IMPORT"));
    if (export_btn_)      export_btn_->setText(tr("EXPORT"));
    if (tpl_btn_)         tpl_btn_->setText(tr("TEMPLATE"));

    // Stats strip labels
    if (universe_stat_label_)   universe_stat_label_->setText(tr("UNIVERSE"));
    if (configured_stat_label_) configured_stat_label_->setText(tr("CONFIGURED"));
    if (active_stat_label_)     active_stat_label_->setText(tr("ACTIVE"));
    if (auth_stat_label_)       auth_stat_label_->setText(tr("AUTH REQ"));

    // Tabs
    if (browse_tab_) browse_tab_->setText(tr("BROWSE"));
    if (conns_tab_)  conns_tab_->setText(tr("CONNECTIONS"));

    // Sidebar
    if (category_hdr_title_) category_hdr_title_->setText(tr("CATEGORY"));
    if (provider_hdr_title_) provider_hdr_title_->setText(tr("TOP PROVIDERS"));

    // Connector panel + table headers
    if (connector_panel_title_) connector_panel_title_->setText(tr("CONNECTORS"));
    if (connector_table_)
        connector_table_->setHorizontalHeaderLabels(
            {"", tr("CONNECTOR"), tr("CATEGORY"), tr("AUTH"), tr("TYPE"), tr("ACTIVE"), tr("TOTAL")});

    // Inspector
    if (inspector_hdr_title_)       inspector_hdr_title_->setText(tr("INSPECTOR"));
    if (detail_category_label_)     detail_category_label_->setText(tr("CATEGORY"));
    if (detail_transport_label_)    detail_transport_label_->setText(tr("TYPE"));
    if (detail_auth_label_)         detail_auth_label_->setText(tr("AUTH"));
    if (detail_test_label_)         detail_test_label_->setText(tr("TESTABLE"));
    if (detail_fields_label_)       detail_fields_label_->setText(tr("FIELDS"));
    if (detail_configured_label_)   detail_configured_label_->setText(tr("CONFIGURED"));
    if (detail_enabled_label_)      detail_enabled_label_->setText(tr("ACTIVE"));
    if (detail_last_status_label_)  detail_last_status_label_->setText(tr("LAST STATUS"));
    if (config_fields_label_)       config_fields_label_->setText(tr("CONFIG FIELDS"));
    if (detail_saved_conns_label_)  detail_saved_conns_label_->setText(tr("SAVED CONNECTIONS"));
    if (field_table_)               field_table_->setHorizontalHeaderLabels({tr("FIELD"), tr("TYPE"), tr("REQ")});
    if (new_connection_btn_)        new_connection_btn_->setText(tr("+ ADD CONNECTION"));
    if (edit_connection_btn_)       edit_connection_btn_->setText(tr("EDIT"));
    if (test_connection_btn_)       test_connection_btn_->setText(tr("TEST"));

    // Connections page
    if (conns_page_title_) conns_page_title_->setText(tr("SAVED CONNECTIONS"));
    if (conns_add_btn_)    conns_add_btn_->setText(tr("+ ADD"));
    if (conn_search_edit_) conn_search_edit_->setPlaceholderText(tr("filter connections..."));
    if (bulk_enable_btn_)  bulk_enable_btn_->setText(tr("ENABLE ALL"));
    if (bulk_disable_btn_) bulk_disable_btn_->setText(tr("DISABLE ALL"));
    if (bulk_delete_btn_)  bulk_delete_btn_->setText(tr("DELETE SEL"));
    if (connections_table_)
        connections_table_->setHorizontalHeaderLabels(
            {"", tr("NAME"), tr("PROVIDER"), tr("CATEGORY"), tr("TYPE"), tr("STATUS"), tr("TAGS"), tr("UPDATED")});

    // Re-render data-driven cells (category ladder, connector/connections tables,
    // provider ladder, detail panel) so their translatable cell text updates.
    build_category_ladder();
    build_connector_table();
    build_connections_table();
    update_provider_ladder();
    update_detail_panel();
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap DataSourcesScreen::save_state() const {
    QVariantMap state{
        {"connector_id", selected_connector_id_},
        {"category", static_cast<int>(active_category_)},
    };
    if (search_edit_) state["search"] = search_edit_->text();
    if (conn_search_edit_) state["conn_search"] = conn_search_edit_->text();
    return state;
}

void DataSourcesScreen::restore_state(const QVariantMap& state) {
    const QString id = state.value("connector_id").toString();
    if (!id.isEmpty())
        select_connector_by_id(id);
    if (search_edit_ && state.contains("search"))
        search_edit_->setText(state.value("search").toString());
    if (conn_search_edit_ && state.contains("conn_search"))
        conn_search_edit_->setText(state.value("conn_search").toString());
}

} // namespace fincept::screens::datasources
