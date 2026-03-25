#pragma once
#include "trading/BrokerInterface.h"

namespace fincept::trading {

class IBKRBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::IBKR; }
    const char* name() const override { return "IBKR"; }
    const char* base_url() const override { return "https://localhost:5000/v1"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "ibkr", .display_name = "IBKR", .region = "US", .currency = "USD",
            .credential_fields = {
                {CredentialField::ApiKey,    "CLIENT ID",  "Enter Client ID (Gateway port)...", false},
                {CredentialField::ApiSecret, "GATEWAY URL","localhost:5000 (or custom)...",     false},
            },
            .exchanges = {"NYSE","NASDAQ","AMEX","CBOE"},
            .product_types = {
                {"Day Order",  ProductType::Intraday},
                {"GTC Order",  ProductType::Delivery},
                {"Margin",     ProductType::Margin},
            },
            .supports_intraday=true, .supports_bracket_order=true, .supports_cover_order=false,
            .has_native_paper=true, .default_paper_balance=100000.0,
            .default_watchlist={"AAPL","MSFT","GOOGL","AMZN","NVDA","META","TSLA","JPM","V","JNJ"},
            .default_symbol="AAPL", .default_exchange="NASDAQ",
            .brokerage_info="$0.005/share (min $1)",
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
};

} // namespace fincept::trading
