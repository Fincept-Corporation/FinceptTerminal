#pragma once
// HyperliquidLiveVenue — PaperExchange accounting + real order mirroring to
// Hyperliquid mainnet via the existing HyperliquidVenue (signer-gated).
// The paper book IS the ledger; live orders are fire-and-record (failures are
// logged to arena_events and surfaced; accounting still updates so the arena
// stays consistent). NOTE: kill_all/liquidation closes are NOT yet mirrored to
// the live book — this must be solved before the real exchange POST is wired.
// stays consistent). Wallet key handle: "alpha_arena/agent_key/<competition_id>"
// (written by LiveModeGateDialog; ArenaEngine::start() re-keys the wizard's
// "pending" handle to the real competition id before constructing this venue).
#include "services/alpha_arena/PaperExchange.h"

namespace fincept::trading::hyperliquid { class HyperliquidVenue; }

namespace fincept::arena {

class HyperliquidLiveVenue : public PaperExchange {
  public:
    HyperliquidLiveVenue(ArenaStore& store, const QString& competition_id);
    ~HyperliquidLiveVenue() override;

    Result<OrderRow> execute(const QString& agent_id, const AgentAction& a, int round_seq) override;

  private:
    void mirror_live(const OrderRow& filled);

    fincept::trading::hyperliquid::HyperliquidVenue* hl_ = nullptr;   // owned (no QObject parent)
};

} // namespace fincept::arena
