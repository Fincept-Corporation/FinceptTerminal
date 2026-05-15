// src/ui/navigation/CommandBar_Assets.cpp
//
// Asset-search mode: switching into a /stock /fund /index search,
// debounced /market/search calls, and rendering the result list. Also owns
// the to_yfinance_symbol helper that normalises broker exchange suffixes
// before publishing the selected symbol onto the event bus.
//
// Part of the partial-class split of CommandBar.cpp.

#include "ui/navigation/CommandBar.h"
#include "ui/navigation/CommandBar_internal.h"

#include "core/events/EventBus.h"
#include "core/keys/KeyConfigManager.h"
#include "core/session/ScreenStateManager.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QRegularExpression>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::ui {

using fincept::ui::commandbar_internal::kMaxResults;

namespace {
static QString to_yfinance_symbol(const QString& symbol, const QString& exchange, const QString& country = {}) {
    // EURONEXT suffix depends on country
    if (exchange.toUpper() == "EURONEXT") {
        static const QHash<QString, QString> euronext_map = {
            {"FR", ".PA"}, {"NL", ".AS"}, {"BE", ".BR"}, {"PT", ".LS"}, {"IE", ".IR"},
        };
        const auto it = euronext_map.find(country.toUpper());
        return symbol + (it != euronext_map.end() ? it.value() : ".PA");
    }

    static const QHash<QString, QString> suffix_map = {
        // India
        {"NSE", ".NS"},
        {"BSE", ".BO"},
        // Asia-Pacific
        {"HKEX", ".HK"},
        {"TSE", ".T"},
        {"NAG", ".T"},
        {"KRX", ".KS"},
        {"SGX", ".SI"},
        {"ASX", ".AX"},
        {"IDX", ".JK"},
        {"MYX", ".KL"},
        {"SET", ".BK"},
        {"PSE", ".PS"},
        {"TPEX", ".TWO"},
        // Europe — Germany
        {"XETR", ".DE"},
        {"FWB", ".F"},
        {"DUS", ".DU"},
        {"HAM", ".HM"},
        {"HAN", ".HA"},
        {"MUN", ".MU"},
        {"SWB", ".SG"},
        {"GETTEX", ".DE"},
        {"TRADEGATE", ".DE"},
        {"LS", ".DE"},
        {"LSX", ".DE"},
        // Europe — UK
        {"LSE", ".L"},
        {"LSIN", ".L"},
        {"AQUIS", ".L"},
        // Europe — Other
        {"BME", ".MC"},
        {"MIL", ".MI"},
        {"EUROTLX", ".MI"},
        {"SIX", ".SW"},
        {"BX", ".SW"},
        {"VIE", ".VI"},
        {"LUXSE", ".LU"},
        // Americas — Canada
        {"TSX", ".TO"},
        {"TSXV", ".V"},
        {"CSE", ".CN"},
        {"NEO", ".NE"},
        // Americas — Latin America
        {"BMFBOVESPA", ".SA"},
        {"BMV", ".MX"},
        {"BIVA", ".MX"},
        {"BCBA", ".BA"},
        {"BCS", ".SN"},
        {"BVC", ".CL"},
        {"BVL", ".LM"},
        // Middle East & Africa
        {"BIST", ".IS"},
        {"QSE", ".QA"},
        {"EGX", ".CA"},
        {"NSENG", ".LG"},
        // South Asia
        {"PSX", ".KA"},
        {"DSEBD", ".BD"},
    };

    const auto it = suffix_map.find(exchange.toUpper());
    if (it != suffix_map.end())
        return symbol + it.value();
    return symbol; // NASDAQ, NYSE, AMEX, OTC, FINRA, crypto exchanges — no suffix
}

// ── command registry ─────────────────────────────────────────────────────────

// Local debounce duration for asset searches.
constexpr int kSearchDebounceMs = 300;
} // namespace

void CommandBar::activate_asset_mode(const QString& api_type) {
    mode_ = Mode::AssetSearch;
    active_asset_type_ = api_type;
    search_debounce_->stop();
}

void CommandBar::schedule_asset_search(const QString& query) {
    pending_query_ = query;
    search_debounce_->start(kSearchDebounceMs);
}

