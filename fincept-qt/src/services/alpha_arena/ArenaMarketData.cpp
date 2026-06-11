#include "services/alpha_arena/ArenaMarketData.h"
#include "core/logging/Logger.h"
#include "services/alpha_arena/ArenaIndicators.h"
#include "trading/exchanges/hyperliquid/HyperliquidClient.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

namespace fincept::arena {
using fincept::trading::hyperliquid::HyperliquidClient;

ArenaMarketData::ArenaMarketData(QObject* parent) : IArenaMarketData(parent) {
    client_ = new HyperliquidClient(this);
}

QHash<QString, double> ArenaMarketData::parse_all_mids(const QJsonDocument& doc, const QStringList& coins) {
    QHash<QString, double> out;
    const QJsonObject o = doc.object();
    for (const QString& c : coins)
        if (o.contains(c)) out[c] = o.value(c).toString().toDouble();
    return out;
}

void ArenaMarketData::apply_asset_ctxs(const QJsonDocument& doc, MarketSnapshot& snap) {
    const QJsonArray top = doc.array();
    if (top.size() < 2) return;
    const QJsonArray universe = top[0].toObject().value("universe").toArray();
    const QJsonArray ctxs = top[1].toArray();
    for (int i = 0; i < universe.size() && i < ctxs.size(); ++i) {
        const QString name = universe[i].toObject().value("name").toString();
        if (!snap.coins.contains(name)) continue;
        const QJsonObject c = ctxs[i].toObject();
        auto& s = snap.coins[name];
        s.funding_rate = c.value("funding").toString().toDouble();
        s.open_interest = c.value("openInterest").toString().toDouble();
        const double prev = c.value("prevDayPx").toString().toDouble();
        if (prev > 0 && s.mid > 0) s.day_change_pct = (s.mid - prev) / prev * 100.0;
    }
}

QVector<Candle> ArenaMarketData::parse_candles(const QJsonDocument& doc) {
    QVector<Candle> out;
    for (const auto& v : doc.array()) {
        const QJsonObject o = v.toObject();
        Candle k;
        k.t = qint64(o.value("t").toDouble());
        k.o = o.value("o").toString().toDouble();
        k.h = o.value("h").toString().toDouble();
        k.l = o.value("l").toString().toDouble();
        k.c = o.value("c").toString().toDouble();
        k.v = o.value("v").toString().toDouble();
        out.append(k);
    }
    return out;
}

void ArenaMarketData::fetch_mids(const QStringList& coins,
                                 std::function<void(Result<QHash<QString, double>>)> cb) {
    auto coins_copy = coins;
    client_->info(QJsonObject{{"type", "allMids"}}, [coins_copy, cb](Result<QJsonDocument> r) {
        if (r.is_err()) { cb(Result<QHash<QString, double>>::err(r.error())); return; }
        const auto mids = parse_all_mids(r.value(), coins_copy);
        if (mids.isEmpty()) { cb(Result<QHash<QString, double>>::err("no mids for requested coins")); return; }
        cb(Result<QHash<QString, double>>::ok(mids));
    });
}

void ArenaMarketData::fetch_snapshot(const QStringList& coins,
                                     std::function<void(Result<MarketSnapshot>)> cb) {
    // Chain: allMids → metaAndAssetCtxs → N candleSnapshots → done.
    auto snap = std::make_shared<MarketSnapshot>();
    snap->ts = QDateTime::currentMSecsSinceEpoch();
    auto coins_copy = coins;

    client_->info(QJsonObject{{"type", "allMids"}}, [this, snap, coins_copy, cb](Result<QJsonDocument> r) {
        if (r.is_err()) { cb(Result<MarketSnapshot>::err(r.error())); return; }
        const auto mids = parse_all_mids(r.value(), coins_copy);
        if (mids.isEmpty()) { cb(Result<MarketSnapshot>::err("no mids for requested coins")); return; }
        for (auto it = mids.begin(); it != mids.end(); ++it) {
            CoinSnapshot s; s.coin = it.key(); s.mid = it.value();
            snap->coins[it.key()] = s;
        }
        client_->info(QJsonObject{{"type", "metaAndAssetCtxs"}}, [this, snap, cb](Result<QJsonDocument> r2) {
            if (r2.is_ok()) apply_asset_ctxs(r2.value(), *snap);   // ctxs are enrichment — non-fatal
            // Candles per coin, sequential chain to stay simple + rate-limit friendly.
            // *step holds a weak ref to itself (pending network callbacks hold the
            // strong ref) so the self-referencing chain frees once it completes.
            auto remaining = std::make_shared<QStringList>(snap->coins.keys());
            auto step = std::make_shared<std::function<void()>>();
            std::weak_ptr<std::function<void()>> wstep = step;
            *step = [this, snap, remaining, cb, wstep]() {
                if (remaining->isEmpty()) {
                    for (auto& s : snap->coins) apply_indicators(s);
                    cb(Result<MarketSnapshot>::ok(*snap));
                    return;
                }
                const QString coin = remaining->takeFirst();
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                const QJsonObject req{{"coin", coin}, {"interval", "1h"},
                                      {"startTime", double(now - 60ll * 3600 * 1000)}, {"endTime", double(now)}};
                auto strong = wstep.lock();
                client_->info(QJsonObject{{"type", "candleSnapshot"}, {"req", req}},
                              [snap, coin, strong](Result<QJsonDocument> r3) {
                    if (r3.is_ok())
                        snap->coins[coin].candles_1h = parse_candles(r3.value());
                    else
                        LOG_WARN("ArenaMarketData",
                                 QStringLiteral("candleSnapshot failed for %1: %2 (indicators will be n/a)")
                                     .arg(coin, QString::fromStdString(r3.error())));
                    if (strong) (*strong)();
                });
            };
            (*step)();
        });
    });
}

} // namespace fincept::arena
