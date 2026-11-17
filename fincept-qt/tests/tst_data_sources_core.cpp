// tst_data_sources_core — the pure layer under Data Sources: the tabular model, the connector
// envelope parser (scripts/fincept_connector.py contract), the text-format parsers and the
// error classification. QtCore only.

#include "services/connectors/ConnectorEnvelope.h"
#include "services/data_sources/SourceError.h"
#include "services/data_sources/TableParsers.h"
#include "services/data_sources/TabularData.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace fincept::services::datasources;
using fincept::services::connectors::parse_connector_envelope;

namespace {

/// JSON fixtures are written with single quotes (moc does not read raw string literals).
QByteArray J(const char* text) {
    QByteArray b(text);
    b.replace('\'', '"');
    return b;
}

} // namespace

class TestDataSourcesCore : public QObject {
    Q_OBJECT
  private slots:
    // ── envelope ──
    void envelope_single_table() {
        const QByteArray line =
            J("{'ok': true, 'data': [{'date': '2024-01-02', 'value': 1.5}, {'date': '2024-01-03', 'value': 2}],"
            "               'columns': ['date', 'value'], 'types': {'date': 'date', 'value': 'number'},"
            "               'meta': {'source': 'Frankfurter', 'as_of': '2026-11-16T21:06:48Z', 'count': 2, 'truncated': false,"
            "                        'extra': {'shape': 'rows'}}}");
        const auto r = parse_connector_envelope(line);
        QVERIFY(r.ok);
        QCOMPARE(r.data.tables.size(), 1);
        const DataTable& t = r.data.tables.first();
        QCOMPARE(t.columns, (QStringList{"date", "value"}));
        QCOMPARE(t.types[0], ColumnType::Date);
        QCOMPARE(t.types[1], ColumnType::Number);
        QCOMPARE(t.row_count(), 2);
        QCOMPARE(t.cell(1, 1).toDouble(), 2.0);
        QCOMPARE(r.data.source, QStringLiteral("Frankfurter"));
        QVERIFY(r.data.as_of.isValid());
        QCOMPARE(r.shape, QStringLiteral("rows"));
    }
    void envelope_multi_table() {
        const QByteArray line =
            J("{'ok': true, 'data': {'a': 1}, 'tables': {'gdp': {'columns': ['year', 'v'], 'types': {'year': 'integer', 'v': 'number'},"
            "               'rows': [{'year': 2023, 'v': 1.0}, {'year': 2024, 'v': 2.0}]}, 'cpi': {'columns': ['m'], 'types': {'m': 'string'},"
            "               'rows': [{'m': '2024-01'}]}}, 'meta': {'source': 'x', 'extra': {'shape': 'tables'}}}");
        const auto r = parse_connector_envelope(line);
        QVERIFY(r.ok);
        QCOMPARE(r.data.tables.size(), 2);
        const int gdp = r.data.index_of(QStringLiteral("gdp"));
        QVERIFY(gdp >= 0);
        QCOMPARE(r.data.tables[gdp].row_count(), 2);
        QCOMPARE(r.data.tables[gdp].types[0], ColumnType::Integer);
        QCOMPARE(r.data.total_rows(), 3);
    }
    void envelope_failure_codes() {
        const auto r = parse_connector_envelope(
            J("{'ok': false, 'error': 'FRED API key not configured.', 'code': 'missing_key', 'env_key': 'FRED_API_KEY', 'docs_url': 'https://fred.stlouisfed.org/docs/api/api_key.html'}"));
        QVERIFY(!r.ok);
        QCOMPARE(r.error.code, ErrorCode::MissingKey);
        QCOMPARE(r.error.env_keys, QStringList{"FRED_API_KEY"});
        QVERIFY(r.error.docs_url.startsWith("https://"));
        const auto rl = parse_connector_envelope(J("{'ok': false, 'error': 'slow down', 'code': 'rate_limited'}"));
        QCOMPARE(rl.error.code, ErrorCode::RateLimited);
        QCOMPARE(rl.error.retry_after_s, 60); // contract default
        const auto ns = parse_connector_envelope(
            J("{'ok': false, 'error': 'keyless host offline', 'code': 'not_supported'}"));
        QCOMPARE(ns.error.code, ErrorCode::NotSupported);
        const auto weird = parse_connector_envelope(J("{'ok': false, 'error': 'x', 'code': 'brand_new_code'}"));
        QCOMPARE(weird.error.code, ErrorCode::Unknown);
    }
    void envelope_noise_and_garbage() {
        // Other lines before the JSON line: the last JSON object line wins.
        const auto r = parse_connector_envelope("Warning: something\n{\"ok\": true, \"data\": [{\"a\": 1}], \"columns\": [\"a\"], "
                                                "\"types\": {\"a\": \"integer\"}, \"meta\": {}}\n");
        QVERIFY(r.ok);
        QCOMPARE(r.data.tables.first().row_count(), 1);
        const auto bad = parse_connector_envelope("Traceback (most recent call last):\n  boom\n");
        QVERIFY(!bad.ok);
        QCOMPARE(bad.error.code, ErrorCode::Unknown);
        QVERIFY(bad.error.message.contains("boom"));
        const auto empty = parse_connector_envelope(J("{'ok': true, 'data': [], 'columns': [], 'meta': {}}"));
        QVERIFY(!empty.ok);
        QCOMPARE(empty.error.code, ErrorCode::NoData);
    }
    void envelope_legacy_payload() {
        // A bare connector payload (no "ok"): {"error": …} fails, anything else is projected.
        const auto f = parse_connector_envelope(J("{'error': 'upstream timed out'}"));
        QVERIFY(!f.ok);
        QCOMPARE(f.error.message, QStringLiteral("upstream timed out"));
        const auto d = parse_connector_envelope(J("{'series': 'GDP', 'data': [{'d': '2024', 'v': 1}, {'d': '2025', 'v': 2}]}"));
        QVERIFY(d.ok);
        QCOMPARE(d.data.tables.first().row_count(), 2);
        const auto kv = parse_connector_envelope(J("{'base': 'EUR', 'rate': 1.08}"));
        QVERIFY(kv.ok);
        QCOMPARE(kv.data.tables.first().columns, (QStringList{"key", "value"}));
    }

