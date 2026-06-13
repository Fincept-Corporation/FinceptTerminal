#pragma once
#include "core/result/Result.h"
#include "services/alpha_arena/ArenaTypes.h"
#include <QHash>
#include <QObject>
#include <functional>
namespace fincept::arena {
class IArenaMarketData : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual void fetch_snapshot(const QStringList& coins,
                                std::function<void(Result<MarketSnapshot>)> cb) = 0;
    /// Lightweight coin→mid fetch for between-round live marks (one allMids call).
    virtual void fetch_mids(const QStringList& coins,
                            std::function<void(Result<QHash<QString, double>>)> cb) = 0;
};
} // namespace fincept::arena
