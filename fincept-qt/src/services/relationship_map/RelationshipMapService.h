// src/services/relationship_map/RelationshipMapService.h
#pragma once

#include "screens/relationship_map/RelationshipMapTypes.h"

#include <QObject>

namespace fincept::services {

/// Fetches corporate relationship data via Python/yfinance.
/// Emits progress signals for progressive UI updates.
class RelationshipMapService : public QObject {
    Q_OBJECT
  public:
    static RelationshipMapService& instance();

    void fetch(const QString& ticker);
    void clear();

    const relmap::RelationshipData& data() const { return data_; }
    bool is_loading() const { return loading_; }

  signals:
    void progress_changed(int percent, const QString& message);
    void data_ready(const fincept::relmap::RelationshipData& data);
    void fetch_failed(const QString& error);

  private:
    RelationshipMapService() = default;

    void parse_result(const QString& json_output);
    relmap::ValuationSignal compute_valuation(const relmap::CompanyInfo& co,
                                               const QVector<relmap::PeerCompany>& peers);

    relmap::RelationshipData data_;
    bool loading_ = false;
    QString current_ticker_;
};

} // namespace fincept::services