    // ── tabular model ──
    void table_from_array_shapes() {
        QJsonArray objs{QJsonObject{{"a", 1}, {"b", "x"}}, QJsonObject{{"a", 2.5}, {"c", true}}};
        DataTable t = table_from_json_array(objs);
        QCOMPARE(t.columns, (QStringList{"a", "b", "c"}));
        QCOMPARE(t.types[0], ColumnType::Number); // 1 and 2.5 merge to number
        QVERIFY(t.cell(0, 2).isNull());
        DataTable arr = table_from_json_array(QJsonArray{QJsonArray{1, 2}, QJsonArray{3}});
        QCOMPARE(arr.columns, (QStringList{"0", "1"}));
        QVERIFY(arr.cell(1, 1).isNull());
        DataTable sc = table_from_json_array(QJsonArray{1, 2, 3});
        QCOMPARE(sc.columns, QStringList{"value"});
        QCOMPARE(sc.row_count(), 3);
    }
    void table_roundtrip_and_csv() {
        DataTable t = table_from_json_array(QJsonArray{QJsonObject{{"n", "a,b"}, {"v", 1}}, QJsonObject{{"n", "q\"x"}, {"v", 2.25}}});
        TabularResult r;
        r.tables << t;
        r.source = "s";
        const TabularResult back = result_from_json(result_to_json(r));
        QCOMPARE(back.tables.first().columns, t.columns);
        QCOMPARE(back.tables.first().cell(1, 1).toDouble(), 2.25);
        const QByteArray csv = table_to_csv(t);
        QVERIFY(csv.startsWith("n,v\r\n"));
        QVERIFY(csv.contains("\"a,b\",1\r\n"));
        QVERIFY(csv.contains("\"q\"\"x\",2.25\r\n"));
    }

