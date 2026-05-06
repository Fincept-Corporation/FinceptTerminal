// ConnectionTester.cpp — provider-specific URL synthesis + async TCP probe.
//
// The provider table here mirrors what each REST API exposes as a cheap
// health-check endpoint. For protocols (gRPC/MQTT/AMQP/Kafka) we return {}
// and let the host+port TCP fallback in test_connection() do the work.

#include "screens/data_sources/ConnectionTester.h"

#include "core/logging/Logger.h"
#include "screens/data_sources/DataSourcesHelpers.h"
#include "storage/repositories/DataSourceRepository.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QStringList>
#include <QTcpSocket>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <utility>

namespace fincept::screens::datasources {

namespace col = fincept::ui::colors;

namespace {
constexpr const char* TAG = "DataSources";
} // namespace

// Provider-specific probe URL synthesis. Returns {} when no HTTP probe is
// applicable — the caller's TCP host+port fallback will handle it.
QString provider_probe_url(const QString& provider_id, const QJsonObject& cfg) {
    // ── Market data ───────────────────────────────────────────────────────────
    if (provider_id == "yahoo-finance")
        return "https://query1.finance.yahoo.com/v8/finance/spark?symbols=AAPL&range=1d&interval=1d";
    if (provider_id == "alpha-vantage") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY"
                       "&symbol=IBM&interval=5min&apikey=%1")
            .arg(key.isEmpty() ? "demo" : key);
    }
    if (provider_id == "finnhub") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://finnhub.io/api/v1/stock/profile2?symbol=AAPL&token=%1").arg(key);
    }
    if (provider_id == "iex-cloud") {
        const QString token = cfg.value("token").toString().trimmed();
        const bool sandbox = cfg.value("sandbox").toBool();
        const QString base = sandbox ? "https://sandbox.iexapis.com" : "https://cloud.iexapis.com";
        return QString("%1/stable/stock/AAPL/quote?token=%2").arg(base, token);
    }
    if (provider_id == "twelve-data") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://api.twelvedata.com/time_series?symbol=AAPL&interval=1min&outputsize=1&apikey=%1")
            .arg(key);
    }
    if (provider_id == "quandl") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://data.nasdaq.com/api/v3/datasets/WIKI/AAPL.json?rows=1&api_key=%1").arg(key);
    }
    if (provider_id == "polygon-io")
        return "https://api.polygon.io/v2/aggs/ticker/AAPL/range/1/day/2023-01-09/2023-01-09";
    if (provider_id == "factset")            return "https://api.factset.com/health/alive";
    if (provider_id == "refinitiv")          return "https://api.refinitiv.com/";
    if (provider_id == "pitchbook")          return "https://api.pitchbook.com/";
    if (provider_id == "nasdaq-totalview")   return "https://api.nasdaq.com/api/quote/AAPL/info?assetClass=stocks";
    if (provider_id == "fincept")            return {};

    // ── Crypto ────────────────────────────────────────────────────────────────
    if (provider_id == "binance")            return "https://api.binance.com/api/v3/ping";
    if (provider_id == "coinbase")           return "https://api.coinbase.com/v2/time";
    if (provider_id == "kraken")             return "https://api.kraken.com/0/public/Time";
    if (provider_id == "coinmarketcap") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://pro-api.coinmarketcap.com/v1/cryptocurrency/listings/latest?limit=1&CMC_PRO_API_KEY=%1")
            .arg(key);
    }
    if (provider_id == "coingecko")          return "https://api.coingecko.com/api/v3/ping";

    // ── EOD / Tiingo / Intrinio ───────────────────────────────────────────────
    if (provider_id == "eod-historical") {
        const QString token = cfg.value("apiToken").toString().trimmed();
        return QString("https://eodhd.com/api/eod/AAPL.US?api_token=%1&fmt=json&limit=1").arg(token);
    }
    if (provider_id == "tiingo") {
        const QString token = cfg.value("apiToken").toString().trimmed();
        return QString("https://api.tiingo.com/api/test?token=%1").arg(token);
    }
    if (provider_id == "intrinio") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://api-v2.intrinio.com/securities/AAPL?api_key=%1").arg(key);
    }

    // ── Reuters / Refinitiv (legacy) ──────────────────────────────────────────
    if (provider_id == "reuters") {
        const QString appKey = cfg.value("appKey").toString().trimmed();
        return QString("https://api.refinitiv.com/user-framework/machine-id/v1/health?appKey=%1").arg(appKey);
    }
    if (provider_id == "marketstack") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        const bool use_https = cfg.value("https").toBool();
        return QString("%1://api.marketstack.com/v1/tickers/AAPL?access_key=%2")
            .arg(use_https ? "https" : "http", key);
    }
    if (provider_id == "finage") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://api.finage.co.uk/agg/stock/prev-close/AAPL?apikey=%1").arg(key);
    }
    if (provider_id == "tradier") {
        const bool sandbox = cfg.value("sandbox").toBool();
        return sandbox ? "https://sandbox.tradier.com/v1/markets/clock" : "https://api.tradier.com/v1/markets/clock";
    }

    // ── Alternative data ─────────────────────────────────────────────────────
    if (provider_id == "ravenpack")               return "https://api.ravenpack.com/user-text";
    if (provider_id == "bloomberg-second-measure") return "https://app.secondmeasure.com/";
    if (provider_id == "safegraph")               return "https://api.safegraph.com/v1/healthcheck";
    if (provider_id == "orbital-insight")         return "https://api.orbitalinsight.com/";
    if (provider_id == "refinitiv-tick-history")
        return "https://selectapi.datascope.refinitiv.com/RestApi/v1/Authentication/RequestToken";
    if (provider_id == "earnest-research")        return "https://api.earnestresearch.com/";
    if (provider_id == "thinknum")                return "https://data.thinknum.com/";
    if (provider_id == "revelio-labs")            return "https://api.reveliolabs.com/";
    if (provider_id == "adanos-market-sentiment") return "https://api.adanos.org/news/stocks/v1/health";

    // ── Open banking ─────────────────────────────────────────────────────────
    if (provider_id == "plaid")     return "https://production.plaid.com/";
    if (provider_id == "mx")        return "https://int-api.mx.com/";
    if (provider_id == "finicity")  return "https://api.finicity.com/";
    if (provider_id == "truelayer") return "https://auth.truelayer.com/";
    if (provider_id == "fdx-api") {
        const QString base = cfg.value("baseUrl").toString().trimmed();
        if (!base.isEmpty()) return base;
        return "https://api.financial-data-exchange.org/";
    }

    // ── Generic protocol connectors ───────────────────────────────────────────
    if (provider_id == "rest-api") {
        const QString base = cfg.value("baseUrl").toString().trimmed();
        if (!base.isEmpty()) return base;
        return {};
    }
    if (provider_id == "graphql") {
        const QString ep = cfg.value("endpoint").toString().trimmed();
        if (!ep.isEmpty()) return ep;
        return {};
    }
    if (provider_id == "websocket") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return url;
        return {};
    }
    if (provider_id == "soap") {
        const QString wsdl = cfg.value("wsdlUrl").toString().trimmed();
        if (!wsdl.isEmpty()) return wsdl;
        return {};
    }
    if (provider_id == "odata") {
        const QString root = cfg.value("serviceRoot").toString().trimmed();
        if (!root.isEmpty()) return (root.endsWith('/') ? root : root + "/") + "$metadata";
        return {};
    }
    if (provider_id == "apache-flink") {
        const QString jm = cfg.value("jobManagerUrl").toString().trimmed();
        if (!jm.isEmpty()) return (jm.endsWith('/') ? jm : jm + "/") + "overview";
        return "http://localhost:8081/overview";
    }
    if (provider_id == "apache-iceberg") {
        const QString catalog = cfg.value("catalog").toString().trimmed().toLower();
        const QString uri     = cfg.value("catalogUri").toString().trimmed();
        if (catalog == "rest" && !uri.isEmpty())
            return (uri.endsWith('/') ? uri : uri + "/") + "v1/config";
        return {};
    }

    // ── Cloud storage ────────────────────────────────────────────────────────
    if (provider_id == "aws-s3")               return "https://s3.amazonaws.com/";
    if (provider_id == "google-cloud-storage") return "https://storage.googleapis.com/";
    if (provider_id == "azure-blob")           return "https://blob.core.windows.net/";
    if (provider_id == "digitalocean-spaces")  return "https://nyc3.digitaloceanspaces.com/";
    if (provider_id == "backblaze-b2")         return "https://api.backblazeb2.com/b2api/v2/b2_authorize_account";
    if (provider_id == "wasabi")               return "https://s3.wasabisys.com/";
    if (provider_id == "cloudflare-r2")        return "https://api.cloudflare.com/client/v4/user/tokens/verify";
    if (provider_id == "oracle-cloud-storage") return "https://objectstorage.us-ashburn-1.oraclecloud.com/";
    if (provider_id == "minio") {
        const QString ep = cfg.value("endpoint").toString().trimmed();
        if (!ep.isEmpty()) return (ep.startsWith("http") ? ep : "http://" + ep) + "/minio/health/live";
        return {};
    }
    if (provider_id == "ibm-cloud-storage") {
        const QString ep = cfg.value("endpoint").toString().trimmed();
        if (!ep.isEmpty()) return ep;
        return "https://s3.us.cloud-object-storage.appdomain.cloud/";
    }

    // ── Search & analytics ────────────────────────────────────────────────────
    if (provider_id == "elasticsearch") {
        const QString node = cfg.value("node").toString().trimmed();
        if (!node.isEmpty()) return (node.endsWith('/') ? node : node + "/") + "_cluster/health";
        return {};
    }
    if (provider_id == "opensearch") {
        const QString node = cfg.value("node").toString().trimmed();
        if (!node.isEmpty()) return (node.endsWith('/') ? node : node + "/") + "_cluster/health";
        return {};
    }
    if (provider_id == "solr") {
        const QString host = cfg.value("host").toString().trimmed();
        if (!host.isEmpty()) return (host.startsWith("http") ? host : "http://" + host) + "/solr/admin/info/system";
        return {};
    }
    if (provider_id == "meilisearch") {
        const QString host = cfg.value("host").toString().trimmed();
        if (!host.isEmpty()) return (host.startsWith("http") ? host : "http://" + host) + "/health";
        return {};
    }
    if (provider_id == "algolia") {
        const QString appId = cfg.value("appId").toString().trimmed();
        if (!appId.isEmpty()) return QString("https://%1-dsn.algolia.net/1/isalive").arg(appId);
        return {};
    }

    // ── Time series ───────────────────────────────────────────────────────────
    if (provider_id == "influxdb") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return (url.endsWith('/') ? url : url + "/") + "health";
        return {};
    }
    if (provider_id == "prometheus") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return (url.endsWith('/') ? url : url + "/") + "-/healthy";
        return {};
    }
    if (provider_id == "victoriametrics") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return (url.endsWith('/') ? url : url + "/") + "health";
        return {};
    }
    if (provider_id == "opentsdb") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return (url.endsWith('/') ? url : url + "/") + "api/version";
        return {};
    }

    // ── Data warehouses ───────────────────────────────────────────────────────
    if (provider_id == "bigquery")  return "https://bigquery.googleapis.com/";
    if (provider_id == "firestore") return "https://firestore.googleapis.com/";
    if (provider_id == "databricks") {
        const QString h = cfg.value("serverHostname").toString().trimmed();
        if (!h.isEmpty()) return QString("https://%1/api/2.0/clusters/list").arg(h);
        return {};
    }
    if (provider_id == "synapse") {
        const QString server = cfg.value("server").toString().trimmed();
        if (!server.isEmpty()) return QString("https://%1").arg(server);
        return {};
    }

    return {};
}

