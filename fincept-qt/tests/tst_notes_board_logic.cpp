// tst_notes_board_logic.cpp — src/screens/notes/NotesBoardLogic.h (+ the board
// row defaults in storage/repositories/NoteBoardTypes.h): lane grouping and
// ordering, filters, manual order (ranks), drag-move rules, board persistence
// round trip, migration defaults for existing notes, command-field parsing.
#include "screens/notes/NotesBoardLogic.h"

#include <QTest>

using namespace fincept::screens::notes;
namespace nb = fincept::notes_board;

namespace {

// Same field names as fincept::FinancialNote (which lives behind QtSql).
struct BoardNote {
    int id = 0;
    QString title;
    QString content;
    QString category;
    QString priority;
    QString tags;
    QString tickers;
    QString sentiment;
    bool is_favorite = false;
    bool is_archived = false;
    QString created_at;
    QString updated_at;
};
using Entry = BasicEntry<BoardNote>;
using Layout = BasicBoardLayout<Entry>;

BoardNote bnote(int id, const QString& title, const QString& cat = QStringLiteral("GENERAL"),
                const QString& prio = QStringLiteral("MEDIUM"), const QString& tags = {}, const QString& tickers = {},
                const QString& updated = QStringLiteral("2026-11-10 10:00:00"), const QString& body = {}) {
    BoardNote n;
    n.id = id;
    n.title = title;
    n.content = body;
    n.category = cat;
    n.priority = prio;
    n.tags = tags;
    n.tickers = tickers;
    n.updated_at = updated;
    n.created_at = QStringLiteral("2026-11-01 09:00:00");
    return n;
}

Entry with_status(Entry e, const QString& status, double rank) {
    nb::NoteBoardMeta m = e.meta;
    m.status = status;
    m.rank = rank;
    m.stored = true;
    apply_meta(e, m);
    return e;
}

QList<int> card_ids(const BasicLane<Entry>& lane) {
    QList<int> out;
    for (const auto& e : lane.cards)
        out << e.note.id;
    return out;
}

QStringList lane_keys(const Layout& l) {
    QStringList out;
    for (const auto& lane : l.lanes)
        out << lane.key;
    return out;
}

const BasicLane<Entry>* lane_of(const Layout& l, const QString& key) {
    for (const auto& lane : l.lanes)
        if (lane.key == key)
            return &lane;
    return nullptr;
}

QDateTime fixed_now() {
    return QDateTime(QDate(2026, 11, 17), QTime(12, 0, 0), QTimeZone(QTimeZone::UTC));
}

} // namespace

