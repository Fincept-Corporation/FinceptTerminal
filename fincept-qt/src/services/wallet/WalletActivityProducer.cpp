#include "services/wallet/WalletActivityProducer.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/SolanaRpcClient.h"
#include "services/wallet/TokenMetadataService.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <QVector>

#include <cmath>

namespace fincept::wallet {

// Inner named namespace avoids a unity-build collision with
// WalletBalanceProducer.cpp which also has anonymous-namespace `kTopicPrefix`.
namespace activity_producer_internal {

constexpr const char* kTopicPrefix = "wallet:activity:";
constexpr const char* kHeliusKeyName = "solana.helius_api_key";
constexpr int kFetchLimit = 50;

QString format_token_amount(double v, int max_dp = 4) {
    if (v <= 0.0) return QStringLiteral("0");
    if (v < 0.0001) return QString::number(v, 'g', 4);
    return QString::number(v, 'f', max_dp);
}

QString resolve_symbol(const QString& mint) {
    if (mint.isEmpty()) return QString();
    if (auto m = TokenMetadataService::instance().lookup(mint)) {
        return m->symbol;
    }
    if (mint.size() >= 12) {
        return mint.left(4) + QStringLiteral("…") + mint.right(4);
    }
    return mint;
}

/// Parse one Helius transaction object into a ParsedActivity row. The
/// classification logic is intentionally narrow — we only extract what
/// the ActivityTab needs to render:
///   - SWAP rows from `events.swap` (Helius's pre-parsed swap events)
///   - SEND/RECEIVE rows from `nativeTransfers` and `tokenTransfers`
///   - Everything else maps to `Other` so the row still shows up
///     (with a Solscan link) instead of silently disappearing.
ParsedActivity parse_helius_tx(const QJsonObject& tx, const QString& owner) {
    ParsedActivity out;
    out.signature = tx.value(QStringLiteral("signature")).toString();

    const auto ts_sec = static_cast<qint64>(
        tx.value(QStringLiteral("timestamp")).toDouble());
    if (ts_sec > 0) out.ts_ms = ts_sec * 1000;

    const bool failed = tx.value(QStringLiteral("transactionError")).isObject();
    out.status = failed ? QStringLiteral("failed") : QStringLiteral("confirmed");

    // Try the classified `events.swap` block first.
    const auto events = tx.value(QStringLiteral("events")).toObject();
    const auto swap = events.value(QStringLiteral("swap")).toObject();
    if (!swap.isEmpty()) {
        out.kind = ParsedActivity::Kind::Swap;
        // Helius swap events expose nativeInput/Output (SOL legs) and
        // tokenInputs/Outputs (SPL legs). One side is "in", the other "out".
        QString in_sym, out_sym;
        QString in_amt, out_amt;
        const auto native_in = swap.value(QStringLiteral("nativeInput")).toObject();
        if (!native_in.isEmpty()) {
            in_sym = QStringLiteral("SOL");
            const auto lamports = native_in.value(QStringLiteral("amount"))
                                      .toString().toLongLong();
            in_amt = format_token_amount(lamports / 1e9, 4);
        } else {
            const auto token_in = swap.value(QStringLiteral("tokenInputs")).toArray();
            if (!token_in.isEmpty()) {
                const auto first = token_in.at(0).toObject();
                in_sym = resolve_symbol(first.value(QStringLiteral("mint")).toString());
                const auto raw = first.value(QStringLiteral("rawTokenAmount")).toObject();
                bool ok = false;
                const auto qty = raw.value(QStringLiteral("tokenAmount"))
                                     .toString().toLongLong(&ok);
                const int dp = raw.value(QStringLiteral("decimals")).toInt();
                if (ok) in_amt = format_token_amount(qty / std::pow(10.0, dp), 4);
            }
        }
        const auto native_out = swap.value(QStringLiteral("nativeOutput")).toObject();
        if (!native_out.isEmpty()) {
            out_sym = QStringLiteral("SOL");
            const auto lamports = native_out.value(QStringLiteral("amount"))
                                      .toString().toLongLong();
            out_amt = format_token_amount(lamports / 1e9, 4);
        } else {
            const auto token_out = swap.value(QStringLiteral("tokenOutputs")).toArray();
            if (!token_out.isEmpty()) {
                const auto first = token_out.at(0).toObject();
                out_sym = resolve_symbol(first.value(QStringLiteral("mint")).toString());
                const auto raw = first.value(QStringLiteral("rawTokenAmount")).toObject();
                bool ok = false;
                const auto qty = raw.value(QStringLiteral("tokenAmount"))
                                     .toString().toLongLong(&ok);
                const int dp = raw.value(QStringLiteral("decimals")).toInt();
                if (ok) out_amt = format_token_amount(qty / std::pow(10.0, dp), 2);
            }
        }
        out.asset = QStringLiteral("%1 → %2").arg(in_sym, out_sym);
        out.amount_ui = QStringLiteral("%1 → %2").arg(in_amt, out_amt);
        return out;
    }

    // Token transfers — narrow to ones involving the owner.
    const auto token_transfers =
        tx.value(QStringLiteral("tokenTransfers")).toArray();
    for (const auto& v : token_transfers) {
        const auto t = v.toObject();
        const auto from = t.value(QStringLiteral("fromUserAccount")).toString();
        const auto to = t.value(QStringLiteral("toUserAccount")).toString();
        const auto mint = t.value(QStringLiteral("mint")).toString();
        const double amt = t.value(QStringLiteral("tokenAmount")).toDouble();
        const auto sym = resolve_symbol(mint);
        if (to == owner) {
            out.kind = ParsedActivity::Kind::Receive;
            out.asset = sym;
            out.amount_ui = format_token_amount(amt, 4);
            out.counterparty = from;
            return out;
        }
        if (from == owner) {
            out.kind = ParsedActivity::Kind::Send;
            out.asset = sym;
            out.amount_ui = format_token_amount(amt, 4);
            out.counterparty = to;
            return out;
        }
    }

    // Native SOL transfers.
    const auto native_transfers =
        tx.value(QStringLiteral("nativeTransfers")).toArray();
    for (const auto& v : native_transfers) {
        const auto t = v.toObject();
        const auto from = t.value(QStringLiteral("fromUserAccount")).toString();
        const auto to = t.value(QStringLiteral("toUserAccount")).toString();
        const auto lamports = static_cast<qint64>(
            t.value(QStringLiteral("amount")).toDouble());
        if (to == owner) {
            out.kind = ParsedActivity::Kind::Receive;
            out.asset = QStringLiteral("SOL");
            out.amount_ui = format_token_amount(lamports / 1e9, 4);
            out.counterparty = from;
            return out;
        }
        if (from == owner) {
            out.kind = ParsedActivity::Kind::Send;
            out.asset = QStringLiteral("SOL");
            out.amount_ui = format_token_amount(lamports / 1e9, 4);
            out.counterparty = to;
            return out;
        }
    }

    // Couldn't classify — leave as Other but keep the signature so the row
    // still renders and Solscan opens.
    return out;
}

} // namespace activity_producer_internal

namespace api = activity_producer_internal;

WalletActivityProducer::WalletActivityProducer(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
    rpc_ = new SolanaRpcClient(this);
}

WalletActivityProducer::~WalletActivityProducer() = default;

QStringList WalletActivityProducer::topic_patterns() const {
    return QStringList{QStringLiteral("wallet:activity:*")};
}

QString WalletActivityProducer::pubkey_from_topic(const QString& topic) {
    return topic.mid(static_cast<int>(qstrlen(api::kTopicPrefix)));
}

void WalletActivityProducer::refresh(const QStringList& topics) {
    auto helius_key_res = SecureStorage::instance().retrieve(
        QString::fromLatin1(api::kHeliusKeyName));
    const QString helius_key = (helius_key_res.is_ok())
                                   ? helius_key_res.value()
                                   : QString();
    for (const auto& topic : topics) {
        const auto pubkey = pubkey_from_topic(topic);
        if (pubkey.isEmpty()) {
            fincept::datahub::DataHub::instance().publish_error(
                topic, QStringLiteral("invalid topic"));
            continue;
        }
        if (!helius_key.isEmpty()) {
            refresh_via_helius(topic, pubkey, helius_key);
        } else {
            refresh_via_rpc(topic, pubkey);
        }
    }
}

void WalletActivityProducer::refresh_via_helius(const QString& topic,
                                                const QString& pubkey,
                                                const QString& helius_key) {
    QUrl url(QStringLiteral("https://api.helius.xyz/v0/addresses/%1/transactions")
                 .arg(pubkey));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("limit"), QString::number(api::kFetchLimit));
    q.addQueryItem(QStringLiteral("api-key"), helius_key);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = nam_->get(req);