namespace {

QString simple_dialog_qss() {
    return QString("QDialog{background:%1;color:%2;font-family:'Consolas','Courier New',monospace;}"
                   "QLabel{font-size:13px;background:transparent;}"
                   "QPushButton{background:%3;color:%2;border:1px solid %4;"
                   "padding:6px 18px;font-size:12px;font-weight:700;}"
                   "QPushButton:hover{background:%4;}")
        .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::BG_RAISED(), col::BORDER_DIM());
}

void show_message_dialog(QWidget* parent, const QString& title, const QString& body) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.resize(420, 160);
    dlg.setModal(true);
    dlg.setStyleSheet(simple_dialog_qss());
    auto* vl = new QVBoxLayout(&dlg);
    vl->setContentsMargins(24, 20, 24, 16);
    vl->setSpacing(10);
    auto* lbl = new QLabel(body);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;").arg(col::TEXT_SECONDARY()));
    vl->addWidget(lbl);
    vl->addStretch();
    auto* btn = new QPushButton("Close");
    btn->setCursor(Qt::PointingHandCursor);
    QObject::connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    auto* row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(btn);
    vl->addLayout(row);
    dlg.exec();
}

void show_result_dialog(QWidget* parent, const QString& display, bool success, const QString& message) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QString("Test: %1").arg(display));
    dlg.resize(440, 190);
    dlg.setModal(true);
    dlg.setStyleSheet(simple_dialog_qss());

    auto* vl = new QVBoxLayout(&dlg);
    vl->setContentsMargins(24, 20, 24, 16);
    vl->setSpacing(10);

    auto* status_lbl = new QLabel(success ? "Connection successful" : "Connection failed");
    status_lbl->setStyleSheet(
        QString("color:%1;font-size:14px;font-weight:700;background:transparent;")
            .arg(success ? col::POSITIVE.operator QString() : col::NEGATIVE.operator QString()));
    vl->addWidget(status_lbl);

    auto* msg_lbl = new QLabel(message);
    msg_lbl->setWordWrap(true);
    msg_lbl->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(col::TEXT_SECONDARY()));
    vl->addWidget(msg_lbl);

    if (success) {
        auto* note = new QLabel("Note: TCP reachability confirmed. API key validity is not verified here.");
        note->setWordWrap(true);
        note->setStyleSheet(
            QString("color:%1;font-size:11px;font-style:italic;background:transparent;").arg(col::TEXT_TERTIARY()));
        vl->addWidget(note);
    }
    vl->addStretch();

    auto* close_btn = new QPushButton("Close");
    close_btn->setCursor(Qt::PointingHandCursor);
    QObject::connect(close_btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch();
    btn_row->addWidget(close_btn);
    vl->addLayout(btn_row);

    dlg.exec();
}

} // namespace

