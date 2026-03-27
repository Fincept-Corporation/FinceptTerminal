#pragma once
#include "trading/BrokerInterface.h"

namespace fincept::trading {

class ZerodhaBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Zerodha; }
    const char* name() const override { return "Zerodha"; }
    const char* base_url() const override { return "https://api.kite.trade"; }
    const char* ws_adapter_name() const override { return "zerodha"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "zerodha",
            .display_name = "Zerodha",
            .region = "IN",
            .currency = "INR",
            .credential_fields = {
                {CredentialField::ApiKey,    "API KEY",      "Enter API Key...",       false},
                {CredentialField::ApiSecret, "API SECRET",   "Enter API Secret...",    true},
                {CredentialField::AuthCode,  "REQUEST TOKEN","Paste request_token...", false},
            },
            .exchanges = {"NSE", "BSE", "NFO", "CDS"},
            .product_types = {
                {"Intraday (MIS)",  ProductType::Intraday},
                {"Delivery (CNC)", ProductType::Delivery},
                {"Margin (NRML)",  ProductType::Margin},
                {"Cover Order",    ProductType::CoverOrder},
                {"Bracket Order",  ProductType::BracketOrder},
            },
            .supports_intraday = true,
            .supports_bracket_order = true,
            .supports_cover_order = true,
            .has_native_paper = false,
            .default_paper_balance = 1000000.0,
            .default_watchlist = {"HDFCBANK","ICICIBANK","SBIN","KOTAKBANK","AXISBANK",
                                  "TCS","INFY","RELIANCE","TATAMOTORS","BAJFINANCE"},
            .default_symbol = "RELIANCE",
            .default_exchange = "NSE",
            .brokerage_info = "\u20B920/order or 0.03% intraday, 0% delivery",
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

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static constexpr const char* kite_api_version = "3";
    static const char* zerodha_order_type(OrderType t);
    static const char* zerodha_product(ProductType p);
    static const char* zerodha_side(OrderSide s);
    static const char* zerodha_variety(ProductType p);
    static QString zerodha_interval(const QString& resolution);
};

} // namespace fincept::trading
