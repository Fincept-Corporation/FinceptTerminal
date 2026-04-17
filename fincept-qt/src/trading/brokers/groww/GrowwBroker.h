#pragma once
#include "trading/BrokerInterface.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

class GrowwBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Groww; }
    const char* name() const override { return "Groww"; }
    const char* base_url() const override { return "https://api.groww.in"; }
    // Streams route through the Python ws_bridge → openalgo groww_adapter.
    // Groww's native protocol is NATS.io over WSS with NKEY/Ed25519 auth +
    // protobuf payloads; a native Qt QWebSocket implementation is non-trivial
    // (would need NATS framing, libsodium, and 4 compiled .proto schemas).
    const char* ws_adapter_name() const override { return "groww"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "groww",
            .display_name = "Groww",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "API KEY", "Enter API Key...", false},
                    {CredentialField::ApiSecret, "API SECRET", "Enter API Secret...", true},
                },
            .exchanges = {"NSE", "BSE", "NFO", "BFO"},
            .product_types =
                {
                    {"Intraday (MIS)", ProductType::Intraday},
                    {"Delivery (CNC)", ProductType::Delivery},
                    {"Margin (NRML)", ProductType::Margin},
                },
            .supports_intraday = true,
            .supports_bracket_order = false,
            .supports_cover_order = false,
            .has_native_paper = false,
            .default_paper_balance = 1000000.0,
            .default_watchlist = {"HDFCBANK", "ICICIBANK", "SBIN", "TCS", "INFY", "RELIANCE", "TATAMOTORS",
                                  "BAJFINANCE", "HINDUNILVR", "ITC"},
            .default_symbol = "RELIANCE",
            .default_exchange = "NSE",
            .brokerage_info = "\u20B920/order intraday, 0% delivery",
        };
    }

    TokenExchangeResponse exchange_token(const QString& api_key, const QString& api_secret,
                                         const QString& auth_code) override;
    OrderPlaceResponse place_order(const BrokerCredentials& creds, const UnifiedOrder& order) override;
    ApiResponse<QJsonObject> modify_order(const BrokerCredentials& creds, const QString& order_id,
                                          const QJsonObject& mods) override;
    ApiResponse<QJsonObject> cancel_order(const BrokerCredentials& creds, const QString& order_id) override;
    ApiResponse<QVector<BrokerOrderInfo>> get_orders(const BrokerCredentials& creds) override;
    ApiResponse<QJsonObject> get_trade_book(const BrokerCredentials& creds) override;
    ApiResponse<QVector<BrokerPosition>> get_positions(const BrokerCredentials& creds) override;
    ApiResponse<QVector<BrokerHolding>> get_holdings(const BrokerCredentials& creds) override;
    ApiResponse<BrokerFunds> get_funds(const BrokerCredentials& creds) override;
    ApiResponse<QVector<BrokerQuote>> get_quotes(const BrokerCredentials& creds,
                                                 const QVector<QString>& symbols) override;
    ApiResponse<QVector<BrokerCandle>> get_history(const BrokerCredentials& creds, const QString& symbol,
                                                   const QString& resolution, const QString& from_date,
                                                   const QString& to_date) override;

    // Pre-trade margin calculator — POST /v1/margins/detail/orders?segment=...
    ApiResponse<OrderMargin> get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) override;
    ApiResponse<BasketMargin> get_basket_margins(const BrokerCredentials& creds,
                                                 const QVector<UnifiedOrder>& orders) override;

    // User profile — GET /v1/user/detail. Groww exposes ucc + segments only.
    ApiResponse<QJsonObject> get_profile(const BrokerCredentials& creds);

    // Smart Orders (GTT / OCO) — /v1/order-advance/*. Groww has no iceberg/slice; exposed
    // via the GTT hooks so callers have a unified surface.
    GttPlaceResponse gtt_place(const BrokerCredentials& creds, const GttOrder& order) override;
    ApiResponse<GttOrder> gtt_get(const BrokerCredentials& creds, const QString& gtt_id) override;
    ApiResponse<QVector<GttOrder>> gtt_list(const BrokerCredentials& creds) override;
    ApiResponse<GttOrder> gtt_modify(const BrokerCredentials& creds, const QString& gtt_id,
                                     const GttOrder& updated) override;
    ApiResponse<QJsonObject> gtt_cancel(const BrokerCredentials& creds, const QString& gtt_id) override;

    static bool is_token_expired(const BrokerHttpResponse& resp);
    static QString checked_error(const BrokerHttpResponse& resp, const QString& fallback);
    // Classify a Groww response by its error.code field (GA000..GA007 + HTTP 429).
    // Returns an empty string when the response is successful or carries no GA code.
    // Callers can branch on the code (e.g. "GA005" → token expiry) for retry logic.
    static QString error_code(const BrokerHttpResponse& resp);
    static bool is_rate_limited(const BrokerHttpResponse& resp);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static QString groww_exchange(const QString& exchange);
    static QString groww_segment(const QString& exchange);
    static QString groww_product(ProductType p);
    static QString groww_order_type(OrderType t);
    static int resolution_to_minutes(const QString& resolution);

    // Batch helpers for /v1/live-data/ltp and /v1/live-data/ohlc.
    // Groww accepts up to 50 exchange_symbols per call (format: "NSE_RELIANCE").
    // Both endpoints share a 10 req/sec, 300/min, 5000/day bucket with /quote.
    struct SymbolRef {
        QString orig;     // caller-supplied, e.g. "NSE:RELIANCE" or "RELIANCE"
        QString exchange; // NSE, BSE, NFO, BFO
        QString segment;  // CASH, FNO
        QString trading_symbol;
        QString exchange_symbol; // groww_exchange + "_" + trading_symbol
    };
    static SymbolRef split_symbol(const QString& sym);
    // Fills ltp/open/high/low/close on the matching BrokerQuote entries.
    // Returns true on successful RPC (partial symbol matches still return true).
    bool fetch_ltp_batch(const BrokerCredentials& creds, const QString& segment,
                         const QVector<SymbolRef>& refs, QVector<BrokerQuote>& out);
    bool fetch_ohlc_batch(const BrokerCredentials& creds, const QString& segment,
                          const QVector<SymbolRef>& refs, QVector<BrokerQuote>& out);

    // /v1/historical/candles candle_interval mapping. Groww accepts integer minutes
    // for 1,5,10,15,30,60; "1D" and "1W" map to 1440 / 10080.
    static int history_interval_minutes(const QString& resolution);
    // Build the order JSON used by /v1/margins/detail/orders (per-order object inside the array).
    static QJsonObject order_to_margin_row(const UnifiedOrder& order);
    // Smart-order helpers
    static QJsonObject gtt_to_advance_body(const GttOrder& g, bool is_create);
    static GttOrder parse_advance_to_gtt(const QJsonObject& payload);
};

} // namespace fincept::trading
