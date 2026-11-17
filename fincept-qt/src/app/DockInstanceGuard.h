#pragma once
// DockInstanceGuard — the pure decisions behind "one screen per dock id".
//
// Why: one click on EQUITY built TWO Equity Trading screens, stacked in one
// panel. DockScreenRouter::materialize_screen() recorded a screen only after
// its factory had returned AND the screen had been shown; the first show of a
// screen runs arbitrary code (account focus, data subscriptions, a chart that
// creates a QWebEngineView), and anything in that window that re-showed the
// dock widget (visibilityChanged(true) -> materialize_screen) or delivered a
// navigate for the same id built a second instance into the same content host.
//
// The router now asks these functions what to do; they hold no state and need
// no widgets, so every branch is unit-tested (tests/tst_dock_instance_guard.cpp).
// Header-only, Qt Core only.

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace fincept::dock_instance {

/// "watchlist#dup3" -> "watchlist"; an id without a "#" suffix is its own type.
inline QString base_type(const QString& dock_id) {
    const qsizetype hash = dock_id.indexOf(QLatin1Char('#'));
    return hash > 0 ? dock_id.left(hash) : dock_id;
}

/// Screen types a window may hold more than one panel of ("Duplicate Panel"
/// makes "<type>#dup<N>"). Read-only data and analysis views whose per-panel
/// state is keyed by the panel's own id, so two copies can show two tickers,
/// series or portfolios side by side.
///
/// Every other type — and any type added later that is not listed here — is
/// single-instance: one panel per window. That covers the screens holding a
/// live order ticket or engine (equity_trading, crypto_trading, fno,
/// algo_trading, alpha_arena), the editors whose two copies would overwrite each
/// other's autosave (settings, profile, notes, report_builder, node_editor,
/// agent_config, agent_workbench, mcp_servers, data_sources, data_mapping),
/// the per-window hosts (dashboard, ai_chat, file_manager) and the static pages.
inline const QStringList& multi_instance_types() {
    static const QStringList kTypes = {
        QStringLiteral("markets"),          QStringLiteral("watchlist"),       QStringLiteral("news"),
        QStringLiteral("equity_research"),  QStringLiteral("screener"),        QStringLiteral("portfolio"),
        QStringLiteral("economics"),        QStringLiteral("dbnomics"),        QStringLiteral("akshare"),
        QStringLiteral("asia_markets"),     QStringLiteral("gov_data"),        QStringLiteral("geopolitics"),
        QStringLiteral("maritime"),         QStringLiteral("polymarket"),      QStringLiteral("relationship_map"),
        QStringLiteral("derivatives"),      QStringLiteral("surface_analytics"), QStringLiteral("calendars"),
        QStringLiteral("crypto_center"),    QStringLiteral("alt_investments"), QStringLiteral("ma_analytics"),
        QStringLiteral("quantlib"),         QStringLiteral("ai_quant_lab"),    QStringLiteral("trade_viz"),
        QStringLiteral("backtesting"),      QStringLiteral("code_editor"),     QStringLiteral("excel"),
    };
    return kTypes;
}

inline bool is_multi_instance_type(const QString& dock_id) {
    return multi_instance_types().contains(base_type(dock_id));
}

/// The default: true for every type not listed in multi_instance_types().
inline bool is_single_instance_type(const QString& dock_id) {
    return !dock_id.isEmpty() && !is_multi_instance_type(dock_id);
}

/// Screens whose second copy would put two order tickets for one account on
/// screen (the reason shown on the disabled "Duplicate Panel" entry).
inline bool holds_order_ticket(const QString& dock_id) {
    const QString base = base_type(dock_id);
    return base == QLatin1String("equity_trading") || base == QLatin1String("crypto_trading") ||
           base == QLatin1String("fno");
}

// ── Building a screen ────────────────────────────────────────────────────────

enum class Build {
    Create,   ///< run the factory
    Existing, ///< the screen is built: use it
    Join,     ///< a build of this id is running further up the stack: do nothing,
              ///< that build finishes (and delivers any queued work) itself
};

inline Build materialise_decision(bool registered, bool build_in_flight) {
    if (registered)
        return Build::Existing;
    return build_in_flight ? Build::Join : Build::Create;
}

// ── Navigating to a screen ───────────────────────────────────────────────────

enum class Nav {
    Proceed, ///< nothing in flight for this id: place, show, build
    Join,    ///< navigate() for this id is running further up the stack — it
             ///< places, shows and builds the panel, so this call adds nothing
    Defer,   ///< only a build of this id is running (a hidden build, a layout
             ///< restore): replay this navigate once that build has finished
};

inline Nav navigate_decision(bool navigating, bool building) {
    if (navigating)
        return Nav::Join;
    return building ? Nav::Defer : Nav::Proceed;
}

// ── Collapsing duplicates ────────────────────────────────────────────────────

/// One instance found for a dock id (a screen inside a panel's content host) or
/// one panel found for a single-instance type.
struct Instance {
    QString dock_id;
    bool registered = false; ///< the one the router records for the id
    bool open = false;       ///< on screen (dock widget not closed)
};

/// Which of several instances survives: the one the router records; else the
/// plain id over a "#dup<N>" copy; else an open one; else the first. -1 when
/// `found` is empty.
inline int keeper(const QList<Instance>& found) {
    if (found.isEmpty())
        return -1;
    for (int i = 0; i < found.size(); ++i)
        if (found.at(i).registered)
            return i;
    int best = 0;
    auto rank = [](const Instance& in) {
        const bool plain = !in.dock_id.contains(QLatin1Char('#'));
        return (plain ? 2 : 0) + (in.open ? 1 : 0);
    };
    for (int i = 1; i < found.size(); ++i)
        if (rank(found.at(i)) > rank(found.at(best)))
            best = i;
    return best;
}

/// Panels (dock ids) to close so every single-instance type has at most one
/// panel in a window. `panels` lists every dock id the window holds; the keeper
/// of each single-instance type is chosen by keeper(). Multi-instance types are
/// never touched. Order of the result follows `panels`.
inline QStringList surplus_panels(const QList<Instance>& panels) {
    QStringList surplus;
    QStringList seen_types;
    for (const Instance& p : panels) {
        const QString type = base_type(p.dock_id);
        if (!is_single_instance_type(p.dock_id) || seen_types.contains(type))
            continue;
        seen_types << type;
        QList<Instance> same;
        for (const Instance& q : panels)
            if (base_type(q.dock_id) == type)
                same << q;
        if (same.size() < 2)
            continue;
        const int keep = keeper(same);
        for (int i = 0; i < same.size(); ++i)
            if (i != keep)
                surplus << same.at(i).dock_id;
    }
    // Stable: in the order the panels were given.
    QStringList ordered;
    for (const Instance& p : panels)
        if (surplus.contains(p.dock_id) && !ordered.contains(p.dock_id))
            ordered << p.dock_id;
    return ordered;
}

// ── nav.switch_screen payload ────────────────────────────────────────────────

/// Whether a nav.switch_screen request replaces the open panels.
///   "exclusive": true  — replace the current view (a clean "take me there").
///   "new_panel": true  — open beside what is there (Ctrl+Enter in the command
///                        bar); "new_panel": false asks for the replacing open.
/// With neither key the panel is added beside the current view (the default
/// since the event existed). When both are present and disagree, new_panel
/// wins: it is the more specific request, and the outcome that closes nothing.
inline bool switch_is_exclusive(const QVariantMap& payload) {
    const bool has_exclusive = payload.contains(QStringLiteral("exclusive"));
    const bool has_new_panel = payload.contains(QStringLiteral("new_panel"));
    if (has_new_panel)
        return !payload.value(QStringLiteral("new_panel")).toBool();
    if (has_exclusive)
        return payload.value(QStringLiteral("exclusive")).toBool();
    return false;
}

} // namespace fincept::dock_instance
