// src/services/relationship_map/RelationshipMapService.h
#pragma once

#include "screens/relationship_map/RelationshipMapTypes.h"

#include <QObject>

#    include "datahub/Producer.h"

namespace fincept::services {

/// Fetches corporate relationship data via Python/yfinance.
/// Emits progress signals for progressive UI updates.
class RelationshipMapService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static RelationshipMapService& instance();

    void fetch(const QString& ticker);
    void clear();

    const relmap::RelationshipData& data() const { return data_; }
    bool is_loading() const { return loading_; }

    /// Register with the hub + install geopolitics:relationship_graph:* policy. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ─────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

  signals:
    void progress_changed(int percent, const QString& message);
    void data_ready(const fincept::relmap::RelationshipData& data);
    void fetch_failed(const QString& error);

  private:
    RelationshipMapService() = default;

    void parse_result(const QString& json_output);
    relmap::ValuationSignal compute_valuation(const relmap::CompanyInfo& co, const QVector<relmap::PeerCompany>& peers);

    relmap::RelationshipData data_;
    bool loading_ = false;
    QString current_ticker_;

    bool hub_registered_ = false;
};

} // namespace fincept::services
