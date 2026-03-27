#pragma once
// Fyers Broker — Indian stock broker
// API: https://api-t1.fyers.in
// Auth: SHA256(api_key:api_secret) for token exchange

#include "trading/BrokerInterface.h"

namespace fincept::trading {

class FyersBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Fyers; }
    const char* name() const override { return "Fyers"; }
    const char* base_url() const override { return "https://api-t1.fyers.in"; }
    const char* ws_adapter_name() const override { return "fyers"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "fyers",
            .display_name = "Fyers",
            .region = "IN",
            .currency = "INR",
            .credential_fields = {
                {CredentialField::ApiKey,    "CLIENT ID",  "Enter Client ID...",   false},
                {CredentialField::ApiSecret, "API SECRET", "Enter API Secret...",  true},
                {CredentialField::AuthCode,  "AUTH CODE",  "Paste auth code from Fyers login...", false},
            },
            .exchanges = {"NSE", "BSE", "NFO", "MCX", "CDS"},
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
            .brokerage_info = "\u20B920/order or 0.03% (whichever lower)",
        };
    }

    TokenExchangeResponse exchange_token(const QString& api_key, const QString& api_secret,
                                         const QString& auth_code) override;
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

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    static int fyers_order_type(OrderType t);
    static int fyers_side(OrderSide s);
    static QString fyers_product(ProductType p);
};

} // namespace fincept::trading
