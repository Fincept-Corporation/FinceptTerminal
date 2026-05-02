#include "core/symbol/SymbolGroupRegistry.h"

#include "core/config/ProfileManager.h"

#include <QSettings>

namespace fincept {

namespace {

/// Compose the QSettings key for a slot field, prefixed with the active
/// profile's settings group so two profiles on the same box keep distinct
/// metadata. For the default profile, ProfileManager::settings_group()
/// returns empty and the key reduces to the legacy "symbol_groups/<letter>/<field>".
QString settings_key_for_active_profile(SymbolGroup g, const char* field) {
    const QString profile_group = ProfileManager::instance().settings_group();
    const QString tail = QStringLiteral("symbol_groups/%1/%2")
                             .arg(symbol_group_letter(g))
                             .arg(QString::fromLatin1(field));
    if (profile_group.isEmpty())
        return tail;
    return profile_group + QStringLiteral("/") + tail;
}

} // namespace

SymbolGroupRegistry& SymbolGroupRegistry::instance() {
    static SymbolGroupRegistry s;
    return s;
}

SymbolGroupRegistry::SymbolGroupRegistry() {
    load();
}

void SymbolGroupRegistry::reload() {
    // Snapshot current state so we only emit signals for slots that actually
    // change. Avoids waking up every badge in the app for slots whose values
    // happen to be identical between profiles (factory defaults match).
    const QHash<SymbolGroup, Slot> before = slots_;
    slots_.clear();
    load();

    for (SymbolGroup g : all_symbol_groups()) {
        const auto bit = before.constFind(g);
        const auto ait = slots_.constFind(g);
        const bool was = bit != before.constEnd();
        const bool is  = ait != slots_.constEnd();
        if (!was || !is) {
            emit group_metadata_changed(g);
            continue;
        }
        if (bit->name != ait->name || bit->color != ait->color || bit->enabled != ait->enabled)
            emit group_metadata_changed(g);
    }
}

QString SymbolGroupRegistry::default_name(SymbolGroup g) {
    if (g == SymbolGroup::None)
        return {};
    return QStringLiteral("Group %1").arg(symbol_group_letter(g));
}

QColor SymbolGroupRegistry::default_color(SymbolGroup g) {
    // Keep the original A..F palette for backwards compatibility, then add
    // four new defaults. All chosen for readability against dark backgrounds.
    switch (g) {
        case SymbolGroup::A: return QColor("#d97706"); // amber
        case SymbolGroup::B: return QColor("#0891b2"); // cyan
        case SymbolGroup::C: return QColor("#c026d3"); // magenta
        case SymbolGroup::D: return QColor("#16a34a"); // green
        case SymbolGroup::E: return QColor("#7c3aed"); // purple
        case SymbolGroup::F: return QColor("#dc2626"); // red
        case SymbolGroup::G: return QColor("#eab308"); // yellow
        case SymbolGroup::H: return QColor("#ea580c"); // orange
        case SymbolGroup::I: return QColor("#0d9488"); // teal
        case SymbolGroup::J: return QColor("#db2777"); // pink
        case SymbolGroup::None: return QColor("#4b5563");
    }
    return QColor("#4b5563");
}

bool SymbolGroupRegistry::default_enabled(SymbolGroup g) {
    // A..G are on by default; H/I/J are spare slots the user opts into.
    switch (g) {
        case SymbolGroup::A:
        case SymbolGroup::B:
        case SymbolGroup::C:
        case SymbolGroup::D:
        case SymbolGroup::E:
        case SymbolGroup::F:
        case SymbolGroup::G:
            return true;
        default:
            return false;
    }
}

void SymbolGroupRegistry::load() {
    QSettings s;
    for (SymbolGroup g : all_symbol_groups()) {
        Slot slot;
        slot.name    = s.value(settings_key_for_active_profile(g, "name"), default_name(g)).toString();
        const QString color_str = s.value(settings_key_for_active_profile(g, "color"), default_color(g).name()).toString();
        slot.color   = QColor(color_str);
        if (!slot.color.isValid())
            slot.color = default_color(g);
        slot.enabled = s.value(settings_key_for_active_profile(g, "enabled"), default_enabled(g)).toBool();
        slots_.insert(g, std::move(slot));
    }
}

void SymbolGroupRegistry::save_slot(SymbolGroup g) const {
    auto it = slots_.constFind(g);
    if (it == slots_.constEnd())
        return;
    QSettings s;
    s.setValue(settings_key_for_active_profile(g, "name"), it->name);
    s.setValue(settings_key_for_active_profile(g, "color"), it->color.name());
    s.setValue(settings_key_for_active_profile(g, "enabled"), it->enabled);
}

QString SymbolGroupRegistry::name(SymbolGroup g) const {
    auto it = slots_.constFind(g);
    return it != slots_.constEnd() ? it->name : default_name(g);
}

QColor SymbolGroupRegistry::color(SymbolGroup g) const {
    auto it = slots_.constFind(g);
    return it != slots_.constEnd() ? it->color : default_color(g);
}

bool SymbolGroupRegistry::enabled(SymbolGroup g) const {
    if (g == SymbolGroup::None)
        return true; // None is always selectable (= unlinked)
    auto it = slots_.constFind(g);
    return it != slots_.constEnd() ? it->enabled : default_enabled(g);
}

void SymbolGroupRegistry::set_name(SymbolGroup g, const QString& name) {
    if (g == SymbolGroup::None)
        return;
    auto& slot = slots_[g];
    const QString trimmed = name.trimmed();
    const QString resolved = trimmed.isEmpty() ? default_name(g) : trimmed;
    if (slot.name == resolved)
        return;
    slot.name = resolved;
    save_slot(g);
    emit group_metadata_changed(g);
}

void SymbolGroupRegistry::set_color(SymbolGroup g, const QColor& color) {
    if (g == SymbolGroup::None || !color.isValid())
        return;
    auto& slot = slots_[g];
    if (slot.color == color)
        return;
    slot.color = color;
    save_slot(g);
    emit group_metadata_changed(g);
}

void SymbolGroupRegistry::set_enabled(SymbolGroup g, bool en) {
    if (g == SymbolGroup::None)
        return;
    auto& slot = slots_[g];
    if (slot.enabled == en)
        return;
    slot.enabled = en;
    save_slot(g);
    emit group_metadata_changed(g);
}

QList<SymbolGroup> SymbolGroupRegistry::enabled_groups() const {
    QList<SymbolGroup> out;
    out.reserve(10);
    for (SymbolGroup g : all_symbol_groups()) {
        if (enabled(g))
            out.append(g);
    }
    return out;
}

void SymbolGroupRegistry::reset_to_default(SymbolGroup g) {
    if (g == SymbolGroup::None)
        return;
    auto& slot = slots_[g];
    slot.name    = default_name(g);
    slot.color   = default_color(g);
    slot.enabled = default_enabled(g);
    save_slot(g);
    emit group_metadata_changed(g);
}

SymbolGroup SymbolGroupRegistry::next_enabled(SymbolGroup current) const {
    const QList<SymbolGroup> enabled_list = enabled_groups();
    if (enabled_list.isEmpty())
        return SymbolGroup::None;

    // Cycle: None → first enabled → next enabled → … → last enabled → None.
    if (current == SymbolGroup::None)
        return enabled_list.first();

    const int idx = enabled_list.indexOf(current);
    if (idx < 0)
        // current is a disabled slot — treat like None and start from beginning.
        return enabled_list.first();
    if (idx == enabled_list.size() - 1)
        return SymbolGroup::None;
    return enabled_list.at(idx + 1);
}

} // namespace fincept
