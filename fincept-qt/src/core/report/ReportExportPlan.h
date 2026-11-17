#pragma once
// ReportExportPlan.h — the chunking rules of Report Builder exports (pure, Qt Core).
//
// QTextDocument layout and painting must stay on the GUI thread, so an export does not
// run in a worker: it runs in SLICES — a little work per event-loop turn (build a few
// blocks, lay out a few hundred paragraphs, paint one page), so the window keeps
// responding, shows progress and can cancel. These helpers are the bookkeeping, unit
// tested in tests/tst_report_builder_core.cpp.

#include <QVector>

#include <functional>

namespace fincept::report::exportplan {

/// One physical page of the export: section (document between page breaks), the page
/// inside it, and its 1-based number in the whole report.
struct PageRef {
    int section = 0;
    int page = 0;
    int number = 1;
};

/// Pages of every section, in print order. A section always has at least one page.
inline QVector<PageRef> page_sequence(const QVector<int>& pages_per_section) {
    QVector<PageRef> out;
    int number = 1;
    for (int s = 0; s < pages_per_section.size(); ++s) {
        const int n = pages_per_section[s] < 1 ? 1 : pages_per_section[s];
        for (int p = 0; p < n; ++p)
            out.append({s, p, number++});
    }
    return out;
}

/// Run `step` (returns false when there is nothing left) until the work is done or
/// `budget_ms` has passed — always at least one step, so a slow step (one big chart, one
/// dense page) still makes progress every turn. `elapsed_ms` is the clock (injected for
/// tests). Returns true when the work is finished.
inline bool run_slice(const std::function<bool()>& step, int budget_ms, const std::function<qint64()>& elapsed_ms) {
    do {
        if (!step())
            return true;
    } while (elapsed_ms() < budget_ms);
    return false;
}

/// Percent for a progress bar over the three phases (build 0-30, layout 30-50, write 50-100).
inline int overall_percent(int phase, int done, int total) {
    const int t = total <= 0 ? 1 : total;
    const int d = done < 0 ? 0 : done > t ? t : done;
    static const int from[] = {0, 30, 50, 100};
    const int p = phase < 0 ? 0 : phase > 2 ? 2 : phase;
    return from[p] + (from[p + 1] - from[p]) * d / t;
}

} // namespace fincept::report::exportplan