    // ── parsers ──
    void csv_detect_and_parse() {
        ParseOptions o;
        auto r = parse_table("date;close;volume\n2024-01-02;185.64;82488700\n2024-01-03;184.25;\n", o);
        QVERIFY(r.ok);
        QCOMPARE(r.detected, TextFormat::Csv);
        QCOMPARE(r.table.columns, (QStringList{"date", "close", "volume"}));
        QCOMPARE(r.table.types[0], ColumnType::Date);
        QCOMPARE(r.table.types[1], ColumnType::Number);
        QVERIFY(r.table.cell(1, 2).isNull());
        // Quotes with embedded delimiter and newline; leading-zero codes stay text.
        auto q = parse_table("code,name\n\"007\",\"Bond, James\nsecond line\"\n00123,x\n", o);
        QVERIFY(q.ok);
        QCOMPARE(q.table.row_count(), 2);
        QCOMPARE(q.table.cell(0, 1).toString(), QStringLiteral("Bond, James\nsecond line"));
        QVERIFY(q.table.cell(1, 0).isString());
        // max_rows sets truncated
        ParseOptions lim;
        lim.max_rows = 1;
        auto t = parse_table("a\n1\n2\n3\n", lim);
        QVERIFY(t.ok);
        QCOMPARE(t.table.row_count(), 1);
        QVERIFY(t.truncated);
        // duplicate / blank headers are made unique
        auto h = parse_table("a,a,\n1,2,3\n", o);
        QCOMPARE(h.table.columns, (QStringList{"a", "a_2", "column_3"}));
    }
    void tsv_and_bom() {
        ParseOptions o;
        o.file_name = "x.tsv";
        auto r = parse_table("\xEF\xBB\xBFsym\tpx\nAAPL\t1.5\n", o);
        QVERIFY(r.ok);
        QCOMPARE(r.table.columns.first(), QStringLiteral("sym"));
    }
    void json_paths() {
        const QJsonObject doc{{"meta", QJsonObject{{"n", 2}}},
                              {"data", QJsonObject{{"rows", QJsonArray{QJsonObject{{"x", 1}}, QJsonObject{{"x", 2}}}}}}};
        QCOMPARE(json_at_path(doc, "$.data.rows[1].x").toInt(), 2);
        QCOMPARE(json_at_path(doc, "data.rows[-1].x").toInt(), 2);
        QCOMPARE(json_at_path(doc, "data['rows'][0]['x']").toInt(), 1);
        QCOMPARE(json_at_path(doc, "$.data.rows[*].x").toArray().size(), 2);
        QVERIFY(json_at_path(doc, "nope.x").isUndefined());
        QString where;
        const QJsonValue found = find_records(doc, &where);
        QCOMPARE(where, QStringLiteral("data.rows"));
        QCOMPARE(found.toArray().size(), 2);
        ParseOptions o;
        auto r = parse_table(QJsonDocument(doc).toJson(), o);
        QVERIFY(r.ok);
        QCOMPARE(r.table.row_count(), 2);
        o.rows_path = "meta.missing";
        auto bad = parse_table(QJsonDocument(doc).toJson(), o);
        QVERIFY(!bad.ok);
        QCOMPARE(bad.error.code, ErrorCode::BadArgs);
        // World Bank v2: [ {page}, [records] ]
        auto wb = parse_table(J("[{'page':1},[{'country':'US','value':1},{'country':'DE','value':2}]]"), ParseOptions());
        QVERIFY(wb.ok);
        QCOMPARE(wb.table.row_count(), 2);
        // OData {"value": [...]}
        auto od = parse_table(J("{'@odata.context':'x','value':[{'a':1},{'a':2},{'a':3}]}"), ParseOptions());
        QCOMPARE(od.table.row_count(), 3);
        // not JSON → bad_args with the start of the body
        ParseOptions js;
        js.format = TextFormat::Json;
        auto nj = parse_table("<html>hello</html>", js);
        QVERIFY(!nj.ok);
        QVERIFY(nj.error.message.contains("<html>"));
    }
    void ndjson_and_sse() {
        auto r = parse_table("{\"a\":1}\n{\"a\":2}\n\n{\"a\":3}\n", ParseOptions());
        QVERIFY(r.ok);
        QCOMPARE(r.detected, TextFormat::NdJson);
        QCOMPARE(r.table.row_count(), 3);
        const QByteArray sse = "id: 0\nevent: price\ndata: {\"p\": 1}\n\n: keep-alive\n\ndata: {\"p\": 2}\n\ndata: plain text\n\n";
        const QJsonArray ev = parse_event_stream(sse);
        QCOMPARE(ev.size(), 3);
        QCOMPARE(ev.at(0).toObject().value("event").toString(), QStringLiteral("price"));
        QCOMPARE(ev.at(2).toObject().value("data").toString(), QStringLiteral("plain text"));
        ParseOptions o;
        o.content_type = "text/event-stream";
        auto t = parse_table(sse, o);
        QVERIFY(t.ok);
        QCOMPARE(t.table.row_count(), 3);
    }
    void xml_records() {
        const QByteArray xml = "<records><record id=\"1\"><date>2024-01-02</date><close>185.64</close></record>"
                               "<record id=\"2\"><date>2024-01-03</date><close>184.25</close></record></records>";
        auto r = parse_table(xml, ParseOptions());
        QVERIFY2(r.ok, qPrintable(r.error.message));
        QCOMPARE(r.table.row_count(), 2);
        QVERIFY(r.table.columns.contains("@id"));
        QVERIFY(r.table.columns.contains("close"));
        QCOMPARE(r.table.cell(1, r.table.column_index("close")).toDouble(), 184.25);
        ParseOptions o;
        o.rows_path = "//record";
        auto p = parse_table(xml, o);
        QCOMPARE(p.table.row_count(), 2);
        auto bad = parse_table("<a><b></a>", ParseOptions());
        QVERIFY(!bad.ok);
    }
    void typed_cells() {
        QVERIFY(typed_text_cell("12.5").isDouble());
        QVERIFY(typed_text_cell("-3").isDouble());
        QVERIFY(typed_text_cell("007").isString());          // leading zero → text
        QVERIFY(typed_text_cell("1234567890123456").isString()); // > 15 digits → text
        QVERIFY(typed_text_cell("1,234").isString());        // locale forms are left to Data Mapping
        QVERIFY(typed_text_cell("TRUE").isBool());
        QVERIFY(typed_text_cell("  ").isNull());
    }

