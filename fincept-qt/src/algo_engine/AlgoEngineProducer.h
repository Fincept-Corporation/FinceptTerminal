// src/algo_engine/AlgoEngineProducer.h
#pragma once
#include "datahub/Producer.h"

#include <QObject>

namespace fincept::algo {

class AlgoEngineProducer : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
public:
    static AlgoEngineProducer& instance();
    void ensure_registered_with_hub();

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

private:
    AlgoEngineProducer();
    bool hub_registered_ = false;
};

} // namespace fincept::algo
