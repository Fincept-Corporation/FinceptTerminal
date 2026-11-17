#pragma once
// FreeOptionChainService — the free, DELAYED listed-option chain for the Derivatives
// tab: Cboe's public delayed-quote feed, Yahoo Finance as the fallback, fetched by
// scripts/derivatives/deriv_market.py. No broker, no key, nothing paid.
//
//   * The script runs as a child process (PythonRunner) with a 90 s time-out, an
//     output cap and a cancel token. Its reply (up to ~2 MB for SPX) is parsed and
//     analysed (IV, Greeks, smiles, SVI, surface) on a worker thread; the result is
//     an immutable shared FreeChain handed to the GUI thread.
//   * Memory cache per symbol (5 minutes fresh; the last 6 symbols kept). The raw
//     reply is also saved to <cache>/derivatives/chain_<SYMBOL>.json, so the next
//     session shows the last chain at once — labelled with its own quote time —
//     while a fresh one loads.
//   * fetch() may answer twice: first Stage::Saved (the saved copy, when memory has
//     nothing), then Stage::Fresh or Stage::Failed. Callbacks run on the GUI thread
//     and are dropped after cancel() or when `ctx` is destroyed.

#include "services/options/DerivMarketData.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::options {

class FreeOptionChainService : public QObject {
    Q_OBJECT
  public:
    using ChainPtr = std::shared_ptr<const market::FreeChain>;
    enum class Stage { Memory, Saved, Fresh, Failed };

    struct Reply {
        Stage stage = Stage::Failed;
        ChainPtr chain;  ///< null on Failed (the caller keeps what it shows)
        QString status;  ///< chain status, or "feed_unavailable" / "cancelled" for a failed run
        QString message; ///< plain sentence for Failed
    };
    using Callback = std::function<void(const Reply&)>;

    static FreeOptionChainService& instance();

    /// Chain of `symbol` (Yahoo-style "^SPX" or plain "SPY"). `source`: "auto" (Cboe,
    /// Yahoo fallback), "cboe" or "yahoo". A memory copy younger than 5 minutes is
    /// returned (Stage::Memory) unless `force`.
    quint64 fetch(const QString& symbol, const QString& source, bool force, QObject* ctx, Callback cb);
    void cancel(quint64 request_id);

    /// Last chain held in memory for `symbol` (any age), or null.
    ChainPtr cached(const QString& symbol) const;
    /// Normalised cache key of a symbol ("^SPX" → "SPX").
    static QString key_of(const QString& symbol);

  signals:
    /// A chain for `symbol` was loaded (fresh or saved copy).
    void chain_updated(const QString& symbol);

  private:
    FreeOptionChainService();
    void deliver(quint64 id, const Reply& r, bool final_reply);
    void store(const QString& key, const ChainPtr& c, bool fresh);
    void run_script(quint64 id, const QString& symbol, const QString& source);

    struct Pending {
        std::shared_ptr<std::atomic<bool>> cancel;
        QPointer<QObject> ctx;
        Callback cb;
    };
    struct Entry {
        ChainPtr chain;
        QDateTime fresh_at; ///< invalid for a saved copy
    };
    QHash<quint64, Pending> pending_;
    QHash<QString, Entry> memory_;
    QStringList lru_;
    quint64 next_id_ = 0;
};

} // namespace fincept::services::options