    // ── errors ──
    void http_status_classes() {
        QCOMPARE(error_from_http_status(401, "x").code, ErrorCode::AuthFailed);
        QCOMPARE(error_from_http_status(403, "x").code, ErrorCode::AuthFailed);
        const SourceError rl = error_from_http_status(429, "x", "7");
        QCOMPARE(rl.code, ErrorCode::RateLimited);
        QCOMPARE(rl.retry_after_s, 7);
        QCOMPARE(error_from_http_status(429, "x", "Wed, 21 Oct").retry_after_s, 60);
        QCOMPARE(error_from_http_status(404, "x").code, ErrorCode::BadArgs);
        QCOMPARE(error_from_http_status(503, "x").code, ErrorCode::UpstreamDown);
        QCOMPARE(error_code_from_name(error_code_name(ErrorCode::NotSupported)), ErrorCode::NotSupported);
    }
    void redaction() {
        const QString s = redact_secrets("GET https://u:p4ss@h.io/a?api_key=SECRET123&x=1 Authorization: Bearer abcdef123456 "
                                         "and the raw SECRET123",
                                         {"SECRET123"});
        QVERIFY(!s.contains("SECRET123"));
        QVERIFY(!s.contains("p4ss"));
        QVERIFY(!s.contains("abcdef123456"));
        QVERIFY(s.contains("x=1"));
    }
};

QTEST_GUILESS_MAIN(TestDataSourcesCore)
#include "tst_data_sources_core.moc"
