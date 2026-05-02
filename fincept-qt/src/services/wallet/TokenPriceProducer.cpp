#include "services/wallet/TokenPriceProducer.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVariant>

namespace fincept::wallet {

namespace {

constexpr const char* kTokenPriceFamilyPrefix = "market:price:token:";
constexpr const char* kFncptAlias   = "market:price:fncpt";
// Jupiter Lite Price API v3. Same host we already use; the response shape
// keys results by mint, so a single call serves any number of mints.
constexpr const char* kJupiterPriceBase = "https://lite-api.jup.ag/price/v3?ids=";

} // namespace

TokenPriceProducer::TokenPriceProducer(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

TokenPriceProducer::~TokenPriceProducer() = default;

QStringList TokenPriceProducer::topic_patterns() const {
    return QStringList{
        QStringLiteral("market:price:token:*"),
        QStringLiteral("market:price:fncpt"),  // back-compat alias
    };
}

QString TokenPriceProducer::mint_for_topic(const QString& topic) {
    if (topic == QString::fromLatin1(kFncptAlias)) {
        return QString::fromLatin1(kFncptMint);
    }
    if (topic.startsWith(QString::fromLatin1(kTokenPriceFamilyPrefix))) {
        return topic.mid(static_cast<int>(qstrlen(kTokenPriceFamilyPrefix)));
    }
    return {};
}

QString TokenPriceProducer::topic_for_mint(const QString& mint) {
    return QString::fromLatin1(kTokenPriceFamilyPrefix) + mint;
}

void TokenPriceProducer::refresh(const QStringList& topics) {
    // Collect the unique mints we need to fetch in this cycle, plus a
    // mint→topic-list map so the publish leg knows where to fan out.
    QSet<QString> mints;
    QHash<QString, QStringList> topic_by_mint;
    for (const auto& t : topics) {
        const auto m = mint_for_topic(t);
        if (m.isEmpty()) continue;
        mints.insert(m);
        topic_by_mint[m].append(t);
    }
    if (mints.isEmpty()) return;

    // Build `?ids=<m1>,<m2>,…`. Jupiter handles arbitrary lengths up to
    // ~60 mints in one call; nothing in this codebase ever asks for that
    // many at once.
    QStringList ids(mints.constBegin(), mints.constEnd());
    const QString url = QString::fromLatin1(kJupiterPriceBase) + ids.join(QLatin1Char(','));

    QNetworkRequest req{QUrl(url)};
    req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = nam_->get(req);

    QPointer<TokenPriceProducer> self = this;
    connect(reply, &QNetworkReply::finished, this,
            [self, reply, topic_by_mint, ids]() {
        reply->deleteLater();
        if (!self) return;
        auto& hub = fincept::datahub::DataHub::instance();
        if (reply->error() != QNetworkReply::NoError) {
            const auto err = reply->errorString();
            LOG_WARN("TokenPrice", "Jupiter fetch failed: " + err);
            for (auto it = topic_by_mint.constBegin();
                 it != topic_by_mint.constEnd(); ++it) {
                for (const auto& t : it.value()) hub.publish_error(t, err);
            }
            return;
        }
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(reply->readAll(), &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARN("TokenPrice", "invalid JSON from Jupiter");
            return;
        }
        const auto root = doc.object();

        // Find the SOL entry for the SOL/FNCPT cross-rate that legacy
        // FncptPrice consumers expect on the `sol` field.
        const auto sol_obj =
            root.value(QString::fromLatin1(kWrappedSolMint)).toObject();
        const double sol_usd = sol_obj.value(QStringLiteral("usdPrice")).toDouble();

        for (const auto& mint : ids) {
            const auto entry = root.value(mint).toObject();
            TokenPrice tp;
            tp.mint = mint;
            tp.usd = entry.value(QStringLiteral("usdPrice")).toDouble();
            tp.sol = (sol_usd > 0.0 && tp.usd > 0.0) ? tp.usd / sol_usd : 0.0;
            tp.ts_ms = QDateTime::currentMSecsSinceEpoch();
            tp.valid = tp.usd > 0.0;
            const auto v = QVariant::fromValue(tp);
            for (const auto& t : topic_by_mint.value(mint)) {
                hub.publish(t, v);
            }
        }
    });
}

} // namespace fincept::wallet
