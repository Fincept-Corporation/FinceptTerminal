#include "core/components/PopularityTracker.h"

#include <QSettings>

namespace fincept {

static constexpr const char* kOrg = "Fincept";
static constexpr const char* kApp = "FinceptTerminal";
static constexpr const char* kGroup = "component_usage";

PopularityTracker& PopularityTracker::instance() {
    static PopularityTracker s;
    return s;
}

PopularityTracker::PopularityTracker() {
    load();
}

void PopularityTracker::load() {
    QSettings settings(kOrg, kApp);
    settings.beginGroup(kGroup);
    const QStringList keys = settings.childKeys();
    for (const QString& k : keys) {
        const int n = settings.value(k, 0).toInt();
        if (n > 0)
            counts_[k] = n;
    }
    settings.endGroup();
}

void PopularityTracker::save(const QString& id, int count) {
    QSettings settings(kOrg, kApp);
    settings.beginGroup(kGroup);
    settings.setValue(id, count);
    settings.endGroup();
}

void PopularityTracker::increment(const QString& id) {
    if (id.isEmpty())
        return;
    const int next = counts_.value(id, 0) + 1;
    counts_[id] = next;
    save(id, next);
    emit usage_changed(id, next);
}

int PopularityTracker::use_count(const QString& id) const {
    return counts_.value(id, 0);
}

void PopularityTracker::clear_all() {
    counts_.clear();
    QSettings settings(kOrg, kApp);
    settings.beginGroup(kGroup);
    settings.remove("");
    settings.endGroup();
}

} // namespace fincept
