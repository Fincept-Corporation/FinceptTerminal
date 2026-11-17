// tst_notes_logic.cpp — src/screens/notes/NotesLogic.h: tag / ticker lists,
// $CASHTAG extraction, previews, search, view filters, sorting, navigation
// counts, templates and the Markdown export / import round trip.
#include "screens/notes/NotesLogic.h"

#include <QTest>

using namespace fincept::screens::notes;

namespace {

// Same field names as fincept::FinancialNote (which lives behind QtSql).
struct TestNote {
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
using Entry = BasicEntry<TestNote>;

TestNote note(int id, const QString& title, const QString& body, const QString& cat = QStringLiteral("GENERAL"),
              const QString& prio = QStringLiteral("MEDIUM"), const QString& tags = {}, const QString& tickers = {},
              const QString& updated = QStringLiteral("2026-11-10 10:00:00")) {
    TestNote n;
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

QList<int> ids(const QVector<Entry>& rows) {
    QList<int> out;
    for (const auto& e : rows)
        out << e.note.id;
    return out;
}

} // namespace

class TestNotesLogic : public QObject {
    Q_OBJECT
  private slots:
    void lists() {
        QCOMPARE(split_tags(QStringLiteral(" ai, Semis ,ai;#macro\nsmall caps,,")),
                 QStringList({"ai", "Semis", "macro", "small caps"}));
        QCOMPARE(split_tickers(QStringLiteral("aapl, $msft BRK.B;  ^gspc 100 aapl 7203.t")),
                 QStringList({"AAPL", "MSFT", "BRK.B", "^GSPC", "7203.T"}));
        QCOMPARE(normalize_ticker(QStringLiteral("$eurusd=x")), QStringLiteral("EURUSD=X"));
        QVERIFY(normalize_ticker(QStringLiteral("12.5")).isEmpty());
        QVERIFY(normalize_ticker(QStringLiteral("not a ticker")).isEmpty());
        QCOMPARE(join_list({"AAPL", "MSFT"}), QStringLiteral("AAPL, MSFT"));
    }

    void cashtags() {
        const QString md = QStringLiteral("Long $NVDA and $amd, not $100 or a$B.\n"
                                          "`$CODE` stays code but $BRK.B counts.\n"
                                          "```\n$FENCED\n```\n"
                                          "Again $NVDA. End $RELIANCE.NS.");
        QCOMPARE(extract_cashtags(md), QStringList({"NVDA", "AMD", "BRK.B", "RELIANCE.NS"}));
        QVERIFY(extract_cashtags(QStringLiteral("no symbols here")).isEmpty());
        const QString linked = markdown_with_symbol_links(QStringLiteral("Buy $AAPL now\n`$X`"));
        QCOMPARE(linked, QStringLiteral("Buy [$AAPL](fincept-symbol:AAPL) now\n`$X`"));
        // Linking twice does not nest links.
        QCOMPARE(markdown_with_symbol_links(linked), linked);
    }

    void preview_and_counts() {
        const QString body = QStringLiteral("# Big Title\n\n## Thesis\n- **Data-centre** capex keeps *surprising*\n"
                                            "> quoted [link](http://x)\n\n---\n| a | b |\n|---|---|\n");
        QCOMPARE(preview_text(QStringLiteral("big title"), body),
                 QStringLiteral("Thesis  Data-centre capex keeps surprising  quoted link  a b"));
        QCOMPARE(title_from_body(QStringLiteral("\n\n## Plan for **today**\nmore")), QStringLiteral("Plan for today"));
        QCOMPARE(word_count(QStringLiteral("  one two\tthree\nfour  ")), 4);
        QCOMPARE(word_count(QString()), 0);
        QCOMPARE(priority_rank(QStringLiteral("high")), 3);
        QCOMPARE(priority_rank(QStringLiteral("CRITICAL")), 4);
        QCOMPARE(priority_rank(QStringLiteral("?")), 0);
        QCOMPARE(category_key(QStringLiteral(" research ")), QStringLiteral("RESEARCH"));
        QCOMPARE(category_key(QString()), QStringLiteral("GENERAL"));
    }

