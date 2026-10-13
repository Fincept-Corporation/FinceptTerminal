#include "services/screener/ScreenerService.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/repositories/SettingsRepository.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace fincept::services {

namespace screener_service_detail {

constexpr const char* kScreenerServiceTag = "ScreenerService";
const QString kScreenerScript = QStringLiteral("screener_data.py");
const QString kSavedScreensKey = QStringLiteral("screener.saved_screens");
const QString kSavedScreensCategory = QStringLiteral("screener");
constexpr int kScreenerRunTimeoutMs = 60'000;     // two Yahoo calls (20 s budget each) + interpreter start
constexpr int kScreenerCatalogTimeoutMs = 30'000; // static data, no network
constexpr int kScreenerMaxCachedResults = 24;
constexpr int kScreenerMaxRows = 250; // Yahoo's per-request cap

double json_number(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : ScreenerRow::kMissing;
}

ScreenerColumn parse_column(const QJsonObject& o) {
    ScreenerColumn c;
    c.id = o.value("id").toString();
    c.field = o.value("field").toString(c.id);
    c.label = o.value("label").toString(c.id);
    c.unit = o.value("unit").toString();
    c.description = o.value("description").toString();
    return c;
}

ScreenerRow parse_row(const QJsonObject& o) {
    ScreenerRow r;
    r.symbol = o.value("symbol").toString();
    r.name = o.value("name").toString();
    r.exchange = o.value("exchange").toString();
    r.exchange_name = o.value("exchange_name").toString(r.exchange);
    r.currency = o.value("currency").toString();
    r.market_cap_currency = o.value("market_cap_currency").toString(r.currency);
    r.financial_currency = o.value("financial_currency").toString();
    r.sector = o.value("sector").toString();
    r.industry = o.value("industry").toString();
    r.price = json_number(o.value("price"));
    r.change = json_number(o.value("change"));
    r.change_pct = json_number(o.value("change_pct"));
    r.volume = json_number(o.value("volume"));
    r.avg_volume_3m = json_number(o.value("avg_volume_3m"));
    r.market_cap = json_number(o.value("market_cap"));
    r.pe = json_number(o.value("pe"));
    r.forward_pe = json_number(o.value("forward_pe"));
    r.beta = json_number(o.value("beta"));
    r.market_time = static_cast<qint64>(o.value("market_time").toDouble(0));
    const QJsonObject fields = o.value("fields").toObject();
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
        r.fields.insert(it.key(), json_number(it.value()));
    return r;
}

/// Script payload → result. Returns false (with *error) when the document is not a screen result.
bool parse_result(const QString& output, const ScreenerQuery& q, const QString& key, ScreenerResult* out,
                  QString* error) {
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QStringLiteral("Screener returned malformed JSON: %1").arg(pe.errorString());
        return false;
    }
    const QJsonObject o = doc.object();
    if (!o.value("rows").isArray()) {
        *error = QStringLiteral("Screener response has no rows array");
        return false;
    }
    ScreenerResult r;
    r.key = key;
    r.query = q;
    r.title = o.value("title").toString();
    r.description = o.value("description").toString();
    r.criteria_text = o.value("criteria_text").toString();
    r.criteria_note = o.value("criteria_note").toString();
    r.source = o.value("source").toString(QStringLiteral("Yahoo Finance screener"));
    r.total = o.value("total").toInt();
    r.offset = o.value("offset").toInt(q.offset);
    r.sort_field = o.value("sort_field").toString();
    r.sort_asc = o.value("sort_asc").toBool();
    r.as_of = static_cast<qint64>(o.value("as_of").toDouble(0));
    r.max_delay_min = o.value("max_delay_min").isDouble() ? o.value("max_delay_min").toInt() : -1;
    r.fetched_at = QDateTime::currentDateTimeUtc();
    for (const auto& c : o.value("columns").toArray())
        r.columns.append(parse_column(c.toObject()));
    const QJsonArray rows = o.value("rows").toArray();
    r.rows.reserve(rows.size());
    for (const auto& v : rows) {
        ScreenerRow row = parse_row(v.toObject());
        if (!row.symbol.isEmpty())
            r.rows.append(std::move(row));
    }
    for (const auto& w : o.value("warnings").toArray())
        r.warnings.append(w.toString());
    *out = std::move(r);
    return true;
}

} // namespace screener_service_detail

