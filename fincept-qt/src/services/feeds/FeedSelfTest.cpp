#include "services/feeds/FeedSelfTest.h"

#include "services/feeds/FeedParseUtil.h"
#include "services/feeds/FeedScraper.h"
#include "services/feeds/FeedTypes.h"

#include <QDateTime>
#include <QString>

#include <cstdio>

namespace fincept::feeds {

int run_feed_selftest() {
    int failures = 0;
    auto check = [&](const char* label, bool ok) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok)
            ++failures;
    };

    { // RSS 2.0 auto
        FeedSubscription s;
        s.id = "t1";
        s.name = "RSS";
        s.format = FeedFormat::Auto;
        const QByteArray rss = "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel>"
                               "<item><title>Hello</title><link>http://x/1</link>"
                               "<description>&lt;b&gt;Body&lt;/b&gt;</description>"
                               "<pubDate>Wed, 04 Jun 2025 10:00:00 GMT</pubDate></item>"
                               "<item><title>World</title><link>http://x/2</link></item>"
                               "</channel></rss>";
        const auto items = FeedScraper::parse(rss, s);
        check("rss: 2 items", items.size() == 2);
        check("rss: title", !items.isEmpty() && items[0].title == "Hello");
        check("rss: html stripped", !items.isEmpty() && items[0].summary == "Body");
    }
    { // Atom auto
        FeedSubscription s;
        s.id = "t2";
        s.name = "Atom";
        s.format = FeedFormat::Auto;
        const QByteArray atom = "<?xml version=\"1.0\"?><feed xmlns=\"http://www.w3.org/2005/Atom\">"
                                "<entry><title>AT</title><link rel=\"alternate\" href=\"http://a/1\"/>"
                                "<summary>S</summary><updated>2025-06-04T10:00:00Z</updated></entry></feed>";
        const auto items = FeedScraper::parse(atom, s);
        check("atom: 1 item", items.size() == 1);
        check("atom: link href", !items.isEmpty() && items[0].link == "http://a/1");
    }
    { // JSON auto (items[] container)
        FeedSubscription s;
        s.id = "t3";
        s.name = "JSON";
        s.format = FeedFormat::Auto;
        const QByteArray json = R"({"items":[{"title":"J1","url":"http://j/1"},{"title":"J2"}]})";
        const auto items = FeedScraper::parse(json, s);
        check("json: 2 items", items.size() == 2);
        check("json: link from url", !items.isEmpty() && items[0].link == "http://j/1");
    }
    { // CSV auto
        FeedSubscription s;
        s.id = "t4";
        s.name = "CSV";
        s.format = FeedFormat::Csv;
        const QByteArray csv = "name,price\nAAPL,200\nMSFT,410\n";
        const auto items = FeedScraper::parse(csv, s);
        check("csv: 2 rows", items.size() == 2);
        check("csv: field price", !items.isEmpty() && items[0].fields.value("price") == "200");
    }
    const QByteArray custom_xml =
        "<data><record><headline>H1</headline><url>http://m/1</url><sym>ABC</sym></record>"
        "<record><headline>H2</headline><url>http://m/2</url><sym>XYZ</sym></record></data>";

    { // Manual XML mapping (role-based field list)
        FeedSubscription s;
        s.id = "t5";
        s.name = "Manual";
        s.format = FeedFormat::Xml;
        s.parse_mode = ParseMode::Manual;
        s.mapping.item_selector = "record";
        s.mapping.fields = {
            {"Title", "headline", "title"},
            {"Link", "url", "link"},
            {"Ticker", "sym", ""},
        };
        const auto items = FeedScraper::parse(custom_xml, s);
        check("manual: 2 records", items.size() == 2);
        check("manual: mapped title role", !items.isEmpty() && items[0].title == "H1");
        check("manual: mapped link role", !items.isEmpty() && items[0].link == "http://m/1");
        check("manual: extra column", !items.isEmpty() && items[0].fields.value("Ticker") == "ABC");
    }
    { // Field discovery on the same custom XML
        FeedSubscription s;
        s.id = "t5d";
        s.format = FeedFormat::Xml;
        const auto schema = FeedScraper::discover(custom_xml, s);
        check("discover: ok", schema.ok);
        check("discover: item selector", schema.item_selector == "record");
        QStringList paths;
        for (const auto& f : schema.fields)
            paths << f.path;
        check("discover: found headline", paths.contains("headline"));
        check("discover: found sym", paths.contains("sym"));
    }
    { // Field discovery on JSON
        FeedSubscription s;
        s.id = "t5j";
        s.format = FeedFormat::Json;
        const QByteArray json = R"({"results":[{"hl":"X","u":"http://k/1","t":"2025-01-01"}]})";
        const auto schema = FeedScraper::discover(json, s);
        check("discover json: ok", schema.ok);
        check("discover json: array path", schema.item_selector == "results");
        QStringList paths;
        for (const auto& f : schema.fields)
            paths << f.path;
        check("discover json: keys", paths.contains("hl") && paths.contains("u"));
    }
    { // HTML access-denied guard
        FeedSubscription s;
        s.id = "t6";
        s.name = "HTML";
        s.format = FeedFormat::Auto;
        const auto items = FeedScraper::parse("<!doctype html><html><body>nope</body></html>", s);
        check("html: zero items (guarded)", items.isEmpty());
    }
    { // Date/time parsing across real-world pubDate formats
        const qint64 ref = QDateTime::fromString("2025-06-04T10:00:00Z", Qt::ISODate).toSecsSinceEpoch();
        QString disp;
        check("date rfc822 GMT", parse_feed_datetime("Wed, 04 Jun 2025 10:00:00 GMT", disp) == ref);
        check("date rfc822 +0000", parse_feed_datetime("Wed, 04 Jun 2025 10:00:00 +0000", disp) == ref);
        check("date rfc822 no-day", parse_feed_datetime("04 Jun 2025 10:00:00 +0000", disp) == ref);
        check("date iso Z", parse_feed_datetime("2025-06-04T10:00:00Z", disp) == ref);
        check("date iso ms Z", parse_feed_datetime("2025-06-04T10:00:00.000Z", disp) == ref);
        check("date iso offset", parse_feed_datetime("2025-06-04T15:30:00+05:30", disp) == ref);
        check("date rfc822 EDT", parse_feed_datetime("Wed, 04 Jun 2025 06:00:00 EDT", disp) == ref);
        check("date epoch sec", parse_feed_datetime("1717495200", disp) == 1717495200);
        check("date epoch ms", parse_feed_datetime("1717495200000", disp) == 1717495200);
        check("date space sep (parsed)", parse_feed_datetime("2025-06-04 10:00:00", disp) != 0);
        check("date date-only (parsed)", parse_feed_datetime("2025-06-04", disp) != 0);
        check("date dd-MMM-yyyy HH:mm:ss", parse_feed_datetime("05-Jun-2026 14:35:36", disp) != 0);
        check("date dd/MMM/yyyy", parse_feed_datetime("05/Jun/2026", disp) != 0);
        check("date garbage -> 0", parse_feed_datetime("not a date", disp) == 0);
    }

    std::printf("\nfeed selftest: %s (%d failures)\n", failures == 0 ? "OK" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}

} // namespace fincept::feeds