    void times() {
        const QDateTime t = parse_db_time(QStringLiteral("2026-11-13 23:03:12"));
        QVERIFY(t.isValid());
        QCOMPARE(t.offsetFromUtc(), 0); // SQLite datetime('now') is UTC
        QCOMPARE(t.toUTC().time(), QTime(23, 3, 12));
        QCOMPARE(db_time(t), QStringLiteral("2026-11-13 23:03:12"));
        QVERIFY(parse_db_time(QStringLiteral("2026-11-13T23:03:12Z")).isValid());
        QVERIFY(!parse_db_time(QStringLiteral("garbage")).isValid());
        const qint64 now = t.toMSecsSinceEpoch();
        QCOMPARE(relative_time(now - 30 * 1000, now), QStringLiteral("now"));
        QCOMPARE(relative_time(now - 5 * 60 * 1000, now), QStringLiteral("5m"));
        QCOMPARE(relative_time(now - 3 * 3600 * 1000LL, now), QStringLiteral("3h"));
        QCOMPARE(relative_time(now - 2 * 86400 * 1000LL, now), QStringLiteral("2d"));
        QVERIFY(relative_time(now - 40 * 86400 * 1000LL, now).contains(QLatin1Char(' ')));
        QVERIFY(relative_time(0, now).isEmpty());
    }

    void search_parse_and_match() {
        const SearchQuery q = parse_search(QStringLiteral("Capex $nvda #AI \"data centre\" capex"));
        QCOMPARE(q.words, QStringList({"capex", "data centre"}));
        QCOMPARE(q.tickers, QStringList({"NVDA"}));
        QCOMPARE(q.tags, QStringList({"ai"}));
        QVERIFY(parse_search(QStringLiteral("   ")).is_empty());

        const Entry e = make_entry(note(1, QStringLiteral("NVDA long"), QStringLiteral("Data centre capex; also $AMD."),
                                        QStringLiteral("trade_idea"), QStringLiteral("HIGH"), QStringLiteral("AI, semis"),
                                        QStringLiteral("NVDA")));
        QCOMPARE(e.ticker_list, QStringList({"NVDA", "AMD"}));
        QCOMPARE(e.category, QStringLiteral("TRADE_IDEA"));
        QVERIFY(matches(e, q));
        QVERIFY(matches(e, parse_search(QStringLiteral("$AM"))));   // ticker prefix while typing
        QVERIFY(matches(e, parse_search(QStringLiteral("#sem"))));  // tag prefix
        QVERIFY(!matches(e, parse_search(QStringLiteral("$TSLA"))));
        QVERIFY(!matches(e, parse_search(QStringLiteral("capex bonds"))));

        const auto r = match_ranges(QStringLiteral("Capex and CAPEX, capexcapex"), {"capex"});
        QCOMPARE(r.size(), 3); // the last two touch and merge
        QCOMPARE(r.at(0), qMakePair(0, 5));
        QCOMPARE(r.at(2), qMakePair(17, 10));
        QCOMPARE(match_ranges(QStringLiteral("abcd"), {"bc", "cd"}).value(0), qMakePair(1, 3));
    }

