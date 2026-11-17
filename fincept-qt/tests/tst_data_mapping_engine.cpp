// tst_data_mapping_engine — Data Mapping v2: casts (numbers in both decimal conventions, booleans,
// dates in explicit and inferred formats, epoch s / ms, periods such as 2024Q1 / 2024-03 / FY2024,
// minor units), per-cell errors instead of a failed run, required fields, defaults, duplicates,
// legacy mapping conversion and spec JSON round trips. QtCore only.

#include "services/data_mapping/MappingEngine.h"
#include "services/data_mapping/MappingSpec.h"
#include "services/data_sources/TabularData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using namespace fincept::services::datamapping;
namespace ds = fincept::services::datasources;

namespace {

FieldMapping field(const QString& target, TargetType type, const QString& source, const QString& format = QString()) {
    FieldMapping f;
    f.target = target;
    f.type = type;
    f.source = source;
    f.format = format;
    return f;
}

ds::DataTable periods_table() {
    // Same rows as the ds134 local test service /api/periods.json.
    const QJsonArray rows{
        QJsonObject{{"period", "2024Q1"}, {"year", "2024"}, {"month", "2024-01"}, {"epoch_s", 1704153600},
                    {"epoch_ms", 1704153600000.0}, {"eu_date", "02.01.2024"}, {"us_date", "01/02/2024"},
                    {"price_cents", 18564}, {"value", "1,234.50"}, {"flag", "yes"}},
        QJsonObject{{"period", "2024Q2"}, {"year", "2023"}, {"month", "2024-02"}, {"epoch_s", 1706832000},
                    {"epoch_ms", 1706832000000.0}, {"eu_date", "01.02.2024"}, {"us_date", "02/01/2024"},
                    {"price_cents", 18418}, {"value", "n/a"}, {"flag", "no"}},
        QJsonObject{{"period", "2024-Q3"}, {"year", "2022"}, {"month", "2024-13"}, {"epoch_s", "bad"},
                    {"epoch_ms", QJsonValue()}, {"eu_date", "31.02.2024"}, {"us_date", "13/45/2024"},
                    {"price_cents", "x"}, {"value", ""}, {"flag", "maybe"}},
    };
    return ds::table_from_json_array(rows);
}

} // namespace

