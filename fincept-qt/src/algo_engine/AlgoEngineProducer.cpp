// src/algo_engine/AlgoEngineProducer.cpp
#include "algo_engine/AlgoEngineProducer.h"
#include "algo_engine/AlgoEngine.h"
#include "datahub/DataHub.h"

namespace fincept::algo {

AlgoEngineProducer& AlgoEngineProducer::instance() {
    static AlgoEngineProducer s;
    return s;
}

AlgoEngineProducer::AlgoEngineProducer() = default;

void AlgoEngineProducer::ensure_registered_with_hub() {
    if (hub_registered_) return;

    auto& hub = datahub::DataHub::instance();
    hub.register_producer(this);

    datahub::TopicPolicy metrics_policy;
    metrics_policy.push_only = true;
    metrics_policy.coalesce_within_ms = 1000;
    metrics_policy.drop_on_idle = true;
    hub.set_policy_pattern(QStringLiteral("algo:metrics:*"), metrics_policy);

    datahub::TopicPolicy trade_policy;
    trade_policy.push_only = true;
    trade_policy.drop_on_idle = true;
    hub.set_policy_pattern(QStringLiteral("algo:trade:*"), trade_policy);

    datahub::TopicPolicy state_policy;
    state_policy.push_only = true;
    hub.set_policy_pattern(QStringLiteral("algo:state:*"), state_policy);

    hub_registered_ = true;

    AlgoEngine::instance().recover_orphaned();
}

QStringList AlgoEngineProducer::topic_patterns() const {
    return {
        QStringLiteral("algo:metrics:*"),
        QStringLiteral("algo:trade:*"),
        QStringLiteral("algo:state:*"),
    };
}

void AlgoEngineProducer::refresh(const QStringList& /*topics*/) {
    // Push-only producer — no pull refresh needed
}

int AlgoEngineProducer::max_requests_per_sec() const {
    return 0;
}

} // namespace fincept::algo
