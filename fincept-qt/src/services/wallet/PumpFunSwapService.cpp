#include "services/wallet/PumpFunSwapService.h"

#include "core/logging/Logger.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

namespace fincept::wallet {

namespace {

constexpr const char* kTradeLocalUrl = "https://pumpportal.fun/api/trade-local";

constexpr int kSlippageMin = 1;
constexpr int kSlippageMax = 5;
constexpr double kPriorityFeeMin = 0.00001;
constexpr double kPriorityFeeMax = 0.005;

QString action_to_str(PumpFunSwapService::Action a) {
    return (a == PumpFunSwapService::Action::Buy)
               ? QStringLiteral("buy")
               : QStringLiteral("sell");
}

int clamp_slippage(int pct) {
    if (pct < kSlippageMin) return kSlippageMin;
    if (pct > kSlippageMax) return kSlippageMax;
    return pct;
}

double clamp_priority_fee(double sol) {
    if (sol < kPriorityFeeMin) return kPriorityFeeMin;
    if (sol > kPriorityFeeMax) return kPriorityFeeMax;
    return sol;
}

} // namespace

PumpFunSwapService::PumpFunSwapService(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

PumpFunSwapService::~PumpFunSwapService() = default;

void PumpFunSwapService::build_swap(Action action,
                                    const QString& mint,
                                    double amount,
                                    bool denominated_in_sol,
                                    const QString& user_pubkey,
                                    int slippage_pct,
                                    double priority_fee_sol,
                                    SwapBuildCallback cb) {
    if (mint.isEmpty()) {
        if (cb) cb(Result<SwapTransaction>::err("missing_mint"));
        return;
    }
    if (user_pubkey.isEmpty()) {
        if (cb) cb(Result<SwapTransaction>::err("missing_pubkey"));
        return;
    }
    if (amount <= 0.0) {
        if (cb) cb(Result<SwapTransaction>::err("amount_must_be_positive"));
        return;
    }

    const int slippage = clamp_slippage(slippage_pct);
    const double prio_fee = clamp_priority_fee(priority_fee_sol);

    // PumpPortal expects:
    //   publicKey, action, mint, amount,
    //   denominatedInSol ("true"/"false" — string!),
    //   slippage (integer percent),
    //   priorityFee (number, SOL),
    //   pool ("auto" — pump.fun bonding curve before graduation, PumpSwap after)
    QJsonObject body;
    body[QStringLiteral("publicKey")] = user_pubkey;
    body[QStringLiteral("action")] = action_to_str(action);
    body[QStringLiteral("mint")] = mint;
    body[QStringLiteral("amount")] = amount;
    body[QStringLiteral("denominatedInSol")] =
        denominated_in_sol ? QStringLiteral("true") : QStringLiteral("false");
    body[QStringLiteral("slippage")] = slippage;
    body[QStringLiteral("priorityFee")] = prio_fee;
    body[QStringLiteral("pool")] = QStringLiteral("auto");

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkRequest req{QUrl(QString::fromLatin1(kTradeLocalUrl))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setRawHeader(QByteArrayLiteral("Accept"),
                     QByteArrayLiteral("application/octet-stream"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = nam_->post(req, payload);
    QPointer<PumpFunSwapService> self = this;
    connect(reply, &QNetworkReply::finished, this,
            [self, reply, cb = std::move(cb)]() {
        reply->deleteLater();
        if (!self) return;

        if (reply->error() != QNetworkReply::NoError) {
            const auto err_str = reply->errorString();
            // PumpPortal's error responses are JSON in the body; surface
            // both the HTTP error and the body text so the panel can show
            // something useful.
            const auto body_bytes = reply->readAll();
            QString detail;
            if (!body_bytes.isEmpty()) {
                QJsonParseError pe;
                const auto doc = QJsonDocument::fromJson(body_bytes, &pe);
                if (pe.error == QJsonParseError::NoError && doc.isObject()) {
                    const auto root = doc.object();
                    detail = root.value(QStringLiteral("error")).toString();
                    if (detail.isEmpty()) {
                        detail = root.value(QStringLiteral("message")).toString();
                    }
                }
                if (detail.isEmpty()) {
                    detail = QString::fromUtf8(body_bytes.left(180));
                }
            }
            const QString combined =
                detail.isEmpty() ? err_str : (err_str + ": " + detail);
            LOG_WARN("PumpFunSwap", "trade-local failed: " + combined);
            if (cb) cb(Result<SwapTransaction>::err(combined.toStdString()));
            return;
        }

        const QByteArray body_bytes = reply->readAll();
        if (body_bytes.isEmpty()) {
            if (cb) cb(Result<SwapTransaction>::err("empty_response"));
            return;
        }

        // Sanity check: a Solana versioned tx always begins with a
        // signatures-count varint of 1 (0x01) followed by 64 zero bytes
        // (signature placeholder). Reject anything that doesn't look
        // remotely like a tx — protects against a server returning JSON
        // unexpectedly with a 200 status.
        if (static_cast<unsigned char>(body_bytes.at(0)) > 16
            || body_bytes.size() < 80) {
            // Likely JSON or HTML error page slipping through. Try to parse
            // it as JSON and surface the message; otherwise return the
            // first slice as-is.
            QJsonParseError pe;
            const auto doc = QJsonDocument::fromJson(body_bytes, &pe);
            if (pe.error == QJsonParseError::NoError && doc.isObject()) {
                const auto root = doc.object();
                auto msg = root.value(QStringLiteral("error")).toString();
                if (msg.isEmpty()) {
                    msg = root.value(QStringLiteral("message")).toString();
                }
                if (msg.isEmpty()) msg = QStringLiteral("unexpected_response");
                if (cb) cb(Result<SwapTransaction>::err(msg.toStdString()));
                return;
            }
            if (cb) cb(Result<SwapTransaction>::err(
                "unexpected_response: " +
                std::string(body_bytes.left(80).constData(),
                            std::min<int>(80, body_bytes.size()))));
            return;
        }

        SwapTransaction out;
        out.tx_base64 = QString::fromLatin1(body_bytes.toBase64());
        // last_valid_block_height left at 0 — wallet/RPC will reject stale txs.
        LOG_INFO("PumpFunSwap",
                 QStringLiteral("trade-local ok (%1 raw bytes, %2 b64 chars)")
                     .arg(body_bytes.size())
                     .arg(out.tx_base64.size()));
        if (cb) cb(Result<SwapTransaction>::ok(std::move(out)));
    });
}

} // namespace fincept::wallet