    void views_sort_and_counts() {
        QVector<Entry> all;
        auto a = note(1, QStringLiteral("beta"), QStringLiteral("$AAPL"), QStringLiteral("RESEARCH"),
                      QStringLiteral("LOW"), QStringLiteral("macro"), {}, QStringLiteral("2026-11-10 10:00:00"));
        auto b = note(2, QStringLiteral("Alpha"), QStringLiteral("x"), QStringLiteral("TRADE_IDEA"),
                      QStringLiteral("HIGH"), QStringLiteral("Macro, rates"), QStringLiteral("MSFT"),
                      QStringLiteral("2026-11-12 10:00:00"));
        auto c = note(3, QStringLiteral("gamma"), QStringLiteral("y"), QStringLiteral("RESEARCH"),
                      QStringLiteral("MEDIUM"), {}, {}, QStringLiteral("2026-11-11 10:00:00"));
        c.is_favorite = true;
        auto d = note(4, QStringLiteral("old"), QStringLiteral("z"), QStringLiteral("RESEARCH"));
        d.is_archived = true;
        for (const auto& n : {a, b, c, d})
            all.append(make_entry(n));

        const SearchQuery none;
        QCOMPARE(ids(filter_and_sort(all, {}, none, SortKey::Updated)), QList<int>({3, 2, 1}));
        QCOMPARE(ids(filter_and_sort(all, {}, none, SortKey::Title)), QList<int>({3, 2, 1}));
        QCOMPARE(ids(filter_and_sort(all, {}, none, SortKey::Priority)), QList<int>({3, 2, 1}));
        QVector<Entry> unpinned = all;
        sort_entries(unpinned, SortKey::Title, /*pinned_first=*/false);
        QCOMPARE(ids(unpinned), QList<int>({2, 1, 3, 4}));
        QCOMPARE(ids(filter_and_sort(all, {ViewKind::Pinned, {}}, none, SortKey::Updated)), QList<int>({3}));
        QCOMPARE(ids(filter_and_sort(all, {ViewKind::Archived, {}}, none, SortKey::Updated)), QList<int>({4}));
        QCOMPARE(ids(filter_and_sort(all, {ViewKind::Category, QStringLiteral("RESEARCH")}, none, SortKey::Updated)),
                 QList<int>({3, 1}));
        QCOMPARE(ids(filter_and_sort(all, {ViewKind::Tag, QStringLiteral("MACRO")}, none, SortKey::Updated)),
                 QList<int>({2, 1}));
        QCOMPARE(ids(filter_and_sort(all, {ViewKind::Ticker, QStringLiteral("AAPL")}, none, SortKey::Updated)),
                 QList<int>({1}));
        QCOMPARE(ids(filter_and_sort(all, {}, parse_search(QStringLiteral("$MSFT")), SortKey::Updated)),
                 QList<int>({2}));

        const NavCounts n = count_nav(all);
        QCOMPARE(n.all, 3);
        QCOMPARE(n.pinned, 1);
        QCOMPARE(n.archived, 1);
        QCOMPARE(n.categories.value(QStringLiteral("RESEARCH")), 2);
        QCOMPARE(n.tags.value(QStringLiteral("macro")), 2);
        QCOMPARE(n.tag_text.value(QStringLiteral("macro")), QStringLiteral("macro"));
        QCOMPARE(n.tickers.value(QStringLiteral("AAPL")), 1);
        QCOMPARE(keys_by_count(n.categories).first(), QStringLiteral("RESEARCH"));

        for (const QString& s : {QStringLiteral("all"), QStringLiteral("pinned"), QStringLiteral("archived"),
                                 QStringLiteral("cat:RESEARCH"), QStringLiteral("tag:ai"), QStringLiteral("ticker:NVDA")})
            QCOMPARE(ViewFilter::from_string(s).to_string(), s);
        QCOMPARE(ViewFilter::from_string(QStringLiteral("bogus")).kind, ViewKind::All);
    }

    void templates() {
        const auto t = builtin_templates();
        QCOMPARE(t.size(), 4);
        QStringList names;
        for (const auto& d : t) {
            names << d.id;
            QVERIFY(!d.body.isEmpty());
            QVERIFY(priority_rank(d.priority) > 0);
        }
        QCOMPARE(names, QStringList({"trade_idea", "earnings_prep", "daily_journal", "post_trade_review"}));
        const TemplateDef& idea = t.first();
        for (const char* section : {"## Thesis", "## Entry", "## Stop", "## Target", "## Risk"})
            QVERIFY2(idea.body.contains(QLatin1String(section)), section);

        const QDate day(2026, 11, 16);
        QCOMPARE(fill_template(idea.title, day, QStringLiteral("nvda")), QStringLiteral("Trade idea: NVDA"));
        const QString body = fill_template(idea.body, day, QStringLiteral("nvda"));
        QVERIFY(body.startsWith(QStringLiteral("## Thesis\n$NVDA")));
        QCOMPARE(extract_cashtags(body), QStringList({"NVDA"})); // inserting it links the ticker
        // No ticker: the date, and a prompt that is not a cashtag.
        QCOMPARE(fill_template(idea.title, day, {}), QStringLiteral("Trade idea: 2026-11-16"));
        QVERIFY(extract_cashtags(fill_template(idea.body, day, {})).isEmpty());
        QCOMPARE(fill_template(t.at(2).title, day, {}), QStringLiteral("Journal 2026-11-16"));
    }