class TestNotesBoardLogic : public QObject {
    Q_OBJECT
  private slots:
    // ── Migration defaults for existing notes ───────────────────────────────
    void migration_defaults() {
        QCOMPARE(nb::default_status(QStringLiteral("TRADE_IDEA"), false), nb::kStatusIdea);
        QCOMPARE(nb::default_status(QStringLiteral("research"), false), nb::kStatusResearching);
        QCOMPARE(nb::default_status(QStringLiteral("EARNINGS"), false), nb::kStatusResearching);
        QCOMPARE(nb::default_status(QStringLiteral("PORTFOLIO"), false), nb::kStatusActive);
        QCOMPARE(nb::default_status(QStringLiteral("GENERAL"), false), nb::kStatusIdea);
        QCOMPARE(nb::default_status(QStringLiteral("something new"), false), nb::kStatusIdea);
        QCOMPARE(nb::default_status(QStringLiteral("TRADE_IDEA"), true), nb::kStatusClosed);
        QCOMPARE(nb::normalize_status(QStringLiteral(" review ")), nb::kStatusReview);
        QCOMPARE(nb::normalize_status(QStringLiteral("bogus")), nb::kStatusIdea);

        // The SQL the migration backfills with carries every rule and the archive case.
        const QString sql = nb::default_status_sql(QStringLiteral("n.category"), QStringLiteral("n.is_archived"));
        QVERIFY(sql.startsWith(QStringLiteral("CASE WHEN COALESCE(n.is_archived, 0) <> 0 THEN 'CLOSED'")));
        for (const auto& r : nb::kDefaultStatusRules)
            QVERIFY2(sql.contains(QStringLiteral("= '%1' THEN '%2'").arg(QLatin1String(r.category), QLatin1String(r.status))),
                     r.category);
        QVERIFY(sql.endsWith(QStringLiteral("ELSE 'IDEA' END")));
        const QString rank_sql = nb::default_rank_sql(QStringLiteral("updated_at"), QStringLiteral("created_at"));
        QVERIFY(rank_sql.contains(QStringLiteral("strftime('%s', NULLIF(updated_at, ''))")));

        // An entry without a stored row gets the same defaults; newest edit = smallest rank.
        const Entry a = make_entry(bnote(1, QStringLiteral("a"), QStringLiteral("RESEARCH"), QStringLiteral("LOW"), {}, {},
                                         QStringLiteral("2026-11-12 10:00:00")));
        const Entry b = make_entry(bnote(2, QStringLiteral("b"), QStringLiteral("PORTFOLIO"), QStringLiteral("LOW"), {}, {},
                                         QStringLiteral("2026-11-10 10:00:00")));
        QCOMPARE(a.meta.status, nb::kStatusResearching);
        QCOMPARE(b.meta.status, nb::kStatusActive);
        QVERIFY(!a.meta.stored);
        QVERIFY(a.meta.rank < b.meta.rank);
        QCOMPARE(a.meta.rank, nb::default_rank(a.updated_ms / 1000));
        // Notes Chat Mode saved (its tag) are linked even before a row exists.
        const Entry chat = make_entry(bnote(3, QStringLiteral("From a run"), QStringLiteral("RESEARCH"), QStringLiteral("MEDIUM"),
                                            QStringLiteral("agent-run")));
        QVERIFY(chat.linked);
        QCOMPARE(chat.meta.origin, nb::kOriginAgentRuns);
        QVERIFY(!a.linked);
        // A stored row overrides the defaults; review date parsed.
        nb::NoteBoardMeta m;
        m.status = QStringLiteral("review");
        m.rank = 5;
        m.review_on = QStringLiteral("2026-11-20");
        m.stored = true;
        Entry c = a;
        apply_meta(c, m);
        QCOMPARE(c.meta.status, nb::kStatusReview);
        QCOMPARE(c.meta.note_id, 1);
        QCOMPARE(c.review, QDate(2026, 11, 20));
    }

    // ── Lane grouping / ordering ───────────────────────────────────────────
    void lanes_by_status_and_order() {
        QVector<Entry> all;
        all << with_status(make_entry(bnote(1, QStringLiteral("one"))), nb::kStatusActive, 2.0);
        all << with_status(make_entry(bnote(2, QStringLiteral("two"))), nb::kStatusActive, 1.0);
        all << with_status(make_entry(bnote(3, QStringLiteral("three"))), nb::kStatusIdea, 0.0);
        Entry archived = with_status(make_entry(bnote(4, QStringLiteral("old"))), nb::kStatusClosed, 0.0);
        archived.note.is_archived = true;
        all << archived;

        BoardDef def; // status grouping, nothing filtered
        Layout l = build_board(all, def, {}, fixed_now());
        // Every workflow lane is there even when empty; ARCHIVED only behind the toggle.
        QCOMPARE(lane_keys(l), nb::status_codes());
        QCOMPARE(card_ids(*lane_of(l, nb::kStatusActive)), QList<int>({2, 1})); // manual rank order
        QCOMPARE(card_ids(*lane_of(l, nb::kStatusIdea)), QList<int>({3}));
        QCOMPARE(l.shown, 3);
        QCOMPARE(l.total, 4);
        def.filter.show_archived = true;
        l = build_board(all, def, {}, fixed_now());
        QCOMPARE(lane_keys(l).last(), kLaneArchived);
        QCOMPARE(card_ids(*lane_of(l, kLaneArchived)), QList<int>({4}));

        // The user's lane order wins; unknown saved keys are ignored.
        def.filter.show_archived = false;
        def.lane_order.insert(QStringLiteral("status"), {nb::kStatusClosed, QStringLiteral("NOPE"), nb::kStatusIdea});
        l = build_board(all, def, {}, fixed_now());
        QCOMPARE(lane_keys(l), QStringList({nb::kStatusClosed, nb::kStatusIdea, nb::kStatusResearching,
                                            nb::kStatusActive, nb::kStatusReview}));
        QCOMPARE(apply_lane_order({"a", "b", "c"}, {"c", "x", "a"}), QStringList({"c", "a", "b"}));
    }