namespace screener_svc_d = screener_service_detail;

// ── ScreenerQuery ─────────────────────────────────────────────────────────────

QString ScreenerQuery::cache_key() const {
    if (is_predefined())
        return QStringLiteral("p|%1|%2|%3").arg(predefined_id).arg(count).arg(offset);
    // QJsonObject keeps its keys sorted, so equal criteria serialise identically.
    return QStringLiteral("c|%1|%2|%3|%4|%5")
        .arg(QString::fromUtf8(QJsonDocument(criteria).toJson(QJsonDocument::Compact)), sort_field,
             sort_asc ? QStringLiteral("asc") : QStringLiteral("desc"))
        .arg(count)
        .arg(offset);
}

QJsonObject ScreenerQuery::to_json() const {
    QJsonObject o;
    o["count"] = count;
    if (is_predefined()) {
        o["predefined"] = predefined_id;
        return o;
    }
    o["criteria"] = criteria;
    o["sort_field"] = sort_field;
    o["sort_asc"] = sort_asc;
    return o;
}

ScreenerQuery ScreenerQuery::from_json(const QJsonObject& o) {
    ScreenerQuery q;
    q.predefined_id = o.value("predefined").toString();
    q.criteria = o.value("criteria").toObject();
    q.sort_field = o.value("sort_field").toString();
    q.sort_asc = o.value("sort_asc").toBool(false);
    q.count = qBound(1, o.value("count").toInt(100), screener_svc_d::kScreenerMaxRows);
    return q;
}

// ── Runs ──────────────────────────────────────────────────────────────────────

ScreenerService& ScreenerService::instance() {
    static ScreenerService s;
    return s;
}

const ScreenerResult* ScreenerService::last_result(const QString& key) const {
    const auto it = cache_.constFind(key);
    return it == cache_.constEnd() ? nullptr : &it.value();
}

void ScreenerService::store(const ScreenerResult& r) {
    cache_.insert(r.key, r);
    cache_order_.removeAll(r.key);
    cache_order_.append(r.key);
    while (cache_order_.size() > screener_svc_d::kScreenerMaxCachedResults)
        cache_.remove(cache_order_.takeFirst());
}

QString ScreenerService::run(const ScreenerQuery& q_in, bool force) {
    ScreenerQuery q = q_in;
    q.count = qBound(1, q.count, screener_svc_d::kScreenerMaxRows);
    q.offset = qMax(0, q.offset);
    const QString key = q.cache_key();

    const auto cached = cache_.constFind(key);
    if (!force && cached != cache_.constEnd() &&
        cached->fetched_at.msecsTo(QDateTime::currentDateTimeUtc()) < kCacheTtlMs) {
        // Queued even for a cache hit: the caller sets up its "running" state after run() returns.
        QTimer::singleShot(0, this, [this, key]() {
            const auto it = cache_.constFind(key);
            if (it != cache_.constEnd())
                emit results_ready(it.value());
        });
        return key;
    }
    if (inflight_.contains(key))
        return key; // the running process answers every listener

    inflight_.insert(key);
    QStringList args;
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = screener_svc_d::kScreenerRunTimeoutMs;
    if (q.is_predefined()) {
        args = {QStringLiteral("predefined"), q.predefined_id, QString::number(q.count), QString::number(q.offset)};
    } else {
        // Criteria travel on stdin — no Windows argv quoting of JSON, no command-line length cap.
        QJsonObject doc = q.criteria;
        doc["count"] = q.count;
        doc["offset"] = q.offset;
        doc["sort_asc"] = q.sort_asc;
        if (!q.sort_field.isEmpty())
            doc["sort_field"] = q.sort_field;
        args = {QStringLiteral("custom"), QStringLiteral("-")};
        opts.stdin_data = QJsonDocument(doc).toJson(QJsonDocument::Compact);
    }

    LOG_DEBUG(screener_svc_d::kScreenerServiceTag, QStringLiteral("Running screen %1").arg(key.left(200)));
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    QPointer<ScreenerService> self = this;
    python::PythonRunner::instance().run_with_options(
        screener_svc_d::kScreenerScript, args, opts, [self, key, q, timer](const python::PythonResult& res) {
            if (!self)
                return;
            self->inflight_.remove(key);
            if (!res.success) {
                const QString err = res.error.trimmed().isEmpty()
                                        ? QStringLiteral("The screener script failed (exit %1)").arg(res.exit_code)
                                        : res.error.trimmed();
                LOG_WARN(screener_svc_d::kScreenerServiceTag, QStringLiteral("Screen failed: %1").arg(err.left(300)));
                emit self->run_failed(key, err);
                return;
            }
            ScreenerResult result;
            QString parse_error;
            if (!screener_svc_d::parse_result(res.output, q, key, &result, &parse_error)) {
                LOG_ERROR(screener_svc_d::kScreenerServiceTag, parse_error);
                emit self->run_failed(key, parse_error);
                return;
            }
            LOG_INFO(screener_svc_d::kScreenerServiceTag, QStringLiteral("Screen '%1': %2 rows of %3 matches in %4 ms")
                                                   .arg(result.title)
                                                   .arg(result.rows.size())
                                                   .arg(result.total)
                                                   .arg(timer->elapsed()));
            self->store(result);
            emit self->results_ready(result);
        });
    return key;
}

