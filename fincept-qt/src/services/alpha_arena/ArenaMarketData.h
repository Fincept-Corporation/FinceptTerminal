#pragma once
// ArenaMarketData — keyless Hyperliquid public-info market data (spec §3.2).
// POST https://api.hyperliquid.xyz/info with {"type":"allMids"} /
// {"type":"metaAndAssetCtxs"} / {"type":"candleSnapshot",...} — verified live
// keyless 2026-06-10. Built on trading/exchanges/hyperliquid/HyperliquidClient.
// allMids also returns builder-deployed "#NNNN" assets — we filter to the
// requested coin list. Pure parsers are static for selftest fixtures.
#include "services/alpha_arena/ArenaMarketDataIface.h"
#include <QJsonDocument>
namespace fincept::trading::hyperliquid { class HyperliquidClient; }

namespace fincept::arena {

class ArenaMarketData : public IArenaMarketData {
    Q_OBJECT
  public:
    explicit ArenaMarketData(QObject* parent = nullptr);
    /// allMids + metaAndAssetCtxs + per-coin 1h candleSnapshot (last 60 bars),
    /// merged into one MarketSnapshot with indicators applied.
    void fetch_snapshot(const QStringList& coins,
                        std::function<void(Result<MarketSnapshot>)> cb) override;
    /// Single {"type":"allMids"} call → coin→mid (live-marks loop between rounds).
    void fetch_mids(const QStringList& coins,
                    std::function<void(Result<QHash<QString, double>>)> cb) override;

    // Pure parsers (selftest-covered):
    static QHash<QString, double> parse_all_mids(const QJsonDocument& doc, const QStringList& coins);
    /// metaAndAssetCtxs → (funding, open_interest, prev_day_px) per coin.
    static void apply_asset_ctxs(const QJsonDocument& doc, MarketSnapshot& snap);
    static QVector<Candle> parse_candles(const QJsonDocument& doc);

  private:
    fincept::trading::hyperliquid::HyperliquidClient* client_ = nullptr;
};

} // namespace fincept::arena
