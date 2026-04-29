#include "services/wallet/SolanaRpcClient.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUuid>

#include <cmath>

namespace fincept::wallet {

namespace {

constexpr const char* kPublicMainnetRpc = "https://api.mainnet-beta.solana.com";
constexpr const char* kHeliusMainnetTpl = "https://mainnet.helius-rpc.com/?api-key=%1";

constexpr const char* kHeliusKey = "solana.helius_api_key";
constexpr const char* kRpcOverrideKey = "solana.rpc_url";

QString stable_request_id() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

double SplTokenBalance::ui_amount() const noexcept {
    if (amount_raw.isEmpty()) {
        return 0.0;
    }
    bool ok = false;
    const auto raw = amount_raw.toLongLong(&ok);
    if (!ok) {
        return 0.0;
    }
    return static_cast<double>(raw) / std::pow(10.0, decimals);
}

SolanaRpcClient::SolanaRpcClient(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
    endpoint_ = resolve_endpoint();
    LOG_INFO("SolanaRpc",
             QStringLiteral("Endpoint configured (%1)")
                 .arg(endpoint_.startsWith(QStringLiteral("https://api.mainnet-beta"))
                          ? QStringLiteral("public")
                          : QStringLiteral("private")));
}

SolanaRpcClient::~SolanaRpcClient() = default;

void SolanaRpcClient::reload_endpoint() {
    endpoint_ = resolve_endpoint();
}

QString SolanaRpcClient::ws_endpoint() const {
    QString ws = endpoint_;
    if (ws.startsWith(QStringLiteral("https://"))) {
        ws.replace(0, 8, QStringLiteral("wss://"));
    } else if (ws.startsWith(QStringLiteral("http://"))) {
        ws.replace(0, 7, QStringLiteral("ws://"));
    }
    return ws;
}

QString SolanaRpcClient::resolve_endpoint() const {
    auto override_res = SecureStorage::instance().retrieve(QString::fromLatin1(kRpcOverrideKey));
    if (override_res.is_ok() && !override_res.value().isEmpty()) {
        return override_res.value();
    }
    auto helius_res = SecureStorage::instance().retrieve(QString::fromLatin1(kHeliusKey));
    if (helius_res.is_ok() && !helius_res.value().isEmpty()) {
        return QString::fromLatin1(kHeliusMainnetTpl).arg(helius_res.value());
    }
    return QString::fromLatin1(kPublicMainnetRpc);
}

void SolanaRpcClient::post_rpc(const QString& method,
                               const QJsonObject& params_obj,
                               std::function<void(Result<QJsonObject>)> callback) {
    QJsonObject envelope;
    envelope[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    envelope[QStringLiteral("id")] = stable_request_id();
    envelope[QStringLiteral("method")] = method;
    if (params_obj.contains(QStringLiteral("__array"))) {
        // Allow callers to request positional params by passing { "__array": [ ... ] }.
        envelope[QStringLiteral("params")] = params_obj.value(QStringLiteral("__array")).toArray();
    } else {
        envelope[QStringLiteral("params")] = params_obj;
    }

    QNetworkRequest req{QUrl(endpoint_)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    const QByteArray body = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    auto* reply = nam_->post(req, body);

    QPointer<SolanaRpcClient> self = this;
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback), self, method]() {
        reply->deleteLater();
        if (!self) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            LOG_WARN("SolanaRpc", method + " failed: " + err);
            callback(Result<QJsonObject>::err(err.toStdString()));
            return;
        }
        const QByteArray payload = reply->readAll();
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(payload, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            callback(Result<QJsonObject>::err("invalid JSON"));
            return;
        }
        const auto root = doc.object();
        if (root.contains(QStringLiteral("error"))) {
            const auto err_obj = root.value(QStringLiteral("error")).toObject();
            const auto err_msg = err_obj.value(QStringLiteral("message")).toString(
                QStringLiteral("rpc error"));
            callback(Result<QJsonObject>::err(err_msg.toStdString()));
            return;
        }
        callback(Result<QJsonObject>::ok(root));
    });
}

