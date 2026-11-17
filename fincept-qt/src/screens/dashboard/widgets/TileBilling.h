#pragma once
// TileBilling — the plan-aware side of the dashboard's billing rule: nothing
// that costs Fincept credits loads by itself. billing::PlanAccess (the server's
// own plan table) says whether the account's plan includes a category at no
// credit cost — then the tile loads by itself as a free one would — and what a
// call costs this account (the LOAD offer's cost note). Without a table (an
// older server, or before it answered) every billed call stays manual and the
// note falls back to the tile's own wording.
//
// Categories as the server names them (the same keys the Economics,
// Geopolitics and Maritime screens use): "market_data" (/data and /market),
// "macro", "news_events", "maritime".

#include "services/billing/PlanAccess.h"

#include <QCoreApplication>
#include <QString>

namespace fincept::screens::widgets::tile_billing {

/// The account's plan includes `category` at no credit cost (false without a table).
inline bool plan_includes(const QString& category) {
    const auto& access = billing::PlanAccess::instance();
    return access.has_table() && access.included(category);
}

/// Agent-approval kind of a billed tile (BaseWidget::set_agent_risk): "spend",
/// or none while the plan includes `category` (a load is then free). Call it
/// again on PlanAccess::changed.
inline const char* agent_kind(const QString& category) {
    return plan_includes(category) ? nullptr : "spend";
}

/// The cost note of a LOAD offer. `list_credits` = the call's list price (<= 0:
/// not known here). With the server's table: this account's own price
/// (effective_cost), else the table's wording for the plan; otherwise `fallback`.
/// `per` names the unit when it is not one load ("vessel").
inline QString cost_note(const QString& category, double list_credits, const QString& fallback,
                         const QString& per = QString()) {
    const auto& access = billing::PlanAccess::instance();
    if (!access.has_table())
        return fallback;
    if (list_credits > 0.0) {
        const double c = access.effective_cost(category, list_credits);
        const QString amount = QString::number(c, 'g', 3);
        return per.isEmpty()
                   ? QCoreApplication::translate("TileBilling", "%1 credit(s) per load on your plan").arg(amount)
                   : QCoreApplication::translate("TileBilling", "%1 credit(s) per %2 per load on your plan")
                         .arg(amount, per);
    }
    if (const billing::PlanPrice* row = access.price(category)) {
        const QString text = access.is_paid() ? row->pro_price : row->free_price;
        if (!text.isEmpty())
            return QCoreApplication::translate("TileBilling", "%1 on your plan").arg(text);
    }
    return fallback;
}

} // namespace fincept::screens::widgets::tile_billing