void CommandBar::fire_asset_search(const QString& query) {
    const QString url = QString("/market/search?q=%1&type=%2&limit=%3").arg(query, active_asset_type_).arg(kMaxResults);

    QPointer<CommandBar> self = this;
    HttpClient::instance().get(url, [self, query](Result<QJsonDocument> result) {
        if (!self)
            return;
        // Only process if user hasn't changed the query since we fired
        if (self->pending_query_ != query)
            return;
        if (!result.is_ok())
            return;

        const auto doc = result.value();
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            // API might wrap results in { "results": [...] } or { "data": [...] }
            const auto obj = doc.object();
            if (obj.contains("results"))
                arr = obj["results"].toArray();
            else if (obj.contains("data"))
                arr = obj["data"].toArray();
        }
        self->on_asset_results(arr);
    });
}

void CommandBar::on_asset_results(const QJsonArray& results) {
    list_->clear();

    if (results.isEmpty()) {
        auto* item = new QListWidgetItem(list_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(10, 6, 10, 6);
        auto* lbl = new QLabel("No results found");
        lbl->setStyleSheet(QString("color:%1;font-size:11px;font-family:'Consolas',monospace;background:transparent;")
                               .arg(colors::TEXT_TERTIARY.get()));
        rl->addWidget(lbl);
        item->setSizeHint(QSize(0, 30));
        list_->setItemWidget(item, row);
        show_dropdown();
        return;
    }

    for (const auto& val : results) {
        const auto obj = val.toObject();
        const QString symbol = obj["symbol"].toString();
        const QString name = obj["name"].toString();
        const QString exchange = obj["exchange"].toString();
        const QString country = obj["country"].toString();
        const QString type = obj["type"].toString(active_asset_type_);

        if (symbol.isEmpty())
            continue;

        // Convert to yfinance-compatible ticker (e.g. RELIANCE → RELIANCE.NS)
        const QString yf_symbol = to_yfinance_symbol(symbol, exchange, country);

        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, yf_symbol);     // yfinance symbol for loading
        item->setData(Qt::UserRole + 1, yf_symbol); // for Tab autocomplete
        item->setData(Qt::UserRole + 2, type);      // asset type

        auto* row = new QWidget;
        row->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(10, 4, 10, 4);
        hl->setSpacing(8);

        auto* sym_lbl = new QLabel(yf_symbol);
        sym_lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;"
                                       "font-family:'Consolas',monospace;background:transparent;")
                                   .arg(colors::TEXT_PRIMARY.get()));
        sym_lbl->setFixedWidth(110);

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;"
                                        "font-family:'Consolas',monospace;")
                                    .arg(colors::TEXT_SECONDARY.get()));
        name_lbl->setMaximumWidth(200);

        hl->addWidget(sym_lbl);
        hl->addWidget(name_lbl, 1);

        if (!exchange.isEmpty()) {
            auto* exch_lbl = new QLabel(exchange);
            exch_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-family:'Consolas',monospace;"
                                            "background:transparent;")
                                        .arg(colors::TEXT_TERTIARY.get()));
            hl->addWidget(exch_lbl);
        }

        // Type badge
        auto* type_lbl = new QLabel(type.toUpper());
        type_lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;"
                                        "font-family:'Consolas',monospace;background:%2;"
                                        "padding:1px 4px;border-radius:2px;")
                                    .arg(colors::AMBER.get())
                                    .arg(colors::BG_RAISED.get()));
        hl->addWidget(type_lbl);

        item->setSizeHint(QSize(0, 32));
        list_->setItemWidget(item, row);
    }

    if (list_->count() > 0) {
        list_->setCurrentRow(0);
        show_dropdown();
    }
}

void CommandBar::select_asset(const QString& symbol, const QString& type) {
    input_->clear();
    mode_ = Mode::Screen;
    active_asset_type_.clear();
    search_debounce_->stop();
    hide_dropdown();
    input_->clearFocus();

    // Navigate to equity_research and tell it to load the symbol
    emit navigate_to("equity_research");
    EventBus::instance().publish("equity_research.load_symbol", {
                                                                    {"symbol", symbol},
                                                                    {"type", type},
                                                                });
}

// ── dropdown helpers ─────────────────────────────────────────────────────────

} // namespace fincept::ui
