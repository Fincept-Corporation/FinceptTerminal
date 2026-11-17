#include "services/alpha_arena/HyperliquidLiveVenue.h"

#include "core/logging/Logger.h"
#include "trading/exchanges/hyperliquid/HyperliquidVenue.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::arena {

namespace hlx = fincept::trading::hyperliquid;
namespace aav = fincept::services::alpha_arena;

static constexpr const char* TAG = "ArenaLive";

HyperliquidLiveVenue::HyperliquidLiveVenue(ArenaStore& store, const QString& competition_id)
    : PaperExchange(store, competition_id) {
    // Same construction as the old engine (AlphaArenaEngine::select_venue_for_kind):
    // mainnet default + immediate connect. The agent wallet key never touches this
    // class — HyperliquidSigner reads it from SecureStorage at sign time under
    // "alpha_arena/agent_key/<competition_id>", which is why we bind the id here.
    hl_ = new hlx::HyperliquidVenue(nullptr);
    hl_->set_competition_id(competition_id);
    hl_->set_testnet(false);
    hl_->connect();
    LOG_INFO(TAG, QStringLiteral("live venue up for competition %1 (mainnet)").arg(competition_id));
}

HyperliquidLiveVenue::~HyperliquidLiveVenue() {
    delete hl_;
}

Result<OrderRow> HyperliquidLiveVenue::execute(const QString& agent_id, const AgentAction& a, int round_seq) {
    auto r = PaperExchange::execute(agent_id, a, round_seq);
    if (r.is_ok() && r.value().status == QLatin1String("filled"))
        mirror_live(r.value());
    return r;
}

void HyperliquidLiveVenue::mirror_live(const OrderRow& o) {
    aav::OrderRequest req;
    req.agent_id = o.agent_id;
    req.coin = o.coin;
    // open long / close short → buy; open short / close long → sell.
    const bool is_buy = (o.action == QLatin1String("open")) == (o.side == QLatin1String("long"));
    req.side = is_buy ? QStringLiteral("buy") : QStringLiteral("sell");
    req.qty = o.qty;
    req.leverage = qMax(1, static_cast<int>(o.leverage));
    req.reduce_only = o.action == QLatin1String("close");

    // Fire-and-record: the paper fill already happened (it is the ledger); a
    // live rejection is logged + persisted so the operator can reconcile.
    // Capture `o` by value only — the ack may outlive this venue.
    // Real-money order flow: submitted / accepted / rejected go to the audit trail
    // (policy B.7), paired by client_id = the paper order id. This live mirror does not
    // go through UnifiedTrading, so these are its only order lines.
    QElapsedTimer sent_clock;
    sent_clock.start();
    LOG_AUDIT("Orders", "Order submitted",
              {{"venue", "hyperliquid"},
               {"account", QStringLiteral("arena:") + o.competition_id},
               {"mode", "live"},
               {"symbol", o.coin},
               {"side", req.side},
               {"type", "market"},
               {"qty", req.qty},
               {"leverage", req.leverage},
               {"reduce_only", req.reduce_only},
               {"client_id", o.id},
               {"source", QStringLiteral("arena:") + o.agent_id}});
    hl_->place_order(req, [o, sent_clock](aav::OrderAck ack) {
        if (ack.status == QLatin1String("accepted")) {
            LOG_AUDIT("Orders", "Order accepted",
                      {{"venue", "hyperliquid"},
                       {"account", QStringLiteral("arena:") + o.competition_id},
                       {"client_id", o.id},
                       {"order_id", ack.venue_order_id},
                       {"elapsed_ms", sent_clock.elapsed()}});
            return;
        }
        LOG_AUDIT("Orders", "Order rejected",
                  {{"venue", "hyperliquid"},
                   {"account", QStringLiteral("arena:") + o.competition_id},
                   {"client_id", o.id},
                   {"symbol", o.coin},
                   {"action", o.action},
                   {"reason", ack.error},
                   {"elapsed_ms", sent_clock.elapsed()}});
        ArenaStore::instance().insert_event(
            o.competition_id, o.agent_id, QStringLiteral("live_mirror_failed"),
            QString::fromUtf8(
                QJsonDocument(
                    QJsonObject{{"order_id", o.id}, {"coin", o.coin}, {"action", o.action}, {"error", ack.error}})
                    .toJson(QJsonDocument::Compact)));
    });
}

} // namespace fincept::arena
