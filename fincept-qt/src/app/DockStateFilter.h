#pragma once
// DockStateFilter — pure helpers over a saved ADS dock-state blob (what
// CDockManager::saveState() returns: qCompress()ed XML, or plain XML).
//
// Why: a duplicate of a single-instance screen ("equity_trading#dup2", saved
// before duplication of those screens was refused) must not come back from a
// saved layout — for a trading screen that is two order tickets for one
// account. The blob is cleaned BEFORE restoreState(): the duplicate's <Widget>
// entry is removed, so ADS restores the rest of the layout untouched (an area
// left empty is dropped by ADS itself, as for any widget it cannot find) and the
// original panel stays the only one. A widget named twice in one blob (a layout
// written by an older build) keeps its first entry only.
// DockScreenRouter::ensure_dup_factory() also refuses such ids, so nothing can
// re-create one from a panel list either.
//
// Which screens are single-instance: every type not listed as multi-instance in
// DockInstanceGuard.h (the default is single).
//
// Header-only, Qt Core only; unit-tested in tst_dock_state_filter.

#include "app/DockInstanceGuard.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <functional>

namespace fincept::dock_state {

/// One panel per window: every screen type not listed as multi-instance in
/// DockInstanceGuard.h — the trading screens among them (order tickets).
inline bool is_single_instance_screen(const QString& id) {
    return dock_instance::is_single_instance_type(id);
}

/// "<single-instance base>#dup<N>".
inline bool is_single_instance_duplicate(const QString& id) {
    static const QRegularExpression kDup(QStringLiteral("#dup\\d+$"));
    return id.contains(kDup) && is_single_instance_screen(id);
}

namespace detail {
/// The XML of a state blob; `compressed` tells how it was stored. Empty on garbage.
inline QByteArray state_xml(const QByteArray& state, bool* compressed) {
    if (state.startsWith("<?xml")) {
        *compressed = false;
        return state;
    }
    *compressed = true;
    return qUncompress(state);
}
} // namespace detail

/// Names of the <Widget Name="…"> entries in a state blob, in document order.
inline QStringList widget_names(const QByteArray& state) {
    bool compressed = false;
    const QByteArray xml = detail::state_xml(state, &compressed);
    QStringList out;
    static const QRegularExpression kName(QStringLiteral("<Widget\\b[^>]*\\bName=\"([^\"]+)\""));
    auto it = kName.globalMatch(QString::fromUtf8(xml));
    while (it.hasNext())
        out << it.next().captured(1);
    return out;
}

/// `state` without the <Widget …/> entries whose name satisfies `drop`
/// (self-closing or with a body). Returns `state` itself when nothing matches
/// or the blob cannot be read; otherwise re-encoded the way it came
/// (compressed or not). `dropped` receives the removed names.
inline QByteArray without_widgets(const QByteArray& state, const std::function<bool(const QString&)>& drop,
                                  QStringList* dropped = nullptr) {
    bool compressed = false;
    const QByteArray xml = detail::state_xml(state, &compressed);
    if (xml.isEmpty())
        return state;
    QString text = QString::fromUtf8(xml);
    // <Widget Name="x" Closed="0"/>  or  <Widget Name="x" Closed="0">…</Widget>
    static const QRegularExpression kWidget(
        QStringLiteral("<Widget\\b[^>]*\\bName=\"([^\"]+)\"[^>]*?(?:/>|>.*?</Widget>)"),
        QRegularExpression::DotMatchesEverythingOption);
    QString out;
    out.reserve(text.size());
    qsizetype last = 0;
    QStringList removed;
    auto it = kWidget.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString name = m.captured(1);
        if (!drop(name))
            continue;
        out += QStringView(text).mid(last, m.capturedStart() - last);
        last = m.capturedEnd();
        removed << name;
    }
    if (removed.isEmpty())
        return state;
    out += QStringView(text).mid(last);
    if (dropped)
        *dropped += removed;
    // An <Area Current="x"> naming a removed entry is cleared: ADS then makes the
    // area's first open widget current instead of looking the dropped name up.
    // (Not for a repeated name whose first entry stays in the blob.)
    for (const QString& name : std::as_const(removed)) {
        if (out.contains(QStringLiteral("Name=\"") + name + QLatin1Char('"')))
            continue;
        out.replace(QStringLiteral("Current=\"") + name + QLatin1Char('"'), QStringLiteral("Current=\"\""));
    }
    const QByteArray new_xml = out.toUtf8();
    return compressed ? qCompress(new_xml) : new_xml;
}

/// The blob without duplicates of single-instance screens: "<type>#dup<N>"
/// copies of a single-instance type, and every repeat of a widget name already
/// seen earlier in the blob (ADS would place that one dock widget twice).
inline QByteArray without_single_instance_duplicates(const QByteArray& state, QStringList* dropped = nullptr) {
    QStringList seen;
    return without_widgets(
        state,
        [&seen](const QString& name) {
            if (is_single_instance_duplicate(name) || seen.contains(name))
                return true;
            seen << name;
            return false;
        },
        dropped);
}

} // namespace fincept::dock_state
