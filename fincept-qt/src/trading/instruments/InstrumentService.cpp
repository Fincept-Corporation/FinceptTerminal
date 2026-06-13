#include "trading/instruments/InstrumentService.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"
#include "trading/brokers/BrokerHttp.h"
#include "trading/instruments/DhanInstrumentParser.h"
#include "trading/instruments/FyersInstrumentParser.h"
#include "trading/instruments/GrowwInstrumentParser.h"
#include "trading/instruments/IciciInstrumentParser.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentRepository.h"
#include "trading/instruments/InstrumentSources.h"
#include "trading/instruments/SymbolResolver.h"
#include "trading/instruments/ZerodhaInstrumentParser.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QMetaObject>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

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
            // Final pass through the shared normalizer so the index name matches every broker.
            inst.symbol = norm::normalise_index_symbol(normalise_index_symbol(inst.name.isEmpty() ? inst.brsymbol : inst.name));
        } else if (raw_type.startsWith("FUT")) {
            inst.symbol = inst.name.toUpper() + exp_nd + "FUT";
        } else if (raw_type.startsWith("OPT")) {
            QString suffix = option_suffix(inst.brsymbol);
            QString strike_str = norm::format_strike(inst.strike);
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

namespace {

/// Register the three first-party instrument sources with SymbolResolver.
/// Idempotent — replacing an existing entry is harmless.
void ensure_builtin_sources_registered() {
    static bool registered = []() {
        auto& r = SymbolResolver::instance();
        r.register_source({QStringLiteral("zerodha"),
                           [](const BrokerCredentials& c) { return InstrumentService::download_zerodha_csv(c); },
                           [](const QByteArray& p) { return ZerodhaInstrumentParser::parse(p); }});
        r.register_source({QStringLiteral("angelone"),
                           [](const BrokerCredentials&) { return InstrumentService::download_angel_master_json(); },
                           [](const QByteArray& p) { return parse_angel_master_json(p); }});
        r.register_source({QStringLiteral("groww"),
                           [](const BrokerCredentials&) { return InstrumentService::download_groww_csv(); },
                           [](const QByteArray& p) { return GrowwInstrumentParser::parse(p); }});
        r.register_source({QStringLiteral("fyers"),
                           [](const BrokerCredentials&) { return InstrumentService::download_fyers_json(); },
                           [](const QByteArray& p) { return FyersInstrumentParser::parse(p); }});
        r.register_source({QStringLiteral("dhan"),
                           [](const BrokerCredentials&) { return InstrumentService::download_dhan_csv(); },
                           [](const QByteArray& p) { return DhanInstrumentParser::parse(p); }});
        r.register_source({QStringLiteral("icicidirect"),
                           [](const BrokerCredentials&) { return InstrumentService::download_icici_csv(); },
                           [](const QByteArray& p) { return IciciInstrumentParser::parse(p); }});
        // The 11 additional Indian brokers (unified cross-broker search).
        register_extra_instrument_sources();
        return true;
    }();
    Q_UNUSED(registered);
}

} // namespace

InstrumentService& InstrumentService::instance() {
    static InstrumentService inst;
    ensure_builtin_sources_registered();
    return inst;
}

// ── Public API ────────────────────────────────────────────────────────────────

void InstrumentService::refresh(const QString& broker_id, const BrokerCredentials& creds, int max_age_hours) {
    // Age-based freshness gate. max_age_hours <= 0 disables it (always download).
    // Timestamp source: MAX(updated_at) on the instruments table (set on every
    // full refresh via DELETE+INSERT).
    if (max_age_hours > 0) {
        const QDateTime last = InstrumentRepository::instance().last_updated(broker_id);
        if (last.isValid()) {
            const qint64 age_hours = last.secsTo(QDateTime::currentDateTimeUtc()) / 3600;
            if (age_hours >= 0 && age_hours < max_age_hours) {
                LOG_INFO("InstrumentService",
                         QString("%1 instruments are fresh (%2h old < %3h) — skipping download")
                             .arg(broker_id)
                             .arg(age_hours)
                             .arg(max_age_hours));
                // Instruments on disk are fresh but may not be in the in-memory
                // cache yet (e.g. first refresh() of a relaunch within
                // max_age_hours). Without this, is_loaded() stays false forever
                // and every lookup fails. load_from_db() uses the shared
                // main-thread QSqlDatabase connection; refresh() is only ever
                // called from the UI/main thread (ChainSubTab + EquityTradingScreen
                // callbacks), so this is safe here.
                if (!is_loaded(broker_id))
                    load_from_db(broker_id);
                return;
            }
        }
    }
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
    (void)QtConcurrent::run([self, broker_id, creds]() {
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

void InstrumentService::load_from_db_async(const QString& broker_id,
                                            std::function<void(int)> callback) {
    // Fast path: if already loaded, fire callback immediately on UI thread.
    if (is_loaded(broker_id)) {
        int n = cached_count(broker_id);
        LOG_INFO("InstrumentService",
                 QString("load_from_db_async: %1 already loaded (%2 instruments)").arg(broker_id).arg(n));
        if (callback)
            callback(n);
        return;
    }

    LOG_INFO("InstrumentService", QString("load_from_db_async: starting background load for %1").arg(broker_id));

    QPointer<InstrumentService> self = this;
    QString db_path = fincept::Database::instance().path(); // read path on UI thread before going async

    (void)QtConcurrent::run([self, broker_id, db_path, callback]() {
        // Each worker thread needs its own named QSqlDatabase connection.
        const QString conn_name =
            "inst_async_" + broker_id + "_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVector<Instrument> all;

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn_name);
            db.setDatabaseName(db_path);
            if (!db.open()) {
                LOG_ERROR("InstrumentService",
                          QString("load_from_db_async: failed to open DB for %1: %2")
                              .arg(broker_id, db.lastError().text()));
            } else {
                // Single query for all exchanges — faster than 9 per-exchange queries.
                QSqlQuery q(db);
                q.prepare("SELECT instrument_token, exchange_token, symbol, brsymbol, name, "
                          "exchange, brexchange, expiry, strike, lot_size, instrument_type, "
                          "tick_size, broker_id, broker_token "
                          "FROM instruments WHERE broker_id = ?");
                q.addBindValue(broker_id);
                if (q.exec()) {
                    while (q.next())
                        all.append(InstrumentRepository::map_row_static(q));
                } else {
                    LOG_ERROR("InstrumentService",
                              QString("load_from_db_async: query failed for %1: %2")
                                  .arg(broker_id, q.lastError().text()));
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(conn_name);

        if (!self)
            return;

        const int count = all.size();
        QVector<Instrument> instruments = std::move(all);

        QMetaObject::invokeMethod(
            self,
            [self, broker_id, instruments = std::move(instruments), count, callback]() mutable {
                if (!self)
                    return;
                if (!instruments.isEmpty()) {
                    self->build_cache(broker_id, instruments);
                    LOG_INFO("InstrumentService",
                             QString("load_from_db_async: loaded %1 instruments for %2").arg(count).arg(broker_id));
                } else {
                    LOG_WARN("InstrumentService",
                             QString("load_from_db_async: no instruments found in DB for %1 — run refresh")
                                 .arg(broker_id));
                }
                if (callback)
                    callback(count);
            },
            Qt::QueuedConnection);
    });
}

void InstrumentService::load_from_db_worker(const QString& broker_id) {
    // Synchronous DB load safe to call from a QtConcurrent worker thread. Opens a
    // private named connection (never touches the shared main-thread connection)
    // then build_cache (mutex-guarded). No-op if already loaded.
    if (is_loaded(broker_id))
        return;
    const QString db_path = fincept::Database::instance().path();
    const QString conn_name =
        "inst_sync_" + broker_id + "_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVector<Instrument> all;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn_name);
        db.setDatabaseName(db_path);
        if (db.open()) {
            QSqlQuery q(db);
            q.prepare("SELECT instrument_token, exchange_token, symbol, brsymbol, name, "
                      "exchange, brexchange, expiry, strike, lot_size, instrument_type, "
                      "tick_size, broker_id FROM instruments WHERE broker_id = ?");
            q.addBindValue(broker_id);
            if (q.exec()) {
                while (q.next())
                    all.append(InstrumentRepository::map_row_static(q));
            } else {
                LOG_ERROR("InstrumentService", QString("load_from_db_worker: query failed for %1: %2")
                                                   .arg(broker_id, q.lastError().text()));
            }
            db.close();
        } else {
            LOG_ERROR("InstrumentService", QString("load_from_db_worker: failed to open DB for %1: %2")
                                               .arg(broker_id, db.lastError().text()));
        }
    }
    QSqlDatabase::removeDatabase(conn_name);
    if (!all.isEmpty())
        build_cache(broker_id, all);
}

// ── Lookups ───────────────────────────────────────────────────────────────────

std::optional<qint64> InstrumentService::instrument_token(const QString& symbol, const QString& exchange,
                                                          const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return std::nullopt;
    // Cache keys are stored upper-case; normalise so callers passing e.g.
    // "reliance"/"nse" still match.
    auto jt = it->by_symbol.find({symbol.toUpper(), exchange.toUpper()});
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
    // Cache keys are stored upper-case; normalise so callers passing e.g.
    // "reliance"/"nse" still match.
    auto jt = it->by_symbol.find({symbol.toUpper(), exchange.toUpper()});
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
    auto jt = it->by_brsymbol.find({brsymbol.toUpper(), brexchange.toUpper()});
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
    // Cache keys are stored upper-case; normalise so callers passing e.g.
    // "reliance"/"nse" still match.
    auto jt = it->by_symbol.find({symbol.toUpper(), exchange.toUpper()});
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
    return search_all(query, exchange, QStringList{broker_id}, limit);
}

QVector<Instrument> InstrumentService::search_all(const QString& query, const QString& exchange,
                                                  const QStringList& broker_ids, int limit) const {
    const QString q = query.trimmed();
    if (q.isEmpty() || limit <= 0)
        return {};
    const QString exch = exchange.trimmed();

    // Prefer the in-memory cache — it is the hot path, fast (no per-keystroke
    // SQL), and stays correct even when the on-disk catalog is empty (e.g. a
    // failed/partial persist). Fall back to the SQLite repository only for
    // brokers whose cache isn't loaded in this session.
    auto type_rank = [](InstrumentType t) {
        switch (t) {
            case InstrumentType::EQ: return 0;
            case InstrumentType::INDEX: return 1;
            case InstrumentType::FUT: return 2;
            default: return 3;
        }
    };

    QVector<Instrument> out;
    QStringList db_fallback;
    {
        QMutexLocker lock(&mutex_);
        // Broker order: explicit list (first = highest priority) else all cached.
        QStringList order = broker_ids;
        order.removeAll(QString());
        if (order.isEmpty())
            for (auto it = caches_.cbegin(); it != caches_.cend(); ++it)
                order << it.key();

        for (const QString& bid : order) {
            auto cit = caches_.find(bid);
            if (cit == caches_.end() || !cit->loaded) {
                db_fallback << bid; // not in memory this session — try the DB below
                continue;
            }
            QVector<Instrument> hits;
            for (const auto& inst : cit->by_token) {
                if (!exch.isEmpty() && inst.exchange.compare(exch, Qt::CaseInsensitive) != 0)
                    continue;
                if (inst.symbol.contains(q, Qt::CaseInsensitive) ||
                    inst.brsymbol.contains(q, Qt::CaseInsensitive) ||
                    inst.name.contains(q, Qt::CaseInsensitive))
                    hits.append(inst);
            }
            // Rank: symbol-prefix matches first, then EQ→INDEX→FUT→other, then A-Z.
            std::sort(hits.begin(), hits.end(), [&](const Instrument& a, const Instrument& b) {
                const bool ap = a.symbol.startsWith(q, Qt::CaseInsensitive);
                const bool bp = b.symbol.startsWith(q, Qt::CaseInsensitive);
                if (ap != bp) return ap;
                const int ar = type_rank(a.instrument_type), br = type_rank(b.instrument_type);
                if (ar != br) return ar < br;
                return a.symbol < b.symbol;
            });
            out += hits;
            if (out.size() >= limit)
                break;
        }
    }

    if (out.size() < limit && !db_fallback.isEmpty())
        out += InstrumentRepository::instance().search_all(q, exchange, db_fallback, limit - out.size());

    if (out.size() > limit)
        out.resize(limit);
    return out;
}

// ── F&O / Options chain helpers ─────────────────────────────────────────────

QStringList InstrumentService::list_underlyings(const QString& broker_id, const QString& exchange) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return {};
    QSet<QString> seen;
    for (const auto& inst : it->by_token) {
        if (inst.exchange != exchange)
            continue;
        if (inst.instrument_type != InstrumentType::CE && inst.instrument_type != InstrumentType::PE
            && inst.instrument_type != InstrumentType::FUT)
            continue;
        if (!inst.name.isEmpty())
            seen.insert(inst.name);
    }
    QStringList out(seen.begin(), seen.end());
    std::sort(out.begin(), out.end());
    return out;
}

QStringList InstrumentService::list_expiries(const QString& broker_id, const QString& underlying,
                                             const QString& exchange) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return {};
    // Use a map keyed by parsed date so we can sort chronologically. Display
    // string ("DD-MMM-YY") sorts lexically wrong (e.g. "01-DEC-25" lex-before
    // "01-NOV-25"). The map's value collapses duplicates automatically.
    QMap<QDate, QString> by_date;
    int match_count = 0, empty_expiry = 0;
    for (const auto& inst : it->by_token) {
        if (inst.exchange != exchange || inst.name != underlying)
            continue;
        if (inst.instrument_type != InstrumentType::CE && inst.instrument_type != InstrumentType::PE
            && inst.instrument_type != InstrumentType::FUT)
            continue;
        ++match_count;
        if (inst.expiry.isEmpty()) {
            ++empty_expiry;
            continue;
        }
        // Parse "DD-MMM-YY" (case-insensitive month) or "YYYY-MM-DD".
        QDate d;
        if (inst.expiry.length() >= 9 && inst.expiry[2] == QLatin1Char('-') && inst.expiry[6] == QLatin1Char('-')) {
            const int day = QStringView(inst.expiry).left(2).toInt();
            const QStringView mon = QStringView(inst.expiry).mid(3, 3);
            const int yr = 2000 + QStringView(inst.expiry).mid(7, 2).toInt();
            int month = 0;
            if      (mon.compare(QLatin1String("JAN"), Qt::CaseInsensitive) == 0) month = 1;
            else if (mon.compare(QLatin1String("FEB"), Qt::CaseInsensitive) == 0) month = 2;
            else if (mon.compare(QLatin1String("MAR"), Qt::CaseInsensitive) == 0) month = 3;
            else if (mon.compare(QLatin1String("APR"), Qt::CaseInsensitive) == 0) month = 4;
            else if (mon.compare(QLatin1String("MAY"), Qt::CaseInsensitive) == 0) month = 5;
            else if (mon.compare(QLatin1String("JUN"), Qt::CaseInsensitive) == 0) month = 6;
            else if (mon.compare(QLatin1String("JUL"), Qt::CaseInsensitive) == 0) month = 7;
            else if (mon.compare(QLatin1String("AUG"), Qt::CaseInsensitive) == 0) month = 8;
            else if (mon.compare(QLatin1String("SEP"), Qt::CaseInsensitive) == 0) month = 9;
            else if (mon.compare(QLatin1String("OCT"), Qt::CaseInsensitive) == 0) month = 10;
            else if (mon.compare(QLatin1String("NOV"), Qt::CaseInsensitive) == 0) month = 11;
            else if (mon.compare(QLatin1String("DEC"), Qt::CaseInsensitive) == 0) month = 12;
            if (month > 0 && day > 0 && day <= 31)
                d = QDate(yr, month, day);
        }
        if (!d.isValid())
            d = QDate::fromString(inst.expiry, "yyyy-MM-dd");
        if (d.isValid())
            by_date.insert(d, inst.expiry);
    }
    LOG_INFO("InstrumentService",
             QString("list_expiries('%1','%2','%3'): matched=%4 empty_expiry=%5 parsed=%6")
                 .arg(broker_id, underlying, exchange).arg(match_count).arg(empty_expiry).arg(by_date.size()));
    const auto vals = by_date.values();
    return QStringList(vals.begin(), vals.end());
}

QVector<Instrument> InstrumentService::find_options_for_underlying(const QString& broker_id,
                                                                   const QString& underlying,
                                                                   const QString& expiry,
                                                                   const QString& exchange) const {
    QMutexLocker lock(&mutex_);
    auto it = caches_.find(broker_id);
    if (it == caches_.end())
        return {};
    QVector<Instrument> options;  // CE/PE
    QVector<Instrument> futures;  // FUT (appended last)
    for (const auto& inst : it->by_token) {
        if (inst.exchange != exchange || inst.name != underlying || inst.expiry != expiry)
            continue;
        if (inst.instrument_type == InstrumentType::CE || inst.instrument_type == InstrumentType::PE)
            options.append(inst);
        else if (inst.instrument_type == InstrumentType::FUT)
            futures.append(inst);
    }
    std::sort(options.begin(), options.end(), [](const Instrument& a, const Instrument& b) {
        if (a.strike != b.strike)
            return a.strike < b.strike;
        // Same strike: CE before PE for stable iteration
        return int(a.instrument_type) < int(b.instrument_type);
    });
    options.append(futures);
    return options;
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

// When two instruments share a numeric token, by_token can only keep one. Dhan
// reuses securityIds across exchange segments (e.g. 2885 is RELIANCE on NSE_EQ and
// an expired EURINR option on CDS), so prefer the "primary" tradable — cash/index
// over derivatives — to keep token→symbol/segment resolution pointing at the
// equity the feed actually wants instead of a colliding (often expired) option.
// Lower rank = preferred. Globally-unique-token brokers never hit a collision.
static int inst_token_collision_rank(const Instrument& i) {
    switch (i.instrument_type) {
    case InstrumentType::EQ:    return 0;
    case InstrumentType::INDEX: return 1;
    case InstrumentType::FUT:   return 2;
    case InstrumentType::CE:
    case InstrumentType::PE:    return 3;
    default:                    return 4;
    }
}

void InstrumentService::build_cache(const QString& broker_id, const QVector<Instrument>& instruments) {
    QMutexLocker lock(&mutex_);
    Cache& cache = caches_[broker_id];
    cache.by_symbol.clear();
    cache.by_token.clear();
    cache.by_brsymbol.clear();

    for (const auto& inst : instruments) {
        cache.by_symbol.insert({inst.symbol, inst.exchange}, inst);
        // by_token is keyed on the bare numeric token; survive cross-segment token
        // collisions by keeping the higher-priority instrument (see ranker above).
        auto existing = cache.by_token.find(inst.instrument_token);
        if (existing == cache.by_token.end() ||
            inst_token_collision_rank(inst) < inst_token_collision_rank(existing.value()))
            cache.by_token.insert(inst.instrument_token, inst);
        cache.by_brsymbol.insert({inst.brsymbol, inst.brexchange}, inst);
    }
    cache.loaded = true;
}

void InstrumentService::do_refresh(const QString& broker_id, const BrokerCredentials& creds) {
    LOG_INFO("InstrumentService", "Downloading instruments for " + broker_id);

    const InstrumentSource* src = SymbolResolver::instance().find(broker_id);
    if (!src || !src->download || !src->parse) {
        QMetaObject::invokeMethod(
            this,
            [this, broker_id]() {
                {
                    QMutexLocker lock(&mutex_);
                    refreshing_.remove(broker_id);
                }
                emit refresh_failed(broker_id, "Instrument source not registered for " + broker_id);
            },
            Qt::QueuedConnection);
        return;
    }

    const QByteArray payload = src->download(creds);
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

    QVector<Instrument> instruments = src->parse(payload);

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

    // Heap-allocate QNAM with deleteLater + DeferredDelete drain. A stack
    // QNAM here would leak Qt's macOS CFSocket-notifier teardown onto the
    // main runloop, causing a use-after-free crash hours later.
    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req{QUrl(kUrl)};
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam->get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(60000); // 60s — large file on slow connection
    loop.exec();

    auto drain = [&]() {
        reply->deleteLater();
        nam->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };

    if (!timer.isActive()) {
        reply->abort();
        LOG_ERROR("InstrumentService", "AngelOne master contract download timed out");
        drain();
        return {};
    }
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_ERROR("InstrumentService", "Failed to download AngelOne master contract: " + reply->errorString());
        drain();
        return {};
    }

    QByteArray data = reply->readAll();
    drain();
    LOG_INFO("InstrumentService", QString("Downloaded AngelOne master contract: %1 bytes").arg(data.size()));
    return data;
}

QByteArray InstrumentService::download_groww_csv() {
    // Public, unauthenticated CSV (~27MB) with the full equity + F&O + commodity master.
    // Own QNAM (not BrokerHttp) because the download dominates the shared client's
    // mutex for 10-30s on slow links and would starve concurrent quote/order calls.
    static const QString kUrl = "https://growwapi-assets.groww.in/instruments/instrument.csv";

    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req{QUrl(kUrl)};
    req.setRawHeader("Accept", "text/csv");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam->get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(60000); // 60s — ~27MB file on slow connection
    loop.exec();

    auto drain = [&]() {
        reply->deleteLater();
        nam->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };

    if (!timer.isActive()) {
        reply->abort();
        LOG_ERROR("InstrumentService", "Groww instrument CSV download timed out");
        drain();
        return {};
    }
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_ERROR("InstrumentService", "Failed to download Groww instrument CSV: " + reply->errorString());
        drain();
        return {};
    }

    QByteArray data = reply->readAll();
    drain();
    LOG_INFO("InstrumentService", QString("Downloaded Groww instrument CSV: %1 bytes").arg(data.size()));
    return data;
}

QByteArray InstrumentService::download_dhan_csv() {
    // Public, unauthenticated scrip master (~27MB, 16-column CSV, no auth needed).
    // Own QNAM (not BrokerHttp) so the long download doesn't starve concurrent
    // quote/order calls — same rationale as download_groww_csv.
    static const QString kUrl = "https://images.dhan.co/api-data/api-scrip-master.csv";

    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req{QUrl(kUrl)};
    req.setRawHeader("Accept", "text/csv");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam->get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(60000); // 60s — ~27MB file on slow connection
    loop.exec();

    auto drain = [&]() {
        reply->deleteLater();
        nam->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };

    if (!timer.isActive()) {
        reply->abort();
        LOG_ERROR("InstrumentService", "Dhan scrip master download timed out");
        drain();
        return {};
    }
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_ERROR("InstrumentService", "Failed to download Dhan scrip master: " + reply->errorString());
        drain();
        return {};
    }

    QByteArray data = reply->readAll();
    drain();
    LOG_INFO("InstrumentService", QString("Downloaded Dhan scrip master: %1 bytes").arg(data.size()));
    return data;
}