void SolanaRpcClient::get_sol_balance(const QString& pubkey_b58, BalanceCallback callback) {
    QJsonObject params;
    params[QStringLiteral("__array")] = QJsonArray{pubkey_b58};
    post_rpc(QStringLiteral("getBalance"), params,
             [cb = std::move(callback)](Result<QJsonObject> r) {
                 if (r.is_err()) {
                     cb(Result<quint64>::err(r.error()));
                     return;
                 }
                 const auto result = r.value().value(QStringLiteral("result")).toObject();
                 const auto value = result.value(QStringLiteral("value"));
                 if (!value.isDouble()) {
                     cb(Result<quint64>::err("missing value"));
                     return;
                 }
                 cb(Result<quint64>::ok(static_cast<quint64>(value.toDouble())));
             });
}

namespace {
// Standard SPL Token Program. Token-2022 is a separate program (TokenzQ…).
// We only fetch from the classic program here; token-2022 holdings are rare
// for retail wallets and can be added in a follow-up if it matters.
constexpr const char* kSplTokenProgramId =
    "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA";
} // namespace

void SolanaRpcClient::get_token_balance(const QString& owner_b58,
                                        const QString& mint_b58,
                                        TokenBalancesCallback callback) {
    QJsonObject mint_filter;
    mint_filter[QStringLiteral("mint")] = mint_b58;
    QJsonObject opts;
    opts[QStringLiteral("encoding")] = QStringLiteral("jsonParsed");

    QJsonArray params_array{owner_b58, mint_filter, opts};
    QJsonObject params;
    params[QStringLiteral("__array")] = params_array;

    post_rpc(QStringLiteral("getTokenAccountsByOwner"), params,
             [cb = std::move(callback)](Result<QJsonObject> r) {
                 if (r.is_err()) {
                     cb(Result<std::vector<SplTokenBalance>>::err(r.error()));
                     return;
                 }
                 std::vector<SplTokenBalance> out;
                 const auto result = r.value().value(QStringLiteral("result")).toObject();
                 const auto accounts = result.value(QStringLiteral("value")).toArray();
                 for (const auto& entry : accounts) {
                     const auto acct = entry.toObject()
                                           .value(QStringLiteral("account"))
                                           .toObject();
                     const auto data = acct.value(QStringLiteral("data")).toObject();
                     const auto parsed = data.value(QStringLiteral("parsed")).toObject();
                     const auto info = parsed.value(QStringLiteral("info")).toObject();
                     const auto token_amount =
                         info.value(QStringLiteral("tokenAmount")).toObject();
                     SplTokenBalance bal;
                     bal.mint = info.value(QStringLiteral("mint")).toString();
                     bal.amount_raw = token_amount.value(QStringLiteral("amount")).toString();
                     bal.decimals = token_amount.value(QStringLiteral("decimals")).toInt();
                     out.push_back(std::move(bal));
                 }
                 cb(Result<std::vector<SplTokenBalance>>::ok(std::move(out)));
             });
}

