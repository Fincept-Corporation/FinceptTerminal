// DataSourcesScreen_Handlers.cpp
//
// User-action handlers (every on_* slot) and the config dialog launcher
// (show_config_dialog) — keeps the slot dispatch and dialog wiring next to
// each other.
//
// Part of the partial-class split of DataSourcesScreen.cpp.

#include "screens/data_sources/DataSourcesScreen.h"

#include "core/logging/Logger.h"

namespace { const QString TAG = "DataSources"; }

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
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace fincept::screens::datasources {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

static const QString TAG = "DataSources";


void DataSourcesScreen::show_config_dialog(const ConnectorConfig& config, const QString& edit_id, bool duplicate) {
    const QString saved_id = show_connection_config_dialog(this, config, edit_id, duplicate);
    if (saved_id.isEmpty())
        return;
    selected_connector_id_  = config.id;
    selected_connection_id_ = saved_id;
    refresh_connections();
}

// ─────────────────────────────────────────────────────────────────────────────
// Data functions
// ─────────────────────────────────────────────────────────────────────────────

QVector<ConnectorConfig> DataSourcesScreen::filtered_connectors() const {
    QVector<ConnectorConfig> filtered;
    const QString filter = search_edit_ ? search_edit_->text().trimmed().toLower() : "";

    QSet<QString> configured_ids, active_ids;
    if (stat_filter_ == 1 || stat_filter_ == 2) {
        for (const auto& ds : connections_cache_) {
            configured_ids.insert(normalized_provider_key(ds));
            if (ds.enabled)
                active_ids.insert(normalized_provider_key(ds));
        }
    }

    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        if (!show_all_categories_ && cfg.category != active_category_)
            continue;
        if (!connector_matches_text(cfg, filter))
            continue;
        if (stat_filter_ == 1 && !configured_ids.contains(cfg.id))
            continue;
        if (stat_filter_ == 2 && !active_ids.contains(cfg.id))
            continue;
        if (stat_filter_ == 3 && !cfg.requires_auth)
            continue;
        filtered.append(cfg);
    }
    return filtered;
}


void DataSourcesScreen::on_category_filter(int idx) {
    show_all_categories_ = (idx == 0);
    if (idx > 0)
        active_category_ = static_cast<Category>(idx - 1);
    build_connector_table();
    build_connections_table();
    update_provider_ladder();
    update_stats_strip();
    update_action_states();
}

void DataSourcesScreen::on_search_changed(const QString& /*text*/) {
    build_category_ladder();
    build_connector_table();
    build_connections_table();
    update_provider_ladder();
    update_stats_strip();
    update_action_states();
}

void DataSourcesScreen::on_connector_clicked(const QString& connector_id) {
    if (const auto* cfg = find_connector_config(connector_id)) {
        selected_connector_id_ = cfg->id;
        ScreenStateManager::instance().notify_changed(this);
        const QString existing_connection = preferred_connection_for_connector(cfg->id);
        show_config_dialog(*cfg, existing_connection);
    }
}

void DataSourcesScreen::on_connection_add() {
    if (selected_connector_id_.isEmpty()) {
        const auto filtered = filtered_connectors();
        if (filtered.isEmpty())
            return;
        selected_connector_id_ = filtered.first().id;
    }
    if (const auto* cfg = find_connector_config(selected_connector_id_))
        show_config_dialog(*cfg);
}

void DataSourcesScreen::on_connection_edit(const QString& conn_id) {
    const auto result = DataSourceRepository::instance().get(conn_id);
    if (result.is_err())
        return;
    const auto ds = result.value();
    if (const auto* cfg = find_connector_config(ds.provider)) {
        selected_connector_id_ = cfg->id;
        selected_connection_id_ = ds.id;
        show_config_dialog(*cfg, ds.id);
    }
}

void DataSourcesScreen::on_connection_delete(const QString& conn_id) {
    const auto result = DataSourceRepository::instance().remove(conn_id);
    if (result.is_err()) {
        LOG_ERROR(TAG, QString("Failed to remove connection %1").arg(conn_id));
        return;
    }
    if (selected_connection_id_ == conn_id)
        selected_connection_id_.clear();
    refresh_connections();
}

void DataSourcesScreen::on_connection_enabled_changed(const QString& conn_id, bool enabled) {
    const auto result = DataSourceRepository::instance().set_enabled(conn_id, enabled);
    if (result.is_err()) {
        LOG_ERROR(TAG, QString("Failed to update connection state for %1").arg(conn_id));
        return;
    }
    LOG_INFO(TAG, QString("%1 connection %2").arg(enabled ? "Enabled" : "Disabled", conn_id));
    refresh_connections();
}

