#include "screens/markets/MarketPanelStore.h"

#include "services/markets/MarketDataService.h"
#include "storage/repositories/SettingsRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::screens {

MarketPanelStore& MarketPanelStore::instance() {
    static MarketPanelStore inst;
    return inst;
}

QVector<MarketPanelConfig> MarketPanelStore::load() {
    auto result = SettingsRepository::instance().get(kSettingsKey);
    if (!result.is_ok() || result.value().isEmpty())
        return build_defaults();

    auto doc = QJsonDocument::fromJson(result.value().toUtf8());
    if (!doc.isArray())
        return build_defaults();

    QVector<MarketPanelConfig> panels;
    for (const auto& v : doc.array()) {
        QJsonObject obj = v.toObject();
        MarketPanelConfig cfg;
        cfg.id        = obj["id"].toString();
        cfg.title     = obj["title"].toString();
        cfg.show_name = obj["show_name"].toBool(false);
        for (const auto& s : obj["symbols"].toArray())
            cfg.symbols << s.toString();
        if (obj.contains("column_order")) {
            QStringList cols;
            for (const auto& v : obj["column_order"].toArray())
                cols << v.toString();
            if (!cols.isEmpty())
                cfg.column_order = cols;
        }
        if (cfg.column_order.isEmpty())
            cfg.column_order = default_market_columns();
        cfg.column_index   = obj["column_index"].toInt(0);
        cfg.splitter_index = obj["splitter_index"].toInt(0);
        if (!cfg.id.isEmpty() && !cfg.title.isEmpty())
            panels.append(cfg);
    }
    return panels.isEmpty() ? build_defaults() : panels;
}

void MarketPanelStore::save(const QVector<MarketPanelConfig>& panels) {
    QJsonArray arr;
    for (const auto& cfg : panels) {
        QJsonObject obj;
        obj["id"]        = cfg.id;
        obj["title"]     = cfg.title;
        obj["show_name"] = cfg.show_name;
        QJsonArray syms;
        for (const auto& s : cfg.symbols)
            syms.append(s);
        obj["symbols"] = syms;
        QJsonArray cols;
        for (const auto& c : cfg.column_order)
            cols.append(c);
        obj["column_order"]    = cols;
        obj["column_index"]    = cfg.column_index;
        obj["splitter_index"]  = cfg.splitter_index;
        arr.append(obj);
    }
    SettingsRepository::instance().set(kSettingsKey,
                                       QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)),
                                       kCategory);
}

void MarketPanelStore::reset_to_defaults() {
    save(build_defaults());
}

QVector<MarketPanelConfig> MarketPanelStore::build_defaults() {
    QVector<MarketPanelConfig> panels;

    // Global markets
    for (const auto& cat : services::MarketDataService::default_global_markets())
        panels.append(MarketPanelConfig::make(cat.category, cat.tickers, false));

    // Regional markets — flatten TickerDef → symbol strings
    for (const auto& reg : services::MarketDataService::default_regional_markets()) {
        QStringList syms;
        for (const auto& t : reg.tickers)
            syms << t.symbol;
        panels.append(MarketPanelConfig::make(reg.region, syms, true));
    }

    return panels;
}

} // namespace fincept::screens
