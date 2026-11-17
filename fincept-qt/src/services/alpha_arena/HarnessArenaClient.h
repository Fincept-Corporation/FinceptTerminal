#pragma once
// HarnessArenaClient — an Alpha Arena competitor answered by an agent harness
// (Claude Code, Codex) instead of an HTTP provider: provider "harness:<id>",
// model = the harness's model. ArenaLlmClient forwards such requests here.
//
// One decision = one harness process through HarnessChatBackend: no terminal
// tools, JSON by the decision schema where the harness supports it, 60 s for
// the whole call. Only by explicit choice in the arena wizard (never an
// automatic default): at the default 3-minute cadence that is about 480 calls
// per agent per day, billed to the user's own API key with its rate limits.
//
// complete() is called on the GUI thread (like every IArenaLlmClient); the
// blocking harness call runs on a small shared worker pool and the callback is
// delivered back on the GUI thread. A client destroyed mid-call cancels it
// (the process tree is killed) and its callback is dropped.

#include "services/alpha_arena/ArenaLlmClient.h"

#include <atomic>
#include <memory>

namespace fincept::arena {

class HarnessArenaClient : public IArenaLlmClient {
    Q_OBJECT
  public:
    explicit HarnessArenaClient(QObject* parent = nullptr);
    ~HarnessArenaClient() override;

    void complete(const ArenaLlmRequest& req, std::function<void(ArenaLlmResult)> cb) override;

    /// Decisions in flight at once (the chat process pool allows two anyway).
    static constexpr int kMaxConcurrentCalls = 2;

  private:
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

} // namespace fincept::arena