// ── Test connection (thin wrapper — implementation in ConnectionTester.cpp) ──
void DataSourcesScreen::on_connection_test(const QString& conn_id) {
    QPointer<DataSourcesScreen> self = this;
    test_connection(this, conn_id, [self](const QString& id, bool ok, const QString& msg) {
        if (!self) return;
        self->live_status_cache_[id] = {ok, msg};
        self->update_connection_status_cell(id, ok, msg);
        self->update_detail_panel();
    });
}

void DataSourcesScreen::on_connector_selection_changed() {
    if (!connector_table_)
        return;
    const auto selected = connector_table_->selectedItems();
    if (selected.isEmpty()) {
        selected_connector_id_.clear();
    } else {
        const int row = connector_table_->currentRow();
        auto* item = connector_table_->item(row, 1);
        if (item)
            selected_connector_id_ = item->data(Qt::UserRole).toString();
    }
    update_detail_panel();
    update_stats_strip();
}

void DataSourcesScreen::on_connection_selection_changed() {
    if (!connections_table_)
        return;
    const auto selected = connections_table_->selectedItems();
    if (selected.isEmpty()) {
        selected_connection_id_.clear();
    } else {
        const int row = connections_table_->currentRow();
        auto* item = connections_table_->item(row, 1);
        if (item)
            selected_connection_id_ = item->data(Qt::UserRole).toString();
    }
    update_action_states();
}

void DataSourcesScreen::on_category_selection_changed(int row) {
    on_category_filter(row);
}

void DataSourcesScreen::on_provider_ladder_activated(QListWidgetItem* item) {
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;
    const QString connector_id = item->data(Qt::UserRole).toString();
    if (connector_id.isEmpty())
        return;
    select_connector_by_id(connector_id);
    update_detail_panel();
    update_stats_strip();
}

void DataSourcesScreen::on_detail_connection_activated(QListWidgetItem* item) {
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;
    const QString conn_id = item->data(Qt::UserRole).toString();
    if (conn_id.isEmpty())
        return;
    selected_connection_id_ = conn_id;
    update_action_states();
}

QVector<DataSource> DataSourcesScreen::filtered_connection_rows() const {
    const QString filter = conn_search_text_.trimmed().toLower();
    QVector<DataSource> result;
    for (const auto& ds : connections_cache_) {
        if (stat_filter_ == 2 && !ds.enabled)
            continue;
        if (stat_filter_ == 3) {
            const auto* cfg = find_connector_config(ds.provider);
            if (!cfg || !cfg->requires_auth)
                continue;
        }
        if (!filter.isEmpty()) {
            const QString hay =
                QString("%1 %2 %3 %4").arg(ds.display_name, ds.provider, ds.category, ds.tags).toLower();
            if (!hay.contains(filter))
                continue;
        }
        result.append(ds);
    }
    return result;
}

void DataSourcesScreen::update_connection_status_cell(const QString& conn_id, bool ok, const QString& msg) {
    if (!connections_table_)
        return;
    for (int row = 0; row < connections_table_->rowCount(); ++row) {
        auto* name_item = connections_table_->item(row, 1);
        if (!name_item || name_item->data(Qt::UserRole).toString() != conn_id)
            continue;
        auto* lbl = qobject_cast<QLabel*>(connections_table_->cellWidget(row, 5));
        if (lbl) {
            lbl->setText(ok ? "OK" : "ERR");
            lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                   .arg(ok ? col::POSITIVE() : col::NEGATIVE()));
            lbl->setToolTip(msg);
        }
        break;
    }
}

void DataSourcesScreen::on_view_mode_toggle() {
    // Legacy — now handled by tab bar
    view_mode_ = (view_mode_ == ViewMode::Gallery) ? ViewMode::Connections : ViewMode::Gallery;
    if (page_stack_)
        page_stack_->setCurrentIndex(view_mode_ == ViewMode::Gallery ? 0 : 1);
}

void DataSourcesScreen::on_connections_search_changed(const QString& text) {
    conn_search_text_ = text;
    build_connections_table();
    update_stats_strip();
}

void DataSourcesScreen::on_stat_box_clicked(int stat_index) {
    apply_stat_filter(stat_index);
}

void DataSourcesScreen::apply_stat_filter(int stat_index) {
    stat_filter_ = (stat_filter_ == stat_index) ? -1 : stat_index;
    build_connector_table();
    build_connections_table();
    update_stats_strip();
}

void DataSourcesScreen::on_connection_duplicate(const QString& conn_id) {
    const auto result = DataSourceRepository::instance().get(conn_id);
    if (result.is_err())
        return;
    const auto ds = result.value();
    if (const auto* cfg = find_connector_config(ds.provider)) {
        selected_connector_id_ = cfg->id;
        show_config_dialog(*cfg, ds.id, /*duplicate=*/true);
    }
}

