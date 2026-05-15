// DataSourcesScreen_Tables.cpp
//
// Table/ladder rebuild + update logic: build_category_ladder,
// build_connector_table, build_connections_table, update_stats_strip,
// update_provider_ladder, update_detail_panel, update_action_states,
// plus the small helpers select_connector_by_id, effective_detail_connection_id,
// preferred_connection_for_connector.
//
// Part of the partial-class split of DataSourcesScreen.cpp.

#include "screens/data_sources/DataSourcesScreen.h"

#include "core/logging/Logger.h"

namespace {
static const QStringList kCategoryLabels = {
    "All Connectors", "Databases",   "APIs",   "Files",      "Streaming",        "Cloud",
    "Time Series",    "Market Data", "Search", "Warehouses", "Alternative Data", "Open Banking"};
} // anonymous namespace

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

void DataSourcesScreen::build_category_ladder() {
    if (!category_list_)
        return;

    QVector<int> counts(12, 0);
    const QString filter = search_edit_ ? search_edit_->text().trimmed().toLower() : "";

    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        if (!connector_matches_text(cfg, filter))
            continue;
        ++counts[0];
        ++counts[static_cast<int>(cfg.category) + 1];
    }

    QSignalBlocker blocker(category_list_);
    category_list_->clear();

    for (int i = 0; i < kCategoryLabels.size(); ++i) {
        const QString text = QString("%1  (%2)").arg(kCategoryLabels[i]).arg(counts[i]);
        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, i);
        item->setForeground(i == 0 ? QColor(col::TEXT_PRIMARY()) : QColor(col::TEXT_SECONDARY()));
        category_list_->addItem(item);
    }

    const int active_row = show_all_categories_ ? 0 : (static_cast<int>(active_category_) + 1);
    if (category_list_->count() > active_row)
        category_list_->setCurrentRow(active_row);
}

void DataSourcesScreen::build_connector_table() {
    if (!connector_table_)
        return;

    const auto filtered = filtered_connectors();
    int preferred_row = -1;

    {
        QSignalBlocker blocker(connector_table_);
        connector_table_->setRowCount(filtered.size());

        for (int row = 0; row < filtered.size(); ++row) {
            const auto& cfg = filtered[row];
            const int total_saved = total_connections_for_provider(connections_cache_, cfg.id);
            const int live_saved = enabled_connections_for_provider(connections_cache_, cfg.id);

            // Col 0: code badge
            auto* code_item = make_item(connector_code(cfg), QColor(cfg.color), Qt::AlignCenter);
            connector_table_->setItem(row, 0, code_item);

            // Col 1: name (carries connector ID in UserRole)
            auto* name_item = make_item(cfg.name);
            name_item->setData(Qt::UserRole, cfg.id);
            connector_table_->setItem(row, 1, name_item);

            // Col 2: category
            connector_table_->setItem(row, 2, make_item(category_label(cfg.category), QColor(col::TEXT_SECONDARY())));

            // Col 3: auth
            connector_table_->setItem(row, 3,
                                      make_item(cfg.requires_auth ? "KEY" : "OPEN",
                                                cfg.requires_auth ? QColor(col::WARNING()) : QColor(col::POSITIVE()),
                                                Qt::AlignCenter));

            // Col 4: transport type
            connector_table_->setItem(
                row, 4, make_item(connector_transport(cfg), QColor(col::TEXT_TERTIARY()), Qt::AlignCenter));

            // Col 5: active connections
            connector_table_->setItem(row, 5,
                                      make_item(live_saved > 0 ? QString::number(live_saved) : "-",
                                                live_saved > 0 ? QColor(col::POSITIVE()) : QColor(col::TEXT_TERTIARY()),
                                                Qt::AlignCenter));

            // Col 6: total connections
            connector_table_->setItem(
                row, 6,
                make_item(total_saved > 0 ? QString::number(total_saved) : "-",
                          total_saved > 0 ? QColor(col::TEXT_PRIMARY()) : QColor(col::TEXT_TERTIARY()),
                          Qt::AlignCenter));

            if (cfg.id == selected_connector_id_)
                preferred_row = row;
        }
    }

    if (count_label_)
        count_label_->setText(QString("%1 / %2").arg(filtered.size()).arg(ConnectorRegistry::instance().count()));

    if (preferred_row >= 0) {
        connector_table_->selectRow(preferred_row);
    } else if (!selected_connector_id_.isEmpty()) {
        connector_table_->clearSelection();
    }

    update_detail_panel();
}

