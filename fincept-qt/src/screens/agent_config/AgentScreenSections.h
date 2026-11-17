#pragma once
// AgentScreenSections.h — the stable section ids of the agents screen
// (screen id "agent_config", which also answers the old "agent_workbench") and
// the pure rules around them: which view group a section belongs to, which
// other words open it, and how the state shapes of the two old tabs map onto
// the merged one.
//
// Header-only, Qt Core only — no widgets. Used by AgentConfigScreen (open_section,
// restore_state) and readable by the command registry / tests.

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace fincept::screens::agent_sections {

// ── Sections (stable, lower-case; the command bar's "section") ──────────────
inline constexpr const char* kAgents = "agents";       ///< agent editor: model, prompt, tools, features, test
inline constexpr const char* kCreate = "create";       ///< agent builder, saved list, import / export
inline constexpr const char* kTeams = "teams";
inline constexpr const char* kWorkflows = "workflows";
inline constexpr const char* kPlanner = "planner";
inline constexpr const char* kTools = "tools";
inline constexpr const char* kRuns = "runs";           ///< the one run list (every engine)
inline constexpr const char* kRun = "run";             ///< NEW RUN: the coding-agent composer
inline constexpr const char* kChat = "chat";
inline constexpr const char* kSchedules = "schedules"; ///< recurring harness runs
inline constexpr const char* kHarnesses = "harnesses";
inline constexpr const char* kConnect = "connect";     ///< the terminal's MCP server for external agents
inline constexpr const char* kSystem = "system";

// ── View groups (the command bar's view switch) ──────────────────────────────
inline constexpr const char* kGroupBuild = "build";
inline constexpr const char* kGroupRun = "run";
inline constexpr const char* kGroupConnect = "connect";

/// Every section, in tab order inside its group.
inline const QStringList& all_sections() {
    static const QStringList ids = {kAgents,   kCreate, kTeams, kWorkflows, kPlanner,   kTools,   kRuns,
                                    kRun,      kChat,   kSchedules,         kHarnesses, kConnect, kSystem};
    return ids;
}

inline QString group_of(const QString& section) {
    if (section == QLatin1String(kRuns) || section == QLatin1String(kRun) || section == QLatin1String(kChat) ||
        section == QLatin1String(kSchedules))
        return QString::fromLatin1(kGroupRun);
    if (section == QLatin1String(kHarnesses) || section == QLatin1String(kConnect) ||
        section == QLatin1String(kSystem))
        return QString::fromLatin1(kGroupConnect);
    return QString::fromLatin1(kGroupBuild);
}

inline QStringList sections_of(const QString& group) {
    QStringList out;
    for (const QString& s : all_sections())
        if (group_of(s) == group)
            out << s;
    return out;
}

inline bool is_section(const QString& id) {
    return all_sections().contains(id);
}

/// What open_section(section, focus) resolves to.
struct Target {
    QString section;     ///< a section id; empty = unknown (stay where the screen is)
    QString focus;       ///< what to select inside it (agent / team / harness id, run key)
    QString engine;      ///< runs: an engine filter to apply ("agentic", "harness:<id>" …)
    bool templates = false; ///< run: open the starter-prompt list
};

/// Resolves a requested section (+ focus) — the merged ids, the old tabs' ids, the
/// command bar's "kind:id" forms and the words of the agent editor's parts:
///   overview / model / prompt / instructions / permissions / memory / features / agent -> agents
///   new_run / composer / templates -> run;  library / history / logs -> runs
///   agentic / tasks -> runs with the AGENTIC filter;  workbench -> runs
///   agent:<id>  team:<id>  harness:<id>  run:<key>  engine:<id>
inline Target resolve(const QString& section_in, const QString& focus = {}) {
    const QString section = section_in.trimmed().toLower();
    Target t;
    t.focus = focus;
    const qsizetype colon = section.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        const QString kind = section.left(colon);
        const QString id = section_in.trimmed().mid(colon + 1); // ids keep their case
        if (kind == QLatin1String("agent"))
            t.section = QString::fromLatin1(kAgents);
        else if (kind == QLatin1String("team"))
            t.section = QString::fromLatin1(kTeams);
        else if (kind == QLatin1String("harness"))
            t.section = QString::fromLatin1(kHarnesses);
        else if (kind == QLatin1String("run"))
            t.section = QString::fromLatin1(kRuns);
        else if (kind == QLatin1String("engine")) {
            t.section = QString::fromLatin1(kRuns);
            t.engine = id;
            return t;
        } else
            return t; // unknown kind
        if (t.focus.isEmpty())
            t.focus = id;
        return t;
    }
    if (is_section(section)) {
        t.section = section;
        return t;
    }
    static const QStringList editor_words = {QStringLiteral("overview"),    QStringLiteral("model"),
                                             QStringLiteral("prompt"),      QStringLiteral("instructions"),
                                             QStringLiteral("permissions"), QStringLiteral("memory"),
                                             QStringLiteral("features"),    QStringLiteral("agent")};
    if (editor_words.contains(section)) {
        t.section = QString::fromLatin1(kAgents);
    } else if (section == QLatin1String("new_run") || section == QLatin1String("composer")) {
        t.section = QString::fromLatin1(kRun);
    } else if (section == QLatin1String("templates")) {
        t.section = QString::fromLatin1(kRun);
        t.templates = true;
    } else if (section == QLatin1String("library") || section == QLatin1String("history") ||
               section == QLatin1String("logs") || section == QLatin1String("workbench")) {
        t.section = QString::fromLatin1(kRuns);
    } else if (section == QLatin1String("agentic") || section == QLatin1String("tasks")) {
        t.section = QString::fromLatin1(kRuns);
        t.engine = QStringLiteral("agentic");
    }
    return t;
}