void DataSourcesScreen::on_bulk_enable_all() {
    const auto rows = filtered_connection_rows();
    for (const auto& ds : rows) {
        if (!ds.enabled)
            DataSourceRepository::instance().set_enabled(ds.id, true);
    }
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-enabled %1 connections").arg(rows.size()));
}

void DataSourcesScreen::on_bulk_disable_all() {
    const auto rows = filtered_connection_rows();
    for (const auto& ds : rows) {
        if (ds.enabled)
            DataSourceRepository::instance().set_enabled(ds.id, false);
    }
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-disabled %1 connections").arg(rows.size()));
}

void DataSourcesScreen::on_bulk_delete_selected() {
    if (!connections_table_)
        return;

    QSet<QString> selected_ids;
    const auto selected_items = connections_table_->selectedItems();
    for (auto* item : selected_items) {
        if (item->column() == 1)
            selected_ids.insert(item->data(Qt::UserRole).toString());
    }
    if (selected_ids.isEmpty())
        return;

    QMessageBox confirm(this);
    confirm.setWindowTitle("Delete Connections");
    confirm.setText(QString("Delete %1 selected connection(s)?").arg(selected_ids.size()));
    confirm.setInformativeText("This cannot be undone.");
    confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirm.setDefaultButton(QMessageBox::Cancel);
    confirm.setStyleSheet(
        QString("QMessageBox { background:%1; color:%2; font-family:'Consolas','Courier New',monospace; }"
                "QLabel { color:%2; font-size:13px; background:transparent; }"
                "QPushButton { background:%3; color:%2; border:1px solid %4;"
                " padding:6px 18px; font-size:12px; font-weight:700; }"
                "QPushButton:hover { background:%4; }")
            .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::BG_RAISED(), col::BORDER_DIM()));
    if (confirm.exec() != QMessageBox::Yes)
        return;

    for (const auto& id : selected_ids) {
        DataSourceRepository::instance().remove(id);
        live_status_cache_.remove(id);
    }
    if (selected_ids.contains(selected_connection_id_))
        selected_connection_id_.clear();
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-deleted %1 connections").arg(selected_ids.size()));
}

// ── Import / export (thin wrappers — implementation in ImportExportConnections.cpp) ──

void DataSourcesScreen::on_export_connections() {
    export_connections(this);
}

void DataSourcesScreen::on_import_connections() {
    if (import_connections(this))
        refresh_connections();
}

void DataSourcesScreen::on_download_template() {
    download_connector_template(this);
}

void DataSourcesScreen::on_poll_timer() {
    for (const auto& ds : connections_cache_) {
        if (!ds.enabled)
            continue;
        const auto* connector_cfg = find_connector_config(ds.provider);
        if (!connector_cfg || !connector_cfg->testable)
            continue;

        const auto cfg_obj = QJsonDocument::fromJson(ds.config.toUtf8()).object();
        const QString probe_url = provider_probe_url(ds.provider, cfg_obj);

        QString host = cfg_obj.value("host").toString().trimmed();
        int port = cfg_obj.value("port").toVariant().toInt();

        if (host.isEmpty() && !probe_url.isEmpty()) {
            const QUrl u(probe_url);
            host = u.host();
            if (port <= 0) {
                const QString s = u.scheme().toLower();
                port = u.port((s == "https" || s == "wss") ? 443 : 80);
            }
        }
        if (host.isEmpty() || port <= 0)
            continue;

        QPointer<DataSourcesScreen> self = this;
        const QString conn_id = ds.id;
        const QString cap_host = host;
        const int cap_port = port;
        const QString cap_probe = probe_url;

        (void)QtConcurrent::run([self, conn_id, cap_host, cap_port, cap_probe]() {
            QTcpSocket socket;
            socket.connectToHost(cap_host, static_cast<quint16>(cap_port));
            const bool ok = socket.waitForConnected(3000);
            const QString msg = ok ? QString("Reachable: %1").arg(cap_probe.isEmpty() ? cap_host : cap_probe)
                                   : QString("Unreachable: %1").arg(socket.errorString());
            if (ok)
                socket.disconnectFromHost();

            QMetaObject::invokeMethod(
                qApp,
                [self, conn_id, ok, msg]() {
                    if (!self)
                        return;
                    self->live_status_cache_[conn_id] = {ok, msg};
                    self->update_connection_status_cell(conn_id, ok, msg);
                    self->update_detail_panel();
                },
                Qt::QueuedConnection);
        });
    }
}

} // namespace fincept::screens::datasources