    void board_rows_on_entries() {
        // Every entry carries a board row: defaults until one is stored.
        TestNote n = note(7, QStringLiteral("From Chat"), QStringLiteral("x"), QStringLiteral("earnings"),
                          QStringLiteral("HIGH"), QStringLiteral("agent-run, q3"));
        const Entry e = make_entry(n);
        QCOMPARE(e.meta.note_id, 7);
        QCOMPARE(e.meta.status, QStringLiteral("RESEARCHING"));
        QVERIFY(!e.meta.stored);
        QVERIFY(e.linked);
        n.is_archived = true;
        QCOMPARE(make_entry(n).meta.status, QStringLiteral("CLOSED"));
        QCOMPARE(priority_code(4), QStringLiteral("CRITICAL"));
        QCOMPARE(priority_code(0), QString());
        QCOMPARE(priority_codes().size(), 4);
        QCOMPARE(default_category_codes().last(), QStringLiteral("GENERAL"));
        QVERIFY(has_agent_run_tag({QStringLiteral("AGENT-RUN")}));
        QVERIFY(!has_agent_run_tag({QStringLiteral("agent-runner")}));
        // Chat Mode's footer carries the run id prefix (in any UI language).
        QCOMPARE(chat_run_ref_from_body(QStringLiteral("Body (12345678)\n\n---\nSaved from Chat Mode: agent run "
                                                       "“Q3 prep” (1A2B3C4D), step 2 · artefact")),
                 QStringLiteral("1a2b3c4d"));
        QVERIFY(chat_run_ref_from_body(QStringLiteral("No footer (1a2b3c4d)")).isEmpty());
    }

    void markdown_round_trip() {
        MarkdownDoc d;
        d.title = QStringLiteral("NVDA \"long\" into GTC: part \\2");
        d.body = QStringLiteral("\n## Thesis\nCapex $NVDA.\n\n| a | b |\n|---|---|\n\n---\nlast line\n");
        d.category = QStringLiteral("TRADE_IDEA");
        d.priority = QStringLiteral("HIGH");
        d.sentiment = QStringLiteral("BULLISH");
        d.tags = {"ai", "semis, chips", "small caps"};
        d.tickers = {"NVDA", "BRK.B"};
        d.created = QStringLiteral("2026-11-01 09:00:00");
        d.updated = QStringLiteral("2026-11-13 23:03:12");
        d.pinned = true;
        d.archived = true;
        const QString md = to_markdown(d);
        QVERIFY(md.startsWith(QStringLiteral("---\ntitle: ")));
        const MarkdownDoc r = from_markdown(md, QStringLiteral("file"));
        QCOMPARE(r.title, d.title);
        QCOMPARE(r.body, d.body);
        QCOMPARE(r.category, d.category);
        QCOMPARE(r.priority, d.priority);
        QCOMPARE(r.sentiment, d.sentiment);
        QCOMPARE(r.tags, d.tags);
        QCOMPARE(r.tickers, d.tickers);
        QCOMPARE(r.created, d.created);
        QCOMPARE(r.updated, d.updated);
        QCOMPARE(r.pinned, true);
        QCOMPARE(r.archived, true);

        // Empty body and CRLF files survive too.
        MarkdownDoc e;
        e.title = QStringLiteral("Empty");
        QCOMPARE(from_markdown(to_markdown(e), {}).body, QString());
        QString crlf = to_markdown(d);
        crlf.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
        QCOMPARE(from_markdown(crlf, {}).body, d.body);

        // Plain Markdown from elsewhere: the heading is the title.
        const MarkdownDoc h =
            from_markdown(QString(QChar(0xFEFF)) + QStringLiteral("# Earnings prep\n\nBody $MSFT\n"), QStringLiteral("x"));
        QCOMPARE(h.title, QStringLiteral("Earnings prep"));
        QCOMPARE(h.body, QStringLiteral("Body $MSFT"));
        // Plain text: the file name is the title, the text is the body.
        const MarkdownDoc p = from_markdown(QStringLiteral("just text\nline 2"), QStringLiteral("journal"));
        QCOMPARE(p.title, QStringLiteral("journal"));
        QCOMPARE(p.body, QStringLiteral("just text\nline 2"));

        QCOMPARE(safe_file_stem(QStringLiteral("a/b: c?  d")), QStringLiteral("a_b_ c_ d"));
        QCOMPARE(safe_file_stem(QStringLiteral("..")), QStringLiteral("note"));
    }
};

QTEST_GUILESS_MAIN(TestNotesLogic)
#include "tst_notes_logic.moc"
