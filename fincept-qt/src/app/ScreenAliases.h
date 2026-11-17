#pragma once
// ScreenAliases — screen ids that live on as aliases of another screen.
//
// Why: a merged tab must not break what still names the old one — saved layouts,
// workspaces and dock states, the last screen of a window, toolbar / F-key /
// menu entries, the command registry, MCP ui_open, docs links. The router maps
// an alias to its screen (DockScreenRouter: navigate / tab_into / add_alongside /
// when_materialised / find_dock_widget / screen_widget …) and opens it on the
// alias's section through screens::ISectionLinked; layouts are rewritten before
// they are restored (WindowFrame / WindowFrame_Layout), so a saved panel of the
// old screen comes back as the new one — and an old layout holding BOTH panels
// collapses to one (the second entry of a single-instance screen is dropped by
// DockStateFilter).
//
//   agent_workbench -> agent_config, section "runs"   (2026-11-17: the Agent
//                      Workbench was merged into AGENT CONFIG)
//
// Header-only, Qt Core only.

#include <QByteArray>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace fincept::screen_alias {

struct Alias {
    const char* old_id;  ///< the id that is no longer a screen of its own
    const char* new_id;  ///< the screen that answers it
    const char* section; ///< ISectionLinked section the alias opens ("" = as it is)
};

inline const Alias* aliases(int* count) {
    static const Alias kAliases[] = {
        {"agent_workbench", "agent_config", "runs"},
    };
    *count = static_cast<int>(sizeof(kAliases) / sizeof(kAliases[0]));
    return kAliases;
}

/// The alias entry for `id` (also for a "<alias>#dup<N>" panel id); nullptr when
/// `id` is not an alias.
inline const Alias* find(const QString& id) {
    const qsizetype hash = id.indexOf(QLatin1Char('#'));
    const QString base = hash > 0 ? id.left(hash) : id;
    int n = 0;
    const Alias* all = aliases(&n);
    for (int i = 0; i < n; ++i) {
        if (base == QLatin1String(all[i].old_id))
            return &all[i];
    }
    return nullptr;
}

inline bool is_alias(const QString& id) {
    return find(id) != nullptr;
}

/// The screen id that answers `id`: itself, or the alias's screen (a "#dup<N>"
/// suffix is kept — single-instance screens drop such a duplicate anyway).
inline QString canonical(const QString& id) {
    const Alias* a = find(id);
    if (!a)
        return id;
    const qsizetype hash = id.indexOf(QLatin1Char('#'));
    return QString::fromLatin1(a->new_id) + (hash > 0 ? id.mid(hash) : QString());
}

/// The section an alias opens its screen on ("" for a non-alias).
inline QString section_for(const QString& id) {
    const Alias* a = find(id);
    return a ? QString::fromLatin1(a->section) : QString();
}

/// Every alias id (screen lists that should still find the old name).
inline QStringList alias_ids() {
    QStringList out;
    int n = 0;
    const Alias* all = aliases(&n);
    for (int i = 0; i < n; ++i)
        out << QString::fromLatin1(all[i].old_id);
    return out;
}

/// A saved ADS dock state (qCompress()ed XML or plain XML) with every alias panel
/// renamed to its screen: <Widget Name="agent_workbench" …> -> Name="agent_config",
/// and an <Area Current="agent_workbench"> likewise. Re-encoded the way it came;
/// `state` itself when nothing was renamed or the blob cannot be read. `renamed`
/// receives the old names. Run DockStateFilter's without_single_instance_duplicates()
/// AFTER this: a layout holding both panels then keeps the first one only.
inline QByteArray with_aliases(const QByteArray& state, QStringList* renamed = nullptr) {
    if (state.isEmpty())
        return state;
    const bool compressed = !state.startsWith("<?xml");
    const QByteArray xml = compressed ? qUncompress(state) : state;
    if (xml.isEmpty())
        return state;
    QString text = QString::fromUtf8(xml);
    QStringList done;
    static const QRegularExpression kAttr(QStringLiteral("\\b(Name|Current)=\"([^\"#]+)(#[^\"]*)?\""));
    QString out;
    out.reserve(text.size());
    qsizetype last = 0;
    auto it = kAttr.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString id = m.captured(2);
        if (!is_alias(id))
            continue;
        out += QStringView(text).mid(last, m.capturedStart() - last);
        out += m.captured(1) + QStringLiteral("=\"") + canonical(id) + m.captured(3) + QLatin1Char('"');
        last = m.capturedEnd();
        if (!done.contains(id))
            done << id;
    }
    if (done.isEmpty())
        return state;
    out += QStringView(text).mid(last);
    if (renamed)
        *renamed += done;
    const QByteArray new_xml = out.toUtf8();
    return compressed ? qCompress(new_xml) : new_xml;
}

} // namespace fincept::screen_alias