QByteArray InstrumentService::download_icici_csv() {
    // Public, unauthenticated security master (~4MB plain CSV, 13 columns: equity
    // + F&O + commodity across NSE/BSE/NFO/BFO/MCX). Deliberately the unzipped
    // StockScriptNew.csv rather than SecurityMaster.zip so we need no ZIP decoder
    // (Qt's QZipReader is a private-header dependency, gated on Qt6::GuiPrivate).
    // Own QNAM (not BrokerHttp) for the same reason as download_dhan_csv: a long
    // download must not starve concurrent quote/order calls.
    static const QString kUrl =
        "https://traderweb.icicidirect.com/Content/File/txtFile/ScripFile/StockScriptNew.csv";

    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req{QUrl(kUrl)};
    req.setRawHeader("Accept", "text/csv");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam->get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(60000); // 60s — ~4MB file on slow connection
    loop.exec();

    auto drain = [&]() {
        reply->deleteLater();
        nam->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };

    if (!timer.isActive()) {
        reply->abort();
        LOG_ERROR("InstrumentService", "ICICI security master download timed out");
        drain();
        return {};
    }
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        LOG_ERROR("InstrumentService", "Failed to download ICICI security master: " + reply->errorString());
        drain();
        return {};
    }

    QByteArray data = reply->readAll();
    drain();
    LOG_INFO("InstrumentService", QString("Downloaded ICICI security master: %1 bytes").arg(data.size()));
    return data;
}

