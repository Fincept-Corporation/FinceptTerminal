#pragma once
// Broker Interface — abstract base class for all broker integrations
// Qt-adapted port from broker_interface.h

#include "trading/TradingTypes.h"

#include <memory>

namespace fincept::trading {

class IBroker {
  public:
    virtual ~IBroker() = default;

    virtual BrokerId id() const = 0;
    virtual const char* name() const = 0;
    virtual const char* base_url() const = 0;

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

    // --- WebSocket streaming ---
    virtual const char* ws_adapter_name() const { return ""; }

    // --- Credential helpers ---
    BrokerCredentials load_credentials() const;
    void save_credentials(const BrokerCredentials& creds) const;

  protected:
    virtual QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const = 0;
};

using BrokerPtr = std::unique_ptr<IBroker>;

} // namespace fincept::trading