    void lanes_by_ticker_tag_priority_date() {
        QVector<Entry> all;
        all << make_entry(bnote(1, QStringLiteral("aapl"), QStringLiteral("GENERAL"), QStringLiteral("HIGH"),
                                QStringLiteral("Macro, rates"), QStringLiteral("AAPL, MSFT"), QStringLiteral("2026-11-17 09:00:00")));
        all << make_entry(bnote(2, QStringLiteral("msft"), QStringLiteral("GENERAL"), QStringLiteral("LOW"),
                                QStringLiteral("macro"), QStringLiteral("MSFT"), QStringLiteral("2026-11-14 09:00:00")));
        all << make_entry(bnote(3, QStringLiteral("body only"), QStringLiteral("GENERAL"), QStringLiteral("CRITICAL"), {},
                                {}, QStringLiteral("2026-10-01 09:00:00"), QStringLiteral("Long $NVDA here")));
        all << make_entry(bnote(4, QStringLiteral("nothing"), QStringLiteral("GENERAL"), QStringLiteral("?")));

        BoardDef def;
        def.group_by = GroupBy::Ticker;
        Layout l = build_board(all, def, {}, fixed_now());
        // One lane per first ticker, the biggest first, "no ticker" last.
        QCOMPARE(lane_keys(l), QStringList({QStringLiteral("AAPL"), QStringLiteral("MSFT"), QStringLiteral("NVDA"), kLaneNone}));
        QCOMPARE(card_ids(*lane_of(l, QStringLiteral("NVDA"))), QList<int>({3})); // a $CASHTAG counts
        QCOMPARE(l.tickers.first(), QStringLiteral("MSFT"));                      // most used among shown cards

        def.group_by = GroupBy::Tag;
        l = build_board(all, def, {}, fixed_now());
        QCOMPARE(lane_keys(l), QStringList({QStringLiteral("macro"), kLaneNone}));
        QCOMPARE(lane_of(l, QStringLiteral("macro"))->title, QStringLiteral("Macro")); // first spelling
        QCOMPARE(l.top_tags.first().first, QStringLiteral("Macro"));
        QCOMPARE(l.top_tags.first().second, 2);

        def.group_by = GroupBy::Priority;
        l = build_board(all, def, {}, fixed_now());
        QCOMPARE(lane_keys(l), QStringList({QStringLiteral("CRITICAL"), QStringLiteral("HIGH"), QStringLiteral("MEDIUM"),
                                            QStringLiteral("LOW"), kLaneNone}));
        QCOMPARE(card_ids(*lane_of(l, QStringLiteral("MEDIUM"))), QList<int>());

        def.group_by = GroupBy::Date;
        const QDateTime now = QDateTime(QDate(2026, 11, 17), QTime(18, 0, 0)); // local
        l = build_board(all, def, {}, now);
        QCOMPARE(lane_keys(l), QStringList({kLaneToday, kLaneWeek, kLaneEarlier}));
        QCOMPARE(date_bucket(0, now), kLaneEarlier);

        def.group_by = GroupBy::Category;
        l = build_board(all, def, {}, fixed_now());
        QCOMPARE(lane_keys(l), default_category_codes());
        QCOMPARE(card_ids(*lane_of(l, QStringLiteral("GENERAL"))).size(), 4);
    }

