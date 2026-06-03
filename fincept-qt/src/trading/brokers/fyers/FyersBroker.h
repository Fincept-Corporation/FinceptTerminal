#pragma once
// Fyers Broker — Indian stock broker
// API: https://api-t1.fyers.in
// Auth: SHA256(api_key:api_secret) for token exchange

#include "trading/BrokerInterface.h"
#include "trading/adapter/BrokerEnumMap.h"

namespace fincept::trading {

class FyersBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Fyers; }
    const char* name() const override { return "Fyers"; }
    const char* base_url() const override { return "https://api-t1.fyers.in"; }
    const char* ws_adapter_name() const override { return "fyers"; }

    static QString fyers_login_url(const QString& client_id, const QString& redirect_uri);

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "fyers",
            .display_name = "Fyers",
            .region = "IN",
            .currency = "INR",
            .credential_fields =
                {
                    {CredentialField::ApiKey, "CLIENT ID", "Enter Client ID...", false},
                    {CredentialField::ApiSecret, "API SECRET", "Enter API Secret...", true},
                    {CredentialField::AuthCode, "AUTH CODE", "Paste auth code from Fyers login...", false},
                },
            .exchanges = {"NSE", "BSE", "NFO", "MCX", "CDS"},
            .product_types =
                {
                    {"Intraday (MIS)", ProductType::Intraday},
                    {"Delivery (CNC)", ProductType::Delivery},
                    {"Margin (NRML)", ProductType::Margin},
                    {"Cover Order", ProductType::CoverOrder},
                    {"Bracket Order", ProductType::BracketOrder},
                },
            .supports_intraday = true,
            .supports_bracket_order = true,
            .supports_cover_order = true,
            .has_native_paper = false,
            .default_paper_balance = 1000000.0,
            // Nifty 50 constituents (NSE large-caps). Plain symbols; the stream's
            // to_fyers() helper maps each to "NSE:<sym>-EQ" when subscribing.
            .default_watchlist = {"ADANIENT",   "ADANIPORTS", "APOLLOHOSP", "ASIANPAINT", "AXISBANK",
                                  "BAJAJ-AUTO", "BAJAJFINSV", "BAJFINANCE", "BEL",        "BHARTIARTL",
                                  "CIPLA",      "COALINDIA",  "DIVISLAB",   "DRREDDY",    "EICHERMOT",
                                  "GRASIM",     "HCLTECH",    "HDFCBANK",   "HDFCLIFE",   "HEROMOTOCO",
                                  "HINDALCO",   "HINDUNILVR", "ICICIBANK",  "INDUSINDBK", "INFY",
                                  "ITC",        "JIOFIN",     "JSWSTEEL",   "KOTAKBANK",  "LT",
                                  "M&M",        "MARUTI",     "NESTLEIND",  "NTPC",       "ONGC",
                                  "POWERGRID",  "RELIANCE",   "SBILIFE",    "SBIN",       "SHRIRAMFIN",
                                  "SUNPHARMA",  "TATACONSUM", "TATASTEEL",  "TCS",
                                  "TECHM",      "TITAN",      "TRENT",      "ULTRACEMCO", "WIPRO"},
            .default_symbol = "RELIANCE",
            .default_exchange = "NSE",
            .brokerage_info = "\u20B920/order or 0.03% (whichever lower)",
        };
    }

    TokenExchangeResponse exchange_token(const QString& api_key, const QString& api_secret,
                                         const QString& auth_code) override;

    // Fyers issues a refresh token valid for 15 days. Silent refresh additionally
    // requires the user's trading PIN, stored in additional_data["pin"].
    bool supports_silent_refresh() const override { return true; }
    TokenExchangeResponse refresh_session(const BrokerCredentials& creds) override;

    OrderPlaceResponse place_order(const BrokerCredentials& creds, const UnifiedOrder& order) override;
    ApiResponse<QJsonObject> modify_order(const BrokerCredentials& creds, const QString& order_id,
                                          const QJsonObject& modifications) override;
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

    // --- Multi-quote & Market Depth ---
    ApiResponse<QVector<BrokerQuote>> get_multi_quotes(
        const BrokerCredentials& creds,
        const QVector<QPair<QString, QString>>& symbols) override;

    ApiResponse<MarketDepth> get_market_depth(
        const BrokerCredentials& creds,
        const QString& symbol, const QString& exchange) override;

    // Market depth — GET /data/depth (5-level bid/ask)
    ApiResponse<QVector<BrokerQuote>> get_historical_quotes_single(const BrokerCredentials& creds,
                                                                    const QString& symbol, const QString& start,
                                                                    const QString& end, int limit = 1000) override;

    // Market clock — GET /api/v3/marketStatus.
    ApiResponse<MarketClock> get_clock(const BrokerCredentials& creds) override;

    // --- Margin Calculator --- POST /api/v3/multiorder/margin (native; total only, no SPAN breakdown)
    ApiResponse<OrderMargin> get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) override;

    // GTT — /api/v3/gtt/orders[/sync].
    GttPlaceResponse gtt_place(const BrokerCredentials& creds, const GttOrder& order) override;
    ApiResponse<GttOrder> gtt_get(const BrokerCredentials& creds, const QString& gtt_id) override;
    ApiResponse<QVector<GttOrder>> gtt_list(const BrokerCredentials& creds) override;
    ApiResponse<GttOrder> gtt_modify(const BrokerCredentials& creds, const QString& gtt_id,
                                     const GttOrder& updated) override;
    ApiResponse<QJsonObject> gtt_cancel(const BrokerCredentials& creds, const QString& gtt_id) override;

    // Multi-order batch (up to 10 legs) — non-virtual; callers reach via dynamic_cast.
    ApiResponse<QJsonObject> place_multi_order(const BrokerCredentials& creds,
                                               const QVector<UnifiedOrder>& orders);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    /// Fyers uses int wire values for type/side and string for product, so
    /// two maps are needed.
    static const BrokerEnumMap<int>& fyers_int_map();
    static const BrokerEnumMap<QString>& fyers_str_map();
};

} // namespace fincept::trading
