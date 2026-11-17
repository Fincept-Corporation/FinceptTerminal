#pragma once
// OrderSourceCapture.h — stamp UnifiedOrder::source at a screen's submit call.
//
// UnifiedTrading fills an EMPTY source with the agent's at its entry points, but only
// on the GUI thread (core/agent/AgentActionScope.h answers "" anywhere else). A
// screen whose order hops to a worker, waits on a network round trip or on a later
// confirmation before reaching UnifiedTrading / the paper engine therefore captures
// the source HERE, on the GUI thread, at the moment it hands the order off — AFTER
// any confirmation dialog it shows: a person who answers that dialog is outside the
// agent's step, so the order is theirs.
//
//     order.source = trading::capture_order_source(QStringLiteral("ticket"));
//     // "agent:claude_code" while an agent pressed the button, else "ticket"
//
// Qt Core only (plus AgentActionScope.cpp at link time).

#include "core/agent/AgentActionScope.h"

#include <QString>

namespace fincept::trading {

/// The agent's source ("agent:<client id>") while an agent step is in progress on the
/// GUI thread, else `screen_kind` (ticket, basket, split, smart, replication, …).
inline QString capture_order_source(const QString& screen_kind) {
    const QString agent = fincept::core::agent::current_agent_order_source();
    return agent.isEmpty() ? screen_kind : agent;
}

} // namespace fincept::trading
