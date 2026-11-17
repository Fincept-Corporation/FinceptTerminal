// Unit tests for the News tab's feed reading (Qt Core only, header-only unit):
//   src/services/news/NewsFeedParse.h — publication-time parsing, RSS / RDF /
//   Atom parsing, the time policy, the canonical order and the source cap.
//
// Every "now" is explicit, so the suite does not depend on the clock or on the
// machine's time zone.

#include "services/news/NewsFeedParse.h"

#include <QString>
#include <QTest>

using namespace fincept::services::news_feed;

namespace {

/// 2026-11-16 07:29:00 UTC
constexpr qint64 kNow = 1794814140;

qint64 tnf_utc(int y, int mo, int d, int h, int mi, int s) {
    return QDateTime(QDate(y, mo, d), QTime(h, mi, s), QTimeZone::UTC).toSecsSinceEpoch();
}

struct TnfArticle {
    QString id;
    QString source;
    qint64 sort_ts = 0;
};

/// An article that can be undated (like services::NewsArticle).
struct TnfTimedArticle {
    QString id;
    QString source;
    qint64 sort_ts = 0;
    bool undated = false;
};

} // namespace

class TestNewsFeedParse : public QObject {
    Q_OBJECT

  private slots:
    void parses_rfc822_variants() {
        const qint64 t = tnf_utc(2026, 11, 16, 7, 22, 41);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 07:22:41 GMT"), t);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 07:22:41 +0000"), t);
        QCOMPARE(parse_time("16 Nov 2026 07:22:41 UT"), t);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 07:22:41 Z"), t);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 07:22:41 +0000 (UTC)"), t);
        // Numeric zones with and without a colon (Korea Herald sends +09:00).
        QCOMPARE(parse_time("Mon, 16 Nov 2026 16:22:41 +0900"), t);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 16:22:41 +09:00"), t);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 12:52:41 +0530"), t);
        // Named zones.
        QCOMPARE(parse_time("Mon, 16 Nov 2026 02:22:41 EST"), t);
        QCOMPARE(parse_time("Sun, 15 Nov 2026 23:22:41 PST"), t);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 12:52:41 IST"), t);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 08:22:41 CET"), t);
        QCOMPARE(parse_time("Mon, 16 Nov 2026 16:22:41 JST"), t);
        // Double spaces (EIA), single-digit day, no day name, missing seconds.
        QCOMPARE(parse_time("Fri, 13 Nov 2026  09:00:00 EST"), tnf_utc(2026, 11, 13, 14, 0, 0));
        QCOMPARE(parse_time("Fri, 6 Nov 2026 09:00:00 GMT"), tnf_utc(2026, 11, 6, 9, 0, 0));
        QCOMPARE(parse_time("16 Nov 2026 07:22 GMT"), tnf_utc(2026, 11, 16, 7, 22, 0));
        // Two-digit year, full month name.
        QCOMPARE(parse_time("Mon, 16 Nov 26 07:22:41 GMT"), t);
        QCOMPARE(parse_time("16 November 2026 07:22:41 GMT"), t);
        // GMT+hh:mm form.
        QCOMPARE(parse_time("Mon, 16 Nov 2026 12:52:41 GMT+05:30"), t);
    }

    void parses_iso8601_variants() {
        const qint64 t = tnf_utc(2026, 11, 16, 7, 22, 41);
        QCOMPARE(parse_time("2026-11-16T07:22:41Z"), t);
        QCOMPARE(parse_time("2026-11-16T07:22:41.123Z"), t);
        QCOMPARE(parse_time("2026-11-16T07:22:41.123456789+00:00"), t);
        QCOMPARE(parse_time("2026-11-16T12:52:41+05:30"), t);
        QCOMPARE(parse_time("2026-11-16T12:52:41+0530"), t);
        QCOMPARE(parse_time("2026-11-16T02:22:41-05"), t);
        QCOMPARE(parse_time("2026-11-16 07:22:41"), t);         // space, no zone: UTC
        QCOMPARE(parse_time("2026-11-16 12:52:41 +05:30"), t);  // space before the offset
        QCOMPARE(parse_time("  2026-11-16T07:22:41Z \n"), t);   // CDATA whitespace
        QCOMPARE(parse_time("2026-01-12T08:21:00.005-05:00"), tnf_utc(2026, 1, 12, 13, 21, 0)); // Blogger
        QCOMPARE(parse_time("2026-11-16"), tnf_utc(2026, 11, 16, 0, 0, 0));
    }

    void parses_other_forms() {
        QCOMPARE(parse_time("Nov 16, 2026 7:22 PM EST"), tnf_utc(2026, 11, 17, 0, 22, 0));
        QCOMPARE(parse_time("November 16, 2026 7:22am"), tnf_utc(2026, 11, 16, 7, 22, 0));
        QCOMPARE(parse_time("Nov 01, 2026"), tnf_utc(2026, 11, 1, 0, 0, 0)); // Sixth Tone
        QCOMPARE(parse_time("Mon Nov 16 07:22:41 UTC 2026"), tnf_utc(2026, 11, 16, 7, 22, 41));
        QCOMPARE(parse_time("16-Nov-2026 07:22:41"), tnf_utc(2026, 11, 16, 7, 22, 41));
        QCOMPARE(parse_time("1794813761"), qint64(1794813761));
        QCOMPARE(parse_time("1794813761000"), qint64(1794813761));
    }

    void naive_times_use_the_feed_offset() {
        // The RBI sends Indian wall-clock time with no zone.
        QCOMPARE(parse_time("Mon, 16 Nov 2026 12:05:00", 330), tnf_utc(2026, 11, 16, 6, 35, 0));
        QCOMPARE(parse_time("Mon, 16 Nov 2026 12:05:00"), tnf_utc(2026, 11, 16, 12, 5, 0));
        // An explicit zone always wins over the feed's default.
        QCOMPARE(parse_time("Mon, 16 Nov 2026 12:05:00 GMT", 330), tnf_utc(2026, 11, 16, 12, 5, 0));
        QCOMPARE(parse_time("2026-11-16T12:05:00", 330), tnf_utc(2026, 11, 16, 6, 35, 0));
    }

    void rejects_non_dates() {
        QCOMPARE(parse_time(""), qint64(0));
        QCOMPARE(parse_time("   "), qint64(0));
        QCOMPARE(parse_time("yesterday"), qint64(0));
        QCOMPARE(parse_time("Fri, 09 Oct  09:00:00 EST"), qint64(0)); // no year (EIA typo)
        QCOMPARE(parse_time("Mon, 31 Feb 2026 07:00:00 GMT"), qint64(0));
        QCOMPARE(parse_time("0000-00-00T00:00:00Z"), qint64(0));
        QCOMPARE(parse_time("1970-01-01T00:00:00Z"), qint64(0)); // implausible default
    }

    void parses_rss2_with_cdata_bom_and_namespaces() {
        const QByteArray xml =
            "\xEF\xBB\xBF\r\n<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            "<rss version=\"2.0\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
            "xmlns:media=\"http://search.yahoo.com/mrss/\" xmlns:content=\"http://purl.org/rss/1.0/modules/content/\">"
            "<channel><title>T</title><lastBuildDate>Mon, 16 Nov 2026 07:20:00 GMT</lastBuildDate>"
            "<item><title><![CDATA[ Rand &amp; <b>dollar</b> ]]></title>"
            "<media:title>Media title</media:title>"
            "<link>https://example.com/a</link>"
            "<description><![CDATA[<p>The rand&#8217;s fall &mdash; again.</p>]]></description>"
            "<pubDate><![CDATA[ Mon, 16 Nov 2026 07:00:00 +0000 ]]></pubDate></item>"
            "<item><title>Second</title><guid isPermaLink=\"true\">https://example.com/b</guid>"
            "<dc:date>2026-11-16T06:00:00Z</dc:date><content:encoded>&lt;p&gt;Body&lt;/p&gt;</content:encoded></item>"
            "</channel></rss>";
        const Feed f = parse_feed(xml);
        QVERIFY(f.well_formed);
        QCOMPARE(f.items.size(), 2);
        QCOMPARE(f.items[0].title, QStringLiteral("Rand & dollar"));
        QCOMPARE(f.items[0].link, QStringLiteral("https://example.com/a"));
        QCOMPARE(clean_text(f.items[0].summary), QString::fromUtf8("The rand’s fall — again."));
        QCOMPARE(f.items[0].ts, tnf_utc(2026, 11, 16, 7, 0, 0));
        QCOMPARE(f.items[1].link, QStringLiteral("https://example.com/b"));
        QCOMPARE(f.items[1].ts, tnf_utc(2026, 11, 16, 6, 0, 0));
        QCOMPARE(clean_text(f.items[1].summary), QStringLiteral("Body"));
        QCOMPARE(f.feed_ts, tnf_utc(2026, 11, 16, 7, 20, 0));
    }

    void parses_atom_published_over_updated() {
        const QByteArray xml =
            "<?xml version=\"1.0\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><updated>2026-11-16T07:00:00Z</updated>"
            "<entry><title type=\"html\">A &amp;lt;b&amp;gt;bold&amp;lt;/b&amp;gt; move</title>"
            "<link rel=\"self\" href=\"https://example.com/self\"/>"
            "<link rel=\"alternate\" type=\"text/html\" href=\"https://example.com/story\"/>"
            "<updated>2026-11-16T07:10:00Z</updated><published>2026-11-15T22:00:00-05:00</published>"
            "<summary>Short</summary></entry>"
            "<entry><title>Only updated</title><link href=\"https://example.com/u\"/>"
            "<updated>2026-11-16T05:00:00Z</updated></entry></feed>";
        const Feed f = parse_feed(xml);
        QVERIFY(f.well_formed);
        QCOMPARE(f.items.size(), 2);
        QCOMPARE(f.items[0].title, QStringLiteral("A bold move"));
        QCOMPARE(f.items[0].link, QStringLiteral("https://example.com/story"));
        QCOMPARE(f.items[0].ts, tnf_utc(2026, 11, 16, 3, 0, 0)); // published, not updated
        QCOMPARE(f.items[1].ts, tnf_utc(2026, 11, 16, 5, 0, 0)); // updated as the fallback
        QCOMPARE(f.feed_ts, tnf_utc(2026, 11, 16, 7, 0, 0));
    }

    void parses_rdf() {
        const QByteArray xml =
            "<?xml version=\"1.0\"?><rdf:RDF xmlns=\"http://purl.org/rss/1.0/\" "
            "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "<channel rdf:about=\"x\"><title>C</title><items><rdf:Seq><rdf:li resource=\"https://e.com/1\"/></rdf:Seq>"
            "</items></channel>"
            "<item rdf:about=\"https://e.com/1\"><title>RDF one</title><link>https://e.com/1</link>"
            "<dc:date>2026-11-16T09:00:00+02:00</dc:date></item></rdf:RDF>";
        const Feed f = parse_feed(xml);
        QCOMPARE(f.items.size(), 1);
        QCOMPARE(f.items[0].title, QStringLiteral("RDF one"));
        QCOMPARE(f.items[0].ts, tnf_utc(2026, 11, 16, 7, 0, 0));
    }

    void decodes_declared_latin1_and_mislabelled_utf8() {
        QByteArray latin1 = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><rss><channel><item><title>Neg";
        latin1 += '\xF3'; // ó in Latin-1
        latin1 += "cios \x93quoted\x94</title><pubDate>Mon, 16 Nov 2026 07:00:00 GMT</pubDate></item></channel></rss>";
        Feed f = parse_feed(latin1);
        QCOMPARE(f.items.size(), 1);
        QCOMPARE(f.items[0].title, QString::fromUtf8("Negócios “quoted”")); // cp1252 quotes

        const QByteArray utf8 = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><rss><channel><item><title>"
                                "Neg\xC3\xB3" "cios</title></item></channel></rss>";
        f = parse_feed(utf8);
        QCOMPARE(f.items.size(), 1);
        QCOMPARE(f.items[0].title, QString::fromUtf8("Negócios"));
    }

    void repairs_html_entities_and_bare_ampersands() {
        const QByteArray xml = "<rss><channel>"
                               "<item><title>AT&T&nbsp;earnings &mdash; beat</title>"
                               "<pubDate>Mon, 16 Nov 2026 07:00:00 GMT</pubDate></item>"
                               "<item><title>Second &rsquo;s</title></item></channel></rss>";
        const Feed f = parse_feed(xml);
        QCOMPARE(f.items.size(), 2);
        QCOMPARE(f.items[0].title, QString::fromUtf8("AT&T earnings — beat"));
        QCOMPARE(f.items[1].title, QString::fromUtf8("Second ’s"));
    }

    void detects_html_pages() {
        QVERIFY(looks_like_html("\xEF\xBB\xBF  <!DOCTYPE html><html><body>Access denied</body></html>"));
        QVERIFY(looks_like_html("<html lang=\"en\">"));
        QVERIFY(!looks_like_html("\xEF\xBB\xBF<?xml version=\"1.0\"?><rss/>"));
    }

    void time_policy_drops_garbage_and_shifts_fast_feeds() {
        QVector<Item> items(3);
        items[0].ts = kNow + 4 * 3600 + 600;  // 4h10 ahead: zone mislabelled
        items[1].ts = kNow + 3 * 3600;
        items[2].ts = kNow + 20 * 3600;       // garbage: beyond any zone error
        const qint64 shift = normalize_times(items, 0, kNow);
        QCOMPARE(shift, qint64(4 * 3600 + 900)); // rounded up to 15 min
        QVERIFY(items[0].ts <= kNow);
        QCOMPARE(items[0].ts - items[1].ts, qint64(3600 + 600)); // order inside the feed kept
        // The garbage date became "undated" and took its dated neighbour's time.
        QVERIFY(items[2].ts_estimated);
        QCOMPARE(items[2].ts, items[1].ts);

        QVector<Item> fine(2);
        fine[0].ts = kNow + 300; // within the tolerated skew
        fine[1].ts = kNow - 600;
        QCOMPARE(normalize_times(fine, 0, kNow), qint64(0));
        QCOMPARE(fine[0].ts, kNow + 300);
    }

    void undated_items_never_become_now() {
        // In a dated feed: nearest dated neighbour in feed order.
        QVector<Item> items(4);
        items[0].ts = 0;                  // before any dated item: takes the next one
        items[1].ts = kNow - 3600;
        items[2].ts = 0;                  // between: takes the previous (newer) one
        items[3].ts = kNow - 7200;
        normalize_times(items, kNow, kNow);
        QCOMPARE(items[0].ts, kNow - 3600);
        QCOMPARE(items[2].ts, kNow - 3600);
        QVERIFY(items[0].ts_estimated && items[2].ts_estimated && !items[1].ts_estimated);

        // A feed that dates nothing stays undated here...
        QVector<Item> undated(2);
        normalize_times(undated, 0, kNow);
        QCOMPARE(undated[0].ts, qint64(0));
        // ...and the caller's first-seen time is capped by the feed's own date.
        QCOMPARE(resolve_first_seen(0, 0, kNow), kNow);                       // first sighting
        QCOMPARE(resolve_first_seen(kNow - 5000, 0, kNow), kNow - 5000);      // remembered
        QCOMPARE(resolve_first_seen(kNow - 5000, kNow - 90000, kNow), kNow - 90000); // stale feed
        QCOMPARE(resolve_first_seen(kNow - 5000, kNow + 99999, kNow), kNow - 5000);  // bogus feed date ignored
    }

    void undated_items_never_rank_above_dated_ones_on_a_fresh_profile() {
        // Fresh profile: a feed that dates none of its items is read for the first
        // time, nothing is remembered, so its first-seen time is "now" — seconds
        // newer than anything any other source has published.
        const QByteArray undated_xml = "<rss><channel><title>U</title>"
                                       "<item><title>Undated one</title><link>https://u.example/1</link></item>"
                                       "<item><title>Undated two</title><link>https://u.example/2</link></item>"
                                       "</channel></rss>";
        Feed undated = parse_feed(undated_xml);
        QCOMPARE(undated.items.size(), 2);
        normalize_times(undated.items, undated.feed_ts, kNow);
        // Other sources: published two minutes and five hours ago.
        const QByteArray dated_xml = "<rss><channel><title>D</title>"
                                     "<item><title>Fresh</title><pubDate>Mon, 16 Nov 2026 07:27:00 GMT</pubDate></item>"
                                     "<item><title>Older</title><pubDate>Mon, 16 Nov 2026 02:29:00 GMT</pubDate></item>"
                                     "</channel></rss>";
        Feed dated = parse_feed(dated_xml);
        QCOMPARE(dated.items.size(), 2);
        normalize_times(dated.items, dated.feed_ts, kNow);

        QVector<TnfTimedArticle> fetched;
        for (int i = 0; i < undated.items.size(); ++i) {
            const ItemTime t = item_time(undated.items[i], /*remembered first seen*/ 0, undated.feed_ts, kNow);
            QVERIFY(t.undated);
            QCOMPARE(t.ts, kNow); // newer than both dated items...
            fetched.append({QStringLiteral("u%1").arg(i), QStringLiteral("UNDATED FEED"), t.ts, t.undated});
        }
        for (int i = 0; i < dated.items.size(); ++i) {
            const ItemTime t = item_time(dated.items[i], 0, dated.feed_ts, kNow);
            QVERIFY(!t.undated);
            fetched.append({QStringLiteral("d%1").arg(i), QStringLiteral("WIRE"), t.ts, t.undated});
        }
        QCOMPARE(fetched[2].sort_ts, kNow - 120);

        // ...yet in the canonical order every dated item comes first.
        sort_newest_first(fetched);
        QCOMPARE(fetched[0].id, QStringLiteral("d0"));
        QCOMPARE(fetched[1].id, QStringLiteral("d1"));
        QVERIFY(fetched[2].undated && fetched[3].undated);

        // The same when the lists are merged (screen: network list + stored list)
        // and when an undated item was remembered from an earlier sighting.
        QVector<TnfTimedArticle> stored = {{"s-old", "WIRE", kNow - 30 * 3600, false}};
        QVector<TnfTimedArticle> net = {{"u-remembered", "UNDATED FEED", kNow - 60, true},
                                        {"d-new", "WIRE", kNow - 3000, false}};
        sort_newest_first(net);
        QCOMPARE(net[0].id, QStringLiteral("d-new"));
        const auto merged = merge_sorted_unique(net, stored);
        QCOMPARE(merged.size(), 3);
        QCOMPARE(merged[0].id, QStringLiteral("d-new"));
        QCOMPARE(merged[1].id, QStringLiteral("s-old")); // a day-old dated story still ranks first
        QCOMPARE(merged[2].id, QStringLiteral("u-remembered"));
        QVERIFY(std::is_sorted(merged.cbegin(), merged.cend(), newer_first<TnfTimedArticle>));
        // The source cap keeps that order too.
        const auto capped = cap_per_source_hour(merged, 4);
        QCOMPARE(capped.last().id, QStringLiteral("u-remembered"));
    }

    void keeps_the_newest_items_of_a_firehose() {
        QVector<Item> items;
        for (int i = 0; i < 100; ++i) {
            Item it;
            it.ts = kNow - (i * 37 % 100) * 60;
            it.title = QString::number(i);
            items.append(it);
        }
        keep_newest(items, 40);
        QCOMPARE(items.size(), 40);
        for (const auto& it : items)
            QVERIFY(it.ts >= kNow - 39 * 60);
    }

    void canonical_order_is_newest_first_ties_by_id() {
        QVector<TnfArticle> v = {{"b", "X", 100}, {"a", "Y", 100}, {"c", "Z", 300}, {"d", "X", 200}};
        sort_newest_first(v);
        QCOMPARE(v[0].id, QStringLiteral("c"));
        QCOMPARE(v[1].id, QStringLiteral("d"));
        QCOMPARE(v[2].id, QStringLiteral("a")); // equal time: by id, whatever the input order
        QCOMPARE(v[3].id, QStringLiteral("b"));

        QVector<TnfArticle> primary = {{"n2", "A", 500}, {"n1", "A", 100}};
        QVector<TnfArticle> secondary = {{"s1", "B", 400}, {"n1", "B", 100}, {"s0", "B", 50}};
        const auto merged = merge_sorted_unique(primary, secondary);
        QCOMPARE(merged.size(), 4); // n1 once — the primary copy
        QCOMPARE(merged[0].id, QStringLiteral("n2"));
        QCOMPARE(merged[1].id, QStringLiteral("s1"));
        QCOMPARE(merged[2].id, QStringLiteral("n1"));
        QCOMPARE(merged[2].source, QStringLiteral("A"));
        QCOMPARE(merged[3].id, QStringLiteral("s0"));
        QVERIFY(std::is_sorted(merged.cbegin(), merged.cend(), newer_first<TnfArticle>));
    }

    void source_cap_limits_bursts_and_keeps_order() {
        QVector<TnfArticle> v;
        const qint64 hour = (kNow / 3600) * 3600;
        for (int i = 0; i < 10; ++i)
            v.append({QStringLiteral("w%1").arg(i), QStringLiteral("WIRE"), hour + 3000 - i});
        v.append({QStringLiteral("o1"), QStringLiteral("OTHER"), hour + 100});
        v.append({QStringLiteral("w-prev"), QStringLiteral("WIRE"), hour - 10}); // previous hour
        int dropped = 0;
        const auto capped = cap_per_source_hour(v, 6, &dropped);
        QCOMPARE(dropped, 4);
        QCOMPARE(capped.size(), 8);
        QCOMPARE(capped[0].id, QStringLiteral("w0"));
        QCOMPARE(capped[5].id, QStringLiteral("w5"));
        QCOMPARE(capped[6].id, QStringLiteral("o1"));
        QCOMPARE(capped[7].id, QStringLiteral("w-prev"));
    }
};

QTEST_GUILESS_MAIN(TestNewsFeedParse)
#include "tst_news_feed_parse.moc"
