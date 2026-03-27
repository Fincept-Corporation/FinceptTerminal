#pragma once
#include "trading/BrokerInterface.h"
#include "trading/brokers/BrokerHttp.h"

namespace fincept::trading {

class KotakBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Kotak; }
    const char* name() const override { return "Kotak"; }
    const char* base_url() const override { return "https://mis.kotaksecurities.com"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "kotak", .display_name = "Kotak Neo", .region = "IN", .currency = "INR",
            .credential_fields = {
                {CredentialField::ClientCode, "UCC (CLIENT ID)",  "Enter UCC / login ID...",          false},
                {CredentialField::ApiKey,     "MOBILE NUMBER",    "Enter mobile e.g. +919876543210...",false},
                {CredentialField::ApiSecret,  "MPIN",             "Enter 6-digit MPIN...",             true},
                {CredentialField::AuthCode,   "TOTP SECRET",      "Enter base32 TOTP secret...",       false},
            },
            .exchanges = {"NSE","BSE","NFO","BFO","CDS","MCX"},
            .product_types = {
                {"Intraday (MIS)", ProductType::Intraday},
                {"Delivery (CNC)", ProductType::Delivery},
                {"Margin (NRML)",  ProductType::Margin},
            },
            .supports_intraday=true, .supports_bracket_order=false, .supports_cover_order=false,
            .has_native_paper=false, .default_paper_balance=1000000.0,
            .default_watchlist={"HDFCBANK","ICICIBANK","SBIN","TCS","INFY","RELIANCE","TATAMOTORS","BAJFINANCE","HINDUNILVR","ITC"},
            .default_symbol="RELIANCE", .default_exchange="NSE",
            .brokerage_info="\u20B920/order or 0.025%",
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

    static bool    is_token_expired(const BrokerHttpResponse& resp);
    static QString checked_error(const BrokerHttpResponse& resp, const QString& fallback);

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    // Packed token: "trading_token:::trading_sid:::base_url:::access_token"
    struct TokenParts {
        QString trading_token;
        QString trading_sid;
        QString base_url;
        QString access_token;
        bool    valid = false;
    };
    static TokenParts unpack(const QString& packed);
    static QString    kotak_exchange(const QString& exchange);
    static QString    kotak_product(ProductType p);
    static QString    kotak_order_type(OrderType t);
    static QString    lookup_psymbol(const QString& symbol, const QString& exchange,
                                     const QString& broker_id);
    static QString    generate_totp(const QString& base32_secret);
    static QMap<QString, QString> jdata_params(const QJsonObject& obj);
};

} // namespace fincept::trading