    void filters_and_search() {
        QVector<Entry> all;
        Entry a = make_entry(bnote(1, QStringLiteral("Pinned AAPL"), QStringLiteral("TRADE_IDEA"), QStringLiteral("HIGH"),
                                   QStringLiteral("ai"), QStringLiteral("AAPL")));
        a.note.is_favorite = true;
        nb::NoteBoardMeta due = a.meta;
        due.review_on = QStringLiteral("2026-11-16");
        apply_meta(a, due);
        all << a;
        all << make_entry(bnote(2, QStringLiteral("Plain"), QStringLiteral("RESEARCH"), QStringLiteral("LOW")));
        all << make_entry(bnote(3, QStringLiteral("Chat"), QStringLiteral("RESEARCH"), QStringLiteral("LOW"),
                                QStringLiteral("agent-run")));
        const QDate today(2026, 11, 17);
        const qint64 now_ms = fixed_now().toMSecsSinceEpoch();
        QVERIFY(is_due(all.at(0), today));
        QVERIFY(!is_due(all.at(1), today));

        BoardFilter f;
        QVERIFY(f.is_empty());
        f.pinned_only = true;
        QVERIFY(passes_filter(all.at(0), f, today, now_ms));
        QVERIFY(!passes_filter(all.at(1), f, today, now_ms));
        f = {};
        f.due_only = true;
        QVERIFY(passes_filter(all.at(0), f, today, now_ms));
        f = {};
        f.linked_only = true;
        QVERIFY(passes_filter(all.at(2), f, today, now_ms));
        QVERIFY(!passes_filter(all.at(1), f, today, now_ms));
        f = {};
        f.priorities = {QStringLiteral("HIGH")};
        QVERIFY(passes_filter(all.at(0), f, today, now_ms));
        QVERIFY(!passes_filter(all.at(1), f, today, now_ms));
        f = {};
        f.tag = QStringLiteral("AI");
        QVERIFY(passes_filter(all.at(0), f, today, now_ms));
        f = {};
        f.stale_days = 5; // edited 2026-11-10: 7 days ago
        QVERIFY(passes_filter(all.at(1), f, today, now_ms));
        f.stale_days = 30;
        QVERIFY(!passes_filter(all.at(1), f, today, now_ms));

        BoardDef def;
        Layout l = build_board(all, def, parse_search(QStringLiteral("$AAPL")), fixed_now());
        QCOMPARE(l.shown, 1);
        QCOMPARE(l.stats.notes, 3);
        QCOMPARE(l.stats.pinned, 1);
        QCOMPARE(l.stats.due, 1);
        QCOMPARE(l.stats.overdue, 1);
        QCOMPARE(l.stats.linked, 1);
        QCOMPARE(l.stats.tickers, 1);
        QCOMPARE(l.stats.oldest_unreviewed_days, 7);

        // Ribbon / command filter keys toggle.
        BoardFilter t = toggle_filter_key({}, QStringLiteral("high"));
        QCOMPARE(t.priorities, QStringList({QStringLiteral("HIGH")}));
        t = toggle_filter_key(t, QStringLiteral("HIGH"));
        QVERIFY(t.priorities.isEmpty());
        t = toggle_filter_key(t, QStringLiteral("week"));
        QCOMPARE(t.edited_days, 7);
        t = toggle_filter_key(t, QStringLiteral("week"));
        QCOMPARE(t.edited_days, 0);
    }

    // ── Manual order ───────────────────────────────────────────────────────
    void ranks() {
        QCOMPARE(ranks_between(std::nullopt, std::nullopt, 3), QVector<double>({0, 1, 2}));
        QCOMPARE(ranks_between(1.0, 2.0, 1), QVector<double>({1.5}));
        QCOMPARE(ranks_between(std::nullopt, 10.0, 2), QVector<double>({8.0, 9.0}));
        QCOMPARE(ranks_between(10.0, std::nullopt, 2), QVector<double>({11.0, 12.0}));
        QVERIFY(ranks_exhausted(1.0, 1.0 + 1e-9, 1));
        QVERIFY(!ranks_exhausted(1.0, 2.0, 3));

        const QVector<QPair<int, double>> lane = {{10, 0.0}, {11, 1.0}, {12, 2.0}, {13, 3.0}};
        // Move 13 to the top.
        RankPlan p = plan_insert(lane, {13}, 0);
        QCOMPARE(p.order, QVector<int>({13, 10, 11, 12}));
        QCOMPARE(p.ranks.size(), 1);
        QVERIFY(p.ranks.first().second < 0.0);
        // Move 10 between 12 and 13 (pos counts the lane as it is, 10 included).
        p = plan_insert(lane, {10}, 3);
        QCOMPARE(p.order, QVector<int>({11, 12, 10, 13}));
        QCOMPARE(p.ranks.first().second, 2.5);
        // Two cards from another lane at the bottom, in order.
        p = plan_insert(lane, {20, 21}, 4);
        QCOMPARE(p.order, QVector<int>({10, 11, 12, 13, 20, 21}));
        QCOMPARE(p.ranks, (QVector<QPair<int, double>>{{20, 4.0}, {21, 5.0}}));
        // Into an empty lane.
        p = plan_insert({}, {5}, 0);
        QCOMPARE(p.order, QVector<int>({5}));
        QCOMPARE(p.ranks.first().second, 0.0);
        // Gap exhausted: the lane is renumbered, only changed cards written.
        const QVector<QPair<int, double>> tight = {{1, 0.0}, {2, 1e-8}, {3, 2e-8}};
        p = plan_insert(tight, {3}, 1);
        QVERIFY(p.renumbered);
        QCOMPARE(p.order, QVector<int>({1, 3, 2}));
        QHash<int, double> r;
        for (const auto& x : p.ranks)
            r.insert(x.first, x.second);
        QCOMPARE(r.value(3), 1.0);
        QCOMPARE(r.value(2), 2.0);
        QVERIFY(!r.contains(1)); // already 0
    }