QByteArray InstrumentService::download_fyers_json() {
    static const QStringList urls = {
        "https://public.fyers.in/sym_details/NSE_CM_sym_master.json",
        "https://public.fyers.in/sym_details/NSE_FO_sym_master.json",
        "https://public.fyers.in/sym_details/BSE_CM_sym_master.json",
        "https://public.fyers.in/sym_details/MCX_COM_sym_master.json",
    };

    QJsonObject merged;
    auto* nam = new QNetworkAccessManager;

    for (const auto& url : urls) {
        QNetworkRequest req{QUrl(url)};
        req.setRawHeader("Accept", "application/json");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply* reply = nam->get(req);
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(90000);
        loop.exec();

        if (!timer.isActive()) {
            reply->abort();
            LOG_WARN("InstrumentService", QString("Fyers download timed out: %1").arg(url));
            reply->deleteLater();
            continue;
        }
        timer.stop();

        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("InstrumentService", QString("Fyers download failed: %1 — %2").arg(url, reply->errorString()));
            reply->deleteLater();
            continue;
        }

        QByteArray data = reply->readAll();
        reply->deleteLater();

        auto doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            LOG_WARN("InstrumentService", QString("Fyers JSON parse failed for %1").arg(url));
            continue;
        }

        QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            merged.insert(it.key(), it.value());

        LOG_INFO("InstrumentService",
                 QString("Fyers: fetched %1 symbols from %2").arg(obj.size()).arg(url));
    }

    nam->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    if (merged.isEmpty()) {
        LOG_ERROR("InstrumentService", "Fyers: no instruments downloaded from any endpoint");
        return {};
    }

    QByteArray result = QJsonDocument(merged).toJson(QJsonDocument::Compact);
    LOG_INFO("InstrumentService",
             QString("Fyers: merged %1 total instruments (%2 bytes)").arg(merged.size()).arg(result.size()));
    return result;
}

} // namespace fincept::trading
