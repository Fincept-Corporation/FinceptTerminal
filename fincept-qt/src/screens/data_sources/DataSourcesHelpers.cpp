// DataSourcesHelpers.cpp — pure helpers on ConnectorConfig / DataSource.

#include "screens/data_sources/DataSourcesHelpers.h"

#include "screens/data_sources/ConnectorRegistry.h"
#include "ui/theme/Theme.h"

namespace fincept::screens::datasources {

namespace col = fincept::ui::colors;

const ConnectorConfig* find_connector_config(const QString& key) {
    if (const auto* cfg = ConnectorRegistry::instance().get(key))
        return cfg;
    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        if (cfg.type == key)
            return &cfg;
    }
    return nullptr;
}

QString normalized_provider_key(const DataSource& ds) {
    if (const auto* cfg = find_connector_config(ds.provider))
        return cfg->id;
    return ds.provider;
}

bool connection_matches_connector(const DataSource& ds, const QString& connector_id) {
    return !connector_id.isEmpty() && normalized_provider_key(ds) == connector_id;
}

bool connector_matches_text(const ConnectorConfig& cfg, const QString& filter) {
    if (filter.isEmpty())
        return true;
    const QString haystack =
        QString("%1 %2 %3 %4 %5").arg(cfg.name, cfg.id, cfg.type, cfg.description, category_label(cfg.category));
    return haystack.toLower().contains(filter);
}

QString connector_code(const ConnectorConfig& cfg) {
    const QString icon = cfg.icon.trimmed().toUpper();
    if (!icon.isEmpty())
        return icon.left(2);
    return cfg.name.left(2).toUpper();
}

QString connector_transport(const ConnectorConfig& cfg) {
    switch (cfg.category) {
        case Category::Database:        return "SQL";
        case Category::Api:             return "REST";
        case Category::File:            return "FILE";
        case Category::Streaming:       return "WS";
        case Category::Cloud:           return "CLOUD";
        case Category::TimeSeries:      return "TSDB";
        case Category::MarketData:      return "MKT";
        case Category::SearchAnalytics: return "SRCH";
        case Category::DataWarehouse:   return "DW";
        case Category::AlternativeData: return "ALT";
        case Category::OpenBanking:     return "OB";
    }
    return "API";
}

QString persistence_type(const ConnectorConfig& cfg) {
    return cfg.category == Category::Streaming ? "websocket" : "rest_api";
}

QString field_type_label(FieldType type) {
    switch (type) {
        case FieldType::Text:     return "Text";
        case FieldType::Password: return "Secret";
        case FieldType::Number:   return "Number";
        case FieldType::Url:      return "URL";
        case FieldType::Select:   return "Select";
        case FieldType::Textarea: return "Textarea";
        case FieldType::Checkbox: return "Bool";
        case FieldType::File:     return "File";
    }
    return "Text";
}

int total_connections_for_provider(const QVector<DataSource>& rows, const QString& connector_id) {
    int count = 0;
    for (const auto& ds : rows)
        if (connection_matches_connector(ds, connector_id))
            ++count;
    return count;
}

int enabled_connections_for_provider(const QVector<DataSource>& rows, const QString& connector_id) {
    int count = 0;
    for (const auto& ds : rows)
        if (ds.enabled && connection_matches_connector(ds, connector_id))
            ++count;
    return count;
}

QTableWidgetItem* make_item(const QString& text, const QColor& color, Qt::Alignment alignment) {
    auto* item = new QTableWidgetItem(text);
    item->setForeground(color.isValid() ? color : QColor(col::TEXT_PRIMARY()));
    item->setTextAlignment(alignment);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace fincept::screens::datasources
