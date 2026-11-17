// tst_data_source_drivers — the per-type knowledge behind Data Sources connections: every type the
// tab lists is either served by a real driver or marked unavailable with a reason and an
// alternative; request plans (auth schemes, URLs, bodies) and answer post-processing for the
// HTTP-served types; AWS Signature V4 against AWS's published example. QtCore only — no network.

#include "screens/data_sources/ConnectorRegistry.h"
#include "services/data_sources/ConnectionProfiles.h"
#include "services/data_sources/RequestSigning.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QtTest>

using namespace fincept::services::datasources;

namespace {

QByteArray hdr(const HttpCall& c, const QByteArray& name) {
    return header_value(c.headers, name);
}

QByteArray J(const char* text) {
    QByteArray b(text);
    b.replace('\'', '"');
    return b;
}

} // namespace

class TestDataSourceDrivers : public QObject {
    Q_OBJECT
  private slots:
    void every_registry_type_is_honest() {
        fincept::screens::datasources::register_all_connectors();
        const auto& all = fincept::screens::datasources::ConnectorRegistry::instance().all();
        QVERIFY(all.size() >= 118);
        int supported = 0;
        for (const auto& cfg : all) {
            const TypeSupport t = type_support(cfg.id);
            QVERIFY2(t.type_id == cfg.id, qPrintable(cfg.id));
            if (t.supported) {
                ++supported;
                QVERIFY2(t.driver != DriverKind::None, qPrintable(cfg.id));
                QVERIFY2(t.can_fetch || t.can_list, qPrintable(cfg.id));
            } else {
                QVERIFY2(!t.reason.isEmpty(), qPrintable(cfg.id + " has no reason"));
                QVERIFY2(!t.fix.isEmpty(), qPrintable(cfg.id + " has no alternative"));
            }
        }
        QVERIFY(supported >= 60);
        QVERIFY(!type_support("no-such-type").supported);
    }

    void sigv4_aws_example() {
        // "Example: GET Object" from the Amazon S3 Signature Version 4 documentation.
        HeaderList h{{"Range", "bytes=0-9"}};
        sign_aws_v4("GET", QUrl("https://examplebucket.s3.amazonaws.com/test.txt"), h, QByteArray(),
                    "AKIAIOSFODNN7EXAMPLE", "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", "us-east-1", "s3",
                    QDateTime(QDate(2013, 5, 24), QTime(0, 0), QTimeZone::UTC));
        const QByteArray auth = header_value(h, "Authorization");
        QVERIFY2(auth.contains("Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41"), auth.constData());
        QVERIFY(auth.contains("SignedHeaders=host;range;x-amz-content-sha256;x-amz-date"));
        QCOMPARE(header_value(h, "x-amz-date"), QByteArray("20130524T000000Z"));
        QCOMPARE(aws_uri_encode("a b/c~", false), QByteArray("a%20b%2Fc~"));
    }
    void azure_shared_key_shape() {
        HeaderList h;
        sign_azure_shared_key("GET", QUrl("https://acct.blob.core.windows.net/?comp=list"), h, QByteArray(), "acct",
                              QString::fromLatin1(QByteArray("0123456789abcdef").toBase64()),
                              QDateTime(QDate(2024, 1, 2), QTime(3, 4, 5), QTimeZone::UTC));
        QVERIFY(header_value(h, "Authorization").startsWith("SharedKey acct:"));
        QCOMPARE(header_value(h, "x-ms-date"), QByteArray("Tue, 02 Jan 2024 03:04:05 GMT"));
    }