    // ── Drag-move rules ────────────────────────────────────────────────────
    void move_rules() {
        const Entry e = make_entry(bnote(1, QStringLiteral("x"), QStringLiteral("RESEARCH"), QStringLiteral("HIGH"),
                                         QStringLiteral("Macro, rates"), QStringLiteral("AAPL, MSFT")));
        const Entry body = make_entry(bnote(2, QStringLiteral("y"), QStringLiteral("RESEARCH"), QStringLiteral("HIGH"), {},
                                            {}, QStringLiteral("2026-11-10 10:00:00"), QStringLiteral("Long $NVDA")));
        QVERIFY(check_move(e, GroupBy::Date, kLaneToday, kLaneToday).allowed); // reorder only
        QCOMPARE(check_move(e, GroupBy::Date, kLaneToday, kLaneWeek).reason, MoveDenied::DateLane);
        QVERIFY(check_move(e, GroupBy::Ticker, QStringLiteral("AAPL"), kLaneNone).allowed);
        const MoveCheck c = check_move(body, GroupBy::Ticker, QStringLiteral("NVDA"), kLaneNone);
        QCOMPARE(c.reason, MoveDenied::TickerInText);
        QCOMPARE(c.detail, QStringLiteral("NVDA"));
        QCOMPARE(check_move(e, GroupBy::Priority, QStringLiteral("HIGH"), kLaneNone).reason, MoveDenied::NoPriority);
        QVERIFY(check_move(e, GroupBy::Status, nb::kStatusIdea, kLaneArchived).allowed);

        const qint64 now = 1000;
        auto r = apply_lane_move(e, GroupBy::Status, nb::kStatusResearching, nb::kStatusActive, {}, now);
        QVERIFY(r.meta_changed && !r.note_changed);
        QCOMPARE(r.meta.status, nb::kStatusActive);
        QCOMPARE(r.old_status, nb::kStatusResearching);
        QCOMPARE(r.meta.status_changed_ms, now);
        r = apply_lane_move(e, GroupBy::Status, nb::kStatusResearching, kLaneArchived, {}, now);
        QVERIFY(r.note.is_archived && r.note_changed && !r.meta_changed);
        Entry arch = e;
        arch.note.is_archived = true;
        r = apply_lane_move(arch, GroupBy::Status, kLaneArchived, nb::kStatusReview, {}, now);
        QVERIFY(!r.note.is_archived);
        QCOMPARE(r.meta.status, nb::kStatusReview);

        r = apply_lane_move(e, GroupBy::Category, QStringLiteral("RESEARCH"), QStringLiteral("EARNINGS"), {}, now);
        QCOMPARE(r.note.category, QStringLiteral("EARNINGS"));
        r = apply_lane_move(e, GroupBy::Priority, QStringLiteral("HIGH"), QStringLiteral("LOW"), {}, now);
        QCOMPARE(r.note.priority, QStringLiteral("LOW"));
        // Ticker: the dragged-from ticker is replaced, the others stay.
        r = apply_lane_move(e, GroupBy::Ticker, QStringLiteral("AAPL"), QStringLiteral("NVDA"), {}, now);
        QCOMPARE(r.note.tickers, QStringLiteral("NVDA, MSFT"));
        r = apply_lane_move(e, GroupBy::Ticker, QStringLiteral("AAPL"), QStringLiteral("MSFT"), {}, now);
        QCOMPARE(r.note.tickers, QStringLiteral("MSFT"));
        r = apply_lane_move(e, GroupBy::Ticker, QStringLiteral("AAPL"), kLaneNone, {}, now);
        QCOMPARE(r.note.tickers, QString());
        // Tag: the lane's spelling is used.
        r = apply_lane_move(e, GroupBy::Tag, QStringLiteral("macro"), QStringLiteral("ai"), QStringLiteral("AI"), now);
        QCOMPARE(r.note.tags, QStringLiteral("AI, rates"));
        r = apply_lane_move(e, GroupBy::Tag, QStringLiteral("macro"), kLaneNone, {}, now);
        QCOMPARE(r.note.tags, QString());
        // Same lane: nothing changes.
        r = apply_lane_move(e, GroupBy::Category, QStringLiteral("RESEARCH"), QStringLiteral("RESEARCH"), {}, now);
        QVERIFY(!r.note_changed && !r.meta_changed);
    }

