// src/algo_engine/fno/FnoDataBridge.cpp
#include "algo_engine/fno/FnoDataBridge.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/options/OptionChainService.h"

#include <QMutexLocker>
#include <QThread>

namespace {

// Reproduces OptionChainService::chain_topic() byte-for-byte without touching
// the singleton, so it is safe to call from the engine thread.
// Format: "option:chain:<broker>:<underlying>:<expiry>"
QString make_chain_topic(const QString& broker, const QString& underlying, const QString& expiry) {
    return QStringLiteral("option:chain:") + broker + QLatin1Char(':') + underlying + QLatin1Char(':') + expiry;
}

} // namespace

namespace fincept::algo::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainService;

FnoDataBridge::FnoDataBridge(QObject* parent) : QObject(parent) {
    // This object lives on the main thread. chain_published fires on the main thread
    // (OptionChainService publishes from there), so this direct connection is safe.
    connect(&OptionChainService::instance(), &OptionChainService::chain_published,
            this, &FnoDataBridge::ingest_chain);
}

void FnoDataBridge::ingest_chain(const OptionChain& chain) {
    const QString topic = make_chain_topic(chain.broker_id, chain.underlying, chain.expiry);
    QMutexLocker l(&mutex_);
    chains_[topic] = chain;
}

OptionChain FnoDataBridge::snapshot(const QString& broker, const QString& underlying,
                                    const QString& expiry) const {
    const QString topic = make_chain_topic(broker, underlying, expiry);
    QMutexLocker l(&mutex_);
    return chains_.value(topic);
}

void FnoDataBridge::ensure_chain(const QString& broker, const QString& underlying,
                                  const QString& expiry) {
    const QString topic = make_chain_topic(broker, underlying, expiry);
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, topic]() { do_ensure_chain(topic); },
                                  Qt::QueuedConnection);
    } else {
        do_ensure_chain(topic);
    }
}

void FnoDataBridge::pin_legs(const QString& broker, const QString& underlying,
                              const QString& expiry, const QStringList& symbols) {
    const QString topic = make_chain_topic(broker, underlying, expiry);
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, topic, symbols]() { do_pin(topic, symbols); },
                                  Qt::QueuedConnection);
    } else {
        do_pin(topic, symbols);
    }
}

void FnoDataBridge::do_ensure_chain(const QString& topic) {
    // Called on the main thread. If not yet subscribed, subscribe to the topic
    // so the DataHub producer wakes up and starts publishing. The chain_published
    // signal connection (ctor) caches whatever is produced into chains_.
    if (subscribed_.value(topic, false))
        return;
    subscribed_[topic] = true;
    auto& hub = fincept::datahub::DataHub::instance();
    // Subscribe with this as owner so the hub can unsubscribe on destruction.
    // Callback is a no-op since ingest_chain (via chain_published signal) already
    // caches the data. The subscribe call is what wakes the producer.
    hub.subscribe(this, topic, [](const QVariant&) {});
    hub.request(topic, /*force*/ false);
}

void FnoDataBridge::do_pin(const QString& topic, const QStringList& symbols) {
    // Called on the main thread.
    OptionChainService::instance().pin_contracts(topic, symbols);
}

} // namespace fincept::algo::fno