// ── Catalogue ─────────────────────────────────────────────────────────────────

void ScreenerService::load_catalog() {
    if (has_catalog() || catalog_loading_)
        return;
    catalog_loading_ = true;
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = screener_svc_d::kScreenerCatalogTimeoutMs;
    QPointer<ScreenerService> self = this;
    python::PythonRunner::instance().run_with_options(
        screener_svc_d::kScreenerScript, {QStringLiteral("fields")}, opts, [self](const python::PythonResult& res) {
            if (!self)
                return;
            self->catalog_loading_ = false;
            if (!res.success) {
                const QString err = res.error.trimmed().isEmpty() ? QStringLiteral("The screener script failed")
                                                                  : res.error.trimmed();
                LOG_WARN(screener_svc_d::kScreenerServiceTag, QStringLiteral("Catalogue load failed: %1").arg(err.left(300)));
                emit self->catalog_failed(err);
                return;
            }
            const QJsonDocument doc = QJsonDocument::fromJson(res.output.toUtf8());
            const QJsonObject o = doc.object();
            if (!o.value("fields").isArray() || o.value("fields").toArray().isEmpty()) {
                LOG_ERROR(screener_svc_d::kScreenerServiceTag, "Catalogue payload has no fields");
                emit self->catalog_failed(QStringLiteral("The screener catalogue is malformed"));
                return;
            }
            self->catalog_ = o;
            emit self->catalog_ready();
        });
}

// ── Saved screens ─────────────────────────────────────────────────────────────

Result<QVector<SavedScreen>> ScreenerService::parse_saved(const QString& json) {
    QVector<SavedScreen> out;
    if (json.trimmed().isEmpty())
        return Result<QVector<SavedScreen>>::ok(out);
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray())
        return Result<QVector<SavedScreen>>::err("saved screens are not a JSON array");
    for (const auto& v : doc.array()) {
        const QJsonObject o = v.toObject();
        SavedScreen s;
        s.name = o.value("name").toString().trimmed();
        s.query = ScreenerQuery::from_json(o.value("query").toObject());
        s.saved_at = o.value("saved_at").toString();
        if (!s.name.isEmpty())
            out.append(s);
    }
    return Result<QVector<SavedScreen>>::ok(out);
}

Result<QVector<SavedScreen>> ScreenerService::saved_screens() const {
    const auto r = SettingsRepository::instance().get(screener_svc_d::kSavedScreensKey);
    if (r.is_err())
        return Result<QVector<SavedScreen>>::err(r.error());
    return parse_saved(r.value());
}

