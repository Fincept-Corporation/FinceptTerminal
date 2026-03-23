#pragma once
// Generic Broker — base class with default "not implemented" for all operations
// Only used for the macro pattern. All 16 brokers now have real implementations.

#include "trading/BrokerInterface.h"

#include <QDateTime>

namespace fincept::trading {

class GenericBroker : public IBroker {
  public:
    TokenExchangeResponse exchange_token(const QString&, const QString&, const QString&) override {
        return {false, "", "", "", QString("Not implemented for %1").arg(name())};
    }
    OrderPlaceResponse place_order(const BrokerCredentials&, const UnifiedOrder&) override {
        return {false, "", QString("Not implemented for %1").arg(name())};
    }
    ApiResponse<QJsonObject> modify_order(const BrokerCredentials&, const QString&, const QJsonObject&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }
    ApiResponse<QJsonObject> cancel_order(const BrokerCredentials&, const QString&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }
    ApiResponse<QVector<BrokerOrderInfo>> get_orders(const BrokerCredentials&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }
    ApiResponse<QJsonObject> get_trade_book(const BrokerCredentials&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }
    ApiResponse<QVector<BrokerPosition>> get_positions(const BrokerCredentials&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }
    ApiResponse<QVector<BrokerHolding>> get_holdings(const BrokerCredentials&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }
    ApiResponse<BrokerFunds> get_funds(const BrokerCredentials&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }
    ApiResponse<QVector<BrokerQuote>> get_quotes(const BrokerCredentials&, const QVector<QString>&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }
    ApiResponse<QVector<BrokerCandle>> get_history(const BrokerCredentials&, const QString&, const QString&,
                                                   const QString&, const QString&) override {
        return {false, std::nullopt, QString("Not implemented for %1").arg(name()), now_ts()};
    }

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials&) const override { return {}; }
    static int64_t now_ts() { return QDateTime::currentSecsSinceEpoch(); }
};

} // namespace fincept::trading
