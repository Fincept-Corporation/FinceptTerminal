#pragma once
// MacroCalendarService — DataHub producer for the upcoming macro/economic
// events feed served by Fincept's API (`api.fincept.in/macro/upcoming-events`).
//
// Topic: `econ:fincept:upcoming_events`. Payload: `QJsonArray` (the events
// array as returned by the API). One topic, one producer, one HTTP fetch
// per refresh — the hub fans out to the EconomicCalendarWidget and any
// future consumers (report builder, MCP tools, agents).
//
// TTL: 5 min. Min interval: 60 s. Both honour the upstream cache and keep
// rate-limit pressure low.

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include "datahub/Producer.h"

namespace fincept::services {

class MacroCalendarService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static MacroCalendarService& instance();

    /// Register with the hub + install the topic policy. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 2; }

  private:
    explicit MacroCalendarService(QObject* parent = nullptr);
    Q_DISABLE_COPY(MacroCalendarService)

    bool hub_registered_ = false;
};

} // namespace fincept::services

// QJsonArray is built into QtCore's metatype system — no explicit
// Q_DECLARE_METATYPE needed.