void DataSourcesScreen::build_connections_table() {
    if (!connections_table_)
        return;

    const auto rows = filtered_connection_rows();

    QSignalBlocker blocker(connections_table_);
    connections_table_->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        const auto& ds = rows[r];
        const auto* cfg = find_connector_config(ds.provider);

        // Col 0: enable checkbox
        auto* toggle = new QCheckBox;
        toggle->setObjectName("dsEnableToggle");
        toggle->setChecked(ds.enabled);
        toggle->setProperty("conn_id", ds.id);
        toggle->setCursor(Qt::PointingHandCursor);
        connect(toggle, &QCheckBox::toggled, this,
                [this, id = ds.id](bool checked) { on_connection_enabled_changed(id, checked); });
        connections_table_->setCellWidget(r, 0, toggle);

        // Col 1: name (carries conn ID in UserRole)
        auto* name_item = make_item(ds.display_name);
        name_item->setData(Qt::UserRole, ds.id);
        connections_table_->setItem(r, 1, name_item);

        // Col 2: provider
        const QString prov_color = cfg ? cfg->color : col::TEXT_SECONDARY();
        connections_table_->setItem(r, 2, make_item(cfg ? cfg->name : ds.provider, QColor(prov_color)));

        // Col 3: category
        connections_table_->setItem(r, 3, make_item(ds.category, QColor(col::TEXT_SECONDARY())));

        // Col 4: type
        connections_table_->setItem(
            r, 4, make_item(cfg ? connector_transport(*cfg) : ds.type, QColor(col::TEXT_TERTIARY()), Qt::AlignCenter));

        // Col 5: live status
        const bool has_status = live_status_cache_.contains(ds.id);
        const bool ok = has_status ? live_status_cache_[ds.id].first : false;
        auto* status_lbl = new QLabel(has_status ? (ok ? "OK" : "ERR") : "--");
        status_lbl->setAlignment(Qt::AlignCenter);
        status_lbl->setObjectName("dsStatusDot");
        status_lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                      .arg(!has_status ? col::TEXT_TERTIARY()
                                           : ok        ? col::POSITIVE()
                                                       : col::NEGATIVE()));
        if (has_status)
            status_lbl->setToolTip(live_status_cache_[ds.id].second);
        connections_table_->setCellWidget(r, 5, status_lbl);

        // Col 6: tags
        connections_table_->setItem(r, 6, make_item(ds.tags, QColor(col::TEXT_TERTIARY())));

        // Col 7: updated_at
        connections_table_->setItem(r, 7, make_item(ds.updated_at.left(16), QColor(col::TEXT_TERTIARY())));
    }
}

void DataSourcesScreen::update_stats_strip() {
    if (!universe_stat_value_)
        return;

    const auto& all = ConnectorRegistry::instance().all();
    const int universe = all.size();

    QSet<QString> configured_providers;
    int active_count = 0, auth_count = 0;
    for (const auto& ds : connections_cache_) {
        configured_providers.insert(normalized_provider_key(ds));
        if (ds.enabled)
            ++active_count;
    }
    for (const auto& cfg : all) {
        if (cfg.requires_auth)
            ++auth_count;
    }

    universe_stat_value_->setText(QString::number(universe));
    configured_stat_value_->setText(QString::number(configured_providers.size()));
    active_stat_value_->setText(QString::number(active_count));
    auth_stat_value_->setText(QString::number(auth_count));

    // Highlight active stat filter
    auto reset_color = [](QLabel* v) {
        v->setStyleSheet(
            QString("color:%1;font-size:22px;font-weight:700;background:transparent;").arg(col::TEXT_PRIMARY()));
    };
    auto set_amber = [](QLabel* v) {
        v->setStyleSheet(QString("color:%1;font-size:22px;font-weight:700;background:transparent;").arg(col::AMBER()));
    };

    reset_color(universe_stat_value_);
    reset_color(configured_stat_value_);
    reset_color(active_stat_value_);
    reset_color(auth_stat_value_);

    switch (stat_filter_) {
        case 0:
            set_amber(universe_stat_value_);
            break;
        case 1:
            set_amber(configured_stat_value_);
            break;
        case 2:
            set_amber(active_stat_value_);
            break;
        case 3:
            set_amber(auth_stat_value_);
            break;
        default:
            break;
    }
}