class TestDataMappingEngine : public QObject {
    Q_OBJECT
  private slots:
    void numbers() {
        bool ok = false;
        QCOMPARE(parse_number_text("1,234.50", "auto", &ok), 1234.5);
        QVERIFY(ok);
        QCOMPARE(parse_number_text("1.234,50", "auto", &ok), 1234.5);
        QCOMPARE(parse_number_text("1,5", "auto", &ok), 1.5);
        QCOMPARE(parse_number_text("1.234.567", "auto", &ok), 1234567.0);
        QCOMPARE(parse_number_text("(12.5)", "auto", &ok), -12.5);
        QCOMPARE(parse_number_text("$ 1 000", "dot", &ok), 1000.0);
        QCOMPARE(parse_number_text("12%", "auto", &ok), 12.0);
        QCOMPARE(parse_number_text("1e3", "auto", &ok), 1000.0);
        QCOMPARE(parse_number_text("1,234", "comma", &ok), 1.234);
        parse_number_text("abc", "auto", &ok);
        QVERIFY(!ok);
        parse_number_text("1.2.3,4,5", "auto", &ok);
        QVERIFY(!ok);
    }
    void booleans() {
        bool ok = false;
        QVERIFY(parse_bool_text("Yes", &ok) && ok);
        QVERIFY(!parse_bool_text("off", &ok) && ok);
        parse_bool_text("maybe", &ok);
        QVERIFY(!ok);
    }
    void periods() {
        QCOMPARE(parse_period("2024Q1").code, QStringLiteral("2024-Q1"));
        QCOMPARE(parse_period("Q3 2023").code, QStringLiteral("2023-Q3"));
        QCOMPARE(parse_period("2024-Q4").end, QDate(2024, 12, 31));
        QCOMPARE(parse_period("2024").start, QDate(2024, 1, 1));
        QCOMPARE(parse_period("FY2023").code, QStringLiteral("2023"));
        QCOMPARE(parse_period("2024-03").end, QDate(2024, 3, 31));
        QCOMPARE(parse_period("2024M02").end, QDate(2024, 2, 29)); // leap year
        QCOMPARE(parse_period("202406").code, QStringLiteral("2024-06"));
        QCOMPARE(parse_period("2024-H2").start, QDate(2024, 7, 1));
        QVERIFY(!parse_period("2024-13").valid());
        QVERIFY(!parse_period("Q5 2024").valid());
    }
    void dates() {
        bool ok = false;
        QCOMPARE(parse_datetime(QJsonValue("2024-01-02"), "auto", &ok).date(), QDate(2024, 1, 2));
        QVERIFY(ok);
        QCOMPARE(parse_datetime(QJsonValue("17/11/2021"), "auto", &ok).date(), QDate(2021, 11, 17)); // unambiguous
        QString why;
        parse_datetime(QJsonValue("01/02/2024"), "auto", &ok, nullptr, &why);
        QVERIFY(!ok);
        QVERIFY(why.contains("ambiguous"));
        QCOMPARE(parse_datetime(QJsonValue("01/02/2024"), "MM/dd/yyyy", &ok).date(), QDate(2024, 1, 2));
        QCOMPARE(parse_datetime(QJsonValue("01/02/2024"), "dd/MM/yyyy", &ok).date(), QDate(2024, 2, 1));
        QCOMPARE(parse_datetime(QJsonValue(1704153600), "epoch_s", &ok).date(), QDate(2024, 1, 2));
        QCOMPARE(parse_datetime(QJsonValue(1704153600000.0), "auto", &ok).date(), QDate(2024, 1, 2));
        QCOMPARE(parse_datetime(QJsonValue("1704153600"), "epoch", &ok).date(), QDate(2024, 1, 2));
        QCOMPARE(parse_datetime(QJsonValue(2023), "auto", &ok).date(), QDate(2023, 1, 1)); // a year
        QCOMPARE(parse_datetime(QJsonValue("2024Q2"), "period_end", &ok).date(), QDate(2024, 6, 30));
        bool date_only = true;
        const QDateTime t = parse_datetime(QJsonValue("2024-01-02 09:30:00"), "auto", &ok, &date_only);
        QVERIFY(ok);
        QVERIFY(!date_only);
        QCOMPARE(t.time(), QTime(9, 30)); // a naive timestamp is UTC, never shifted by the machine zone
        parse_datetime(QJsonValue("31.02.2024"), "dd.MM.yyyy", &ok);
        QVERIFY(!ok); // 31 February
        QCOMPARE(parse_datetime(QJsonValue("05/03/24"), "dd/MM/yy", &ok).date(), QDate(2024, 3, 5));
        QCOMPARE(parse_datetime(QJsonValue("12 Mar 2024"), "d MMM yyyy", &ok).date(), QDate(2024, 3, 12));
    }
    void casts_and_cell_errors() {
        const ds::DataTable in = periods_table();
        MappingSpec spec;
        spec.target_schema = "CUSTOM";
        spec.fields << field("quarter", TargetType::Period, "period") << field("year_start", TargetType::Date, "year", "year")
                    << field("month", TargetType::Period, "month") << field("t_s", TargetType::DateTime, "epoch_s", "epoch_s")
                    << field("t_ms", TargetType::DateTime, "epoch_ms", "epoch_ms")
                    << field("eu", TargetType::Date, "eu_date", "dd.MM.yyyy") << field("us", TargetType::Date, "us_date")
                    << field("price", TargetType::Number, "price_cents") << field("value", TargetType::Number, "value")
                    << field("flag", TargetType::Bool, "flag");
        spec.fields[7].scale = 0.01; // minor units → major
        const MappingOutput out = apply_mapping(in, spec);
        QVERIFY(out.spec_problems.isEmpty());
        QCOMPARE(out.rows_in, 3);
        QCOMPARE(out.rows_out, 3); // bad cells never drop the run
        const auto col = [&](const char* name) { return out.table.column_index(QString::fromLatin1(name)); };
        QCOMPARE(out.table.cell(0, col("quarter")).toString(), QStringLiteral("2024-Q1"));
        QCOMPARE(out.table.cell(2, col("quarter")).toString(), QStringLiteral("2024-Q3"));
        QCOMPARE(out.table.cell(1, col("year_start")).toString(), QStringLiteral("2023-01-01"));
        QCOMPARE(out.table.cell(1, col("month")).toString(), QStringLiteral("2024-02"));
        QVERIFY(out.table.cell(2, col("month")).isNull()); // 2024-13
        QVERIFY(out.issue_at(2, col("month")) != nullptr);
        QCOMPARE(out.table.cell(0, col("t_s")).toString(), QStringLiteral("2024-01-02T00:00:00Z"));
        QCOMPARE(out.table.cell(0, col("t_ms")).toString(), QStringLiteral("2024-01-02T00:00:00Z"));
        QVERIFY(out.issue_at(2, col("t_s")) != nullptr);  // "bad"
        QVERIFY(out.issue_at(2, col("t_ms")) == nullptr); // null is empty, not an error
        QCOMPARE(out.table.cell(0, col("eu")).toString(), QStringLiteral("2024-01-02"));
        QVERIFY(out.issue_at(2, col("eu")) != nullptr);   // 31.02.2024
        // "us" is auto: 01/02/2024 and 02/01/2024 are ambiguous, 13/45/2024 is invalid → per-cell errors.
        QVERIFY(out.issue_at(0, col("us")) != nullptr);
        QVERIFY(out.issue_at(0, col("us"))->message.contains("ambiguous"));
        QCOMPARE(out.table.cell(0, col("price")).toDouble(), 185.64);
        QVERIFY(out.issue_at(2, col("price")) != nullptr);
        QCOMPARE(out.table.cell(0, col("value")).toDouble(), 1234.5);
        QVERIFY(out.table.cell(1, col("value")).isNull()); // "n/a" is a null token
        QVERIFY(out.issue_at(1, col("value")) == nullptr);
        QCOMPARE(out.table.cell(0, col("flag")).toBool(), true);
        QVERIFY(out.issue_at(2, col("flag")) != nullptr); // "maybe"
        QVERIFY(out.error_cells >= 7);
        QVERIFY(!out.field_stats[col("eu")].first_error.isEmpty());
        QVERIFY(out.field_stats[col("eu")].first_error.startsWith("row 3"));
    }
    void inferred_day_first_column() {
        const ds::DataTable in = ds::table_from_json_array(QJsonArray{QJsonObject{{"d", "17/11/2021"}},
                                                                      QJsonObject{{"d", "01/12/2021"}}});
        MappingSpec spec;
        spec.fields << field("date", TargetType::Date, "d");
        const MappingOutput out = apply_mapping(in, spec);
        // 17/11 proves day-first for the whole column, so 01/12 is 1 December, not 12 January.
        QCOMPARE(out.table.cell(1, 0).toString(), QStringLiteral("2021-12-01"));
        QVERIFY(out.clean());
    }
    void required_defaults_duplicates() {
        const ds::DataTable in = ds::table_from_json_array(QJsonArray{
            QJsonObject{{"t", "AAPL"}, {"d", "2024-01-02"}, {"c", 1.0}},
            QJsonObject{{"t", "AAPL"}, {"d", "2024-01-02"}, {"c", 1.0}},
            QJsonObject{{"t", QJsonValue()}, {"d", "2024-01-03"}, {"c", QJsonValue()}},
        });
        MappingSpec spec;
        FieldMapping sym = field("symbol", TargetType::String, "t");
        sym.default_value = "UNKNOWN";
        FieldMapping close = field("close", TargetType::Number, "c");
        close.required = true;
        FieldMapping exch = field("exchange", TargetType::String, QString());
        exch.default_value = "NASDAQ"; // constant column
        spec.fields << sym << field("date", TargetType::Date, "d") << close << exch;
        spec.options.key_fields = {"symbol", "date"};
        spec.options.duplicates = "drop";
        MappingOutput out = apply_mapping(in, spec);
        QCOMPARE(out.duplicates, 1);
        QCOMPARE(out.rows_dropped_duplicate, 1);
        QCOMPARE(out.rows_out, 2);
        QCOMPARE(out.table.cell(1, 0).toString(), QStringLiteral("UNKNOWN"));
        QCOMPARE(out.table.cell(0, 3).toString(), QStringLiteral("NASDAQ"));
        QVERIFY(out.issue_at(1, 2) != nullptr); // required close missing
        QCOMPARE(out.source_rows, (QVector<int>{0, 2}));
        spec.options.duplicates = "flag";
        spec.options.drop_invalid_rows = true;
        out = apply_mapping(in, spec);
        QCOMPARE(out.rows_dropped_invalid, 2); // the flagged duplicate + the row missing close
        QCOMPARE(out.rows_out, 1);
    }
    void spec_problems_and_paths() {
        const ds::DataTable in = ds::table_from_json_array(QJsonArray{
            QJsonObject{{"quote", QJsonObject{{"px", 10.5}, {"sym", "X"}}}},
            QJsonObject{{"quote", "{\"px\": 11, \"sym\": \"Y\"}"}}, // JSON text inside a cell
        });
        MappingSpec spec;
        FieldMapping px = field("price", TargetType::Number, "quote");
        px.path = "px";
        spec.fields << px << field("missing", TargetType::String, "nope");
        const MappingOutput out = apply_mapping(in, spec);
        QCOMPARE(out.table.cell(0, 0).toDouble(), 10.5);
        QCOMPARE(out.table.cell(1, 0).toDouble(), 11.0);
        QCOMPARE(out.spec_problems.size(), 1);
        QVERIFY(out.spec_problems.first().contains("nope"));
        MappingSpec dup;
        dup.fields << field("a", TargetType::String, "quote") << field("a", TargetType::String, "quote");
        QVERIFY(!apply_mapping(in, dup).spec_problems.isEmpty());
        QVERIFY(!apply_mapping(in, MappingSpec()).spec_problems.isEmpty());
        // preview limit
        QCOMPARE(apply_mapping(in, spec, 1).rows_out, 1);
    }
    void integer_and_scale() {
        const ds::DataTable in = ds::table_from_json_array(QJsonArray{QJsonObject{{"q", "1,000"}}, QJsonObject{{"q", 2.5}}});
        MappingSpec spec;
        spec.fields << field("qty", TargetType::Integer, "q");
        const MappingOutput out = apply_mapping(in, spec);
        QCOMPARE(out.table.cell(0, 0).toDouble(), 1000.0);
        QVERIFY(out.issue_at(1, 0) != nullptr); // 2.5 is not whole
        spec.fields[0].scale = 1000; // thousands
        spec.fields[0].type = TargetType::Number;
        QCOMPARE(apply_mapping(in, spec).table.cell(1, 0).toDouble(), 2500.0);
    }
    void legacy_conversion() {
        LegacyMapping lm;
        lm.schema_name = "QUOTE";
        lm.base_url = "https://api.coingecko.com/api/v3";
        lm.endpoint = "/simple/price?ids=bitcoin";
        lm.field_mappings = QJsonArray{QJsonObject{{"target", "symbol"}, {"expression", ""}, {"default_val", "bitcoin"}},
                                       QJsonObject{{"target", "price"}, {"expression", "$.bitcoin.usd"}},
                                       QJsonObject{{"target", "timestamp"}, {"expression", "$.bitcoin.last_updated_at"},
                                                   {"transform", "unix_to_iso"}}};
        MappingSpec s = spec_from_legacy(lm);
        QCOMPARE(s.source.kind, QStringLiteral("http"));
        QVERIFY(s.source.http.value("document_mode").toBool());
        const int p = s.index_of("price");
        QCOMPARE(s.fields[p].source, QStringLiteral("$"));
        QCOMPARE(s.fields[p].path, QStringLiteral("bitcoin.usd"));
        QCOMPARE(s.fields[p].type, TargetType::Number); // from the QUOTE schema
        const int ts = s.index_of("timestamp");
        QCOMPARE(s.fields[ts].type, TargetType::DateTime);
        QCOMPARE(s.fields[ts].format, QStringLiteral("epoch"));
        // Document mode end to end: the whole response is one row in column "$".
        ds::DataTable doc;
        doc.columns = {"$"};
        doc.types = {ds::ColumnType::Object};
        doc.rows << QJsonArray{QJsonObject{{"bitcoin", QJsonObject{{"usd", 64000.5}, {"last_updated_at", 1704153600}}}}};
        const MappingOutput out = apply_mapping(doc, s);
        QCOMPARE(out.table.cell(0, s.index_of("symbol")).toString(), QStringLiteral("bitcoin"));
        QCOMPARE(out.table.cell(0, p).toDouble(), 64000.5);
        QCOMPARE(out.table.cell(0, ts).toString(), QStringLiteral("2024-01-02T00:00:00Z"));

        // Every expression projects one array → rows mode with a rows path.
        LegacyMapping arr;
        arr.schema_name = "OHLCV";
        arr.field_mappings = QJsonArray{QJsonObject{{"target", "close"}, {"expression", "$.data[*].c"}},
                                        QJsonObject{{"target", "timestamp"}, {"expression", "$.data[*].t"}}};
        MappingSpec rs = spec_from_legacy(arr);
        QVERIFY(!rs.source.http.value("document_mode").toBool());
        QCOMPARE(rs.source.http.value("rows_path").toString(), QStringLiteral("data"));
        QCOMPARE(rs.fields[0].source, QStringLiteral("c"));
    }
    void spec_roundtrip_and_suggestions() {
        MappingSpec s;
        s.target_schema = "OHLCV";
        s.source.kind = "connector";
        s.source.ref = "data_fred";
        s.source.entity = "series";
        s.source.args = {"GDP"};
        FieldMapping f = field("close", TargetType::Number, "value");
        f.scale = 0.01;
        f.required = true;
        s.fields << f;
        s.options.key_fields = {"timestamp"};
        s.options.decimal = "comma";
        const MappingSpec back = MappingSpec::from_json(s.to_json());
        QCOMPARE(back.source.args, QStringList{"GDP"});
        QCOMPARE(back.fields.first().scale, 0.01);
        QVERIFY(back.fields.first().required);
        QCOMPARE(back.options.decimal, QStringLiteral("comma"));
        QCOMPARE(suggest_source_column("timestamp", {"Date", "Close"}), QStringLiteral("Date"));
        QCOMPARE(suggest_source_column("close", {"date", "Adj_Close"}), QStringLiteral("Adj_Close"));
        QCOMPARE(suggest_source_column("symbol", {"Ticker"}), QStringLiteral("Ticker"));
        QVERIFY(suggest_source_column("vwap", {"date"}).isEmpty());
        QVERIFY(find_target_schema("ohlcv") != nullptr);
        QVERIFY(find_target_schema("MACRO") != nullptr);
    }
};

QTEST_GUILESS_MAIN(TestDataMappingEngine)
#include "tst_data_mapping_engine.moc"