    // ── Board persistence round trip ───────────────────────────────────────
    void board_round_trip() {
        BoardDef d;
        d.id = 7;
        d.name = QStringLiteral("Semis & AI");
        d.position = 3;
        d.view = BoardViewMode::Wall;
        d.group_by = GroupBy::Tag;
        d.filter.pinned_only = true;
        d.filter.has_ticker = true;
        d.filter.due_only = true;
        d.filter.linked_only = true;
        d.filter.show_archived = true;
        d.filter.priorities = {QStringLiteral("HIGH"), QStringLiteral("CRITICAL")};
        d.filter.statuses = {nb::kStatusActive};
        d.filter.categories = {QStringLiteral("TRADE_IDEA")};
        d.filter.tag = QStringLiteral("ai");
        d.filter.ticker = QStringLiteral("NVDA");
        d.filter.edited_days = 7;
        d.filter.stale_days = 30;
        d.lane_order.insert(QStringLiteral("tag"), {QStringLiteral("ai"), QStringLiteral("semis")});
        d.set_collapsed(GroupBy::Tag, QStringLiteral("semis"), true);
        d.set_soft_limit(GroupBy::Status, nb::kStatusActive, 10);
        d.wall_zoom = 0.75;
        d.wall_cx = 120.5;
        d.wall_cy = -40.0;
        d.wall_snap = false;

        const nb::NoteBoardRecord rec = d.to_record();
        QCOMPARE(rec.id, 7);
        QCOMPARE(rec.name, d.name);
        const BoardDef r = BoardDef::from_record(rec);
        QCOMPARE(r.id, d.id);
        QCOMPARE(r.name, d.name);
        QCOMPARE(r.position, 3);
        QCOMPARE(r.view, BoardViewMode::Wall);
        QCOMPARE(r.group_by, GroupBy::Tag);
        QVERIFY(r.filter == d.filter);
        QCOMPARE(r.lane_order.value(QStringLiteral("tag")), d.lane_order.value(QStringLiteral("tag")));
        QVERIFY(r.is_collapsed(GroupBy::Tag, QStringLiteral("semis")));
        QVERIFY(!r.is_collapsed(GroupBy::Tag, QStringLiteral("ai")));
        QCOMPARE(r.soft_limit(GroupBy::Status, nb::kStatusActive), 10);
        QCOMPARE(r.wall_zoom, 0.75);
        QCOMPARE(r.wall_cx, 120.5);
        QCOMPARE(r.wall_cy, -40.0);
        QVERIFY(!r.wall_snap);

        // Garbage and an empty definition give a usable default board.
        nb::NoteBoardRecord bad;
        bad.definition_json = QStringLiteral("{not json");
        const BoardDef g = BoardDef::from_record(bad);
        QCOMPARE(g.group_by, GroupBy::Status);
        QCOMPARE(g.view, BoardViewMode::Board);
        QVERIFY(g.filter.is_empty());
        QCOMPARE(g.wall_zoom, 1.0);

        const auto boards = default_boards();
        QCOMPARE(boards.size(), 4);
        QCOMPARE(boards.at(2).group_by, GroupBy::Ticker);
        QCOMPARE(boards.at(3).group_by, GroupBy::Date);
        QCOMPARE(boards.at(3).filter.tag, QStringLiteral("journal"));
        // Keys survive the string forms.
        for (GroupBy gb : {GroupBy::Status, GroupBy::Category, GroupBy::Ticker, GroupBy::Priority, GroupBy::Tag,
                           GroupBy::Date})
            QCOMPARE(group_by_from_key(group_by_key(gb)), gb);
        for (BoardViewMode v : {BoardViewMode::Board, BoardViewMode::Wall, BoardViewMode::List})
            QCOMPARE(view_mode_from_key(view_mode_key(v)), v);
    }

