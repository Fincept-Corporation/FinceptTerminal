#include "trading/instruments/InstrumentService.h"

#include "core/logging/Logger.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/InstrumentRepository.h"
#include "trading/instruments/ZerodhaInstrumentParser.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

namespace fincept::trading {

namespace {

static QString normalise_index_symbol(const QString& raw_name) {
    QString s = raw_name.toUpper();
    s.replace("S&P ", "");
    s.remove(' ');
    s.remove('-');

    if (s == "NIFTY50")
        return "NIFTY";
    if (s == "NIFTYBANK")
        return "BANKNIFTY";
    if (s == "NIFTYFINSERVICE")
        return "FINNIFTY";
    if (s == "NIFTYNEXT50")
        return "NIFTYNXT50";
    if (s == "NIFTYMIDSELECT" || s == "NIFTYMIDCAPSELECT")
        return "MIDCPNIFTY";
    if (s == "SNSX50")
        return "SENSEX50";
    return s;
}

static QString parse_expiry_nodash(const QString& expiry_raw) {
    if (expiry_raw.isEmpty())
        return {};

    QDate d = QDate::fromString(expiry_raw.toUpper(), "ddMMMyyyy");
    if (!d.isValid())
        d = QDate::fromString(expiry_raw, "yyyy-MM-dd");
    if (!d.isValid())
        return {};

    return d.toString("ddMMMyy").toUpper();
}

static QString expiry_display(const QString& expiry_nd) {
    if (expiry_nd.size() != 7)
        return {};
    return expiry_nd.left(2) + "-" + expiry_nd.mid(2, 3) + "-" + expiry_nd.right(2);
}

static InstrumentType map_angel_type(const QString& raw_type, const QString& symbol) {
    const QString t = raw_type.toUpper();
    const QString s = symbol.toUpper();

    if (s.endsWith("CE"))
        return InstrumentType::CE;
    if (s.endsWith("PE"))
        return InstrumentType::PE;
    if (t.startsWith("FUT"))
        return InstrumentType::FUT;
    if (t == "AMXIDX" || t == "INDEX")
        return InstrumentType::INDEX;
    if (t == "EQ" || s.endsWith("-EQ") || s.endsWith("-BE"))
        return InstrumentType::EQ;
    return InstrumentType::UNKNOWN;
}

static QString normalise_spot_symbol(const QString& symbol) {
    QString s = symbol.toUpper();
    s.replace(QRegularExpression("(-EQ|-BE|-MF|-SG)$"), "");
    return s;
}

static QString option_suffix(const QString& symbol) {
    const QString s = symbol.toUpper();
    if (s.endsWith("CE"))
        return "CE";
    if (s.endsWith("PE"))
        return "PE";
    return {};
}

static QVector<Instrument> parse_angel_master_json(const QByteArray& json_data) {
    QVector<Instrument> out;
    if (json_data.isEmpty())
        return out;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(json_data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        LOG_ERROR("InstrumentService", "AngelOne master JSON parse failed: " + err.errorString());
        return {};
    }

    const auto arr = doc.array();
    out.reserve(arr.size());

    for (const auto& v : arr) {
        if (!v.isObject())
            continue;

        const auto o = v.toObject();
        const QString token_s = o.value("token").toString().trimmed();
        bool token_ok = false;
        const qint64 token = token_s.toLongLong(&token_ok);
        if (!token_ok || token <= 0)
            continue;

        Instrument inst;
        inst.broker_id = "angelone";
        inst.instrument_token = token;
        inst.exchange_token = token;
        inst.brsymbol = o.value("symbol").toString().trimmed().toUpper();
        inst.name = o.value("name").toString().trimmed().toUpper();
        inst.brexchange = o.value("exch_seg").toString().trimmed().toUpper();
        inst.exchange = inst.brexchange;

        const QString raw_type = o.value("instrumenttype").toString().trimmed().toUpper();
        const QString exp_nd = parse_expiry_nodash(o.value("expiry").toString().trimmed());
        if (!exp_nd.isEmpty())
            inst.expiry = expiry_display(exp_nd);

        double strike = o.value("strike").toString().toDouble();
        if (strike == 0.0)
            strike = o.value("strike").toDouble();
        inst.strike = strike > 0 ? (strike / 100.0) : 0.0;

        int lot = o.value("lotsize").toString().toInt();
        if (lot <= 0)
            lot = o.value("lotsize").toInt();
        inst.lot_size = lot > 0 ? lot : 1;

        double tick = o.value("tick_size").toString().toDouble();
        if (tick == 0.0)
            tick = o.value("tick_size").toDouble();
        inst.tick_size = tick > 0.0 ? (tick / 100.0) : 0.05;

        if (raw_type == "AMXIDX") {
            inst.exchange = inst.brexchange + "_INDEX";
            inst.symbol = normalise_index_symbol(inst.name.isEmpty() ? inst.brsymbol : inst.name);
        } else if (raw_type.startsWith("FUT")) {
            inst.symbol = inst.name.toUpper() + exp_nd + "FUT";
        } else if (raw_type.startsWith("OPT")) {
            QString suffix = option_suffix(inst.brsymbol);
            QString strike_str = QString::number(static_cast<int>(inst.strike));
            inst.symbol = inst.name.toUpper() + exp_nd + strike_str + suffix;
        } else {
            inst.symbol = normalise_spot_symbol(inst.brsymbol);
        }

        if (inst.symbol.isEmpty())
            inst.symbol = normalise_spot_symbol(inst.brsymbol);

        inst.instrument_type = map_angel_type(raw_type, inst.symbol);
        out.append(std::move(inst));
    }

    LOG_INFO("InstrumentService", QString("Parsed %1 AngelOne instruments").arg(out.size()));
    return out;
}

} // namespace

InstrumentService& InstrumentService::instance() {
    static InstrumentService inst;
    return inst;
}

// ── Public API ────────────────────────────────────────────────────────────────

void InstrumentService::refresh(const QString& broker_id, const BrokerCredentials& creds, int max_age_hours) {
    Q_UNUSED(max_age_hours)
    // For AngelOne we need NSE equities to be present — check specifically for NSE count.
    // A partial cache (e.g. only NFO/MCX) means a previous download was incomplete.
    {
        QMutexLocker lock(&mutex_);
        if (caches_.contains(broker_id) && caches_[broker_id].loaded) {
            const Cache& c = caches_[broker_id];
            // Count NSE equity entries (exchange == "NSE", no expiry = spot equity)
            int nse_count = 0;
            for (auto it = c.by_symbol.constBegin(); it != c.by_symbol.constEnd(); ++it) {
                if (it.key().second == "NSE" && it->expiry.isEmpty())
                    ++nse_count;
            }
            if (nse_count > 100) {
                LOG_INFO("InstrumentService",
                         QString("%1 instruments already loaded (%2 NSE equities), skipping refresh")
                             .arg(broker_id)
                             .arg(nse_count));
                return;
            }
            LOG_WARN(
                "InstrumentService",
                QString("%1 cache incomplete (only %2 NSE equities) — re-downloading").arg(broker_id).arg(nse_count));
        }
    }
    force_refresh(broker_id, creds);
}

void InstrumentService::force_refresh(const QString& broker_id, const BrokerCredentials& creds) {
    {
        QMutexLocker lock(&mutex_);
        if (refreshing_.contains(broker_id)) {
            LOG_INFO("InstrumentService",
                     QString("%1 refresh already in progress — skipping duplicate").arg(broker_id));
            return;
        }
        refreshing_.insert(broker_id);
    }
    emit refresh_started(broker_id);
    QPointer<InstrumentService> self = this;
    QtConcurrent::run([self, broker_id, creds]() {
        if (!self)
            return;
        self->do_refresh(broker_id, creds);
    });
}

void InstrumentService::load_from_db(const QString& broker_id) {
    // Called at startup before QApplication::exec() — runs synchronously on
    // the main thread so that the single QSqlDatabase connection is not
    // accessed concurrently (QSqlDatabase is not thread-safe).
    QVector<Instrument> all;
    for (const QString& exch : QStringList{"NSE", "BSE", "NFO", "CDS", "MCX", "NSE_INDEX", "BSE_INDEX", "BFO", "BCD"}) {
        auto rows = InstrumentRepository::instance().list(exch, broker_id);
        all.append(rows);
    }
    if (all.isEmpty()) {
        LOG_WARN("InstrumentService", QString("No instruments found in DB for %1 — run refresh").arg(broker_id));
        return;
    }
    build_cache(broker_id, all);
    LOG_INFO("InstrumentService", QString("Loaded %1 instruments from DB for %2").arg(all.size()).arg(broker_id));
}

// ── Lookups ───────────────────────────────────────────────────────────────────

std::optional<qint64> InstrumentService::instrument_token(const QString& symbol, const QString& exchange,
                                                          const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return std::nullopt;
    auto jt = it->by_symbol.find({symbol, exchange});
    if (jt == it->by_symbol.end())
        return std::nullopt;
    return jt->instrument_token;
}

std::optional<QString> InstrumentService::to_brsymbol(const QString& symbol, const QString& exchange,
                                                      const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return std::nullopt;
    auto jt = it->by_symbol.find({symbol, exchange});
    if (jt == it->by_symbol.end())
        return std::nullopt;
    return jt->brsymbol;
}

std::optional<QString> InstrumentService::from_brsymbol(const QString& brsymbol, const QString& brexchange,
                                                        const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return std::nullopt;
    auto jt = it->by_brsymbol.find({brsymbol, brexchange});
    if (jt == it->by_brsymbol.end())
        return std::nullopt;
    return jt->symbol;
}

std::optional<Instrument> InstrumentService::find(const QString& symbol, const QString& exchange,
                                                  const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return std::nullopt;
    auto jt = it->by_symbol.find({symbol, exchange});
    if (jt == it->by_symbol.end())
        return std::nullopt;
    return *jt;
}

std::optional<Instrument> InstrumentService::find_by_token(quint32 instrument_token, const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return std::nullopt;
    auto jt = it->by_token.find(qint64(instrument_token));
    if (jt == it->by_token.end())
        return std::nullopt;
    return *jt;
}

QVector<Instrument> InstrumentService::search(const QString& query, const QString& exchange, const QString& broker_id,
                                              int limit) const {
    // Delegate to DB for search (cache doesn't hold a text index)
    return InstrumentRepository::instance().search(query, exchange, broker_id, limit);
}

int InstrumentService::cached_count(const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return 0;
    return it->by_symbol.size();
}

bool InstrumentService::is_loaded(const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    return it != caches_.end() && it->loaded;
}

// ── Private ───────────────────────────────────────────────────────────────────

void InstrumentService::build_cache(const QString& broker_id, const QVector<Instrument>& instruments) {
    QMutexLocker lock(&mutex_);
    Cache& cache = caches_[broker_id];
    cache.by_symbol.clear();
    cache.by_token.clear();
    cache.by_brsymbol.clear();

    for (const auto& inst : instruments) {
        cache.by_symbol.insert({inst.symbol, inst.exchange}, inst);
        cache.by_token.insert(inst.instrument_token, inst);
        cache.by_brsymbol.insert({inst.brsymbol, inst.brexchange}, inst);
    }
    cache.loaded = true;
}

void InstrumentService::do_refresh(const QString& broker_id, const BrokerCredentials& creds) {
    LOG_INFO("InstrumentService", "Downloading instruments for " + broker_id);

    QByteArray payload;
    if (broker_id == "zerodha") {
        payload = download_zerodha_csv(creds);
    } else if (broker_id == "angelone") {
        payload = download_angel_master_json();
    } else {
        QMetaObject::invokeMethod(
            this,
            [this, broker_id]() {
                {
                    QMutexLocker lock(&mutex_);
                    refreshing_.remove(broker_id);
                }
                emit refresh_failed(broker_id, "Instrument download not implemented for " + broker_id);
            },
            Qt::QueuedConnection);
        return;
    }

    if (payload.isEmpty()) {
        QMetaObject::invokeMethod(
            this,
            [this, broker_id]() {
                {
                    QMutexLocker lock(&mutex_);
                    refreshing_.remove(broker_id);
                }
                emit refresh_failed(broker_id, "Empty instrument data received");
            },
            Qt::QueuedConnection);
        return;
    }

    QVector<Instrument> instruments;
    if (broker_id == "zerodha") {
        instruments = ZerodhaInstrumentParser::parse(payload);
    } else if (broker_id == "angelone") {
        instruments = parse_angel_master_json(payload);
    }

    if (instruments.isEmpty()) {
        QMetaObject::invokeMethod(
            this,
            [this, broker_id]() {
                {
                    QMutexLocker lock(&mutex_);
                    refreshing_.remove(broker_id);
                }
                emit refresh_failed(broker_id, "Failed to parse instrument data");
            },
            Qt::QueuedConnection);
        return;
    }

    // DB write + cache build MUST happen on the main thread —
    // QSqlDatabase connections are not thread-safe (Qt docs: use only from owning thread).
    // Worker thread hands off instruments to the main thread here.
    int count = instruments.size();
    QMetaObject::invokeMethod(
        this,
        [this, broker_id, instruments, count]() {
            // Persist to DB (main thread — safe)
            auto r = InstrumentRepository::instance().replace_all(broker_id, instruments);
            if (r.is_err()) {
                QString err = QString::fromStdString(r.error());
                {
                    QMutexLocker lock(&mutex_);
                    refreshing_.remove(broker_id);
                }
                emit refresh_failed(broker_id, err);
                return;
            }
            // Rebuild in-memory cache
            build_cache(broker_id, instruments);
            {
                QMutexLocker lock(&mutex_);
                refreshing_.remove(broker_id);
            }
            LOG_INFO("InstrumentService", QString("Refresh complete: %1 instruments for %2").arg(count).arg(broker_id));
            emit refresh_done(broker_id, count);
        },
        Qt::QueuedConnection);
}

QByteArray InstrumentService::download_zerodha_csv(const BrokerCredentials& creds) {
    // GET https://api.kite.trade/instruments  (no auth required — public endpoint)
    // Returns raw CSV text, ~8MB
    QMap<QString, QString> headers = {
        {"X-Kite-Version", "3"},
        {"Authorization", "token " + creds.api_key + ":" + creds.access_token},
    };
    auto resp = BrokerHttp::instance().get("https://api.kite.trade/instruments", headers);
    if (!resp.success) {
        LOG_ERROR("InstrumentService", "Failed to download Zerodha instruments: " + resp.error);
        return {};
    }
    return resp.raw_body.toUtf8();
}

QByteArray InstrumentService::download_angel_master_json() {
    // Public endpoint — ~8MB JSON. Uses its own QNAM to avoid blocking BrokerHttp's
    // mutex (which would starve all concurrent quote/history API calls for 10-20s).
    static const QString kUrl = "https://margincalculator.angelbroking.com/OpenAPI_File/files/OpenAPIScripMaster.json";

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(kUrl)};
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(60000); // 60s — large file on slow connection
    loop.exec();

    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        LOG_ERROR("InstrumentService", "AngelOne master contract download timed out");
        return {};
    }
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_ERROR("InstrumentService", "Failed to download AngelOne master contract: " + reply->errorString());
        reply->deleteLater();
        return {};
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    LOG_INFO("InstrumentService", QString("Downloaded AngelOne master contract: %1 bytes").arg(data.size()));
    return data;
}

} // namespace fincept::trading