void SolanaRpcClient::get_all_token_balances(const QString& owner_b58,
                                             TokenBalancesCallback callback) {
    // Same response shape as `get_token_balance`, just no mint filter:
    // we filter by programId instead, returning every SPL token account.
    QJsonObject program_filter;
    program_filter[QStringLiteral("programId")] = QString::fromLatin1(kSplTokenProgramId);
    QJsonObject opts;
    opts[QStringLiteral("encoding")] = QStringLiteral("jsonParsed");

    QJsonObject params;
    params[QStringLiteral("__array")] = QJsonArray{owner_b58, program_filter, opts};

    post_rpc(QStringLiteral("getTokenAccountsByOwner"), params,
             [cb = std::move(callback)](Result<QJsonObject> r) {
                 if (r.is_err()) {
                     cb(Result<std::vector<SplTokenBalance>>::err(r.error()));
                     return;
                 }
                 std::vector<SplTokenBalance> out;
                 const auto result = r.value().value(QStringLiteral("result")).toObject();
                 const auto accounts = result.value(QStringLiteral("value")).toArray();
                 out.reserve(accounts.size());
                 for (const auto& entry : accounts) {
                     const auto acct = entry.toObject()
                                           .value(QStringLiteral("account"))
                                           .toObject();
                     const auto data = acct.value(QStringLiteral("data")).toObject();
                     const auto parsed = data.value(QStringLiteral("parsed")).toObject();
                     const auto info = parsed.value(QStringLiteral("info")).toObject();
                     const auto token_amount =
                         info.value(QStringLiteral("tokenAmount")).toObject();
                     SplTokenBalance bal;
                     bal.mint = info.value(QStringLiteral("mint")).toString();
                     bal.amount_raw = token_amount.value(QStringLiteral("amount")).toString();
                     bal.decimals = token_amount.value(QStringLiteral("decimals")).toInt();
                     // Skip zero-amount accounts to keep the list tight; the
                     // wallet may have orphan accounts from past holdings.
                     if (bal.amount_raw.isEmpty() || bal.amount_raw == QStringLiteral("0")) {
                         continue;
                     }
                     out.push_back(std::move(bal));
                 }
                 cb(Result<std::vector<SplTokenBalance>>::ok(std::move(out)));
             });
}

// ── Phase 2 RPC additions ──────────────────────────────────────────────────

void SolanaRpcClient::get_latest_blockhash(LatestBlockhashCallback callback) {
    QJsonObject opts;
    opts[QStringLiteral("commitment")] = QStringLiteral("confirmed");
    QJsonObject params;
    params[QStringLiteral("__array")] = QJsonArray{opts};

    post_rpc(QStringLiteral("getLatestBlockhash"), params,
             [cb = std::move(callback)](Result<QJsonObject> r) {
                 if (r.is_err()) {
                     cb(Result<LatestBlockhash>::err(r.error()));
                     return;
                 }
                 const auto result = r.value().value(QStringLiteral("result")).toObject();
                 const auto value = result.value(QStringLiteral("value")).toObject();
                 const auto bh = value.value(QStringLiteral("blockhash")).toString();
                 if (bh.isEmpty()) {
                     cb(Result<LatestBlockhash>::err("missing blockhash"));
                     return;
                 }
                 LatestBlockhash out;
                 out.blockhash = bh;
                 out.last_valid_block_height = static_cast<quint64>(
                     value.value(QStringLiteral("lastValidBlockHeight")).toDouble());
                 cb(Result<LatestBlockhash>::ok(std::move(out)));
             });
}

void SolanaRpcClient::send_transaction(const QString& tx_base64,
                                       std::function<void(Result<QString>)> callback) {
    // Solana's sendTransaction takes the encoded tx as a positional arg, with
    // an options object as the second positional arg (encoding="base64",
    // skipPreflight=false, preflightCommitment="confirmed").
    QJsonObject opts;
    opts[QStringLiteral("encoding")] = QStringLiteral("base64");
    opts[QStringLiteral("skipPreflight")] = false;
    opts[QStringLiteral("preflightCommitment")] = QStringLiteral("confirmed");
    opts[QStringLiteral("maxRetries")] = 3;

    QJsonObject params;
    params[QStringLiteral("__array")] = QJsonArray{tx_base64, opts};

    post_rpc(QStringLiteral("sendTransaction"), params,
             [cb = std::move(callback)](Result<QJsonObject> r) {
                 if (r.is_err()) {
                     cb(Result<QString>::err(r.error()));
                     return;
                 }
                 const auto result = r.value().value(QStringLiteral("result"));
                 if (!result.isString()) {
                     cb(Result<QString>::err("missing signature"));
                     return;
                 }
                 cb(Result<QString>::ok(result.toString()));
             });
}

