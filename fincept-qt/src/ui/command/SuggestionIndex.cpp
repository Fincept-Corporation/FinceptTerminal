#include "ui/command/SuggestionIndex.h"

#include "core/actions/ActionRegistry.h"
#include "core/layout/LayoutCatalog.h"

namespace fincept::ui {

SuggestionIndex& SuggestionIndex::instance() {
    static SuggestionIndex s;
    return s;
}

QList<SuggestionIndex::Match> SuggestionIndex::query(const QString& prefix, int limit) {
    QList<Match> out;
    if (prefix.trimmed().isEmpty() || limit <= 0)
        return out;

    // Action source — tier on top of ActionRegistry::match's existing
    // 4-tier relevance bucket.
    const auto action_matches = ActionRegistry::instance().match(prefix, limit);
    int score = 100;
    for (const ActionDef* def : action_matches) {
        if (!def) continue;
        Match m;
        m.source = SourceKind::Action;
        m.id = def->id;
        m.display = def->display.isEmpty() ? def->id : def->display;
        m.category = def->category;
        m.score = score--;
        out.append(m);
        if (out.size() >= limit) return out;
    }

    // Layout source — match by name (case-insensitive contains).
    auto layouts_r = LayoutCatalog::instance().list_layouts();
    if (layouts_r.is_ok()) {
        const QString p = prefix.toLower();
        for (const auto& e : layouts_r.value()) {
            if (e.name.toLower().contains(p)) {
                Match m;
                m.source = SourceKind::Layout;
                m.id = e.id.to_string();
                m.display = e.name;
                m.category = "Layout";
                m.score = 50;
                out.append(m);
                if (out.size() >= limit) return out;
            }
        }
    }

    // Phase 9 follow-up: open-panels source via PanelRegistry, profiles
    // via ProfileManager::profile_entries(), symbols via the (future)
    // symbol catalogue. Each is a slice that drops in here.

    return out;
}

} // namespace fincept::ui
