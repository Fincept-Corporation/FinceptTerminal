#pragma once
// MacroCalendarService — DataHub producer for the upcoming macro/economic
// events feed served by Fincept's API (`api.fincept.in/macro/upcoming-events`).
//
// Topic: `econ:fincept:upcoming_events`. Payload: `QJsonArray` (the events
// array as returned by the API). One topic, one producer, one HTTP fetch
// per refresh — the hub fans out to the EconomicCalendarWidget and any
// future consumers (report builder, MCP tools, agents).
//
// Every uncached call costs the user a credit, and the default dashboard shows
// this topic, so the cadence is deliberately slow: TTL 15 min, min interval
// 5 min (the widget's ⟳ forces a fetch). A refusal the account cannot fix by
// waiting — 402 no credits, 401/403 not signed in or not allowed — is not
// retried on a timer: the topic backs off for hours and the backoff is lifted
// when the balance goes up, the user signs in (or the key changes), or the
// user presses ⟳. While signed out nothing is requested at all.

#include "auth/AuthTypes.h"
#include "datahub/Producer.h"

#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept::services {

class MacroCalendarService : public QObject, public fincept::datahub::Producer {
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

    /// publish_error with a long retry hint and remember what should lift it.
    void publish_refused(const QString& message, auth::RefusalLatch::Kind kind);
    /// Lift a refusal backoff (sign-in, key change, top-up) and, if the widget
    /// is showing, fetch at once (the topic's min-interval would delay it).
    void release_refusal(const char* why);

    bool hub_registered_ = false;
    auth::RefusalLatch refusal_;
};

} // namespace fincept::services

// QJsonArray is built into QtCore's metatype system — no explicit
// Q_DECLARE_METATYPE needed.
