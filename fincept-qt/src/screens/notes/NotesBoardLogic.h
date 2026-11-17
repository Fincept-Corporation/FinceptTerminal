// src/screens/notes/NotesBoardLogic.h
//
// The pure half of the Notes BOARD — Qt Core only, no widgets, no database — so
// it runs on the screen's worker thread and is unit tested
// (tests/tst_notes_board_logic.cpp):
//
//   * grouping: which lane a card is in (status, category, ticker, priority, tag,
//     date) and the lane order (fixed workflows first, saved order applied),
//   * board filters (pinned, has ticker, due, linked, priority, status, category,
//     tag, ticker, edited within N days, stale) and named boards (saved views) as
//     JSON,
//   * the board layout (lanes of sorted cards) and the ribbon statistics,
//   * manual order: ranks between neighbours, renumbering when they run out,
//   * drag-move rules (what may be dropped where) and what a move changes,
//   * the command field ("new trade idea AAPL", "group by ticker", "open …").
#pragma once

#include "screens/notes/NotesLogic.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <optional>

namespace fincept::screens::notes {

namespace nb = fincept::notes_board;

// ── Grouping / view keys ─────────────────────────────────────────────────────

enum class GroupBy { Status = 0, Category = 1, Ticker = 2, Priority = 3, Tag = 4, Date = 5 };
enum class BoardViewMode { Board = 0, Wall = 1, List = 2 };

inline QString group_by_key(GroupBy g) {
    switch (g) {
    case GroupBy::Status:
        return QStringLiteral("status");
    case GroupBy::Category:
        return QStringLiteral("category");
    case GroupBy::Ticker:
        return QStringLiteral("ticker");
    case GroupBy::Priority:
        return QStringLiteral("priority");
    case GroupBy::Tag:
        return QStringLiteral("tag");
    case GroupBy::Date:
        return QStringLiteral("date");
    }
    return QStringLiteral("status");
}

/// "status", "category"/"cat", "ticker"/"tickers"/"symbol", "priority"/"prio",
/// "tag"/"tags", "date"/"time"/"edited"; anything else -> `fallback`.
inline GroupBy group_by_from_key(const QString& key, GroupBy fallback = GroupBy::Status) {
    const QString k = key.trimmed().toLower();
    if (k == QLatin1String("status") || k == QLatin1String("workflow") || k == QLatin1String("stage"))
        return GroupBy::Status;
    if (k == QLatin1String("category") || k == QLatin1String("cat") || k == QLatin1String("categories"))
        return GroupBy::Category;
    if (k == QLatin1String("ticker") || k == QLatin1String("tickers") || k == QLatin1String("symbol") ||
        k == QLatin1String("symbols"))
        return GroupBy::Ticker;
    if (k == QLatin1String("priority") || k == QLatin1String("prio") || k == QLatin1String("priorities"))
        return GroupBy::Priority;
    if (k == QLatin1String("tag") || k == QLatin1String("tags"))
        return GroupBy::Tag;
    if (k == QLatin1String("date") || k == QLatin1String("time") || k == QLatin1String("edited") ||
        k == QLatin1String("dates"))
        return GroupBy::Date;
    return fallback;
}

inline QString view_mode_key(BoardViewMode v) {
    switch (v) {
    case BoardViewMode::Board:
        return QStringLiteral("board");
    case BoardViewMode::Wall:
        return QStringLiteral("wall");
    case BoardViewMode::List:
        return QStringLiteral("list");
    }
    return QStringLiteral("board");
}

inline BoardViewMode view_mode_from_key(const QString& key, BoardViewMode fallback = BoardViewMode::Board) {
    const QString k = key.trimmed().toLower();
    if (k == QLatin1String("board") || k == QLatin1String("lanes") || k == QLatin1String("kanban"))
        return BoardViewMode::Board;
    if (k == QLatin1String("wall") || k == QLatin1String("canvas") || k == QLatin1String("free"))
        return BoardViewMode::Wall;
    if (k == QLatin1String("list") || k == QLatin1String("table"))
        return BoardViewMode::List;
    return fallback;
}

// Lane keys that are not a status / category / priority / ticker / tag value.
inline const QString kLaneNone = QString();                          ///< no ticker / untagged / no priority
inline const QString kLaneArchived = QStringLiteral("ARCHIVED");     ///< status grouping, behind a toggle
inline const QString kLaneToday = QStringLiteral("TODAY");
inline const QString kLaneWeek = QStringLiteral("WEEK");
inline const QString kLaneEarlier = QStringLiteral("EARLIER");

/// Local-calendar bucket of a time: edited today, in the 6 days before, or earlier.
inline QString date_bucket(qint64 ms, const QDateTime& now) {
    if (ms <= 0)
        return kLaneEarlier;
    const QDate d = QDateTime::fromMSecsSinceEpoch(ms).toLocalTime().date();
    const QDate today = now.toLocalTime().date();
    if (d >= today)
        return kLaneToday;
    if (d >= today.addDays(-6))
        return kLaneWeek;
    return kLaneEarlier;
}

/// The lane a card belongs to. A note is in ONE lane: tickers and tags group by
/// the first one (the note's own tickers before $CASHTAGS in the text).
template <class Entry>
QString lane_key_of(const Entry& e, GroupBy g, const QDateTime& now) {
    switch (g) {
    case GroupBy::Status:
        return e.note.is_archived ? kLaneArchived : e.meta.status;
    case GroupBy::Category:
        return e.category;
    case GroupBy::Ticker:
        return e.ticker_list.value(0);
    case GroupBy::Priority:
        return priority_code(e.priority);
    case GroupBy::Tag:
        return e.tag_list.isEmpty() ? kLaneNone : e.tag_list.first().toLower();
    case GroupBy::Date:
        return date_bucket(e.updated_ms, now);
    }
    return {};
}

/// Lanes that are shown even when empty (the workflow, the priorities, the date
/// buckets, the editor's categories). Ticker and tag lanes exist only with cards.
inline QStringList fixed_lane_keys(GroupBy g, bool show_archived) {
    switch (g) {
    case GroupBy::Status: {
        QStringList k = nb::status_codes();
        if (show_archived)
            k << kLaneArchived;
        return k;
    }
    case GroupBy::Category:
        return default_category_codes();
    case GroupBy::Priority:
        return priority_codes();
    case GroupBy::Date:
        return {kLaneToday, kLaneWeek, kLaneEarlier};
    case GroupBy::Ticker:
    case GroupBy::Tag:
        return {};
    }
    return {};
}

// ── Review / openness ────────────────────────────────────────────────────────

/// Still in the workflow: not archived, not CLOSED.
template <class Entry>
bool is_open(const Entry& e) {
    return !e.note.is_archived && e.meta.status != nb::kStatusClosed;
}

/// Has a review date that is today or past, and is still open.
template <class Entry>
bool is_due(const Entry& e, const QDate& today) {
    return e.review.isValid() && e.review <= today && is_open(e);
}

/// Days since the note was last looked at (edited or marked reviewed).
template <class Entry>
int unreviewed_days(const Entry& e, qint64 now_ms) {
    const qint64 last = std::max(e.updated_ms, e.meta.reviewed_ms);
    if (last <= 0)
        return 0;
    return static_cast<int>(std::max<qint64>(0, now_ms - last) / (86400LL * 1000LL));
}

// ── Filters ──────────────────────────────────────────────────────────────────

struct BoardFilter {
    bool pinned_only = false;
    bool has_ticker = false;
    bool due_only = false;
    bool linked_only = false;
    bool show_archived = false;
    QStringList priorities; ///< priority codes; empty = any
    QStringList statuses;   ///< status codes; empty = any
    QStringList categories; ///< category keys; empty = any
    QString tag;            ///< any case; empty = any
    QString ticker;         ///< upper case; empty = any
    int edited_days = 0;    ///< edited within the last N days; 0 = any
    int stale_days = 0;     ///< open and not looked at for N days or more; 0 = off

