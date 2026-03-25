#pragma once
#include "trading/BrokerInterface.h"

namespace fincept::trading {

class AngelOneBroker : public IBroker {
  public:
    BrokerId id() const override { return BrokerId::AngelOne; }
    const char* name() const override { return "AngelOne"; }
    const char* base_url() const override { return "https://apiconnect.angelone.in"; }
    const char* ws_adapter_name() const override { return "angelone"; }

    BrokerProfile profile() const override {
        return BrokerProfile{
            .id = "angelone", .display_name = "Angel One", .region = "IN", .currency = "INR",
            .credential_fields = {
                // Angel One needs 4 separate values for login
                {CredentialField::ClientCode, "CLIENT CODE",  "Enter login/client ID...",    false},
                {CredentialField::ApiKey,     "API KEY",      "Enter API key...",            false},
                {CredentialField::ApiSecret,  "MPIN",         "Enter MPIN/PIN...",           true},
                // base32 TOTP secret — 6-digit code auto-generated internally at login time
                {CredentialField::AuthCode,   "TOTP SECRET",  "Enter base32 TOTP secret...", false},
            },
            .exchanges = {"NSE","BSE","NFO","MCX","NCDEX"},
            .product_types = {
                {"Intraday (MIS)", ProductType::Intraday},
                {"Delivery (CNC)",ProductType::Delivery},
                {"Margin (NRML)", ProductType::Margin},
            },
            .supports_intraday=true, .supports_bracket_order=false, .supports_cover_order=false,
            .has_native_paper=false, .default_paper_balance=1000000.0,
            .default_watchlist={"HDFCBANK","ICICIBANK","SBIN","TCS","INFY","RELIANCE","TATASTEEL","BHARTIARTL","ITC","LT"},
            .default_symbol="RELIANCE", .default_exchange="NSE",
            .brokerage_info="\u20B920/order flat",
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

    // additional_data JSON keys stored in BrokerCredentials:
    //   feed_token    — required for WebSocket connection
    //   refresh_token — used to silently renew expired JWT
    //   client_code   — Angel One client/user ID

  protected:
    QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const override;

  private:
    // Generate a 6-digit TOTP from a base32 secret (RFC 6238 / SHA-1 / 30s window)
    static QString generate_totp(const QString& base32_secret);
    // Look up AngelOne symbol token from master contract (needed for quotes/history)
    static QString lookup_token(const QString& symbol, const QString& exchange);
    // Map UnifiedOrder fields to AngelOne API strings
    static QString ao_order_type(OrderType t);
    static QString ao_variety(OrderType t, bool amo);
    static QString ao_product(ProductType p);
    static QString ao_transaction(OrderSide s);
};

} // namespace fincept::trading