// ── The old tabs' saved state ────────────────────────────────────────────────

/// AGENT CONFIG saved its sub-view as services::AgentViewMode's int
/// (Agents, Create, Teams, Workflows, Planner, Tools, Chat, System, Agentic).
inline QString section_for_legacy_view(int view) {
    static const char* const kByView[] = {kAgents, kCreate, kTeams, kWorkflows, kPlanner,
                                          kTools,  kChat,   kSystem, kRuns};
    if (view < 0 || view >= static_cast<int>(sizeof(kByView) / sizeof(kByView[0])))
        return {};
    return QString::fromLatin1(kByView[view]);
}

/// AGENT WORKBENCH saved "tab_id" (run, runs, schedules, harnesses, connect) or, in
/// its first shape, "tab" by index (RUN, RUNS, SCHEDULES, CONNECT). The workbench's
/// RUN tab showed the composer beside the run, so it opens NEW RUN.
inline QString section_for_workbench_state(const QVariantMap& state) {
    const QString id = state.value(QStringLiteral("tab_id")).toString();
    if (id == QLatin1String("run"))
        return QString::fromLatin1(kRun);
    if (id == QLatin1String("runs"))
        return QString::fromLatin1(kRuns);
    if (id == QLatin1String("schedules"))
        return QString::fromLatin1(kSchedules);
    if (id == QLatin1String("harnesses"))
        return QString::fromLatin1(kHarnesses);
    if (id == QLatin1String("connect"))
        return QString::fromLatin1(kConnect);
    if (!id.isEmpty())
        return QString::fromLatin1(kRuns);
    switch (state.value(QStringLiteral("tab"), 1).toInt()) {
        case 0:
            return QString::fromLatin1(kRun);
        case 2:
            return QString::fromLatin1(kSchedules);
        case 3:
            return QString::fromLatin1(kConnect);
        default:
            break;
    }
    return QString::fromLatin1(kRuns);
}

/// Which saved shape a state map has.
enum class StateShape { Empty, Merged, AgentConfigV1, WorkbenchV1 };

inline StateShape state_shape(const QVariantMap& state) {
    if (state.isEmpty())
        return StateShape::Empty;
    if (state.contains(QStringLiteral("section")))
        return StateShape::Merged;
    if (state.contains(QStringLiteral("tab_id")) || state.contains(QStringLiteral("tab")) ||
        (state.contains(QStringLiteral("run")) && !state.contains(QStringLiteral("view"))))
        return StateShape::WorkbenchV1;
    return StateShape::AgentConfigV1;
}

/// The section a saved state opens on (any of the three shapes); "" = the default.
inline QString section_for_state(const QVariantMap& state) {
    switch (state_shape(state)) {
        case StateShape::Merged: {
            const QString s = state.value(QStringLiteral("section")).toString();
            return is_section(s) ? s : QString();
        }
        case StateShape::WorkbenchV1:
            return section_for_workbench_state(state);
        case StateShape::AgentConfigV1:
            return section_for_legacy_view(state.value(QStringLiteral("view"), -1).toInt());
        case StateShape::Empty:
            break;
    }
    return {};
}

/// The composer / inspector part of a saved state: the workbench kept it under
/// "run" ({harness, model, effort, permission, tools, budget, time_limit_s,
/// max_turns, run_id}); the merged state keeps the same map under "run".
inline QVariantMap run_part_of_state(const QVariantMap& state) {
    return state.value(QStringLiteral("run")).toMap();
}

/// Carry-over of the old Workbench's screen-state row: decides whether it still
/// has to be carried (once; never over a merged state that already holds a
/// composer of its own).
inline bool should_carry_workbench_state(bool already_carried, const QVariantMap& workbench_state,
                                         const QVariantMap& merged_state) {
    if (already_carried || workbench_state.isEmpty())
        return false;
    if (state_shape(workbench_state) != StateShape::WorkbenchV1)
        return false;
    return run_part_of_state(merged_state).isEmpty();
}

} // namespace fincept::screens::agent_sections
