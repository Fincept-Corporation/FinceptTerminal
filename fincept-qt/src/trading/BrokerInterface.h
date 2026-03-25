#pragma once
// Broker Interface — abstract base class for all broker integrations

#include "trading/TradingTypes.h"

#include <QStringList>
#include <memory>

namespace fincept::trading {

// ============================================================================
// BrokerProfile — single source of truth for per-broker UI/config
// Each broker returns one of these from profile(). The UI reads it on switch.
// ============================================================================

enum class CredentialField {
    ApiKey,       // "API Key" / "Client ID"
    ApiSecret,    // "API Secret" / "Secret Key" / "MPIN"
    AuthCode,     // "Auth Code" / "TOTP secret" (base32)
    Environment,  // "Live / Paper" toggle — Alpaca only
    ClientCode,   // Secondary login ID (AngelOne: client code separate from API key)
};

struct CredentialFieldDef {
    CredentialField field;
    QString label;        // display label, e.g. "API Key ID"
    QString placeholder;  // input placeholder
    bool secret = false;  // echo as password
};

struct ProductTypeDef {
    QString label;               // display label, e.g. "Intraday (MIS)"
    trading::ProductType value;
};

struct BrokerProfile {
    // Identity
    QString id;           // e.g. "alpaca"
    QString display_name; // e.g. "Alpaca"
    QString region;       // "IN", "US", "EU"
    QString currency;     // "INR", "USD", "GBP", "EUR"

    // Credentials dialog — only listed fields are shown
    QVector<CredentialFieldDef> credential_fields;

    // Order form
    QStringList exchanges;           // available exchanges in order form
    QVector<ProductTypeDef> product_types; // available product types
    bool supports_intraday = true;
    bool supports_bracket_order = false;
    bool supports_cover_order = false;

    // Paper trading
    bool has_native_paper = false;   // broker provides its own paper trading (Alpaca)
    double default_paper_balance = 1000000.0;

    // Market data
    QStringList default_watchlist;   // symbols to show on first switch
    QString default_symbol;          // first selected symbol
    QString default_exchange;        // first selected exchange

    // Brokerage info (for display only)
    QString brokerage_info;          // e.g. "₹20/order or 0.03%"
};

// ============================================================================
// IBroker
// ============================================================================

class IBroker {
  public:
    virtual ~IBroker() = default;

    virtual BrokerId id() const = 0;
    virtual const char* name() const = 0;
    virtual const char* base_url() const = 0;
    virtual BrokerProfile profile() const = 0;

    // --- Authentication ---
    virtual TokenExchangeResponse exchange_token(const QString& api_key, const QString& api_secret,
                                                 const QString& auth_code) = 0;