    void rest_auth_schemes() {
        const TypeSupport rest = type_support("rest-api");
        QJsonObject cfg{{"baseUrl", "http://127.0.0.1:18934/"}, {"authType", "bearer"}, {"apiKey", "s3cr3t-token"}};
        HttpPlan p = http_plan(rest, SourceOp::Fetch, cfg, "/secure/bearer", "a=1", 50);
        QVERIFY(p.ok);
        QCOMPARE(p.calls.size(), 1);
        QCOMPARE(p.calls[0].url.toString(), QStringLiteral("http://127.0.0.1:18934/secure/bearer?a=1"));
        QCOMPARE(hdr(p.calls[0], "Authorization"), QByteArray("Bearer s3cr3t-token"));
        QVERIFY(p.calls[0].secrets.contains("s3cr3t-token"));

        cfg = {{"baseUrl", "http://h/api"}, {"authType", "apikey"}, {"apiKey", "k3y"}, {"keyIn", "query"}};
        p = http_plan(rest, SourceOp::Test, cfg, QString(), QString(), 10);
        QCOMPARE(QUrlQuery(p.calls[0].url).queryItemValue("api_key"), QStringLiteral("k3y"));
        cfg.insert("keyIn", "header");
        p = http_plan(rest, SourceOp::Test, cfg, QString(), QString(), 10);
        QCOMPARE(hdr(p.calls[0], "X-API-Key"), QByteArray("k3y"));
        cfg = {{"baseUrl", "http://h"}, {"authType", "basic"}, {"username", "trader"}, {"password", "pa55"}};
        p = http_plan(rest, SourceOp::Test, cfg, "/secure/basic", QString(), 10);
        QCOMPARE(hdr(p.calls[0], "Authorization"), "Basic " + QByteArray("trader:pa55").toBase64());
        cfg = {{"baseUrl", "http://h"}, {"headers", "{\"X-Custom\": \"v\"}"}};
        p = http_plan(rest, SourceOp::Test, cfg, QString(), QString(), 10);
        QCOMPARE(hdr(p.calls[0], "X-Custom"), QByteArray("v"));
        cfg = {{"baseUrl", "http://h"}, {"headers", "not a header line"}};
        QVERIFY(!http_plan(rest, SourceOp::Test, cfg, QString(), QString(), 10).ok);
        QCOMPARE(http_plan(rest, SourceOp::Test, QJsonObject(), QString(), QString(), 10).error.field, QStringLiteral("baseUrl"));
        cfg = {{"baseUrl", "http://h"}, {"authType", "bearer"}};
        QCOMPARE(http_plan(rest, SourceOp::Test, cfg, QString(), QString(), 10).error.code, ErrorCode::MissingKey);
    }
    void vendor_plans() {
        HttpPlan es = http_plan(type_support("elasticsearch"), SourceOp::Fetch,
                                {{"node", "http://es:9200"}, {"username", "u"}, {"password", "p"}}, "prices", "sym:AAPL", 25);
        QVERIFY(es.ok);
        QCOMPARE(es.calls[0].url.path(), QStringLiteral("/prices/_search"));
        QCOMPARE(QJsonDocument::fromJson(es.calls[0].body).object().value("size").toInt(), 25);
        QVERIFY(hdr(es.calls[0], "Authorization").startsWith("Basic "));

        HttpPlan ch = http_plan(type_support("clickhouse"), SourceOp::Fetch, {{"host", "ch"}, {"port", 8123}}, "trades",
                                QString(), 10);
        QCOMPARE(ch.calls[0].body, QByteArray("SELECT * FROM `trades` LIMIT 10 FORMAT JSON"));
        QCOMPARE(ch.calls[0].url.port(), 8123);

        HttpPlan neo = http_plan(type_support("neo4j"), SourceOp::Test, {{"uri", "bolt://graph:7687"}}, QString(), QString(), 5);
        QCOMPARE(neo.calls[0].url.toString(), QStringLiteral("http://graph:7474/db/neo4j/tx/commit"));

        HttpPlan prom = http_plan(type_support("prometheus"), SourceOp::Fetch, {{"url", "http://p:9090"}}, "up", QString(), 5);
        QCOMPARE(QUrlQuery(prom.calls[0].url).queryItemValue("query"), QStringLiteral("up"));

        HttpPlan s3 = http_plan(type_support("aws-s3"), SourceOp::Fetch,
                                {{"region", "eu-west-1"}, {"bucket", "b"}, {"accessKeyId", "AK"}, {"secretAccessKey", "SK"}},
                                "data/prices.csv", QString(), 100);
        QVERIFY(s3.ok);
        QCOMPARE(s3.calls[0].sign, QStringLiteral("aws_v4"));
        QCOMPARE(s3.calls[0].url.toString(), QStringLiteral("https://s3.eu-west-1.amazonaws.com/b/data/prices.csv"));
        QCOMPARE(s3.calls[0].object_name, QStringLiteral("data/prices.csv"));
        HttpPlan s3l = http_plan(type_support("minio"), SourceOp::List,
                                 {{"endpoint", "http://minio:9000"}, {"bucket", "b"}, {"accessKey", "AK"}, {"secretKey", "SK"}},
                                 QString(), QString(), 100);
        QCOMPARE(QUrlQuery(s3l.calls[0].url).queryItemValue("list-type"), QStringLiteral("2"));

        HttpPlan az = http_plan(type_support("azure-blob"), SourceOp::List,
                                {{"accountName", "acct"}, {"accountKey", "?sv=2022&sig=abc"}, {"container", "c"}}, QString(),
                                QString(), 10);
        QVERIFY(az.calls[0].sign.isEmpty()); // SAS: appended, not signed
        QCOMPARE(QUrlQuery(az.calls[0].url).queryItemValue("sig"), QStringLiteral("abc"));

        HttpPlan bn = http_plan(type_support("binance"), SourceOp::List, {}, QString(), QString(), 10);
        QVERIFY(bn.ok);
        QVERIFY(bn.calls.isEmpty());
        QVERIFY(!bn.static_rows.isEmpty());

        HttpPlan db = http_plan(type_support("databricks"), SourceOp::Test,
                                {{"serverHostname", "x.cloud.databricks.com"}, {"httpPath", "/sql/1.0/warehouses/abc123"},
                                 {"accessToken", "dapi"}},
                                QString(), QString(), 10);
        QCOMPARE(db.calls[0].url.path(), QStringLiteral("/api/2.0/sql/warehouses/abc123"));
        QVERIFY(!http_plan(type_support("databricks"), SourceOp::Test,
                           {{"serverHostname", "x"}, {"httpPath", "/sql/protocolv1/o/1/2"}, {"accessToken", "t"}}, QString(),
                           QString(), 10)
                     .ok);
    }
    void post_processing() {
        HttpCall prom;
        prom.post = "prom";
        PostResult r = post_process(prom, SourceOp::Fetch,
                                    J("{'status':'success','data':{'resultType':'vector','result':[{'metric':{'job':'api'},"
                                      "'value':[1704153600,'1.5']}]}}"),
                                    "application/json");
        QVERIFY(r.ok);
        const DataTable& t = r.data.tables.first();
        QCOMPARE(t.cell(0, t.column_index("value")).toDouble(), 1.5);
        QCOMPARE(t.cell(0, t.column_index("timestamp")).toString(), QStringLiteral("2024-01-02T00:00:00Z"));
        r = post_process(prom, SourceOp::Fetch, J("{'status':'error','error':'parse error at char 3'}"), "application/json");
        QVERIFY(!r.ok);
        QCOMPARE(r.error.code, ErrorCode::BadArgs);

        HttpCall q;
        q.post = "questdb";
        r = post_process(q, SourceOp::Fetch,
                         J("{'columns':[{'name':'ts','type':'TIMESTAMP'},{'name':'px','type':'DOUBLE'}],'dataset':[['2024-01-02T00:00:00Z',1.5]]}"),
                         "application/json");
        QVERIFY(r.ok);
        QCOMPARE(r.data.tables.first().types[1], ColumnType::Number);

        HttpCall ch;
        ch.post = "clickhouse";
        r = post_process(ch, SourceOp::Fetch,
                         J("{'meta':[{'name':'n','type':'UInt64'},{'name':'s','type':'String'}],'data':[{'n':'18446744073','s':'x'}]}"),
                         "application/json");
        QVERIFY(r.ok);
        QVERIFY(r.data.tables.first().cell(0, 0).isDouble()); // quoted 64-bit integer → number

        HttpCall es;
        es.post = "es_hits";
        r = post_process(es, SourceOp::Fetch,
                         J("{'hits':{'total':{'value':5},'hits':[{'_id':'1','_source':{'sym':'AAPL','q':{'px':1}}}]}}"),
                         "application/json");
        QVERIFY(r.ok);
        QVERIFY(r.data.tables.first().columns.contains("q.px"));
        QVERIFY(r.data.truncated);

        HttpCall influx;
        influx.post = "influx_csv";
        r = post_process(influx, SourceOp::Fetch,
                         "#datatype,string,long,dateTime:RFC3339,double\r\n#group,false,false,false,false\r\n#default,_result,,,\r\n"
                         ",result,table,_time,_value\r\n,,0,2024-01-02T00:00:00Z,1.5\r\n,,0,2024-01-03T00:00:00Z,2\r\n",
                         "text/csv");
        QVERIFY2(r.ok, qPrintable(r.error.message));
        QCOMPARE(r.data.tables.first().columns, (QStringList{"_time", "_value"}));
        QCOMPARE(r.data.tables.first().row_count(), 2);

        HttpCall neo;
        neo.post = "neo4j";
        r = post_process(neo, SourceOp::Test,
                         J("{'results':[],'errors':[{'code':'Neo.ClientError.Security.Unauthorized','message':'The client is "
                           "unauthorized due to authentication failure.'}]}"),
                         "application/json");
        QCOMPARE(r.error.code, ErrorCode::AuthFailed);

        HttpCall gql;
        gql.post = "graphql";
        r = post_process(gql, SourceOp::Fetch, J("{'errors':[{'message':'Cannot query field'}]}"), "application/json");
        QCOMPARE(r.error.code, ErrorCode::BadArgs);
        r = post_process(gql, SourceOp::Fetch, J("{'data':{'prices':[{'d':'2024-01-02','c':1},{'d':'2024-01-03','c':2}]}}"),
                         "application/json");
        QVERIFY(r.ok);
        QCOMPARE(r.data.tables.first().row_count(), 2);

        HttpCall s3;
        s3.post = "s3_list";
        s3.parse.format = TextFormat::Xml;
        s3.parse.rows_path = "Contents";
        r = post_process(s3, SourceOp::Test, "<ListBucketResult><Name>b</Name><KeyCount>0</KeyCount></ListBucketResult>",
                         "application/xml");
        QVERIFY(r.ok); // empty bucket is fine
        r = post_process(s3, SourceOp::List,
                         "<ListBucketResult><Contents><Key>a.csv</Key><Size>10</Size></Contents>"
                         "<Contents><Key>b.json</Key><Size>20</Size></Contents></ListBucketResult>",
                         "application/xml");
        QCOMPARE(r.data.tables.first().row_count(), 2);

        HttpCall k;
        k.post = "binance_klines";
        r = post_process(k, SourceOp::Fetch,
                         "[[1704153600000,\"42000.1\",\"43000\",\"41000\",\"42500.5\",\"1234.5\",1704239999999,\"0\",100,\"0\",\"0\",\"0\"]]",
                         "application/json");
        QVERIFY(r.ok);
        QCOMPARE(r.data.tables.first().cell(0, r.data.tables.first().column_index("close")).toDouble(), 42500.5);
    }
};

QTEST_GUILESS_MAIN(TestDataSourceDrivers)
#include "tst_data_source_drivers.moc"
