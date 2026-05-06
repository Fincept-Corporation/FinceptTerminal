#pragma once
#include <QChar>
#include <QList>
#include <QMetaType>
#include <QString>

namespace fincept {

/// Symbol link groups. None = unlinked; A–J are ten slots (A–G enabled by default).
/// Stored as a single byte for QSettings/JSON; do NOT renumber — persisted layouts reference these literals.
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

/// All slots A..J (excludes None, includes disabled). For active-only, see SymbolGroupRegistry::enabled_groups().
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

/// Cycle forward ignoring enabled/disabled — stable total ordering. For UI cycling, use SymbolGroupRegistry.
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
