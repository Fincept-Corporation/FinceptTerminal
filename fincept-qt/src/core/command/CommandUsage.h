#pragma once
// CommandUsage — what the user picked in the search box: recent picks, how often,
// pins. Feeds ranking (CommandRank::usage_boost) and the empty-box RECENT / PINNED
// lists. Pure (Qt Core, no I/O): the box loads / saves it as one JSON document in
// the profile's settings (key "search/usage", see the box). tests/tst_command_rank.
//
// Privacy: only what a ROW is (id, title, group and a small payload such as the
// picked instrument) is kept — never the text typed, never order details (ticket
// rows are not recorded), nothing leaves the computer. Capped: kMaxEntries rows,
// the least recently used unpinned ones go first; pins are kept (max kMaxPins).

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantMap>

#include "core/command/CommandRank.h"

namespace fincept::command {

class CommandUsage {
  public:
    static constexpr int kMaxEntries = 150;
    static constexpr int kMaxPins = 24;
    static constexpr int kFormatVersion = 1;

    struct Entry {
        QString id;          ///< row id ("screen:markets", "instrument:US:AAPL", "settings.appearance.theme")
        QString title;       ///< as shown when picked (recent list label)
        QString group;       ///< groups:: id
        QString subtitle;    ///< as shown when picked
        QVariantMap payload; ///< what re-opening needs without a lookup (instrument JSON, screen id …)
        int count = 0;
        qint64 last_used_ms = 0;
        bool pinned = false;
        qint64 pinned_ms = 0;
    };

    /// A pick: count + 1, now = most recent. Title / subtitle / payload refreshed.
    void record(const Entry& row, qint64 now_ms);
    /// Pin / unpin (a pin of an unknown row adds it with count 0). Returns false when the pin cap is reached.
    bool set_pinned(const Entry& row, bool pinned, qint64 now_ms);
    bool is_pinned(const QString& id) const;
    void forget(const QString& id);
    void clear_history(); ///< drops every unpinned row and the counts / dates of pinned ones

    rank::Usage usage(const QString& id) const;
    /// Unpinned rows, most recent first.
    QList<Entry> recent(int limit) const;
    /// Pinned rows, in pin order.
    QList<Entry> pinned() const;
    int size() const { return static_cast<int>(entries_.size()); }
    bool contains(const QString& id) const;

    QJsonObject to_json() const;
    /// Unknown versions / malformed rows are skipped (never an error).
    static CommandUsage from_json(const QJsonObject& doc);

  private:
    QList<Entry> entries_; ///< any order; small (<= kMaxEntries)
    int index_of(const QString& id) const;
    void prune();
};

} // namespace fincept::command
