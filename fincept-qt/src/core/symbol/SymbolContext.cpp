#include "core/symbol/SymbolContext.h"

#include "core/logging/Logger.h"

#include <QJsonObject>

namespace fincept {

SymbolContext& SymbolContext::instance() {
    static SymbolContext s;
    return s;
}

SymbolContext::SymbolContext() = default;

SymbolRef SymbolContext::group_symbol(SymbolGroup g) const {
    if (g == SymbolGroup::None)
        return {};
    return groups_.value(g);
}

bool SymbolContext::has_group_symbol(SymbolGroup g) const {
    return g != SymbolGroup::None && groups_.contains(g) && groups_[g].is_valid();
}

void SymbolContext::set_group_symbol(SymbolGroup g, const SymbolRef& ref, QObject* source) {
    // Update the "active anywhere" symbol regardless of group — this drives
    // the breadcrumb/titlebar even for unlinked panels.
    if (ref.is_valid() && ref != active_) {
        active_ = ref;
        emit active_symbol_changed(active_, source);
    }

    if (g == SymbolGroup::None)
        return;

    const SymbolRef prev = groups_.value(g);
    if (prev == ref)
        return; // no-op

    groups_[g] = ref;
    LOG_DEBUG("SymbolContext",
              QString("Group %1 → %2").arg(symbol_group_letter(g)).arg(ref.display()));
    emit group_symbol_changed(g, ref, source);
}

void SymbolContext::clear() {
    if (groups_.isEmpty() && !active_.is_valid())
        return;
    groups_.clear();
    active_ = {};
    for (SymbolGroup g : all_symbol_groups())
        emit group_symbol_changed(g, SymbolRef{}, nullptr);
    emit active_symbol_changed(SymbolRef{}, nullptr);
}

QJsonObject SymbolContext::to_json() const {
    QJsonObject o;
    QJsonObject grp;
    for (auto it = groups_.begin(); it != groups_.end(); ++it) {
        if (!it.value().is_valid())
            continue;
        grp[QString(symbol_group_letter(it.key()))] = it.value().to_json();
    }
    o["groups"] = grp;
    if (active_.is_valid())
        o["active"] = active_.to_json();
    return o;
}

void SymbolContext::from_json(const QJsonObject& o) {
    clear();
    const QJsonObject grp = o.value("groups").toObject();
    for (auto it = grp.begin(); it != grp.end(); ++it) {
        if (it.key().isEmpty())
            continue;
        const SymbolGroup g = symbol_group_from_char(it.key().at(0));
        if (g == SymbolGroup::None)
            continue;
        const SymbolRef ref = SymbolRef::from_json(it.value().toObject());
        if (ref.is_valid()) {
            groups_[g] = ref;
            emit group_symbol_changed(g, ref, nullptr);
        }
    }
    if (o.contains("active")) {
        active_ = SymbolRef::from_json(o.value("active").toObject());
        if (active_.is_valid())
            emit active_symbol_changed(active_, nullptr);
    }
}

} // namespace fincept
