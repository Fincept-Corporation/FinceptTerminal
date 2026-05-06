#include "core/symbol/SymbolContext.h"

#include "core/logging/Logger.h"

#include <QJsonObject>
#include <QSet>

namespace fincept {

SymbolContext& SymbolContext::instance() {
    static SymbolContext s;
    return s;
}

SymbolContext::SymbolContext() = default;

SymbolRef SymbolContext::group_symbol(SymbolGroup g) const {
    return group_slot_symbol(g, kPrimarySlot);
}

bool SymbolContext::has_group_symbol(SymbolGroup g) const {
    return has_group_slot_symbol(g, kPrimarySlot);
}

void SymbolContext::set_group_symbol(SymbolGroup g, const SymbolRef& ref, QObject* source) {
    set_group_slot_symbol(g, kPrimarySlot, ref, source);
}

SymbolRef SymbolContext::group_slot_symbol(SymbolGroup g, const QString& slot) const {
    if (g == SymbolGroup::None)
        return {};
    return slot_symbols_.value({g, slot});
}

bool SymbolContext::has_group_slot_symbol(SymbolGroup g, const QString& slot) const {
    if (g == SymbolGroup::None)
        return false;
    const auto it = slot_symbols_.constFind({g, slot});
    return it != slot_symbols_.constEnd() && it.value().is_valid();
}

void SymbolContext::set_group_slot_symbol(SymbolGroup g, const QString& slot,
                                          const SymbolRef& ref, QObject* source) {
    // Update the "active anywhere" symbol regardless of group/slot — this
    // drives the breadcrumb / titlebar even for unlinked panels.
    if (ref.is_valid() && ref != active_) {
        active_ = ref;
        emit active_symbol_changed(active_, source);
    }

    if (g == SymbolGroup::None)
        return;

    const QPair<SymbolGroup, QString> key{g, slot};
    const SymbolRef prev = slot_symbols_.value(key);
    if (prev == ref)
        return; // no-op

    slot_symbols_[key] = ref;
    LOG_DEBUG("SymbolContext", QString("Group %1 slot=%2 → %3")
                                   .arg(symbol_group_letter(g))
                                   .arg(slot)
                                   .arg(ref.display()));

    // Slot-aware signal first; legacy primary-slot signal second.
    // Order matters for subscribers that bridge from the slot-aware
    // signal back into UI state — they typically want to win over
    // the legacy listener so the panel is consistent on the first
    // event delivery.
    emit group_slot_symbol_changed(g, slot, ref, source);
    if (slot == kPrimarySlot)
        emit group_symbol_changed(g, ref, source);
}

void SymbolContext::clear() {
    if (slot_symbols_.isEmpty() && !active_.is_valid())
        return;
    // Snapshot before clearing so signal emit doesn't race re-entrant
    // writes triggered by handlers.
    const auto cleared_keys = slot_symbols_.keys();
    slot_symbols_.clear();
    active_ = {};

    // Notify per-(group, slot) for actual entries that were cleared.
    // Track which groups we've already emitted a primary-slot legacy
    // signal for so the belt-and-braces loop below doesn't double-fire.
    QSet<SymbolGroup> primary_emitted;
    for (const auto& key : cleared_keys) {
        emit group_slot_symbol_changed(key.first, key.second, SymbolRef{}, nullptr);
        if (key.second == kPrimarySlot) {
            emit group_symbol_changed(key.first, SymbolRef{}, nullptr);
            primary_emitted.insert(key.first);
        }
    }
    // Belt-and-braces: legacy primary-slot consumers (Watchlist, Equity
    // Research, etc.) wired pre-extension assume `clear()` always emits
    // a primary-slot empty for every group, even ones that had nothing
    // populated. Preserve that behaviour without double-firing for
    // groups whose primary slot was already in cleared_keys above.
    for (SymbolGroup g : all_symbol_groups()) {
        if (!primary_emitted.contains(g))
            emit group_symbol_changed(g, SymbolRef{}, nullptr);
    }
    emit active_symbol_changed(SymbolRef{}, nullptr);
}

QJsonObject SymbolContext::to_json() const {
    QJsonObject o;
    QJsonObject grp;        // legacy v1 schema: { "<letter>": SymbolRef }
    QJsonObject slot_blob;  // v2 schema: { "<letter>": { "<slot>": SymbolRef } }
    for (auto it = slot_symbols_.begin(); it != slot_symbols_.end(); ++it) {
        if (!it.value().is_valid())
            continue;
        const QString letter(symbol_group_letter(it.key().first));
        const QString slot = it.key().second;
        // Always populate slot blob.
        QJsonObject entry = slot_blob.value(letter).toObject();
        entry[slot] = it.value().to_json();
        slot_blob[letter] = entry;
        // Legacy schema only carries the primary slot so older builds can
        // still read the workspace JSON. Non-primary slots only land in
        // the new "group_slots" key.
        if (slot == kPrimarySlot)
            grp[letter] = it.value().to_json();
    }
    o["groups"] = grp;
    o["group_slots"] = slot_blob;
    if (active_.is_valid())
        o["active"] = active_.to_json();
    return o;
}

void SymbolContext::from_json(const QJsonObject& o) {
    clear();
    // Prefer the slot-aware schema if present (workspaces saved by builds
    // with the slot extension). Fall back to the legacy "groups" schema
    // for older saves so existing user workspaces continue to load.
    //
    // NB: local variable can't be named `slots` — Qt's moc keyword macros
    // (`slots`, `signals`) are typically defined to nothing in headers
    // included before this point and would obliterate the identifier.
    const QJsonObject slot_obj = o.value("group_slots").toObject();
    if (!slot_obj.isEmpty()) {
        for (auto git = slot_obj.begin(); git != slot_obj.end(); ++git) {
            if (git.key().isEmpty())
                continue;
            const SymbolGroup g = symbol_group_from_char(git.key().at(0));
            if (g == SymbolGroup::None)
                continue;
            const QJsonObject entries = git.value().toObject();
            for (auto sit = entries.begin(); sit != entries.end(); ++sit) {
                const SymbolRef ref = SymbolRef::from_json(sit.value().toObject());
                if (!ref.is_valid())
                    continue;
                slot_symbols_[{g, sit.key()}] = ref;
                emit group_slot_symbol_changed(g, sit.key(), ref, nullptr);
                if (sit.key() == kPrimarySlot)
                    emit group_symbol_changed(g, ref, nullptr);
            }
        }
    } else {
        const QJsonObject grp = o.value("groups").toObject();
        for (auto it = grp.begin(); it != grp.end(); ++it) {
            if (it.key().isEmpty())
                continue;
            const SymbolGroup g = symbol_group_from_char(it.key().at(0));
            if (g == SymbolGroup::None)
                continue;
            const SymbolRef ref = SymbolRef::from_json(it.value().toObject());
            if (ref.is_valid()) {
                slot_symbols_[{g, kPrimarySlot}] = ref;
                emit group_slot_symbol_changed(g, kPrimarySlot, ref, nullptr);
                emit group_symbol_changed(g, ref, nullptr);
            }
        }
    }
    if (o.contains("active")) {
        active_ = SymbolRef::from_json(o.value("active").toObject());
        if (active_.is_valid())
            emit active_symbol_changed(active_, nullptr);
    }
}

} // namespace fincept