void SolanaRpcClient::get_signature_statuses(const QStringList& signatures,
                                             SignatureStatusesCallback callback) {
    if (signatures.isEmpty()) {
        callback(Result<std::vector<SignatureStatus>>::ok({}));
        return;
    }
    QJsonArray sigs_arr;
    for (const auto& s : signatures) {
        sigs_arr.append(s);
    }
    QJsonObject opts;
    opts[QStringLiteral("searchTransactionHistory")] = true;

    QJsonObject params;
    params[QStringLiteral("__array")] = QJsonArray{sigs_arr, opts};

    // Capture the input order so we can align result rows even if some are null.
    auto input_sigs = std::make_shared<QStringList>(signatures);
    post_rpc(QStringLiteral("getSignatureStatuses"), params,
             [cb = std::move(callback), input_sigs](Result<QJsonObject> r) {
                 if (r.is_err()) {
                     cb(Result<std::vector<SignatureStatus>>::err(r.error()));
                     return;
                 }
                 const auto result = r.value().value(QStringLiteral("result")).toObject();
                 const auto value = result.value(QStringLiteral("value")).toArray();
                 std::vector<SignatureStatus> out;
                 out.reserve(static_cast<std::size_t>(input_sigs->size()));
                 for (int i = 0; i < input_sigs->size(); ++i) {
                     SignatureStatus s;
                     s.signature = input_sigs->at(i);
                     if (i >= value.size() || value.at(i).isNull()) {
                         s.found = false;
                         out.push_back(std::move(s));
                         continue;
                     }
                     const auto entry = value.at(i).toObject();
                     s.found = true;
                     s.slot = static_cast<quint64>(
                         entry.value(QStringLiteral("slot")).toDouble());
                     s.confirmation_status =
                         entry.value(QStringLiteral("confirmationStatus")).toString();
                     const auto err_v = entry.value(QStringLiteral("err"));
                     if (!err_v.isNull()) {
                         // err may be an object or a string; stringify either way.
                         s.err = QString::fromUtf8(
                             QJsonDocument::fromVariant(err_v.toVariant())
                                 .toJson(QJsonDocument::Compact));
                     }
                     out.push_back(std::move(s));
                 }
                 cb(Result<std::vector<SignatureStatus>>::ok(std::move(out)));
             });
}

void SolanaRpcClient::get_signatures_for_address(const QString& address_b58,
                                                 int limit,
                                                 const QString& before_signature,
                                                 SignaturesForAddressCallback callback) {
    if (limit < 1) limit = 1;
    if (limit > 1000) limit = 1000;
    QJsonObject opts;
    opts[QStringLiteral("limit")] = limit;
    if (!before_signature.isEmpty()) {
        opts[QStringLiteral("before")] = before_signature;
    }
    opts[QStringLiteral("commitment")] = QStringLiteral("confirmed");

    QJsonObject params;
    params[QStringLiteral("__array")] = QJsonArray{address_b58, opts};

    post_rpc(QStringLiteral("getSignaturesForAddress"), params,
             [cb = std::move(callback)](Result<QJsonObject> r) {
                 if (r.is_err()) {
                     cb(Result<std::vector<SignatureRow>>::err(r.error()));
                     return;
                 }
                 const auto rows = r.value().value(QStringLiteral("result")).toArray();
                 std::vector<SignatureRow> out;
                 out.reserve(static_cast<std::size_t>(rows.size()));
                 for (const auto& v : rows) {
                     const auto e = v.toObject();
                     SignatureRow row;
                     row.signature = e.value(QStringLiteral("signature")).toString();
                     row.slot = static_cast<quint64>(
                         e.value(QStringLiteral("slot")).toDouble());
                     const auto bt = e.value(QStringLiteral("blockTime"));
                     if (bt.isDouble()) {
                         row.block_time = static_cast<qint64>(bt.toDouble());
                     }
                     const auto err_v = e.value(QStringLiteral("err"));
                     if (!err_v.isNull() && !err_v.isUndefined()) {
                         row.err = QString::fromUtf8(
                             QJsonDocument::fromVariant(err_v.toVariant())
                                 .toJson(QJsonDocument::Compact));
                     }
                     row.memo = e.value(QStringLiteral("memo")).toString();
                     out.push_back(std::move(row));
                 }
                 cb(Result<std::vector<SignatureRow>>::ok(std::move(out)));
             });
}

} // namespace fincept::wallet
