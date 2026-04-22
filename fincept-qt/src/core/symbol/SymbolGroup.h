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
/// A–F = six colour-coded groups (Amber, Cyan, Magenta, Green, Purple, Red).
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
};

/// Returns all concrete groups in order (A..F); excludes None.
inline const QList<SymbolGroup>& all_symbol_groups() {
    static const QList<SymbolGroup> k = {SymbolGroup::A, SymbolGroup::B, SymbolGroup::C,
                                         SymbolGroup::D, SymbolGroup::E, SymbolGroup::F};
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
        default:  return SymbolGroup::None;
    }
}

/// Cycle forward: None → A → B → … → F → None.
inline SymbolGroup next_symbol_group(SymbolGroup g) {
    switch (g) {
        case SymbolGroup::None: return SymbolGroup::A;
        case SymbolGroup::A:    return SymbolGroup::B;
        case SymbolGroup::B:    return SymbolGroup::C;
        case SymbolGroup::C:    return SymbolGroup::D;
        case SymbolGroup::D:    return SymbolGroup::E;
        case SymbolGroup::E:    return SymbolGroup::F;
        case SymbolGroup::F:    return SymbolGroup::None;
    }
    return SymbolGroup::None;
}

} // namespace fincept

Q_DECLARE_METATYPE(fincept::SymbolGroup)