    // --- Orders ---
    virtual OrderPlaceResponse place_order(const BrokerCredentials& creds, const UnifiedOrder& order) = 0;
    virtual ApiResponse<QJsonObject> modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                  const QJsonObject& modifications) = 0;
    virtual ApiResponse<QJsonObject> cancel_order(const BrokerCredentials& creds, const QString& order_id) = 0;
    virtual ApiResponse<QVector<BrokerOrderInfo>> get_orders(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<QJsonObject> get_trade_book(const BrokerCredentials& creds) = 0;

    // --- Portfolio ---
    virtual ApiResponse<QVector<BrokerPosition>> get_positions(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<QVector<BrokerHolding>> get_holdings(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<BrokerFunds> get_funds(const BrokerCredentials& creds) = 0;

    // --- Market Data ---
    virtual ApiResponse<QVector<BrokerQuote>> get_quotes(const BrokerCredentials& creds,
                                                         const QVector<QString>& symbols) = 0;
    virtual ApiResponse<QVector<BrokerCandle>> get_history(const BrokerCredentials& creds, const QString& symbol,
                                                           const QString& resolution, const QString& from_date,
                                                           const QString& to_date) = 0;

    // --- Calendar & Clock ---
    virtual ApiResponse<QVector<MarketCalendarDay>> get_calendar(const BrokerCredentials& creds,
                                                                 const QString& start, const QString& end) {
        Q_UNUSED(creds); Q_UNUSED(start); Q_UNUSED(end);
        return {false, std::nullopt, "Calendar not supported for this broker"};
    }
    virtual ApiResponse<MarketClock> get_clock(const BrokerCredentials& creds) {
        Q_UNUSED(creds);
        return {false, std::nullopt, "Clock not supported for this broker"};
    }

    // --- Stock market data (multi-symbol) ---
    virtual ApiResponse<QVector<BrokerCandle>> get_latest_bars(const BrokerCredentials& creds,
                                                               const QVector<QString>& symbols) {
        Q_UNUSED(creds); Q_UNUSED(symbols);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerCandle>> get_historical_bars(const BrokerCredentials& creds,
                                                                   const QVector<QString>& symbols,
                                                                   const QString& timeframe,
                                                                   const QString& start, const QString& end) {
        Q_UNUSED(creds); Q_UNUSED(symbols); Q_UNUSED(timeframe); Q_UNUSED(start); Q_UNUSED(end);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerQuote>> get_latest_quotes(const BrokerCredentials& creds,
                                                                const QVector<QString>& symbols) {
        Q_UNUSED(creds); Q_UNUSED(symbols);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerTrade>> get_latest_trades(const BrokerCredentials& creds,
                                                                const QVector<QString>& symbols) {
        Q_UNUSED(creds); Q_UNUSED(symbols);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerTrade>> get_historical_trades(const BrokerCredentials& creds,
                                                                    const QVector<QString>& symbols,
                                                                    const QString& start, const QString& end,
                                                                    int limit = 1000) {
        Q_UNUSED(creds); Q_UNUSED(symbols); Q_UNUSED(start); Q_UNUSED(end); Q_UNUSED(limit);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerAuction>> get_historical_auctions(const BrokerCredentials& creds,
                                                                        const QVector<QString>& symbols,
                                                                        const QString& start, const QString& end) {
        Q_UNUSED(creds); Q_UNUSED(symbols); Q_UNUSED(start); Q_UNUSED(end);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerMetaEntry>> get_condition_codes(const BrokerCredentials& creds,
                                                                      const QString& ticktype,
                                                                      const QString& tape) {
        Q_UNUSED(creds); Q_UNUSED(ticktype); Q_UNUSED(tape);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerMetaEntry>> get_exchange_codes(const BrokerCredentials& creds,
                                                                     const QString& asset_class) {
        Q_UNUSED(creds); Q_UNUSED(asset_class);
        return {false, std::nullopt, "Not supported"};
    }

    // --- Stock market data (single symbol) ---
    virtual ApiResponse<BrokerCandle> get_latest_bar(const BrokerCredentials& creds,
                                                     const QString& symbol) {
        Q_UNUSED(creds); Q_UNUSED(symbol);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<BrokerQuote> get_latest_quote(const BrokerCredentials& creds,
                                                      const QString& symbol) {
        Q_UNUSED(creds); Q_UNUSED(symbol);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<BrokerTrade> get_latest_trade(const BrokerCredentials& creds,
                                                      const QString& symbol) {
        Q_UNUSED(creds); Q_UNUSED(symbol);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<BrokerQuote> get_snapshot(const BrokerCredentials& creds,
                                                  const QString& symbol) {
        Q_UNUSED(creds); Q_UNUSED(symbol);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerTrade>> get_historical_trades_single(const BrokerCredentials& creds,
                                                                           const QString& symbol,
                                                                           const QString& start,
                                                                           const QString& end,
                                                                           int limit = 1000) {
        Q_UNUSED(creds); Q_UNUSED(symbol); Q_UNUSED(start); Q_UNUSED(end); Q_UNUSED(limit);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerQuote>> get_historical_quotes_single(const BrokerCredentials& creds,
                                                                           const QString& symbol,
                                                                           const QString& start,
                                                                           const QString& end,
                                                                           int limit = 1000) {
        Q_UNUSED(creds); Q_UNUSED(symbol); Q_UNUSED(start); Q_UNUSED(end); Q_UNUSED(limit);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerAuction>> get_historical_auctions_single(const BrokerCredentials& creds,
                                                                               const QString& symbol,
                                                                               const QString& start,
                                                                               const QString& end) {
        Q_UNUSED(creds); Q_UNUSED(symbol); Q_UNUSED(start); Q_UNUSED(end);
        return {false, std::nullopt, "Not supported"};
    }

    // --- WebSocket streaming ---
    virtual const char* ws_adapter_name() const { return ""; }

    // --- Credential helpers ---
    BrokerCredentials load_credentials() const;
    BrokerCredentials load_credentials_for_env(const QString& env) const; // env="live"/"paper"/""
    void save_credentials(const BrokerCredentials& creds) const;

  protected:
    virtual QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const = 0;
};

using BrokerPtr = std::unique_ptr<IBroker>;

} // namespace fincept::trading
