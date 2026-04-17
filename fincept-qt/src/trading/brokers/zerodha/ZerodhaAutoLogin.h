#pragma once
// 5-step Zerodha TOTP auto-login chain:
//   1. POST /api/login (user_id, password)                  -> request_id
//   2. POST /api/twofa (user_id, request_id, twofa_value)   -> enctoken cookie
//   3. GET  /connect/finish?api_key=...                     -> redirect URL carrying request_token
//   4. Exchange request_token via SHA-256 checksum           -> access_token
//
// Owns its own QNetworkAccessManager + QNetworkCookieJar so the enctoken
// cookie persists between steps. Uses QEventLoop internally - MUST be called
// from a worker thread, never the UI thread.

#include "trading/TradingTypes.h"
#include <QString>
#include <functional>

namespace fincept::trading::zerodha {

struct AutoLoginResult {
    TokenExchangeResponse token;   // success + access_token + user_id, or error
    QString detailed_error;        // e.g., "TwoFAException: Invalid TOTP"
};

using ProgressCallback = std::function<void(const QString& stage)>;

AutoLoginResult run_auto_login(const QString& user_id,
                               const QString& password,
                               const QString& api_key,
                               const QString& api_secret,
                               const QString& totp_secret,
                               const ProgressCallback& progress = {});

} // namespace fincept::trading::zerodha
