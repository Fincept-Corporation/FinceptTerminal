#pragma once
// Header-only deterministic fakes for --selftest-arena. No network.
#include "services/alpha_arena/ArenaLlmClient.h"
#include "services/alpha_arena/ArenaMarketDataIface.h"
#include <QQueue>
#include <QTimer>

namespace fincept::arena {

/// Scripted per-model responses; FIFO per model_id; default = hold.
class FakeLlmClient : public IArenaLlmClient {
  public:
    using IArenaLlmClient::IArenaLlmClient;
    QHash<QString, QQueue<QString>> scripts;   // model_id → raw responses
    void complete(const ArenaLlmRequest& req, std::function<void(ArenaLlmResult)> cb) override {
        ArenaLlmResult r;
        r.success = true;
        r.content = scripts.contains(req.model_id) && !scripts[req.model_id].isEmpty()
                        ? scripts[req.model_id].dequeue()
                        : QStringLiteral(R"({"actions":[{"action":"hold","reason":"default"}]})");
        r.prompt_tokens = 100; r.completion_tokens = 20; r.latency_ms = 1;
        // Async like the real client — engine must not assume sync completion.
        QTimer::singleShot(0, this, [cb, r]() { cb(r); });
    }
};

class FakeMarketData : public IArenaMarketData {
  public:
    using IArenaMarketData::IArenaMarketData;
    MarketSnapshot next;
    bool fail = false;
    void fetch_snapshot(const QStringList& coins, std::function<void(Result<MarketSnapshot>)> cb) override {
        MarketSnapshot out = next;
        QTimer::singleShot(0, this, [this, cb, out]() {
            if (fail) { cb(Result<MarketSnapshot>::err("scripted failure")); return; }
            cb(Result<MarketSnapshot>::ok(out));
        });
        Q_UNUSED(coins)
    }
    void fetch_mids(const QStringList& coins,
                    std::function<void(Result<QHash<QString, double>>)> cb) override {
        QHash<QString, double> mids;
        for (auto it = next.coins.begin(); it != next.coins.end(); ++it) mids[it.key()] = it.value().mid;
        QTimer::singleShot(0, this, [this, cb, mids]() {
            if (fail) { cb(Result<QHash<QString, double>>::err("scripted failure")); return; }
            cb(Result<QHash<QString, double>>::ok(mids));
        });
        Q_UNUSED(coins)
    }
};

} // namespace fincept::arena
