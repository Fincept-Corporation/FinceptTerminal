// tst_connector_args — catalogue connector arguments (services/connectors/ConnectorArgs):
// the typed-params contract (parse_param_list / build_argv / inputs_from_args) and the
// args-hint fallback used when a command's params are unknown.

#include "services/connectors/ConnectorArgs.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace fincept::services::connectors;

namespace {

QVector<ConnectorParam> parse(const char* json) {
    const ParamList pl = parse_param_list(QJsonDocument::fromJson(QByteArray(json).replace('\'', '"')).array());
    return pl.params;
}

QVector<ParamInput> in(std::initializer_list<const char*> values) {
    QVector<ParamInput> out;
    for (const char* v : values)
        out << ParamInput::of(QString::fromUtf8(v));
    return out;
}

QStringList sl(std::initializer_list<const char*> values) {
    QStringList out;
    for (const char* v : values)
        out << QString::fromUtf8(v);
    return out;
}

// series <series_id> [start=2020-01-01] [end=""] [limit=null]
const char* kSeries = "[ {'name':'series_id','required':true,'default':null,'kind':'string'}, {'name':'start','required':false,'default':'2020-01-01','kind':'date'}, {'name':'end','required':false,'default':'','kind':'date'}, {'name':'limit','required':false,'default':null,'kind':'int'} ]";

} // namespace

class TestConnectorArgs : public QObject {
    Q_OBJECT
  private slots:
    // ── parse_param_list ────────────────────────────────────────────────
    void parse_null_empty_and_malformed() {
        ParamList pl = parse_param_list(QJsonValue());
        QVERIFY(!pl.known);
        QVERIFY(pl.error.isEmpty());
        pl = parse_param_list(QJsonValue(QJsonValue::Null));
        QVERIFY(!pl.known);
        pl = parse_param_list(QJsonArray());
        QVERIFY(pl.known);
        QVERIFY(pl.params.isEmpty());
        QVERIFY(runs_without_input(pl.params));

        // Unknown kind, enum without choices, positional after a variadic, no name: rejected.
        for (const char* bad : {"[{'name':'x','required':false,'default':null,'kind':'money'}]",
                                "[{'name':'x','required':true,'default':null,'kind':'enum'}]",
                                "[{'name':'a','required':false,'default':null,'kind':'string','variadic':true}, {'name':'b','required':false,'default':null,'kind':'string'}]",
                                "[{'required':true,'default':null,'kind':'string'}]"}) {
            pl = parse_param_list(QJsonDocument::fromJson(QByteArray(bad).replace('\'', '"')).array());
            QVERIFY2(!pl.known, bad);
            QVERIFY(!pl.error.isEmpty());
            QVERIFY(pl.params.isEmpty());
        }
    }

    void parse_defaults_and_kinds() {
        const auto p = parse("[ {'name':'a','required':true,'default':null,'kind':'symbol','help':'ticker, e.g. AAPL'}, {'name':'b','required':false,'default':'','kind':'date'}, {'name':'c','required':false,'default':1000,'kind':'int'}, {'name':'d','required':false,'default':48.8,'kind':'float'}, {'name':'e','required':false,'default':true,'kind':'bool'}, {'name':'f','required':false,'default':'monthly','kind':'enum','choices':['daily','monthly']}, {'name':'g','required':false,'default':null,'kind':'country','named':true}, {'name':'h','required':false,'default':'US,GB','kind':'country'} ]");
        QCOMPARE(p.size(), 8);
        QCOMPARE(p[0].kind, ParamKind::Symbol);
        QVERIFY(p[0].required);
        QVERIFY(!p[0].has_default);
        QVERIFY(p[1].has_default);
        QCOMPARE(p[1].default_value, QString());
        QCOMPARE(p[2].default_value, QStringLiteral("1000"));
        QCOMPARE(p[3].default_value, QStringLiteral("48.8"));
        QCOMPARE(p[4].default_value, QStringLiteral("true"));
        QCOMPARE(p[5].choices, sl({"daily", "monthly"}));
        QVERIFY(p[6].named);
        QVERIFY(!p[6].has_default);
        QVERIFY(p[7].is_list());
        QVERIFY(!runs_without_input(p));
        QCOMPARE(param_kind_name(ParamKind::Country), QStringLiteral("country"));
    }

    // ── build_argv: positionals ─────────────────────────────────────────
    void command_first_and_required() {
        const auto p = parse(kSeries);
        ArgvResult r = build_argv(QStringLiteral("series"), p, in({"GDP", "", "", ""}));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.argv, sl({"series", "GDP"}));
        QCOMPARE(r.args(), sl({"GDP"}));