    // ── Command field ──────────────────────────────────────────────────────
    void commands() {
        Command c = parse_command(QStringLiteral("new trade idea AAPL"));
        QCOMPARE(c.kind, CommandKind::NewFromTemplate);
        QCOMPARE(c.template_id, QStringLiteral("trade_idea"));
        QCOMPARE(c.ticker, QStringLiteral("AAPL"));
        c = parse_command(QStringLiteral("new earnings prep for $msft"));
        QCOMPARE(c.template_id, QStringLiteral("earnings_prep"));
        QCOMPARE(c.ticker, QStringLiteral("MSFT"));
        c = parse_command(QStringLiteral("new post-trade review NVDA"));
        QCOMPARE(c.template_id, QStringLiteral("post_trade_review"));
        c = parse_command(QStringLiteral("new journal"));
        QCOMPARE(c.kind, CommandKind::NewFromTemplate);
        QCOMPARE(c.template_id, QStringLiteral("daily_journal"));
        QVERIFY(c.ticker.isEmpty());
        c = parse_command(QStringLiteral("new Fed minutes takeaways"));
        QCOMPARE(c.kind, CommandKind::NewNote);
        QCOMPARE(c.text, QStringLiteral("Fed minutes takeaways"));
        c = parse_command(QStringLiteral("new $nvda"));
        QCOMPARE(c.kind, CommandKind::NewNote);
        QCOMPARE(c.ticker, QStringLiteral("NVDA"));
        QCOMPARE(parse_command(QStringLiteral("new")).kind, CommandKind::NewNote);

        c = parse_command(QStringLiteral("group by ticker"));
        QCOMPARE(c.kind, CommandKind::GroupBy);
        QCOMPARE(c.group, GroupBy::Ticker);
        QCOMPARE(parse_command(QStringLiteral("group date")).group, GroupBy::Date);
        QCOMPARE(parse_command(QStringLiteral("group by colour")).kind, CommandKind::Search);

        c = parse_command(QStringLiteral("view wall"));
        QCOMPARE(c.kind, CommandKind::View);
        QCOMPARE(c.view, BoardViewMode::Wall);
        QCOMPARE(parse_command(QStringLiteral("list")).view, BoardViewMode::List);
        QCOMPARE(parse_command(QStringLiteral("board")).kind, CommandKind::View);
        c = parse_command(QStringLiteral("board Trade ideas"));
        QCOMPARE(c.kind, CommandKind::Board);
        QCOMPARE(c.text, QStringLiteral("Trade ideas"));

        c = parse_command(QStringLiteral("open nvda long"));
        QCOMPARE(c.kind, CommandKind::Open);
        QCOMPARE(c.text, QStringLiteral("nvda long"));
        QCOMPARE(parse_command(QStringLiteral("open")).kind, CommandKind::Search);

        c = parse_command(QStringLiteral("filter pinned"));
        QCOMPARE(c.kind, CommandKind::Filter);
        QCOMPARE(c.text, QStringLiteral("pinned"));
        QCOMPARE(parse_command(QStringLiteral("show due")).text, QStringLiteral("due"));
        QCOMPARE(parse_command(QStringLiteral("filter has ticker")).text, QStringLiteral("ticker"));
        QCOMPARE(parse_command(QStringLiteral("filter nonsense")).kind, CommandKind::Search);
        QCOMPARE(parse_command(QStringLiteral("clear")).kind, CommandKind::Clear);
        QCOMPARE(parse_command(QStringLiteral("?")).kind, CommandKind::Help);

        // Searches stay searches; quotes force a search.
        c = parse_command(QStringLiteral("$AAPL #macro capex"));
        QCOMPARE(c.kind, CommandKind::Search);
        QVERIFY(!c.is_command());
        QCOMPARE(parse_command(QStringLiteral("\"list\"")).kind, CommandKind::Search);
        QCOMPARE(parse_command(QStringLiteral("new trade idea about rates")).kind, CommandKind::Search);
        QCOMPARE(parse_command(QString()).kind, CommandKind::Search);

        // "open <title>" lookup.
        QVector<Entry> all;
        all << make_entry(bnote(1, QStringLiteral("NVDA long into GTC"), QStringLiteral("GENERAL"), QStringLiteral("LOW"), {},
                                {}, QStringLiteral("2026-11-10 10:00:00")));
        all << make_entry(bnote(2, QStringLiteral("NVDA"), QStringLiteral("GENERAL"), QStringLiteral("LOW"), {}, {},
                                QStringLiteral("2026-11-01 10:00:00")));
        all << make_entry(bnote(3, QStringLiteral("Rates and NVDA"), QStringLiteral("GENERAL"), QStringLiteral("LOW"), {}, {},
                                QStringLiteral("2026-11-15 10:00:00")));
        QCOMPARE(find_note_by_title(all, QStringLiteral("nvda")), 2);
        QCOMPARE(find_note_by_title(all, QStringLiteral("nvda long")), 1);
        QCOMPARE(find_note_by_title(all, QStringLiteral("and nvda")), 3);
        QCOMPARE(find_note_by_title(all, QStringLiteral("tsla")), 0);
    }