void DataSourcesScreen::update_provider_ladder() {
    if (!provider_ladder_)
        return;

    // Build connector -> total_connections map
    QMap<QString, int> conn_counts;
    for (const auto& ds : connections_cache_) {
        const QString key = normalized_provider_key(ds);
        conn_counts[key]++;
    }

    // Top 5 by count
    QVector<QPair<int, QString>> ranked;
    for (auto it = conn_counts.begin(); it != conn_counts.end(); ++it)
        ranked.append({it.value(), it.key()});
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    ranked = ranked.mid(0, 8);

    QSignalBlocker blocker(provider_ladder_);
    provider_ladder_->clear();

    for (const auto& [count, id] : ranked) {
        const auto* cfg = find_connector_config(id);
        const QString name = cfg ? cfg->name : id;
        auto* item = new QListWidgetItem(QString("%1  (%2)").arg(name).arg(count));
        item->setData(Qt::UserRole, id);
        item->setForeground(QColor(col::TEXT_SECONDARY()));
        provider_ladder_->addItem(item);
    }

    if (ranked.isEmpty()) {
        auto* empty = new QListWidgetItem("no connections yet");
        empty->setForeground(QColor(col::TEXT_TERTIARY()));
        empty->setFlags(Qt::NoItemFlags);
        provider_ladder_->addItem(empty);
    }
}

void DataSourcesScreen::update_detail_panel() {
    if (!detail_title_)
        return;

    const auto* cfg = find_connector_config(selected_connector_id_);

    if (!cfg) {
        detail_symbol_->setText("--");
        detail_symbol_->setStyleSheet(QString("min-width:38px;max-width:38px;min-height:38px;max-height:38px;"
                                              "font-size:13px;font-weight:700;color:%1;background:%2;"
                                              "border:1px solid %1;")
                                          .arg(col::AMBER(), col::BG_BASE()));
        detail_title_->setText("Select a connector");
        detail_description_->setText("Double-click any row to configure");
        detail_category_value_->setText("--");
        detail_transport_value_->setText("--");
        detail_auth_value_->setText("--");
        detail_test_value_->setText("--");
        detail_fields_value_->setText("--");
        detail_configured_value_->setText("--");
        detail_enabled_value_->setText("--");
        detail_last_status_value_->setText("--");
        if (field_table_)
            field_table_->setRowCount(0);
        if (detail_connections_list_)
            detail_connections_list_->clear();
        if (new_connection_btn_)
            new_connection_btn_->setEnabled(false);
        if (edit_connection_btn_)
            edit_connection_btn_->setEnabled(false);
        if (test_connection_btn_)
            test_connection_btn_->setEnabled(false);
        return;
    }

    // Identity
    detail_symbol_->setText(connector_code(*cfg));
    detail_symbol_->setStyleSheet(QString("min-width:38px;max-width:38px;min-height:38px;max-height:38px;"
                                          "font-size:13px;font-weight:700;color:%1;background:%2;border:1px solid %1;")
                                      .arg(cfg->color, col::BG_BASE()));
    detail_title_->setText(cfg->name);
    detail_description_->setText(cfg->description);

    // Metadata
    detail_category_value_->setText(category_label(cfg->category));
    detail_transport_value_->setText(connector_transport(*cfg));
    detail_auth_value_->setText(cfg->requires_auth ? "Required" : "None");
    detail_auth_value_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                          .arg(cfg->requires_auth ? col::WARNING() : col::POSITIVE()));
    detail_test_value_->setText(cfg->testable ? "Yes" : "No");

    const int total = total_connections_for_provider(connections_cache_, cfg->id);
    const int active = enabled_connections_for_provider(connections_cache_, cfg->id);
    detail_fields_value_->setText(QString::number(cfg->fields.size()));
    detail_configured_value_->setText(QString::number(total));
    detail_enabled_value_->setText(QString::number(active));
    detail_enabled_value_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                             .arg(active > 0 ? col::POSITIVE() : col::TEXT_TERTIARY()));

    // Last test status
    const QString conn_id = effective_detail_connection_id();
    if (!conn_id.isEmpty() && live_status_cache_.contains(conn_id)) {
        const bool ok = live_status_cache_[conn_id].first;
        detail_last_status_value_->setText(ok ? "OK" : "ERR");
        detail_last_status_value_->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                .arg(ok ? col::POSITIVE() : col::NEGATIVE()));
    } else {
        detail_last_status_value_->setText("--");
        detail_last_status_value_->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;background:transparent;").arg(col::TEXT_TERTIARY()));
    }

    // Fields table
    if (field_table_) {
        QSignalBlocker fb(field_table_);
        field_table_->setRowCount(cfg->fields.size());
        for (int i = 0; i < cfg->fields.size(); ++i) {
            const auto& f = cfg->fields[i];
            field_table_->setItem(i, 0, make_item(f.label));
            field_table_->setItem(i, 1,
                                  make_item(field_type_label(f.type), QColor(col::TEXT_SECONDARY()), Qt::AlignCenter));
            field_table_->setItem(i, 2,
                                  make_item(f.required ? "Y" : "N",
                                            f.required ? QColor(col::WARNING()) : QColor(col::TEXT_TERTIARY()),
                                            Qt::AlignCenter));
        }
        // Shrink to content (max 120px = ~5 rows)
        const int row_h = 24;
        const int hdr_h = 24;
        field_table_->setFixedHeight(std::min(hdr_h + static_cast<int>(cfg->fields.size()) * row_h, 120));
    }

    // Saved connections list
    if (detail_connections_list_) {
        QSignalBlocker lb(detail_connections_list_);
        detail_connections_list_->clear();
        for (const auto& ds : connections_cache_) {
            if (!connection_matches_connector(ds, cfg->id))
                continue;
            const QString status_str = ds.enabled ? "ACTIVE" : "OFF";
            const QString label = QString("%1  [%2]").arg(ds.display_name).arg(status_str);
            auto* item = new QListWidgetItem(label);
            item->setData(Qt::UserRole, ds.id);
            item->setForeground(ds.enabled ? QColor(col::TEXT_PRIMARY()) : QColor(col::TEXT_SECONDARY()));
            detail_connections_list_->addItem(item);
            if (ds.id == selected_connection_id_) {
                detail_connections_list_->setCurrentItem(item);
            }
        }
        if (detail_connections_list_->count() == 0) {
            auto* empty = new QListWidgetItem("No connections configured");
            empty->setForeground(QColor(col::TEXT_TERTIARY()));
            empty->setFlags(Qt::NoItemFlags);
            detail_connections_list_->addItem(empty);
        }
    }

    update_action_states();
}

