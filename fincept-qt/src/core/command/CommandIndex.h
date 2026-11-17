#pragma once
// CommandIndex — the in-memory, pre-normalised list of every local row the search
// box can find (screens, sub-tabs, actions, settings, help, registry rows). Built
// once and rebuilt when the registry changes; a query walks it synchronously
// (a few thousand rows: well under a millisecond). Pure (Qt Core, no QObject).
//
// ResultLayout (below) turns scored rows of several groups into the sections the
// popup shows — group order by the best hit, caps, "more", and stability: rows
// already on screen do not jump when a slower group arrives.

#include "core/command/CommandRank.h"
#include "core/command/CommandRegistry.h"
#include "core/command/CommandUsage.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace fincept::command {

class CommandIndex {
  public:
    struct Entry {
        CommandItem item;
        rank::Prepared prepared;
    };
    struct Hit {
        int entry = -1;
        rank::Match match;
        qint64 score = 0;
    };

    /// Codes a row is found by exactly: its aliases, plus the screen id for a screen's own row.
    static QStringList codes_of(const CommandItem& item);

    void rebuild(const QList<CommandItem>& items);
    /// Hits for `query`, best first (ties keep index order). `groups` empty = every group.
    QList<Hit> query(const rank::Query& query, const CommandUsage& usage, qint64 now_ms,
                     const QSet<QString>& groups = {}, int limit = 200) const;

    const Entry& entry(int i) const { return entries_[i]; }
    int size() const { return static_cast<int>(entries_.size()); }
    int find(const QString& id) const { return by_id_.value(id, -1); }
    /// The screen a typed token / title names ("mkts", "markets", "Equity Research", "equity_research");
    /// "" when none does. The old bar's resolve_screen_id: exact id, title, alias first, then the best
    /// title / alias prefix or word match (never a keyword / typo guess).
    QString screen_for(const QString& token) const;

  private:
    QList<Entry> entries_;
    QHash<QString, int> by_id_;
};

namespace layout {

struct Candidate {
    QString key;
    qint64 score = 0;
};

struct GroupInput {
    QString group;
    QList<Candidate> rows;   ///< any order; arranged best first
    bool loading = false;    ///< a slower source of this group is still working
    qint64 predicted = 0;    ///< where the group's slot goes while it has no rows yet (score of its expected best)
    int cap = 5;             ///< rows shown before "more"
    bool expanded = false;   ///< the user asked for every row
};

struct Section {
    QString group;
    QStringList keys;        ///< rows shown
    int total = 0;           ///< rows found
    bool loading = false;
    bool more = false;       ///< total > keys.size()
};

/// What was on screen for the same query (empty / same_query false for a new query).
struct Previous {
    bool same_query = false;
    QStringList group_order;
    QHash<QString, QStringList> keys; ///< group -> keys shown
};

/// Sections to show. New query: groups by their best score (a loading group by `predicted`), ties in
/// `default_order`. Same query: groups and rows already shown keep their order; new groups and rows are
/// added after them (a group that was loading keeps its slot).
QList<Section> arrange(const QList<GroupInput>& groups, const QStringList& default_order, const Previous& previous);

} // namespace layout

} // namespace fincept::command