    /// True when nothing but the archive toggle is set.
    bool is_empty() const {
        return !pinned_only && !has_ticker && !due_only && !linked_only && priorities.isEmpty() &&
               statuses.isEmpty() && categories.isEmpty() && tag.isEmpty() && ticker.isEmpty() && edited_days == 0 &&
               stale_days == 0;
    }
    bool operator==(const BoardFilter& o) const {
        return pinned_only == o.pinned_only && has_ticker == o.has_ticker && due_only == o.due_only &&
               linked_only == o.linked_only && show_archived == o.show_archived && priorities == o.priorities &&
               statuses == o.statuses && categories == o.categories && tag == o.tag && ticker == o.ticker &&
               edited_days == o.edited_days && stale_days == o.stale_days;
    }
    bool operator!=(const BoardFilter& o) const { return !(*this == o); }

    QJsonObject to_json() const {
        QJsonObject o;
        const auto put_bool = [&o](const char* k, bool v) {
            if (v)
                o.insert(QLatin1String(k), true);
        };
        put_bool("pinned", pinned_only);
        put_bool("has_ticker", has_ticker);
        put_bool("due", due_only);
        put_bool("linked", linked_only);
        put_bool("archived", show_archived);
        if (!priorities.isEmpty())
            o.insert(QStringLiteral("priorities"), QJsonArray::fromStringList(priorities));
        if (!statuses.isEmpty())
            o.insert(QStringLiteral("statuses"), QJsonArray::fromStringList(statuses));
        if (!categories.isEmpty())
            o.insert(QStringLiteral("categories"), QJsonArray::fromStringList(categories));
        if (!tag.isEmpty())
            o.insert(QStringLiteral("tag"), tag);
        if (!ticker.isEmpty())
            o.insert(QStringLiteral("ticker"), ticker);
        if (edited_days > 0)
            o.insert(QStringLiteral("edited_days"), edited_days);
        if (stale_days > 0)
            o.insert(QStringLiteral("stale_days"), stale_days);
        return o;
    }
    static BoardFilter from_json(const QJsonObject& o) {
        const auto list = [&o](const char* k) {
            QStringList out;
            for (const QJsonValue& v : o.value(QLatin1String(k)).toArray())
                if (!v.toString().trimmed().isEmpty())
                    out << v.toString().trimmed().toUpper();
            return out;
        };
        BoardFilter f;
        f.pinned_only = o.value(QStringLiteral("pinned")).toBool();
        f.has_ticker = o.value(QStringLiteral("has_ticker")).toBool();
        f.due_only = o.value(QStringLiteral("due")).toBool();
        f.linked_only = o.value(QStringLiteral("linked")).toBool();
        f.show_archived = o.value(QStringLiteral("archived")).toBool();
        f.priorities = list("priorities");
        f.statuses = list("statuses");
        f.categories = list("categories");
        f.tag = o.value(QStringLiteral("tag")).toString().trimmed();
        f.ticker = normalize_ticker(o.value(QStringLiteral("ticker")).toString());
        f.edited_days = std::clamp(o.value(QStringLiteral("edited_days")).toInt(), 0, 3650);
        f.stale_days = std::clamp(o.value(QStringLiteral("stale_days")).toInt(), 0, 3650);
        return f;
    }
};

template <class Entry>
bool passes_filter(const Entry& e, const BoardFilter& f, const QDate& today, qint64 now_ms) {
    if (e.note.is_archived && !f.show_archived)
        return false;
    if (f.pinned_only && !e.note.is_favorite)
        return false;
    if (f.has_ticker && e.ticker_list.isEmpty())
        return false;
    if (f.due_only && !is_due(e, today))
        return false;
    if (f.linked_only && !e.linked)
        return false;
    if (!f.priorities.isEmpty() && !f.priorities.contains(priority_code(e.priority)))
        return false;
    if (!f.statuses.isEmpty() && (e.note.is_archived || !f.statuses.contains(e.meta.status)))
        return false;
    if (!f.categories.isEmpty() && !f.categories.contains(e.category))
        return false;
    if (!f.tag.isEmpty()) {
        bool hit = false;
        for (const QString& t : e.tag_list)
            hit = hit || t.compare(f.tag, Qt::CaseInsensitive) == 0;
        if (!hit)
            return false;
    }
    if (!f.ticker.isEmpty() && !e.ticker_list.contains(f.ticker))
        return false;
    if (f.edited_days > 0 && (e.updated_ms <= 0 || now_ms - e.updated_ms > qint64(f.edited_days) * 86400LL * 1000LL))
        return false;
    if (f.stale_days > 0 && (!is_open(e) || unreviewed_days(e, now_ms) < f.stale_days))
        return false;
    return true;
}

// ── Named boards (saved views) ───────────────────────────────────────────────

struct BoardDef {
    qint64 id = 0; ///< note_boards.id; 0 = not stored yet
    QString name;
    int position = 0;
    BoardViewMode view = BoardViewMode::Board;
    GroupBy group_by = GroupBy::Status;
    BoardFilter filter;
    QHash<QString, QStringList> lane_order; ///< group key -> lane keys in the user's order
    QHash<QString, QStringList> collapsed;  ///< group key -> collapsed lane keys
    QHash<QString, int> soft_limits;        ///< "<group>:<lane>" -> soft card limit
    double wall_zoom = 1.0;
    double wall_cx = 0.0;
    double wall_cy = 0.0;
    bool wall_snap = true;
    QString preset; ///< "all", "trade_ideas", "earnings", "journal" for the seeded boards