void DataSourcesScreen::update_action_states() {
    const bool has_connector = !selected_connector_id_.isEmpty();
    const QString conn_id = effective_detail_connection_id();
    const bool has_conn = !conn_id.isEmpty();

    if (new_connection_btn_)
        new_connection_btn_->setEnabled(has_connector);
    if (edit_connection_btn_)
        edit_connection_btn_->setEnabled(has_conn);
    if (test_connection_btn_)
        test_connection_btn_->setEnabled(has_conn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

QString DataSourcesScreen::effective_detail_connection_id() const {
    if (!selected_connection_id_.isEmpty())
        return selected_connection_id_;
    return preferred_connection_for_connector(selected_connector_id_);
}

QString DataSourcesScreen::preferred_connection_for_connector(const QString& connector_id) const {
    QString first_match;
    for (const auto& ds : connections_cache_) {
        if (!connection_matches_connector(ds, connector_id))
            continue;
        if (ds.enabled)
            return ds.id;
        if (first_match.isEmpty())
            first_match = ds.id;
    }
    return first_match;
}

void DataSourcesScreen::select_connector_by_id(const QString& connector_id) {
    const auto* cfg = find_connector_config(connector_id);
    if (!cfg)
        return;
    selected_connector_id_ = cfg->id;
    if (!connector_table_)
        return;
    for (int row = 0; row < connector_table_->rowCount(); ++row) {
        auto* item = connector_table_->item(row, 1);
        if (item && item->data(Qt::UserRole).toString() == selected_connector_id_) {
            connector_table_->selectRow(row);
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────
} // namespace fincept::screens::datasources
