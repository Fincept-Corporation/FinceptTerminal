#include "core/actions/ActionRegistry.h"

#include "core/logging/Logger.h"
#include "core/telemetry/TelemetryProvider.h"

#include <QVariantMap>
#include <algorithm>

namespace fincept {

namespace {
constexpr const char* kActionRegistryTag = "ActionRegistry";
} // namespace

ActionRegistry& ActionRegistry::instance() {
    static ActionRegistry s;
    return s;
}

bool ActionRegistry::register_action(ActionDef def) {
    if (def.id.isEmpty()) {
        LOG_WARN(kActionRegistryTag, "register_action called with empty id — ignoring");
        return false;
    }

    const bool replacing = by_id_.contains(def.id);
    const QString id = def.id; // copy before move
    by_id_.insert(id, std::move(def));
    if (!replacing)
        insertion_order_.append(id);

    if (replacing) {
        LOG_DEBUG(kActionRegistryTag, QString("Replaced action: %1").arg(id));
        emit action_replaced(id);
    } else {
        LOG_DEBUG(kActionRegistryTag, QString("Registered action: %1").arg(id));
        emit action_registered(id);
    }
    return true;
}

bool ActionRegistry::unregister_action(const QString& id) {
    if (!by_id_.contains(id))
        return false;
    by_id_.remove(id);
    insertion_order_.removeAll(id);
    LOG_DEBUG(kActionRegistryTag, QString("Unregistered action: %1").arg(id));
    emit action_unregistered(id);
    return true;
}

const ActionDef* ActionRegistry::find(const QString& id) const {
    auto it = by_id_.constFind(id);
    return it == by_id_.constEnd() ? nullptr : &it.value();
}

QStringList ActionRegistry::all_ids() const {
    return insertion_order_;
}

int ActionRegistry::size() const {
    return by_id_.size();
}

QList<const ActionDef*> ActionRegistry::match(const QString& prefix, int max_results) const {
    QList<const ActionDef*> out;
    if (prefix.isEmpty() || max_results <= 0)
        return out;

    const QString p = prefix.toLower();

    // Bucket by relevance tier so order is deterministic regardless of
    // QHash iteration. Cheap enough at our action count (~30 in v1, ~100
    // upper bound) — no need for an index until Phase 9.
    QList<const ActionDef*> tier_id_exact;
    QList<const ActionDef*> tier_id_prefix;
    QList<const ActionDef*> tier_display_prefix;
    QList<const ActionDef*> tier_alias_prefix;

    for (const QString& id : insertion_order_) {
        const ActionDef* def = find(id);
        if (!def) continue;
        const QString id_lc = def->id.toLower();
        if (id_lc == p) {
            tier_id_exact.append(def);
            continue;
        }
        if (id_lc.startsWith(p)) {
            tier_id_prefix.append(def);
            continue;
        }
        if (def->display.toLower().startsWith(p)) {
            tier_display_prefix.append(def);
            continue;
        }
        for (const QString& alias : def->aliases) {
            if (alias.toLower().startsWith(p)) {
                tier_alias_prefix.append(def);
                break;
            }
        }
    }

    for (const auto* d : tier_id_exact) { if (out.size() >= max_results) break; out.append(d); }
    for (const auto* d : tier_id_prefix) { if (out.size() >= max_results) break; out.append(d); }
    for (const auto* d : tier_display_prefix) { if (out.size() >= max_results) break; out.append(d); }
    for (const auto* d : tier_alias_prefix) { if (out.size() >= max_results) break; out.append(d); }
    return out;
}

Result<void> ActionRegistry::invoke(const QString& id, const CommandContext& ctx) const {
    const ActionDef* def = find(id);
    if (!def)
        return Result<void>::err(("Unknown action: " + id).toStdString());

    if (def->predicate && !def->predicate(ctx)) {
        LOG_DEBUG(kActionRegistryTag, QString("Predicate denied invocation of %1").arg(id));
        return Result<void>::err(("Action unavailable in current context: " + id).toStdString());
    }

    Result<void> result = def->handler
                              ? def->handler(ctx)
                              : Result<void>::ok();
    if (!def->handler) {
        // Placeholder handler. Common during phased rollout — registered
        // for discoverability but not yet wired. Treat as success so
        // the palette/menu grey-out logic can rely on Result for failure
        // signalling, not no-op detection.
        LOG_DEBUG(kActionRegistryTag, QString("Action %1 has no handler — no-op").arg(id));
    }

    // Phase 10: telemetry. Logs every action invocation (id + outcome) to
    // whichever provider is installed; no-op if none. Privacy: we record
    // only the action id and ok/err outcome — the args bag may contain
    // user-typed text (e.g. ticker symbols) so we deliberately do NOT
    // include it. Stats-counting telemetry, not behavioural replay.
    if (telemetry::TelemetrySink::instance().has_provider()) {
        QVariantMap payload;
        payload.insert("action_id", id);
        payload.insert("ok", result.is_ok());
        if (result.is_err()) {
            // Truncate the error message to keep payloads cheap. Errors
            // come from our own code so they don't carry user PII, but
            // we cap length defensively.
            QString err = QString::fromStdString(result.error());
            if (err.size() > 200) err = err.left(200);
            payload.insert("err", err);
        }
        telemetry::TelemetrySink::instance().record("action.invoke", payload);
    }

    return result;
}

} // namespace fincept