    QPointer<WalletActivityProducer> self = this;
    connect(reply, &QNetworkReply::finished, this,
            [self, reply, topic, pubkey]() {
        reply->deleteLater();
        if (!self) return;
        auto& hub = fincept::datahub::DataHub::instance();
        if (reply->error() != QNetworkReply::NoError) {
            const auto err = reply->errorString();
            LOG_WARN("WalletActivity",
                     "Helius fetch failed: " + err);
            hub.publish_error(topic, err);
            return;
        }
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(reply->readAll(), &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
            hub.publish_error(topic, QStringLiteral("invalid_helius_json"));
            return;
        }
        const auto arr = doc.array();
        QVector<ParsedActivity> out;
        out.reserve(arr.size());
        for (const auto& v : arr) {
            out.append(api::parse_helius_tx(v.toObject(), pubkey));
        }
        hub.publish(topic, QVariant::fromValue(out));
    });
}

void WalletActivityProducer::refresh_via_rpc(const QString& topic,
                                             const QString& pubkey) {
    rpc_->reload_endpoint();
    QPointer<WalletActivityProducer> self = this;
    rpc_->get_signatures_for_address(
        pubkey, api::kFetchLimit, QString(),
        [self, topic](Result<std::vector<SolanaRpcClient::SignatureRow>> r) {
            if (!self) return;
            auto& hub = fincept::datahub::DataHub::instance();
            if (r.is_err()) {
                hub.publish_error(topic, QString::fromStdString(r.error()));
                return;
            }
            QVector<ParsedActivity> out;
            out.reserve(static_cast<int>(r.value().size()));
            for (const auto& row : r.value()) {
                ParsedActivity a;
                a.signature = row.signature;
                a.ts_ms = row.block_time > 0 ? row.block_time * 1000 : 0;
                a.kind = ParsedActivity::Kind::Other;
                a.status = row.err.isEmpty() ? QStringLiteral("confirmed")
                                             : QStringLiteral("failed");
                out.append(std::move(a));
            }
            hub.publish(topic, QVariant::fromValue(out));
        });
}

} // namespace fincept::wallet