    static QString limit_key(GroupBy g, const QString& lane) { return group_by_key(g) + QLatin1Char(':') + lane; }
    bool is_collapsed(GroupBy g, const QString& lane) const {
        return collapsed.value(group_by_key(g)).contains(lane);
    }
    void set_collapsed(GroupBy g, const QString& lane, bool on) {
        QStringList& l = collapsed[group_by_key(g)];
        l.removeAll(lane);
        if (on)
            l << lane;
    }
    int soft_limit(GroupBy g, const QString& lane) const { return soft_limits.value(limit_key(g, lane), 0); }
    void set_soft_limit(GroupBy g, const QString& lane, int n) {
        if (n > 0)
            soft_limits.insert(limit_key(g, lane), n);
        else
            soft_limits.remove(limit_key(g, lane));
    }

    QJsonObject to_json() const {
        QJsonObject o;
        o.insert(QStringLiteral("v"), 1);
        o.insert(QStringLiteral("view"), view_mode_key(view));
        o.insert(QStringLiteral("group_by"), group_by_key(group_by));
        o.insert(QStringLiteral("filter"), filter.to_json());
        const auto map_of_lists = [](const QHash<QString, QStringList>& m) {
            QJsonObject out;
            for (auto it = m.cbegin(); it != m.cend(); ++it)
                if (!it.value().isEmpty())
                    out.insert(it.key(), QJsonArray::fromStringList(it.value()));
            return out;
        };
        o.insert(QStringLiteral("lane_order"), map_of_lists(lane_order));
        o.insert(QStringLiteral("collapsed"), map_of_lists(collapsed));
        QJsonObject limits;
        for (auto it = soft_limits.cbegin(); it != soft_limits.cend(); ++it)
            limits.insert(it.key(), it.value());
        o.insert(QStringLiteral("soft_limits"), limits);
        o.insert(QStringLiteral("wall"), QJsonObject{{QStringLiteral("zoom"), wall_zoom},
                                                     {QStringLiteral("cx"), wall_cx},
                                                     {QStringLiteral("cy"), wall_cy},
                                                     {QStringLiteral("snap"), wall_snap}});
        if (!preset.isEmpty())
            o.insert(QStringLiteral("preset"), preset);
        return o;
    }
    static BoardDef from_json(const QJsonObject& o) {
        BoardDef d;
        d.view = view_mode_from_key(o.value(QStringLiteral("view")).toString());
        d.group_by = group_by_from_key(o.value(QStringLiteral("group_by")).toString());
        d.filter = BoardFilter::from_json(o.value(QStringLiteral("filter")).toObject());
        const auto lists = [](const QJsonObject& m) {
            QHash<QString, QStringList> out;
            for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
                QStringList l;
                for (const QJsonValue& v : it.value().toArray())
                    l << v.toString();
                if (!l.isEmpty())
                    out.insert(it.key(), l);
            }
            return out;
        };
        d.lane_order = lists(o.value(QStringLiteral("lane_order")).toObject());
        d.collapsed = lists(o.value(QStringLiteral("collapsed")).toObject());
        const QJsonObject limits = o.value(QStringLiteral("soft_limits")).toObject();
        for (auto it = limits.constBegin(); it != limits.constEnd(); ++it)
            if (it.value().toInt() > 0)
                d.soft_limits.insert(it.key(), std::min(it.value().toInt(), 100000));
        const QJsonObject wall = o.value(QStringLiteral("wall")).toObject();
        d.wall_zoom = std::clamp(wall.value(QStringLiteral("zoom")).toDouble(1.0), 0.1, 4.0);
        d.wall_cx = wall.value(QStringLiteral("cx")).toDouble();
        d.wall_cy = wall.value(QStringLiteral("cy")).toDouble();
        d.wall_snap = wall.value(QStringLiteral("snap")).toBool(true);
        d.preset = o.value(QStringLiteral("preset")).toString();
        return d;
    }
    QString to_json_string() const { return QString::fromUtf8(QJsonDocument(to_json()).toJson(QJsonDocument::Compact)); }
    static BoardDef from_record(const nb::NoteBoardRecord& r) {
        BoardDef d = from_json(QJsonDocument::fromJson(r.definition_json.toUtf8()).object());
        d.id = r.id;
        d.name = r.name;
        d.position = r.position;
        return d;
    }
    nb::NoteBoardRecord to_record() const {
        nb::NoteBoardRecord r;
        r.id = id;
        r.name = name;
        r.position = position;
        r.definition_json = to_json_string();
        return r;
    }
};