void test_connection(QWidget* parent, const QString& conn_id, const TestResultCallback& on_result) {
    const auto get_result = DataSourceRepository::instance().get(conn_id);
    if (get_result.is_err())
        return;

    const auto ds = get_result.value();
    const auto cfg_doc = QJsonDocument::fromJson(ds.config.toUtf8());
    const auto cfg_obj = cfg_doc.object();

    const auto* connector_cfg = find_connector_config(ds.provider);
    if (connector_cfg && !connector_cfg->testable) {
        show_message_dialog(parent, QString("Test: %1").arg(ds.display_name),
                            "This connector does not support connectivity testing.");
        return;
    }

    const QString display  = ds.display_name;
    const QString provider = ds.provider;

    const QString probe_url = provider_probe_url(provider, cfg_obj);

    QString explicit_url;
    if (probe_url.isEmpty()) {
        const QStringList url_keys = {"url", "baseUrl", "endpoint", "wsdlUrl", "serviceRoot"};
        for (const auto& key : url_keys) {
            const QString v = cfg_obj.value(key).toString().trimmed();
            if (!v.isEmpty()) {
                explicit_url = v;
                break;
            }
        }
    }

    const QString test_url = probe_url.isEmpty() ? explicit_url : probe_url;

    QString host = cfg_obj.value("host").toString().trimmed();
    int port = cfg_obj.value("port").toVariant().toInt();
    if (port <= 0) port = cfg_obj.value("port").toString().trimmed().toInt();

    if (host.isEmpty()) {
        const QString brokers = cfg_obj.value("brokers").toString().trimmed();
        if (!brokers.isEmpty()) {
            const QString first = brokers.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) {
                host = first.left(idx);
                port = first.mid(idx + 1).toInt();
            } else {
                host = first;
            }
        }
    }
    if (host.isEmpty()) {
        for (const auto& key : {"servers", "hosts"}) {
            const QString val = cfg_obj.value(key).toString().trimmed();
            if (val.isEmpty()) continue;
            const QString first = val.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const QUrl u(first);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0) port = u.port();
            } else {
                const int idx = first.lastIndexOf(':');
                if (idx > 0) {
                    host = first.left(idx);
                    port = first.mid(idx + 1).toInt();
                } else {
                    host = first;
                }
            }
            if (!host.isEmpty()) break;
        }
    }
    if (host.isEmpty()) {
        const QString cs = cfg_obj.value("connectionString").toString().trimmed();
        if (!cs.isEmpty()) {
            const QUrl u(cs);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0) port = u.port(27017);
            }
        }
    }
    if (host.isEmpty()) {
        const QString uri = cfg_obj.value("uri").toString().trimmed();
        if (!uri.isEmpty()) {
            const QUrl u(uri);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0) port = u.port(7687);
            }
        }
    }
    if (host.isEmpty()) {
        const QString cp = cfg_obj.value("contactPoints").toString().trimmed();
        if (!cp.isEmpty()) {
            const QString first = cp.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) {
                host = first.left(idx);
                port = first.mid(idx + 1).toInt();
            } else {
                host = first;
            }
            if (port <= 0) port = cfg_obj.value("port").toVariant().toInt();
            if (port <= 0) port = 9042;
        }
    }
    if (host.isEmpty()) {
        const QString zk = cfg_obj.value("zkQuorum").toString().trimmed();
        if (!zk.isEmpty()) {
            const QString first = zk.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) {
                host = first.left(idx);
                port = first.mid(idx + 1).toInt();
            } else {
                host = first;
                port = 2181;
            }
        }
    }
    if (host.isEmpty()) {
        for (const auto& key : {"serverHostname", "server", "account"}) {
            const QString val = cfg_obj.value(key).toString().trimmed();
            if (!val.isEmpty()) {
                host = val;
                break;
            }
        }
    }
    if (host.isEmpty() && !test_url.isEmpty()) {
        const QUrl u(test_url);
        if (u.isValid() && !u.host().isEmpty()) {
            host = u.host();
            if (port <= 0) {
                const QString s = u.scheme().toLower();
                port = u.port((s == "https" || s == "wss") ? 443 : 80);
            }
        }
    }

    if (test_url.isEmpty() && (host.isEmpty() || port <= 0)) {
        show_message_dialog(parent, QString("Test: %1").arg(display),
                            "No testable endpoint found in the saved configuration.\n"
                            "Ensure required fields (URL, host, or API key) are filled in.");
        return;
    }

    QPointer<QWidget> parent_guard = parent;
    const QString captured_test_url = test_url;
    const QString captured_host = host;
    const int captured_port = port;
    const QString captured_display = display;
    const QString captured_conn_id = conn_id;
    const TestResultCallback captured_cb = on_result;

    const auto test_future = QtConcurrent::run(
        [parent_guard, captured_conn_id, captured_display, captured_test_url, captured_host, captured_port, captured_cb]() {
            bool success = false;
            QString message = "No testable endpoint found";

            auto tcp_probe = [](const QString& h, int p, int timeout_ms) -> std::pair<bool, QString> {
                QTcpSocket socket;
                socket.connectToHost(h, static_cast<quint16>(p));
                if (socket.waitForConnected(timeout_ms)) {
                    socket.disconnectFromHost();
                    return {true, QString("TCP connected to %1:%2").arg(h).arg(p)};
                }
                return {false, QString("Cannot connect to %1:%2 — %3").arg(h).arg(p).arg(socket.errorString())};
            };

            if (!captured_test_url.isEmpty()) {
                const QUrl url(captured_test_url);
                if (!url.isValid() || url.host().isEmpty()) {
                    message = QString("Invalid URL: %1").arg(captured_test_url);
                } else {
                    const QString url_host = url.host();
                    const QString scheme = url.scheme().toLower();
                    const int default_port = (scheme == "https" || scheme == "wss") ? 443 : 80;
                    const int url_port = url.port(default_port);
                    auto [ok, msg] = tcp_probe(url_host, url_port, 5000);
                    success = ok;
                    message = ok ? QString("Endpoint reachable: %1").arg(captured_test_url) : msg;
                }
            } else if (!captured_host.isEmpty() && captured_port > 0) {
                auto [ok, msg] = tcp_probe(captured_host, captured_port, 3000);
                success = ok;
                message = msg;
            }

            QMetaObject::invokeMethod(
                qApp,
                [parent_guard, captured_conn_id, captured_display, success, message, captured_cb]() {
                    if (!parent_guard) return;
                    LOG_INFO(TAG, QString("Test %1: %2 — %3")
                                      .arg(captured_display, success ? "OK" : "FAIL", message));
                    if (captured_cb) captured_cb(captured_conn_id, success, message);
                    show_result_dialog(parent_guard.data(), captured_display, success, message);
                },
                Qt::QueuedConnection);
        });
    Q_UNUSED(test_future);
}

} // namespace fincept::screens::datasources
