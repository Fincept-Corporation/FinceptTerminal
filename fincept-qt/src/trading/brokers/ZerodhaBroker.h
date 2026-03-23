#pragma once
#include "trading/BrokerInterface.h"

namespace fincept::trading {

class ZerodhaBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::Zerodha; }
    const char* name() const override { return "Zerodha"; }
    const char* base_url() const override { return "https://api.kite.trade"; }
    const char* ws_adapter_name() const override { return "zerodha"; }

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
};

} // namespace fincept::trading