/// The boards a notebook starts with (names are English here; the screen shows
/// them translated by preset): everything by workflow, trade ideas, earnings by
/// ticker, the journal by date.
inline QVector<BoardDef> default_boards() {
    QVector<BoardDef> out;
    BoardDef all;
    all.name = QStringLiteral("All notes");
    all.preset = QStringLiteral("all");
    all.position = 0;
    out << all;
    BoardDef ideas;
    ideas.name = QStringLiteral("Trade ideas");
    ideas.preset = QStringLiteral("trade_ideas");
    ideas.position = 1;
    ideas.filter.categories = {QStringLiteral("TRADE_IDEA")};
    out << ideas;
    BoardDef earnings;
    earnings.name = QStringLiteral("Earnings");
    earnings.preset = QStringLiteral("earnings");
    earnings.position = 2;
    earnings.group_by = GroupBy::Ticker;
    earnings.filter.categories = {QStringLiteral("EARNINGS")};
    out << earnings;
    BoardDef journal;
    journal.name = QStringLiteral("Journal");
    journal.preset = QStringLiteral("journal");
    journal.position = 3;
    journal.group_by = GroupBy::Date;
    journal.filter.tag = QStringLiteral("journal");
    out << journal;
    return out;
}

/// Move board `from` to index `to` (the board menu's "Move up / down") and number
/// every board's position by its index again. Returns the indexes whose stored
/// position changed — the rows to store. Nothing moves when `from` == `to` or
/// either is out of range (the caller checks that; the result is then empty).
inline QVector<int> reorder_boards(QVector<BoardDef>& boards, int from, int to) {
    QVector<int> changed;
    const int n = static_cast<int>(boards.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return changed;
    boards.move(from, to);
    for (int i = 0; i < n; ++i) {
        if (boards.at(i).position != i) {
            boards[i].position = i;
            changed << i;
        }
    }
    return changed;
}

// ── Layout: lanes of sorted cards ────────────────────────────────────────────

template <class Entry>
struct BasicLane {
    QString key;
    QString title; ///< tag lanes: the first spelling seen; empty = the screen names it
    QVector<Entry> cards;
    int due = 0;
    QStringList tickers; ///< distinct tickers of the lane's cards, most used first (the header's ▲ / ▼)
};

struct BoardStats {
    int notes = 0;    ///< live (not archived)
    int archived = 0;
    int active_ideas = 0; ///< open notes in IDEA / RESEARCHING / ACTIVE
    int in_position = 0;  ///< ACTIVE
    int due = 0;
    int overdue = 0; ///< due before today
    int pinned = 0;
    int tickers = 0; ///< distinct tickers on live notes
    int edited_week = 0;
    int linked = 0;
    int oldest_unreviewed_days = -1; ///< -1: no open note
    int oldest_unreviewed_id = 0;
    QString oldest_unreviewed_title;
    int last_edited_id = 0; ///< the live note edited last ("continue" in the empty detail pane)
    QString last_edited_title;
};

template <class Entry>
struct BasicBoardLayout {
    QVector<BasicLane<Entry>> lanes;
    int shown = 0;            ///< cards on the board
    int total = 0;            ///< notes in the library (archived included)
    BoardStats stats;         ///< the whole notebook
    QStringList tickers;      ///< distinct tickers of the shown cards, most used first
    QStringList all_tickers;  ///< distinct tickers of live notes, most used first
    QVector<QPair<QString, int>> top_tags; ///< (first spelling, live notes) most used first
};

/// Card order inside a lane: manual rank, then newest edit, then id.
template <class Entry>
void sort_lane(QVector<Entry>& cards) {
    std::stable_sort(cards.begin(), cards.end(), [](const Entry& a, const Entry& b) {
        if (a.meta.rank != b.meta.rank)
            return a.meta.rank < b.meta.rank;
        if (a.updated_ms != b.updated_ms)
            return a.updated_ms > b.updated_ms;
        return a.note.id > b.note.id;
    });
}

/// `natural` (the fixed lanes, then the others as found) re-ordered by the user's
/// saved order: saved keys that exist first, in saved order, then the rest.
inline QStringList apply_lane_order(const QStringList& natural, const QStringList& saved) {
    QStringList out;
    for (const QString& k : saved)
        if (natural.contains(k) && !out.contains(k))
            out << k;
    for (const QString& k : natural)
        if (!out.contains(k))
            out << k;
    return out;
}

template <class Entry>
BoardStats board_stats(const QVector<Entry>& all, const QDate& today, qint64 now_ms) {
    BoardStats s;
    QSet<QString> tickers;
    const qint64 week_ms = 7LL * 86400LL * 1000LL;
    qint64 last_ms = -1;
    for (const Entry& e : all) {
        if (e.note.is_archived) {
            ++s.archived;
            continue;
        }
        ++s.notes;
        if (e.updated_ms > last_ms) {
            last_ms = e.updated_ms;
            s.last_edited_id = e.note.id;
            s.last_edited_title = e.note.title;
        }
        if (e.note.is_favorite)
            ++s.pinned;
        if (e.linked)
            ++s.linked;
        for (const QString& t : e.ticker_list)
            tickers.insert(t);
        if (e.updated_ms > 0 && now_ms - e.updated_ms <= week_ms)
            ++s.edited_week;
        if (!is_open(e))
            continue;
        const QString& st = e.meta.status;
        if (st == nb::kStatusIdea || st == nb::kStatusResearching || st == nb::kStatusActive)
            ++s.active_ideas;
        if (st == nb::kStatusActive)
            ++s.in_position;
        if (is_due(e, today)) {
            ++s.due;
            if (e.review < today)
                ++s.overdue;
        }
        const int age = unreviewed_days(e, now_ms);
        if (age > s.oldest_unreviewed_days) {
            s.oldest_unreviewed_days = age;
            s.oldest_unreviewed_id = e.note.id;
            s.oldest_unreviewed_title = e.note.title;
        }
    }
    s.tickers = static_cast<int>(tickers.size());
    return s;
}

/// The board for one saved view + search: filtered, grouped, sorted.
template <class Entry>
BasicBoardLayout<Entry> build_board(const QVector<Entry>& all, const BoardDef& def, const SearchQuery& q,
                                    const QDateTime& now) {
    BasicBoardLayout<Entry> out;
    out.total = static_cast<int>(all.size());
    const QDate today = now.toLocalTime().date();
    const qint64 now_ms = now.toMSecsSinceEpoch();
    const bool search = !q.is_empty();
    // Archived notes only appear in the status grouping's ARCHIVED lane, or in any
    // grouping while the archive toggle is on.
    QHash<QString, int> lane_index;
    QHash<QString, int> ticker_count;
    QHash<QString, int> live_ticker_count;
    QHash<QString, int> tag_count;
    QHash<QString, QString> tag_text;
    const QStringList fixed = fixed_lane_keys(def.group_by, def.filter.show_archived);
    for (const QString& k : fixed) {
        lane_index.insert(k, static_cast<int>(out.lanes.size()));
        BasicLane<Entry> lane;
        lane.key = k;
        out.lanes << lane;
    }
    for (const Entry& e : all) {
        if (!e.note.is_archived) {
            for (const QString& t : e.ticker_list)
                ++live_ticker_count[t];
            for (const QString& t : e.tag_list) {
                const QString k = t.toLower();
                ++tag_count[k];
                if (!tag_text.contains(k))
                    tag_text.insert(k, t);
            }
        }
        if (!passes_filter(e, def.filter, today, now_ms))
            continue;
        if (search && !matches(e, q))
            continue;
        const QString key = lane_key_of(e, def.group_by, now);
        auto it = lane_index.find(key);
        if (it == lane_index.end()) {
            BasicLane<Entry> lane;
            lane.key = key;
            if (def.group_by == GroupBy::Tag && !e.tag_list.isEmpty())
                lane.title = e.tag_list.first();
            it = lane_index.insert(key, static_cast<int>(out.lanes.size()));
            out.lanes << lane;
        }
        BasicLane<Entry>& lane = out.lanes[it.value()];
        lane.cards.append(e);
        if (is_due(e, today))
            ++lane.due;
        for (const QString& t : e.ticker_list)
            ++ticker_count[t];
        ++out.shown;
    }
    for (auto& lane : out.lanes) {
        sort_lane(lane.cards);
        QHash<QString, int> lane_tickers;
        for (const Entry& e : lane.cards)
            for (const QString& t : e.ticker_list)
                ++lane_tickers[t];
        lane.tickers = keys_by_count(lane_tickers);
    }

    // Lane order: fixed lanes first (their own order), then the found ones by size
    // (largest first, then name), the "none" lane last; then the user's order.
    QStringList natural = fixed;
    QVector<int> extra;
    for (int i = 0; i < out.lanes.size(); ++i)
        if (!fixed.contains(out.lanes.at(i).key))
            extra << i;
    std::sort(extra.begin(), extra.end(), [&out](int a, int b) {
        const auto& la = out.lanes.at(a);
        const auto& lb = out.lanes.at(b);
        if (la.key.isEmpty() != lb.key.isEmpty())
            return lb.key.isEmpty();
        if (la.cards.size() != lb.cards.size())
            return la.cards.size() > lb.cards.size();
        return la.key.compare(lb.key, Qt::CaseInsensitive) < 0;
    });
    for (int i : extra)
        natural << out.lanes.at(i).key;
    const QStringList order = apply_lane_order(natural, def.lane_order.value(group_by_key(def.group_by)));
    QVector<BasicLane<Entry>> ordered;
    ordered.reserve(out.lanes.size());
    for (const QString& k : order)
        ordered << std::move(out.lanes[lane_index.value(k)]);
    out.lanes = std::move(ordered);

    out.stats = board_stats(all, today, now_ms);
    out.tickers = keys_by_count(ticker_count);
    out.all_tickers = keys_by_count(live_ticker_count);
    for (const QString& k : keys_by_count(tag_count))
        out.top_tags.append({tag_text.value(k), tag_count.value(k)});
    return out;
}

// ── Manual order ─────────────────────────────────────────────────────────────

/// Smallest gap kept between neighbouring ranks before a lane is renumbered.
inline constexpr double kMinRankGap = 1e-6;

/// Ranks for `count` cards placed between the card above (`above`, none = top of
/// the lane) and the card below (`below`, none = bottom), in order.
inline QVector<double> ranks_between(std::optional<double> above, std::optional<double> below, int count) {
    QVector<double> out;
    out.reserve(std::max(0, count));
    for (int i = 0; i < count; ++i) {
        if (above && below)
            out << *above + (*below - *above) * double(i + 1) / double(count + 1);
        else if (below)
            out << *below - double(count - i);
        else if (above)
            out << *above + double(i + 1);
        else
            out << double(i);
    }
    return out;
}

/// True when `count` cards no longer fit between `above` and `below`.
inline bool ranks_exhausted(std::optional<double> above, std::optional<double> below, int count) {
    if (!above || !below)
        return false;
    return (*below - *above) / double(count + 1) < kMinRankGap || !(*below > *above);
}

/// What a drop does to the order of a lane.
struct RankPlan {
    QVector<QPair<int, double>> ranks; ///< (note id, new rank) for every card that changes
    QVector<int> order;                ///< the lane's card ids afterwards, top to bottom
    bool renumbered = false;           ///< the whole lane was renumbered (gap exhausted)
};

/// Insert `moving` (note ids, in order) into a lane whose cards are `lane`
/// ((id, rank), top to bottom; cards already in it may be among `moving`) before
/// the card at `pos` of `lane` (pos == lane.size() = at the bottom).
inline RankPlan plan_insert(const QVector<QPair<int, double>>& lane, const QVector<int>& moving, int pos) {
    RankPlan plan;
    QSet<int> mv(moving.cbegin(), moving.cend());
    QVector<QPair<int, double>> rest;
    int idx = 0;
    pos = std::clamp(pos, 0, static_cast<int>(lane.size()));
    for (int i = 0; i < lane.size(); ++i) {
        if (mv.contains(lane.at(i).first))
            continue;
        if (i < pos)
            ++idx;
        rest << lane.at(i);
    }
    std::optional<double> above, below;
    if (idx > 0)
        above = rest.at(idx - 1).second;
    if (idx < rest.size())
        below = rest.at(idx).second;
    const int n = static_cast<int>(moving.size());
    for (int i = 0; i < rest.size(); ++i) {
        if (i == idx)
            plan.order << moving;
        plan.order << rest.at(i).first;
    }
    if (idx >= rest.size())
        plan.order << moving;
    if (ranks_exhausted(above, below, n)) {
        plan.renumbered = true;
        QHash<int, double> old;
        for (const auto& p : lane)
            old.insert(p.first, p.second);
        for (int i = 0; i < plan.order.size(); ++i) {
            const int id = plan.order.at(i);
            const double r = double(i);
            if (!old.contains(id) || old.value(id) != r || mv.contains(id))
                plan.ranks.append({id, r});
        }
        return plan;
    }
    const QVector<double> r = ranks_between(above, below, n);
    for (int i = 0; i < n; ++i)
        plan.ranks.append({moving.at(i), r.at(i)});
    return plan;
}

// ── Drag-move rules ──────────────────────────────────────────────────────────

enum class MoveDenied { None, DateLane, TickerInText, NoPriority, SameLane };

struct MoveCheck {
    bool allowed = true;
    MoveDenied reason = MoveDenied::None;
    QString detail; ///< e.g. the cashtag that keeps a note in its ticker lane
};

/// May a card in lane `from` be dropped into lane `to` under grouping `g`?
/// Reordering inside the same lane is always allowed.
template <class Entry>
MoveCheck check_move(const Entry& e, GroupBy g, const QString& from, const QString& to) {
    MoveCheck c;
    if (from == to)
        return c;
    switch (g) {
    case GroupBy::Date:
        // The date lanes follow the last edit: only reordering inside a lane.
        c.allowed = false;
        c.reason = MoveDenied::DateLane;
        return c;
    case GroupBy::Ticker:
        if (to.isEmpty()) {
            const QStringList tags = extract_cashtags(e.note.content);
            if (!tags.isEmpty()) {
                c.allowed = false;
                c.reason = MoveDenied::TickerInText;
                c.detail = tags.first();
            }
        }
        return c;
    case GroupBy::Priority:
        if (to.isEmpty()) {
            c.allowed = false;
            c.reason = MoveDenied::NoPriority;
        }
        return c;
    case GroupBy::Status:
    case GroupBy::Category:
    case GroupBy::Tag:
        return c;
    }
    return c;
}

/// The note and board row after a card moved from lane `from` to lane `to`.
/// `to_title` is the target lane's display text (a tag's spelling).
template <class Note>
struct MoveResult {
    Note note;
    nb::NoteBoardMeta meta;
    bool note_changed = false;
    bool meta_changed = false;
    QString old_status; ///< for the history when the status changed
};

template <class Note>
MoveResult<Note> apply_lane_move(const BasicEntry<Note>& e, GroupBy g, const QString& from, const QString& to,
                                 const QString& to_title, qint64 now_ms) {
    MoveResult<Note> r;
    r.note = e.note;
    r.meta = e.meta;
    if (from == to)
        return r;
    switch (g) {
    case GroupBy::Status:
        if (to == kLaneArchived) {
            r.note.is_archived = true;
            r.note_changed = !e.note.is_archived;
            break;
        }
        if (e.note.is_archived) {
            r.note.is_archived = false;
            r.note_changed = true;
        }
        if (r.meta.status != to) {
            r.old_status = r.meta.status;
            r.meta.status = nb::normalize_status(to);
            r.meta.status_changed_ms = now_ms;
            r.meta_changed = true;
        }
        break;
    case GroupBy::Category:
        if (category_key(r.note.category) != to) {
            r.note.category = to;
            r.note_changed = true;
        }
        break;
    case GroupBy::Priority:
        if (!to.isEmpty() && r.note.priority.trimmed().toUpper() != to) {
            r.note.priority = to;
            r.note_changed = true;
        }
        break;
    case GroupBy::Ticker: {
        QStringList mine = split_tickers(r.note.tickers);
        if (!from.isEmpty())
            mine.removeAll(from);
        if (to.isEmpty()) {
            mine.clear();
        } else {
            mine.removeAll(to);
            mine.prepend(to);
        }
        const QString joined = join_list(mine);
        if (joined != r.note.tickers) {
            r.note.tickers = joined;
            r.note_changed = true;
        }
        break;
    }
    case GroupBy::Tag: {
        QStringList mine = split_tags(r.note.tags);
        const auto drop = [&mine](const QString& t) {
            for (int i = static_cast<int>(mine.size()) - 1; i >= 0; --i)
                if (mine.at(i).compare(t, Qt::CaseInsensitive) == 0)
                    mine.removeAt(i);
        };
        if (!from.isEmpty())
            drop(from);
        if (to.isEmpty()) {
            mine.clear();
        } else {
            drop(to);
            mine.prepend(to_title.isEmpty() ? to : to_title);
        }
        const QString joined = join_list(mine);
        if (joined != r.note.tags) {
            r.note.tags = joined;
            r.note_changed = true;
        }
        break;
    }
    case GroupBy::Date:
        break;
    }
    return r;
}

// ── Command field ────────────────────────────────────────────────────────────

enum class CommandKind { Search, NewNote, NewFromTemplate, GroupBy, View, Open, Board, Filter, Clear, Help };

struct Command {
    CommandKind kind = CommandKind::Search;
    QString text;        ///< search text / note title / "open" query / board name / filter key
    QString template_id{}; ///< NewFromTemplate
    QString ticker{};    ///< NewNote / NewFromTemplate
    GroupBy group = GroupBy::Status;
    BoardViewMode view = BoardViewMode::Board;
    bool is_command() const { return kind != CommandKind::Search; }
};

/// Filter keys the "filter" command (and the ribbon) understands.
inline QStringList command_filter_keys() {
    return {QStringLiteral("pinned"),   QStringLiteral("ticker"), QStringLiteral("due"),  QStringLiteral("linked"),
            QStringLiteral("archived"), QStringLiteral("critical"), QStringLiteral("high"), QStringLiteral("medium"),
            QStringLiteral("low"),      QStringLiteral("today"),  QStringLiteral("week"), QStringLiteral("month"),
            QStringLiteral("stale")};
}

namespace detail {

/// Template id for the words after "new" (longest match first), and how many
/// words it used; ("", 0) when none matches.
inline QPair<QString, int> match_template_words(const QStringList& words) {
    struct Alias {
        const char* id;
        QStringList words;
    };
    const QVector<Alias> aliases = {
        {"post_trade_review", {QStringLiteral("post-trade"), QStringLiteral("review")}},
        {"post_trade_review", {QStringLiteral("post"), QStringLiteral("trade"), QStringLiteral("review")}},
        {"post_trade_review", {QStringLiteral("trade"), QStringLiteral("review")}},
        {"trade_idea", {QStringLiteral("trade"), QStringLiteral("idea")}},
        {"earnings_prep", {QStringLiteral("earnings"), QStringLiteral("prep")}},
        {"daily_journal", {QStringLiteral("daily"), QStringLiteral("journal")}},
        {"post_trade_review", {QStringLiteral("review")}},
        {"trade_idea", {QStringLiteral("idea")}},
        {"earnings_prep", {QStringLiteral("earnings")}},
        {"daily_journal", {QStringLiteral("journal")}},
    };
    QPair<QString, int> best;
    for (const Alias& a : aliases) {
        if (a.words.size() > words.size() || a.words.size() <= best.second)
            continue;
        bool ok = true;
        for (int i = 0; i < a.words.size() && ok; ++i)
            ok = words.at(i).compare(a.words.at(i), Qt::CaseInsensitive) == 0;
        if (ok)
            best = {QString::fromLatin1(a.id), static_cast<int>(a.words.size())};
    }
    return best;
}

} // namespace detail

/// What the command field means. Commands start with a verb: new, group (by),
/// view / board / wall / list, open, filter / show, clear, help (or "?").
/// Anything else — and anything in quotes — is a search ($TICKER, #tag, words).
inline Command parse_command(const QString& raw) {
    Command c;
    const QString text = raw.trimmed();
    c.text = text;
    if (text.isEmpty() || text.startsWith(QLatin1Char('"')))
        return c;
    QStringList words = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const QString verb = words.first().toLower();
    const QString rest = text.mid(words.first().size()).trimmed();
    words.removeFirst();

    if (verb == QLatin1String("?") || verb == QLatin1String("help")) {
        if (words.isEmpty()) {
            c.kind = CommandKind::Help;
            c.text.clear();
        }
        return c;
    }
    if (verb == QLatin1String("clear") || verb == QLatin1String("reset")) {
        if (words.isEmpty() || (words.size() == 1 && words.first().compare(QLatin1String("filters"),
                                                                           Qt::CaseInsensitive) == 0)) {
            c.kind = CommandKind::Clear;
            c.text.clear();
        }
        return c;
    }
    if (verb == QLatin1String("new") || verb == QLatin1String("add") || verb == QLatin1String("create")) {
        // "new [template words] [for] [$]TICKER" / "new <title>"
        const auto tpl = detail::match_template_words(words);
        QStringList after = words.mid(tpl.second);
        if (!after.isEmpty() && (after.first().compare(QLatin1String("for"), Qt::CaseInsensitive) == 0 ||
                                 after.first().compare(QLatin1String("on"), Qt::CaseInsensitive) == 0))
            after.removeFirst();
        if (!tpl.first.isEmpty()) {
            if (after.size() > 1)
                return c; // "new trade idea about rates": not a command, a search
            c.kind = CommandKind::NewFromTemplate;
            c.template_id = tpl.first;
            c.ticker = after.isEmpty() ? QString() : normalize_ticker(after.first());
            if (!after.isEmpty() && c.ticker.isEmpty())
                return Command{CommandKind::Search, text};
            c.text.clear();
            return c;
        }
        c.kind = CommandKind::NewNote;
        c.text = rest;
        if (words.size() == 1 && words.first().startsWith(QLatin1Char('$'))) {
            c.ticker = normalize_ticker(words.first());
            if (!c.ticker.isEmpty())
                c.text.clear();
        } else if (words.size() == 1 && rest == rest.toUpper() && !normalize_ticker(rest).isEmpty() &&
                   rest.size() <= 6) {
            c.ticker = normalize_ticker(rest); // "new NVDA"
            c.text.clear();
        }
        return c;
    }
    if (verb == QLatin1String("group") || verb == QLatin1String("g")) {
        if (!words.isEmpty() && words.first().compare(QLatin1String("by"), Qt::CaseInsensitive) == 0)
            words.removeFirst();
        if (words.size() != 1)
            return c;
        const QString k = words.first().toLower();
        const GroupBy g = group_by_from_key(k, GroupBy::Status);
        if (g == GroupBy::Status && k != QLatin1String("status") && k != QLatin1String("workflow") &&
            k != QLatin1String("stage"))
            return c; // unknown grouping: a search
        c.kind = CommandKind::GroupBy;
        c.group = g;
        c.text.clear();
        return c;
    }
    if (verb == QLatin1String("view") || verb == QLatin1String("show") || verb == QLatin1String("filter")) {
        if (words.size() != 1 && !(verb == QLatin1String("filter") && words.size() == 2))
            return c;
        const QString k = words.join(QLatin1Char(' ')).toLower();
        if (verb != QLatin1String("filter")) {
            const BoardViewMode m = view_mode_from_key(k, BoardViewMode::Board);
            const bool known_view = k == QLatin1String("board") || k == QLatin1String("lanes") ||
                                    k == QLatin1String("kanban") || m != BoardViewMode::Board;
            if (known_view) {
                c.kind = CommandKind::View;
                c.view = m;
                c.text.clear();
                return c;
            }
            if (verb == QLatin1String("view"))
                return c;
        }
        // filter / show <key>
        QString key = k;
        if (key == QLatin1String("has ticker") || key == QLatin1String("tickers") || key == QLatin1String("ticker"))
            key = QStringLiteral("ticker");
        else if (key == QLatin1String("pins") || key == QLatin1String("favorites") || key == QLatin1String("starred"))
            key = QStringLiteral("pinned");
        else if (key == QLatin1String("review") || key == QLatin1String("reviews"))
            key = QStringLiteral("due");
        else if (key == QLatin1String("archive"))
            key = QStringLiteral("archived");
        else if (key == QLatin1String("chat") || key == QLatin1String("agent"))
            key = QStringLiteral("linked");
        if (!command_filter_keys().contains(key))
            return c;
        c.kind = CommandKind::Filter;
        c.text = key;
        return c;
    }
    if (verb == QLatin1String("board") || verb == QLatin1String("wall") || verb == QLatin1String("list")) {
        if (words.isEmpty()) {
            c.kind = CommandKind::View;
            c.view = view_mode_from_key(verb);
            c.text.clear();
            return c;
        }
        if (verb == QLatin1String("board")) {
            c.kind = CommandKind::Board;
            c.text = rest;
        }
        return c;
    }
    if (verb == QLatin1String("open") || verb == QLatin1String("go") || verb == QLatin1String("goto")) {
        if (rest.isEmpty())
            return c;
        c.kind = CommandKind::Open;
        c.text = rest;
        return c;
    }
    return c;
}

/// Best match for "open <query>": exact title (any case), then a title starting
/// with it, then one containing it; newest edit wins ties. 0 when none.
template <class Entry>
int find_note_by_title(const QVector<Entry>& all, const QString& query) {
    const QString q = query.trimmed();
    if (q.isEmpty())
        return 0;
    int best = 0;
    int best_score = 0;
    qint64 best_ms = 0;
    for (const Entry& e : all) {
        const QString& t = e.note.title;
        int score = 0;
        if (t.compare(q, Qt::CaseInsensitive) == 0)
            score = 3;
        else if (t.startsWith(q, Qt::CaseInsensitive))
            score = 2;
        else if (t.contains(q, Qt::CaseInsensitive))
            score = 1;
        if (score == 0)
            continue;
        if (e.note.is_archived)
            score = std::max(1, score - 1); // a live note wins over an archived one with the same title
        if (score > best_score || (score == best_score && e.updated_ms > best_ms)) {
            best = e.note.id;
            best_score = score;
            best_ms = e.updated_ms;
        }
    }
    return best;
}

/// The board filter after the "filter <key>" command (or a ribbon tile): toggles
/// the key's condition on `f`.
inline BoardFilter toggle_filter_key(BoardFilter f, const QString& key) {
    const QString k = key.toLower();
    if (k == QLatin1String("pinned"))
        f.pinned_only = !f.pinned_only;
    else if (k == QLatin1String("ticker"))
        f.has_ticker = !f.has_ticker;
    else if (k == QLatin1String("due"))
        f.due_only = !f.due_only;
    else if (k == QLatin1String("linked"))
        f.linked_only = !f.linked_only;
    else if (k == QLatin1String("archived"))
        f.show_archived = !f.show_archived;
    else if (priority_codes().contains(k.toUpper())) {
        const QString p = k.toUpper();
        if (f.priorities.contains(p))
            f.priorities.removeAll(p);
        else
            f.priorities << p;
    } else if (k == QLatin1String("today"))
        f.edited_days = f.edited_days == 1 ? 0 : 1;
    else if (k == QLatin1String("week"))
        f.edited_days = f.edited_days == 7 ? 0 : 7;
    else if (k == QLatin1String("month"))
        f.edited_days = f.edited_days == 30 ? 0 : 30;
    else if (k == QLatin1String("stale"))
        f.stale_days = f.stale_days > 0 ? 0 : 30;
    return f;
}

} // namespace fincept::screens::notes
