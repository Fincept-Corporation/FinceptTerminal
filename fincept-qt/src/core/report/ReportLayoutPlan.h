#pragma once
// ReportLayoutPlan.h — what the Report Builder canvas must re-lay after an edit.
//
// The canvas keeps one QTextDocument per section (the components between two
// page_break blocks) and remembers the character range every component occupies.
// After a change it compares the component list it last drew with the current one
// and re-renders only what changed: one edited block is replaced in place inside its
// section (Qt then re-lays that section from the block onwards); a block added or
// removed touches only its own section; only a structural change (page breaks moved,
// several blocks moved at once) rebuilds everything. Pure logic, Qt Core only, so the
// planner is unit-tested without any UI (tests/tst_report_builder_core.cpp).

#include "core/report/ReportDocument.h"

#include <QSet>
#include <QVector>

namespace fincept::report::layout {

/// Components [first, last) of one section; page_break blocks are the boundaries and
/// belong to no section. A document always has at least one (possibly empty) section.
struct Section {
    int first = 0;
    int last = 0;
};

inline QVector<Section> split_sections(const QVector<ReportComponent>& comps) {
    QVector<Section> out;
    Section cur;
    for (int i = 0; i < comps.size(); ++i) {
        if (comps[i].type == QLatin1String("page_break")) {
            cur.last = i;
            out.append(cur);
            cur.first = i + 1;
        }
    }
    cur.last = static_cast<int>(comps.size());
    out.append(cur);
    return out;
}

/// Section index holding component `index` (a page break counts for the section it ends).
inline int section_of(const QVector<Section>& sections, int index) {
    for (int s = 0; s < sections.size(); ++s) {
        if (index < sections[s].last || (index == sections[s].last && s + 1 < sections.size()))
            return s;
    }
    return sections.isEmpty() ? 0 : static_cast<int>(sections.size()) - 1;
}

struct UpdatePlan {
    enum class Kind { None, Replace, Insert, Remove, Full };
    Kind kind = Kind::None;
    QVector<int> replace_ids; ///< Replace: components to re-render in place
    int inserted_index = -1;  ///< Insert: index (in the new list) of the added component
    int removed_id = 0;       ///< Remove: id of the removed component
    bool toc = false;         ///< headings changed: TOC blocks must be re-rendered too
    QSet<int> sections;       ///< sections (of the new list) whose layout changes
};

inline bool same_component(const ReportComponent& a, const ReportComponent& b) {
    return a.id == b.id && a.type == b.type && a.content == b.content && a.config == b.config;
}

inline bool headings_differ(const QVector<ReportComponent>& a, const QVector<ReportComponent>& b) {
    QStringList ha, hb;
    for (const auto& c : a)
        if (c.type == QLatin1String("heading"))
            ha << c.content;
    for (const auto& c : b)
        if (c.type == QLatin1String("heading"))
            hb << c.content;
    return ha != hb;
}

inline bool has_toc(const QVector<ReportComponent>& comps) {
    for (const auto& c : comps)
        if (c.type == QLatin1String("toc"))
            return true;
    return false;
}

inline UpdatePlan plan_update(const QVector<ReportComponent>& before, const QVector<ReportComponent>& after) {
    UpdatePlan plan;
    const QVector<Section> sections = split_sections(after);
    const bool toc = has_toc(after) && headings_differ(before, after);

    if (before.size() == after.size()) {
        bool same_order = true;
        for (int i = 0; i < before.size() && same_order; ++i)
            same_order = before[i].id == after[i].id;
        if (same_order) {
            for (int i = 0; i < after.size(); ++i) {
                if (same_component(before[i], after[i]))
                    continue;
                // A block turning into / out of a page break changes the sections.
                if (before[i].type != after[i].type &&
                    (before[i].type == QLatin1String("page_break") || after[i].type == QLatin1String("page_break"))) {
                    plan.kind = UpdatePlan::Kind::Full;
                    return plan;
                }
                plan.replace_ids.append(after[i].id);
                plan.sections.insert(section_of(sections, i));
            }
            if (plan.replace_ids.isEmpty() && !toc)
                return plan; // nothing to do
            plan.kind = UpdatePlan::Kind::Replace;
            plan.toc = toc;
            if (toc)
                for (int i = 0; i < after.size(); ++i)
                    if (after[i].type == QLatin1String("toc"))
                        plan.sections.insert(section_of(sections, i));
            return plan;
        }
    }

    auto equal_except = [](const QVector<ReportComponent>& longer, const QVector<ReportComponent>& shorter, int skip) {
        for (int i = 0, j = 0; i < longer.size(); ++i) {
            if (i == skip)
                continue;
            if (!same_component(longer[i], shorter[j++]))
                return false;
        }
        return true;
    };

    if (after.size() == before.size() + 1) {
        int at = 0;
        while (at < before.size() && before[at].id == after[at].id)
            ++at;
        if (after[at].type != QLatin1String("page_break") && equal_except(after, before, at)) {
            plan.kind = UpdatePlan::Kind::Insert;
            plan.inserted_index = at;
            plan.sections.insert(section_of(sections, at));
            plan.toc = toc;
            return plan;
        }
    } else if (before.size() == after.size() + 1) {
        int at = 0;
        while (at < after.size() && before[at].id == after[at].id)
            ++at;
        if (before[at].type != QLatin1String("page_break") && equal_except(before, after, at)) {
            plan.kind = UpdatePlan::Kind::Remove;
            plan.removed_id = before[at].id;
            plan.sections.insert(section_of(sections, qMin(at, static_cast<int>(after.size()))));
            plan.toc = toc;
            return plan;
        }
    }
    plan.kind = UpdatePlan::Kind::Full;
    return plan;
}

} // namespace fincept::report::layout