Result<void> ScreenerService::write_saved(const QVector<SavedScreen>& screens) {
    QJsonArray arr;
    for (const auto& s : screens) {
        QJsonObject o;
        o["name"] = s.name;
        o["query"] = s.query.to_json();
        o["saved_at"] = s.saved_at;
        arr.append(o);
    }
    const auto r = SettingsRepository::instance().set(
        screener_svc_d::kSavedScreensKey, QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)),
        screener_svc_d::kSavedScreensCategory);
    if (r.is_ok())
        emit saved_screens_changed();
    return r;
}

Result<void> ScreenerService::save_screen(const QString& name, const ScreenerQuery& q) {
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        return Result<void>::err("a saved screen needs a name");
    if (q.is_predefined())
        return Result<void>::err("predefined screens cannot be saved");
    auto existing = saved_screens();
    if (existing.is_err()) // never overwrite what we could not read
        return Result<void>::err("could not read saved screens: " + existing.error());
    QVector<SavedScreen> screens = existing.value();
    SavedScreen s{clean, q, QDateTime::currentDateTimeUtc().toString(Qt::ISODate)};
    s.query.offset = 0;
    bool replaced = false;
    for (auto& cur : screens) {
        if (cur.name.compare(clean, Qt::CaseInsensitive) == 0) {
            cur = s;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        screens.append(s);
    return write_saved(screens);
}

Result<void> ScreenerService::delete_screen(const QString& name) {
    auto existing = saved_screens();
    if (existing.is_err())
        return Result<void>::err("could not read saved screens: " + existing.error());
    QVector<SavedScreen> screens = existing.value();
    const auto before = screens.size();
    screens.erase(std::remove_if(screens.begin(), screens.end(),
                                 [&name](const SavedScreen& s) {
                                     return s.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0;
                                 }),
                  screens.end());
    if (screens.size() == before)
        return Result<void>::ok();
    return write_saved(screens);
}

// ── Watchlists ────────────────────────────────────────────────────────────────

Result<QVector<Watchlist>> ScreenerService::watchlists() const {
    return WatchlistRepository::instance().list_all();
}

Result<Watchlist> ScreenerService::create_watchlist(const QString& name) {
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        return Result<Watchlist>::err("a watchlist needs a name");
    auto r = WatchlistRepository::instance().create(clean);
    if (r.is_ok())
        EventBus::instance().publish("watchlist.created", {{"id", r.value().id}, {"name", r.value().name}});
    return r;
}

Result<ScreenerService::WatchlistAddReport>
ScreenerService::add_to_watchlist(const QString& watchlist_id, const QVector<QPair<QString, QString>>& symbols_and_names) {
    auto& repo = WatchlistRepository::instance();
    // add_stock() is INSERT OR IGNORE: an unknown id would "succeed" into nowhere.
    if (repo.get(watchlist_id).is_err())
        return Result<WatchlistAddReport>::err("watchlist not found");
    const auto existing = repo.get_stocks(watchlist_id);
    if (existing.is_err())
        return Result<WatchlistAddReport>::err(existing.error());

    QSet<QString> have;
    for (const auto& s : existing.value())
        have.insert(s.symbol.toUpper());

    WatchlistAddReport report;
    QStringList added;
    for (const auto& [symbol, name] : symbols_and_names) {
        const QString sym = symbol.trimmed().toUpper();
        if (sym.isEmpty())
            continue;
        if (have.contains(sym)) {
            ++report.already_present;
            continue;
        }
        if (repo.add_stock(watchlist_id, sym, name).is_ok()) {
            have.insert(sym);
            added.append(sym);
            ++report.added;
        } else {
            ++report.failed;
        }
    }
    if (!added.isEmpty()) {
        LOG_INFO(screener_svc_d::kScreenerServiceTag,
                 QStringLiteral("Added %1 symbol(s) to watchlist %2").arg(added.size()).arg(watchlist_id));
        // One event for the batch; subscribers reload from the DB (the payload is informational).
        EventBus::instance().publish("watchlist.updated", {{"watchlist_id", watchlist_id},
                                                           {"action", QStringLiteral("add")},
                                                           {"symbol", added.first()},
                                                           {"symbols", added}});
    }
    return Result<WatchlistAddReport>::ok(report);
}

} // namespace fincept::services
