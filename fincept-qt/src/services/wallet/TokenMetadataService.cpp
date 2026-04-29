#include "services/wallet/TokenMetadataService.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QReadLocker>
#include <QUrl>
#include <QWriteLocker>

namespace fincept::wallet {

namespace {

constexpr const char* kStorageKey = "token_metadata.cache_v1";
constexpr const char* kJupiterTokensUrl =
    "https://api.jup.ag/tokens/v2/tagged/verified";
constexpr qint64 kStaleAfterMs = 24LL * 60 * 60 * 1000;  // 24h

} // namespace

TokenMetadataService& TokenMetadataService::instance() {
    static TokenMetadataService s;
    return s;
}

TokenMetadataService::TokenMetadataService(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

TokenMetadataService::~TokenMetadataService() = default;

bool TokenMetadataService::cache_is_stale() const {
    QReadLocker rl(&lock_);
    if (last_refreshed_ms_ == 0) return true;
    return (QDateTime::currentMSecsSinceEpoch() - last_refreshed_ms_) > kStaleAfterMs;
}

std::optional<TokenMetadata> TokenMetadataService::lookup(const QString& mint) const {
    QReadLocker rl(&lock_);
    auto it = cache_.constFind(mint);
    if (it == cache_.constEnd()) return std::nullopt;
    return it.value();
}

bool TokenMetadataService::is_verified(const QString& mint) const {
    QReadLocker rl(&lock_);
    auto it = cache_.constFind(mint);
    return it != cache_.constEnd() && it.value().verified;
}

int TokenMetadataService::size() const {
    QReadLocker rl(&lock_);
    return cache_.size();
}

void TokenMetadataService::load_from_storage() {
    auto r = SecureStorage::instance().retrieve(QString::fromLatin1(kStorageKey));
    if (r.is_err() || r.value().isEmpty()) {
        LOG_INFO("TokenMetadata", "no cached token list — will fetch from Jupiter");
        return;
    }
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(r.value().toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN("TokenMetadata", "cached token list is malformed; ignoring");
        return;
    }
    const auto root = doc.object();
    const auto entries = root.value(QStringLiteral("entries")).toArray();
    QHash<QString, TokenMetadata> next;
    next.reserve(entries.size());
    for (const auto& v : entries) {
        const auto obj = v.toObject();
        TokenMetadata m;
        m.mint = obj.value(QStringLiteral("mint")).toString();
        m.symbol = obj.value(QStringLiteral("symbol")).toString();
        m.name = obj.value(QStringLiteral("name")).toString();
        m.decimals = obj.value(QStringLiteral("decimals")).toInt();
        m.icon_url = obj.value(QStringLiteral("icon_url")).toString();
        m.verified = obj.value(QStringLiteral("verified")).toBool();
        if (!m.mint.isEmpty()) next.insert(m.mint, std::move(m));
    }
    {
        QWriteLocker wl(&lock_);
        cache_ = std::move(next);
        last_refreshed_ms_ = static_cast<qint64>(
            root.value(QStringLiteral("ts_ms")).toDouble());
    }
    LOG_INFO("TokenMetadata",
             QStringLiteral("loaded %1 tokens from cache (age %2 h)")
                 .arg(size())
                 .arg((QDateTime::currentMSecsSinceEpoch() - last_refreshed_ms_) / 3600000));
}

void TokenMetadataService::persist_to_storage() {
    QJsonArray entries;
    {
        QReadLocker rl(&lock_);
        for (auto it = cache_.constBegin(); it != cache_.constEnd(); ++it) {
            const auto& m = it.value();
            QJsonObject obj;
            obj[QStringLiteral("mint")] = m.mint;
            obj[QStringLiteral("symbol")] = m.symbol;
            obj[QStringLiteral("name")] = m.name;
            obj[QStringLiteral("decimals")] = m.decimals;
            obj[QStringLiteral("icon_url")] = m.icon_url;
            obj[QStringLiteral("verified")] = m.verified;
            entries.append(obj);
        }
    }
    QJsonObject root;
    root[QStringLiteral("ts_ms")] = static_cast<double>(last_refreshed_ms_);
    root[QStringLiteral("entries")] = entries;
    const auto blob = QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
    auto r = SecureStorage::instance().store(QString::fromLatin1(kStorageKey), blob);
    if (r.is_err()) {
        LOG_WARN("TokenMetadata",
                 "failed to persist token cache: " + QString::fromStdString(r.error()));
    } else {
        LOG_INFO("TokenMetadata",
                 QStringLiteral("persisted %1 tokens (%2 KB)")
                     .arg(entries.size())
                     .arg(blob.size() / 1024));
    }
}

void TokenMetadataService::refresh_from_jupiter_async() {
    if (refresh_in_flight_) return;
    if (!cache_is_stale()) {
        LOG_DEBUG("TokenMetadata", "cache fresh; skipping Jupiter refresh");
        return;
    }
    refresh_in_flight_ = true;

    QNetworkRequest req{QUrl(QString::fromLatin1(kJupiterTokensUrl))};
    req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = nam_->get(req);

    QPointer<TokenMetadataService> self = this;
    connect(reply, &QNetworkReply::finished, this, [self, reply]() {
        reply->deleteLater();
        if (!self) return;
        self->refresh_in_flight_ = false;
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("TokenMetadata",
                     "Jupiter fetch failed: " + reply->errorString());
            return;
        }
        const auto payload = reply->readAll();
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(payload, &pe);
        if (pe.error != QJsonParseError::NoError) {
            LOG_WARN("TokenMetadata", "Jupiter response is malformed JSON");
            return;
        }
        // Endpoint returns a top-level array of token objects.
        const auto arr = doc.isArray() ? doc.array()
                                       : doc.object()
                                             .value(QStringLiteral("data")).toArray();
        if (arr.isEmpty()) {
            LOG_WARN("TokenMetadata", "Jupiter returned 0 tokens; keeping old cache");
            return;
        }

        QHash<QString, TokenMetadata> next;
        next.reserve(arr.size());
        for (const auto& v : arr) {
            const auto obj = v.toObject();
            TokenMetadata m;
            // Jupiter v2 uses `id` for the mint; v1 used `address`.
            m.mint = obj.value(QStringLiteral("id")).toString();
            if (m.mint.isEmpty()) {
                m.mint = obj.value(QStringLiteral("address")).toString();
            }
            if (m.mint.isEmpty()) continue;
            m.symbol = obj.value(QStringLiteral("symbol")).toString();
            m.name = obj.value(QStringLiteral("name")).toString();
            m.decimals = obj.value(QStringLiteral("decimals")).toInt();
            m.icon_url = obj.value(QStringLiteral("icon")).toString();
            if (m.icon_url.isEmpty()) {
                m.icon_url = obj.value(QStringLiteral("logoURI")).toString();
            }
            m.verified = true; // we explicitly fetched the verified-tagged set
            next.insert(m.mint, std::move(m));
        }

        {
            QWriteLocker wl(&self->lock_);
            self->cache_ = std::move(next);
            self->last_refreshed_ms_ = QDateTime::currentMSecsSinceEpoch();
        }
        self->persist_to_storage();
        LOG_INFO("TokenMetadata",
                 QStringLiteral("refreshed: %1 verified tokens").arg(self->size()));
        emit self->metadata_changed();
    });
}

} // namespace fincept::wallet
