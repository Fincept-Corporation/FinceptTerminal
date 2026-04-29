#pragma once
#include <QChar>
#include <QList>
#include <QMetaType>
#include <QString>

namespace fincept {

/// Symbol link groups (Bloomberg Launchpad "Security Groups" equivalent).
/// A panel can be assigned to a group; all panels in the same group share
/// the same "active security" and update in lockstep when any member panel
/// publishes a symbol change.
///
/// `None` = unlinked; the panel ignores group traffic and publishes nothing.
/// A–J = ten slots. Default colour palette: A=amber, B=cyan, C=magenta,
/// D=green, E=purple, F=red, G=yellow, H=orange, I=teal, J=pink. The first
/// seven (A–G) are enabled by default; H/I/J start disabled and the user
/// activates them from the badge menu. Names and colours per slot are
/// user-editable via SymbolGroupRegistry.
///
/// Stored as a single byte so it can live inside QSettings and JSON without
/// ambiguity. Do NOT renumber — persisted workspaces reference these literals.
enum class SymbolGroup : char {
    None = '\0',
    A = 'A',
    B = 'B',
    C = 'C',
    D = 'D',
    E = 'E',
    F = 'F',
    G = 'G',
    H = 'H',
    I = 'I',
    J = 'J',
};

/// Returns all concrete slots in order (A..J); excludes None. Includes
/// disabled slots — callers that only want active groups should consult
/// SymbolGroupRegistry::enabled_groups() instead.
inline const QList<SymbolGroup>& all_symbol_groups() {
    static const QList<SymbolGroup> k = {SymbolGroup::A, SymbolGroup::B, SymbolGroup::C,
                                         SymbolGroup::D, SymbolGroup::E, SymbolGroup::F,
                                         SymbolGroup::G, SymbolGroup::H, SymbolGroup::I,
                                         SymbolGroup::J};
    return k;
}

inline QChar symbol_group_letter(SymbolGroup g) {
    return g == SymbolGroup::None ? QChar(' ') : QChar(static_cast<char>(g));
}

inline SymbolGroup symbol_group_from_char(QChar c) {
    const char ch = c.toLatin1();
    switch (ch) {
        case 'A': return SymbolGroup::A;
        case 'B': return SymbolGroup::B;
        case 'C': return SymbolGroup::C;
        case 'D': return SymbolGroup::D;
        case 'E': return SymbolGroup::E;
        case 'F': return SymbolGroup::F;
        case 'G': return SymbolGroup::G;
        case 'H': return SymbolGroup::H;
        case 'I': return SymbolGroup::I;
        case 'J': return SymbolGroup::J;
        default:  return SymbolGroup::None;
    }
}

/// Cycle forward through the static slot list, ignoring enabled/disabled
/// state. GroupBadge uses the enabled-aware variant in SymbolGroupRegistry
/// for click-cycle; this raw version exists for callers that need a stable
/// total ordering (tests, deterministic enumeration).
inline SymbolGroup next_symbol_group(SymbolGroup g) {
    switch (g) {
        case SymbolGroup::None: return SymbolGroup::A;
        case SymbolGroup::A:    return SymbolGroup::B;
        case SymbolGroup::B:    return SymbolGroup::C;
        case SymbolGroup::C:    return SymbolGroup::D;
        case SymbolGroup::D:    return SymbolGroup::E;
        case SymbolGroup::E:    return SymbolGroup::F;
        case SymbolGroup::F:    return SymbolGroup::G;
        case SymbolGroup::G:    return SymbolGroup::H;
        case SymbolGroup::H:    return SymbolGroup::I;
        case SymbolGroup::I:    return SymbolGroup::J;
        case SymbolGroup::J:    return SymbolGroup::None;
    }
    return SymbolGroup::None;
}

} // namespace fincept

Q_DECLARE_METATYPE(fincept::SymbolGroup)