        r = build_argv(QStringLiteral("series"), p, in({"", "", "", ""}));
        QVERIFY(!r.ok);
        QCOMPARE(r.param, 0);
        QVERIFY(r.error.contains(QStringLiteral("series_id")));
    }

    void optional_tail_is_omitted() {
        const auto p = parse(kSeries);
        // A value equal to its default at the tail is the same as leaving it out.
        ArgvResult r = build_argv(QStringLiteral("series"), p, in({"GDP", "2020-01-01", "", ""}));
        QCOMPARE(r.argv, sl({"series", "GDP"}));
        r = build_argv(QStringLiteral("series"), p, in({"GDP", "2021-06-30", "", ""}));
        QCOMPARE(r.argv, sl({"series", "GDP", "2021-06-30"}));
    }

    void later_param_sends_earlier_default() {
        const auto p = parse(kSeries);
        // end given, start left empty: start's default goes in its place.
        ArgvResult r = build_argv(QStringLiteral("series"), p, in({"GDP", "", "2024-12-31", ""}));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.argv, sl({"series", "GDP", "2020-01-01", "2024-12-31"}));
    }

    void empty_default_is_sent_as_empty_argument() {
        const auto p = parse(kSeries);
        // limit given; end's default "" means "not given" to the script, so "" is sent.
        ArgvResult r = build_argv(QStringLiteral("series"), p, in({"GDP", "2021-01-01", "", "50"}));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.argv, sl({"series", "GDP", "2021-01-01", "", "50"}));
        QCOMPARE(r.argv.size(), 5);
    }

    void null_default_before_a_given_param_is_an_input_error() {
        const auto p = parse("[ {'name':'country','required':false,'default':null,'kind':'country'}, {'name':'year','required':false,'default':'2023','kind':'int'} ]");
        ArgvResult r = build_argv(QStringLiteral("gdp"), p, in({"", "2020"}));
        QVERIFY(!r.ok);
        QCOMPARE(r.param, 0);
        QVERIFY(r.error.contains(QStringLiteral("country")));
        QVERIFY(r.error.contains(QStringLiteral("year")));
        QCOMPARE(r.argv, sl({"gdp"})); // nothing half-built
        // Leaving both out is fine: a null default can be omitted with everything after it.
        r = build_argv(QStringLiteral("gdp"), p, in({"", ""}));
        QVERIFY(r.ok);
        QCOMPARE(r.argv, sl({"gdp"}));
        r = build_argv(QStringLiteral("gdp"), p, in({"USA", ""}));
        QCOMPARE(r.argv, sl({"gdp", "USA"}));
    }

    // ── build_argv: named, bool, variadic ───────────────────────────────
    void named_params_after_positionals() {
        const auto p = parse("[ {'name':'start','required':false,'default':null,'kind':'date','named':true}, {'name':'symbol','required':true,'default':null,'kind':'symbol'}, {'name':'interval','required':false,'default':'1d','kind':'enum','choices':['1d','1h'],'named':true}, {'name':'compounded','required':false,'default':false,'kind':'bool','named':true} ]");
        ArgvResult r = build_argv(QStringLiteral("prices"), p, in({"2024-01-02", "AAPL", "1h", "true"}));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.argv, sl({"prices", "AAPL", "--start=2024-01-02", "--interval=1h", "--compounded"}));
        // Default value of a named param, a false bool and an empty named date: all left out.
        r = build_argv(QStringLiteral("prices"), p, in({"", "AAPL", "1d", "false"}));
        QCOMPARE(r.argv, sl({"prices", "AAPL"}));
    }

    void positional_bool() {
        const auto p = parse("[ {'name':'country','required':true,'default':null,'kind':'country'}, {'name':'latest','required':false,'default':true,'kind':'bool'} ]");
        QCOMPARE(build_argv(QStringLiteral("profile"), p, in({"USA", "true"})).argv, sl({"profile", "USA"}));
        QCOMPARE(build_argv(QStringLiteral("profile"), p, in({"USA", "false"})).argv, sl({"profile", "USA", "false"}));
        QCOMPARE(build_argv(QStringLiteral("profile"), p, in({"USA", "No"})).argv, sl({"profile", "USA", "false"}));
        QVERIFY(!build_argv(QStringLiteral("profile"), p, in({"USA", "maybe"})).ok);
    }

    void variadic_takes_several_items() {
        const auto p = parse("[ {'name':'indicator','required':true,'default':null,'kind':'string'}, {'name':'countries','required':false,'default':null,'kind':'country','variadic':true, 'help':'ISO3 country codes, each its own argument'} ]");
        ArgvResult r = build_argv(QStringLiteral("rates"), p, in({"gdp", "USA, CHN;IND"}));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.argv, sl({"rates", "gdp", "USA", "CHN", "IND"}));
        r = build_argv(QStringLiteral("rates"), p, in({"gdp", ""}));
        QCOMPARE(r.argv, sl({"rates", "gdp"}));
    }

    // ── value rules ─────────────────────────────────────────────────────
    void enum_validation() {
        const auto p = parse("[{'name':'freq','required':true,'default':null,'kind':'enum','choices':['A','Q','M']}]");
        QCOMPARE(build_argv(QStringLiteral("x"), p, in({"Q"})).argv, sl({"x", "Q"}));
        QCOMPARE(build_argv(QStringLiteral("x"), p, in({"m"})).argv, sl({"x", "M"})); // exact choice sent
        const ArgvResult bad = build_argv(QStringLiteral("x"), p, in({"W"}));
        QVERIFY(!bad.ok);
        QCOMPARE(bad.param, 0);
        QVERIFY(bad.error.contains(QStringLiteral("A, Q, M")));
    }

    void date_and_number_formatting() {
        QCOMPARE(date_arg(QDate(2024, 3, 7)), QStringLiteral("2024-03-07"));
        const auto p = parse("[ {'name':'d','required':true,'default':null,'kind':'date'}, {'name':'i','required':true,'default':null,'kind':'int'}, {'name':'f','required':true,'default':null,'kind':'float'} ]");
        ArgvResult r = build_argv(QStringLiteral("x"), p, in({"2024-02-29", "+42", "1e3"}));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.argv, sl({"x", "2024-02-29", "42", "1000"}));
        r = build_argv(QStringLiteral("x"), p, in({"2024-02-29", "7", ".5"}));
        QCOMPARE(r.args().last(), QStringLiteral("0.5"));
        QVERIFY(!build_argv(QStringLiteral("x"), p, in({"2023-02-29", "1", "1"})).ok); // not a date
        QVERIFY(!build_argv(QStringLiteral("x"), p, in({"07/03/2024", "1", "1"})).ok);
        QVERIFY(!build_argv(QStringLiteral("x"), p, in({"2024-01-01", "1.5", "1"})).ok);
        QVERIFY(!build_argv(QStringLiteral("x"), p, in({"2024-01-01", "1", "1,5"})).ok);
        QCOMPARE(float_arg(48.8), QStringLiteral("48.8"));
        QCOMPARE(float_arg(1000.0), QStringLiteral("1000"));
        QCOMPARE(float_arg(-0.25), QStringLiteral("-0.25"));
        QCOMPARE(float_arg(-0.0), QStringLiteral("0"));
    }

    void symbol_kind() {
        const auto p = parse("[ {'name':'symbol','required':true,'default':null,'kind':'symbol','help':'ticker, e.g. AAPL'}, {'name':'symbols','required':false,'default':null,'kind':'symbol','help':'comma-separated list'} ]");
        // As typed (no case change), trimmed.
        QCOMPARE(build_argv(QStringLiteral("q"), p, in({"  RELIANCE.NS ", ""})).argv, sl({"q", "RELIANCE.NS"}));
        QCOMPARE(build_argv(QStringLiteral("q"), p, in({"brk-b", ""})).argv, sl({"q", "brk-b"}));
        // One symbol has no spaces; a list is normalised to "a,b".
        const ArgvResult bad = build_argv(QStringLiteral("q"), p, in({"BRK B", ""}));
        QVERIFY(!bad.ok);
        QCOMPARE(bad.param, 0);
        QCOMPARE(build_argv(QStringLiteral("q"), p, in({"AAPL", "msft, goog  nvda"})).argv,
                 sl({"q", "AAPL", "msft,goog,nvda"}));
        const auto free = parse("[{'name':'s','required':true,'default':null,'kind':'symbol','help':'free text'}]");
        QVERIFY(build_argv(QStringLiteral("q"), free, in({"S&P 500"})).ok);
    }

    void country_and_string_as_typed() {
        const auto p = parse("[ {'name':'country','required':false,'default':'united_states','kind':'country'}, {'name':'query','required':false,'default':null,'kind':'string'} ]");
        QCOMPARE(build_argv(QStringLiteral("s"), p, in({"United States", "Apple Inc"})).argv,
                 sl({"s", "United States", "Apple Inc"}));
        QCOMPARE(build_argv(QStringLiteral("s"), p, in({"", "x"})).argv, sl({"s", "united_states", "x"}));
    }

    // ── samples / remembered runs back into the form ────────────────────
    void inputs_from_args_round_trip() {
        const auto p = parse("[ {'name':'symbol','required':true,'default':null,'kind':'symbol'}, {'name':'start','required':false,'default':'','kind':'date'}, {'name':'limit','required':false,'default':'100','kind':'int'}, {'name':'interval','required':false,'default':'1d','kind':'enum','choices':['1d','1h'],'named':true}, {'name':'adjusted','required':false,'default':false,'kind':'bool','named':true} ]");
        QVector<ParamInput> inputs;
        QVERIFY(inputs_from_args(p, sl({"AAPL", "", "20", "--interval=1h", "--adjusted"}), &inputs));
        QCOMPARE(inputs[0].value, QStringLiteral("AAPL"));
        QVERIFY(!inputs[1].given); // "" = not given
        QCOMPARE(inputs[2].value, QStringLiteral("20"));
        QCOMPARE(inputs[3].value, QStringLiteral("1h"));
        QCOMPARE(inputs[4].value, QStringLiteral("true"));
        const ArgvResult r = build_argv(QStringLiteral("hist"), p, inputs);
        QCOMPARE(r.argv, sl({"hist", "AAPL", "", "20", "--interval=1h", "--adjusted"}));

        QVERIFY(!inputs_from_args(p, sl({"AAPL", "--nope=1"}), &inputs));       // unknown named
        QVERIFY(!inputs_from_args(p, sl({"a", "b", "c", "d"}), &inputs));       // too many positionals
        QVERIFY(!inputs_from_args(p, sl({"AAPL", "--interval"}), &inputs));     // value missing
    }

    void inputs_from_args_variadic() {
        const auto p = parse("[ {'name':'ind','required':true,'default':null,'kind':'string'}, {'name':'countries','required':false,'default':null,'kind':'country','variadic':true} ]");
        QVector<ParamInput> inputs;
        QVERIFY(inputs_from_args(p, sl({"gdp", "USA", "CHN"}), &inputs));
        QCOMPARE(inputs[1].value, QStringLiteral("USA,CHN"));
        QCOMPARE(build_argv(QStringLiteral("r"), p, inputs).argv, sl({"r", "gdp", "USA", "CHN"}));
    }

    // ── args_hint fallback (params null) ────────────────────────────────
    void hint_fallback() {
        ArgsForm f = args_form_from_hint(QStringLiteral("<series_id> [start_date] [end_date]"));
        QVERIFY(f.structured);
        QCOMPARE(f.fields.size(), 3);
        BuiltArgs b = build_args(f, {QStringLiteral("GDP"), QString(), QStringLiteral("2024-01-01")});
        QVERIFY(!b.ok);
        QCOMPARE(b.field, 1);
        b = build_args(f, {QStringLiteral("GDP"), QStringLiteral("2020-01-01"), QString()});
        QCOMPARE(b.args, sl({"GDP", "2020-01-01"}));

        f = args_form_from_hint(QStringLiteral("[--fields=field1,field2] [--limit=N] [--all]"));
        b = build_args(f, {QStringLiteral("a,b"), QString(), QStringLiteral("true")});
        QCOMPARE(b.args, sl({"--fields=a,b", "--all"}));

        QVERIFY(!args_form_from_hint(QStringLiteral("[countries...] [start_period]")).structured);
        QVERIFY(!args_form_from_hint(QStringLiteral("<symbol [limit]")).structured);
        QStringList vals;
        QVERIFY(values_from_args(args_form_from_hint(QStringLiteral("<a> [b]")), sl({"x"}), &vals));
        QCOMPARE(vals, (QStringList{QStringLiteral("x"), QString()}));
    }

    void free_text_split_and_join() {
        QCOMPARE(split_args_text(QStringLiteral("a \"b c\" d")), sl({"a", "b c", "d"}));
        QCOMPARE(split_args_text(QStringLiteral("{\"k\": [1, 2]}")), QStringList{QStringLiteral("{\"k\": [1, 2]}")});
        const QStringList args = sl({"x", "has space", "say \"hi\""});
        QCOMPARE(split_args_text(join_args_text(args)), args);
    }
};

QTEST_GUILESS_MAIN(TestConnectorArgs)
#include "tst_connector_args.moc"
