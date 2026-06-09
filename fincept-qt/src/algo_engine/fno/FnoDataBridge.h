// src/algo_engine/fno/FnoDataBridge.h
#pragma once
#include "services/options/OptionChainTypes.h"
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept::algo::fno {

// GUI/hub-thread adapter so the algo ENGINE THREAD can safely read option-chain
// snapshots and pin its legs into OptionChainService's live WS window. Lives on
// the main thread (created before AlgoEngine::moveToThread). Engine-thread callers
// use ensure_chain()/snapshot()/pin_legs(); mutating calls marshal to this object's
// (main) thread; snapshot() returns a mutex-guarded COPY.
class FnoDataBridge : public QObject {
    Q_OBJECT
  public:
    explicit FnoDataBridge(QObject* parent = nullptr);

    // Engine-thread-safe. Ensures a chain for (broker,underlying,expiry) is being
    // produced + cached (subscribes to its topic on the main thread).
    void ensure_chain(const QString& broker, const QString& underlying, const QString& expiry);
    // Engine-thread-safe. Returns a COPY of the last cached chain (empty if none yet).
    fincept::services::options::OptionChain snapshot(const QString& broker, const QString& underlying,
                                                     const QString& expiry) const;
    // Engine-thread-safe. Pins concrete contract symbols into the live WS window.
    void pin_legs(const QString& broker, const QString& underlying, const QString& expiry,
                  const QStringList& symbols);

    // Test/seam hook — ingest a chain as if chain_published fired (used by the
    // headless selftest; also the slot connected to OptionChainService).
    void ingest_chain(const fincept::services::options::OptionChain& chain);

  private:
    void do_ensure_chain(const QString& topic);
    void do_pin(const QString& topic, const QStringList& symbols);

    mutable QMutex mutex_;
    QHash<QString, fincept::services::options::OptionChain> chains_; // keyed by topic; guarded by mutex_
    QHash<QString, bool> subscribed_; // main-thread-only (always accessed in do_ensure_chain); no mutex needed
};

} // namespace fincept::algo::fno