    void templates_have_lanes() {
        const auto t = builtin_templates();
        QCOMPARE(t.at(0).status, nb::kStatusIdea);
        QCOMPARE(t.at(1).status, nb::kStatusResearching);
        QCOMPARE(t.at(3).status, nb::kStatusReview);
        for (const auto& d : t)
            QVERIFY(nb::status_codes().contains(d.status));
    }

    // ── Board menu: move this board up / down (positions stored) ─────────────
    void reorder_boards_moves_and_renumbers() {
        QVector<BoardDef> boards = default_boards(); // All, Trade ideas, Earnings, Journal (0..3)
        const auto names = [&boards]() {
            QStringList out;
            for (const BoardDef& b : boards)
                out << b.preset;
            return out;
        };
        // Earnings up one: it and Trade ideas swap; only those two rows change.
        QCOMPARE(reorder_boards(boards, 2, 1), (QVector<int>{1, 2}));
        QCOMPARE(names(), (QStringList{"all", "earnings", "trade_ideas", "journal"}));
        for (int i = 0; i < boards.size(); ++i)
            QCOMPARE(boards.at(i).position, i);
        // The first board down one, the last board up one.
        QCOMPARE(reorder_boards(boards, 0, 1), (QVector<int>{0, 1}));
        QCOMPARE(names(), (QStringList{"earnings", "all", "trade_ideas", "journal"}));
        QCOMPARE(reorder_boards(boards, 3, 2), (QVector<int>{2, 3}));
        QCOMPARE(names(), (QStringList{"earnings", "all", "journal", "trade_ideas"}));
        // Out of range or no move: nothing changes.
        const QStringList before = names();
        QVERIFY(reorder_boards(boards, 0, -1).isEmpty());
        QVERIFY(reorder_boards(boards, 3, 4).isEmpty());
        QVERIFY(reorder_boards(boards, 1, 1).isEmpty());
        QVERIFY(reorder_boards(boards, -1, 0).isEmpty());
        QCOMPARE(names(), before);
        // Gaps left by deleted boards are closed on the next move (every row renumbered).
        boards[0].position = 0;
        boards[1].position = 4;
        boards[2].position = 7;
        boards[3].position = 9;
        QCOMPARE(reorder_boards(boards, 1, 0), (QVector<int>{0, 1, 2, 3}));
        for (int i = 0; i < boards.size(); ++i)
            QCOMPARE(boards.at(i).position, i);
        // The stored record carries the new position.
        QCOMPARE(boards.at(0).to_record().position, 0);
        QCOMPARE(boards.at(3).to_record().position, 3);
    }

    // ── Chat Mode hand-off event: which run a new note came from ─────────────
    void chat_run_from_event_reads_the_run() {
        const QVariantMap ev{{QStringLiteral("id"), 5},
                             {QStringLiteral("source"), nb::kOriginAgentRuns},
                             {QStringLiteral("run_id"), QStringLiteral(" 9f8e7d6c-1111 ")}};
        QCOMPARE(chat_run_from_event(ev).value_or(QStringLiteral("none")), QStringLiteral("9f8e7d6c-1111"));
        // Older spellings of the key.
        QCOMPARE(chat_run_from_event({{QStringLiteral("source"), nb::kOriginAgentRuns}, {QStringLiteral("run"), "r1"}})
                     .value_or(QString()),
                 QStringLiteral("r1"));
        QCOMPARE(chat_run_from_event({{QStringLiteral("source"), nb::kOriginAgentRuns},
                                      {QStringLiteral("agent_run_id"), "r2"}})
                     .value_or(QString()),
                 QStringLiteral("r2"));
        // From Chat Mode without a run id: linked, run unknown.
        QCOMPARE(chat_run_from_event({{QStringLiteral("source"), nb::kOriginAgentRuns}}).value_or(QStringLiteral("x")),
                 QString());
        // Not from Chat Mode.
        QVERIFY(!chat_run_from_event({{QStringLiteral("source"), QStringLiteral("notes_screen")}}).has_value());
        QVERIFY(!chat_run_from_event({}).has_value());
        // The footer fallback (8-hex prefix) is what OPEN CHAT MODE sends when no full id is stored.
        QCOMPARE(chat_run_ref_from_body(QStringLiteral("Body\n\n---\nSaved from Chat Mode: agent run “X” (1A2B3C4D) · t")),
                 QStringLiteral("1a2b3c4d"));
    }
};

QTEST_GUILESS_MAIN(TestNotesBoardLogic)
#include "tst_notes_board_logic.moc"
