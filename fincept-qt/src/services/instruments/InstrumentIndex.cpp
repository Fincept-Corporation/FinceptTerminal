// InstrumentIndex.cpp — see InstrumentIndex.h. File-scope names carry an iix_ /
// kIix prefix (unity builds).

#include "services/instruments/InstrumentIndex.h"

#include "core/instrument/VenueSymbols.h"
#include "core/logging/Logger.h"
#include "services/instruments/SymbolSeed.h"
#include "storage/repositories/SymbolMasterRepository.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/SqliteSupport.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

#include <utility>

namespace fincept::services {

namespace {

constexpr const char* kIixTag = "InstrumentIndex";
constexpr int kIixStartRetryMs = 2000;
constexpr int kIixBrokerCheckMs = 30 * 60 * 1000;
constexpr int kIixLearnDelayMs = 1500;
constexpr int kIixBrokerPage = 2000;
constexpr int kIixMaxIdsInSignal = 200;
constexpr int kIixMaxLearnedSeen = 5000;
constexpr int kIixPoolExpiryMs = 120000;

using IixRepo = fincept::SymbolMasterRepository;

QString iix_label() {
    return QStringLiteral("The symbol index");
}

/// Broker rows of the EQ series (or without a series) win over BE / BZ / SM ... rows that
/// map to the same listing.
bool iix_is_eq_series(const IixRepo::BrokerRow& row) {
    const QString br = row.brsymbol.trimmed().toUpper();
    return br.isEmpty() || br == row.symbol.trimmed().toUpper() || br.endsWith(QLatin1String("-EQ")) ||
           !br.contains(QLatin1Char('-'));
}

} // namespace

InstrumentIndex& InstrumentIndex::instance() {
    static InstrumentIndex s;
    return s;
}

InstrumentIndex::InstrumentIndex() {
    reader_pool_.setMaxThreadCount(2);
    reader_pool_.setExpiryTimeout(kIixPoolExpiryMs);
    writer_pool_.setMaxThreadCount(1);
    writer_pool_.setExpiryTimeout(kIixPoolExpiryMs);

    retry_timer_ = new QTimer(this);
    retry_timer_->setSingleShot(true);
    retry_timer_->setInterval(kIixStartRetryMs);
    connect(retry_timer_, &QTimer::timeout, this, &InstrumentIndex::start);

    broker_timer_ = new QTimer(this);
    broker_timer_->setInterval(kIixBrokerCheckMs);
    connect(broker_timer_, &QTimer::timeout, this, &InstrumentIndex::import_all_broker_instruments);

    learn_timer_ = new QTimer(this);
    learn_timer_->setSingleShot(true);
    learn_timer_->setInterval(kIixLearnDelayMs);
    connect(learn_timer_, &QTimer::timeout, this, &InstrumentIndex::flush_learned);

    // Timers and queued deliveries need the main thread's event loop.
    if (QCoreApplication* app = QCoreApplication::instance(); app && thread() != app->thread())
        moveToThread(app->thread());
}

InstrumentIndex::~InstrumentIndex() {
    quiesce();
}

void InstrumentIndex::quiesce() {
    shutting_down_.store(true, std::memory_order_release);
    reader_pool_.clear();
    writer_pool_.clear();
    reader_pool_.waitForDone();
    writer_pool_.waitForDone();
}

// ── Plumbing ────────────────────────────────────────────────────────────────

void InstrumentIndex::start() {
    if (shutting_down_.load(std::memory_order_acquire))
        return;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { start(); }, Qt::QueuedConnection);
        return;
    }
    auto& database = Database::instance();
    if (!database.is_open()) {
        if (!retry_timer_->isActive())
            retry_timer_->start();
        return;
    }
    const QString path = database.path();
    {
        QMutexLocker lock(&start_mutex_);
        if (path == started_path_)
            return;
        started_path_ = path;
    }
    IixRepo::set_forbidden_thread(thread());
    // The workers are joined when the application object goes away - before static
    // destruction, where this singleton's own destructor would be too late (a short run,
    // e.g. a self-test, returned from main() with the seed task still on its thread).
    static bool post_routine_added = false;
    if (!post_routine_added) {
        post_routine_added = true;
        qAddPostRoutine([]() { InstrumentIndex::instance().quiesce(); });
    }
    run_write([this](QSqlDatabase& db) { seed_task(db); });
    run_write([this](QSqlDatabase& db) { broker_task(db, QString(), false); });
    if (!broker_timer_->isActive())
        broker_timer_->start();
}

void InstrumentIndex::ensure_started() {
    const QString path = Database::instance().path(); // empty while closed
    {
        QMutexLocker lock(&start_mutex_);
        if (!path.isEmpty() && path == started_path_)
            return;
    }
    start();
}

void InstrumentIndex::run_read(std::function<void(QSqlDatabase&)> task) {
    reader_pool_.start([this, task = std::move(task)]() {
        if (shutting_down_.load(std::memory_order_acquire))
            return;
        QSqlDatabase db = Database::instance().connection();
        task(db);
    });
}

void InstrumentIndex::run_write(std::function<void(QSqlDatabase&)> task) {
    writer_pool_.start([this, task = std::move(task)]() {
        if (shutting_down_.load(std::memory_order_acquire))
            return;
        QSqlDatabase db = Database::instance().connection();
        task(db);
    });
}

quint64 InstrumentIndex::register_request(const QString& channel) {
    const quint64 id = next_request_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (channel.isEmpty())
        return id;
    QMutexLocker lock(&request_mutex_);
    const auto it = latest_by_channel_.constFind(channel);
    if (it != latest_by_channel_.cend())
        cancelled_.insert(it.value());
    latest_by_channel_.insert(channel, id);
    channel_of_.insert(id, channel);
    return id;
}

bool InstrumentIndex::is_current(quint64 id) {
    QMutexLocker lock(&request_mutex_);
    if (cancelled_.contains(id))
        return false;
    const auto it = channel_of_.constFind(id);
    return it == channel_of_.cend() || latest_by_channel_.value(it.value()) == id;
}

bool InstrumentIndex::take_if_current(quint64 id) {
    QMutexLocker lock(&request_mutex_);
    const bool cancelled = cancelled_.remove(id);
    bool current = !cancelled;
    const auto it = channel_of_.find(id);
    if (it != channel_of_.end()) {
        const QString channel = it.value();
        if (latest_by_channel_.value(channel) == id)
            latest_by_channel_.remove(channel);
        else
            current = false;
        channel_of_.erase(it);
    }
    return current;
}

void InstrumentIndex::forget(quint64 id) {
    take_if_current(id);
}

void InstrumentIndex::cancel(quint64 request_id) {
    if (request_id == 0)
        return;
    QMutexLocker lock(&request_mutex_);
    cancelled_.insert(request_id);
}

QString InstrumentIndex::report_error(const std::string& raw, const char* what) {
    const QString text = QString::fromStdString(raw);
    LOG_WARN(kIixTag, logf("Instrument index operation failed", {{"op", what}, {"error", text}}));
    QString message = db::user_message(db::classify_text(text), iix_label());
    if (message.isEmpty())
        message = db::user_message(db::SqlErrorKind::Other, iix_label());
    QMetaObject::invokeMethod(
        this, [this, message]() { emit index_error(message); }, Qt::QueuedConnection);
    return message;
}

void InstrumentIndex::post_changed(int changed, const QStringList& ids) {
    if (changed <= 0)
        return;
    const QStringList list = ids.size() <= kIixMaxIdsInSignal ? ids : QStringList();
    QMetaObject::invokeMethod(
        this, [this, changed, list]() { emit index_changed(changed, list); }, Qt::QueuedConnection);
}

// ── Search / lookup ─────────────────────────────────────────────────────────

quint64 InstrumentIndex::search(const SearchRequest& req, QObject* context, SearchCallback done) {
    ensure_started();
    const quint64 id = register_request(req.channel);
    const bool has_context = context != nullptr;
    QPointer<QObject> ctx(context);
    run_read([this, id, req, ctx, has_context, done = std::move(done)](QSqlDatabase& db) {
        if (!is_current(id)) {
            forget(id);
            return;
        }
        SearchResult result;
        result.request_id = id;
        if (!db.isOpen()) {
            result.error = report_error("the database connection is not available on the index worker", "search");
        } else {
            namespace ci = core::instrument;
            const ParsedInput parsed = (!req.text.isEmpty() || req.parsed.kind == ci::InputKind::Empty)
                                           ? ci::parse_symbol_input(req.text, req.venue_hint, req.ctx.home_country)
                                           : req.parsed;
            auto r = IixRepo::search(db, parsed, req.ctx, req.limit);
            if (r.is_ok())
                result.hits = std::move(r.value());
            else
                result.error = report_error(r.error(), "search");
        }
        QMetaObject::invokeMethod(
            this,
            [this, result = std::move(result), ctx, has_context, done]() {
                if (!take_if_current(result.request_id))
                    return;
                if (has_context) {
                    QObject* target = ctx.data();
                    if (!target)
                        return;
                    if (done) {
                        if (target->thread() == QThread::currentThread())
                            done(result);
                        else
                            QMetaObject::invokeMethod(target, [done, result]() { done(result); }, Qt::QueuedConnection);
                    }
                } else if (done) {
                    done(result);
                }
                emit search_finished(result);
            },
            Qt::QueuedConnection);
    });
    return id;
}

quint64 InstrumentIndex::lookup(const LookupRequest& req, QObject* context, LookupCallback done) {
    ensure_started();
    const quint64 id = register_request(req.channel);
    const bool has_context = context != nullptr;
    QPointer<QObject> ctx(context);
    run_read([this, id, req, ctx, has_context, done = std::move(done)](QSqlDatabase& db) {
        if (!is_current(id)) {
            forget(id);
            return;
        }
        LookupResult result;
        result.request_id = id;
        if (!db.isOpen()) {
            result.error = report_error("the database connection is not available on the index worker", "lookup");
        } else {
            Result<QVector<Instrument>> r = Result<QVector<Instrument>>::ok({});
            switch (req.kind) {
                case LookupKind::Id:
                    r = IixRepo::find_by_ids(db, {req.key});
                    break;
                case LookupKind::Symbol:
                    r = IixRepo::find_by_symbol(db, req.key);
                    break;
                case LookupKind::ListingCode:
                    r = IixRepo::find_by_listing_code(db, req.key, req.limit);
                    break;
                case LookupKind::Alias:
                    r = IixRepo::find_by_alias(db, req.key, req.limit);
                    break;
                case LookupKind::Isin:
                    r = IixRepo::find_by_isin(db, req.key);
                    break;
                case LookupKind::Prefix:
                    r = IixRepo::search_prefix(db, req.key, req.classes, req.limit);
                    break;
                case LookupKind::NameContains:
                    r = IixRepo::search_name_contains(db, req.key, req.classes, req.limit);
                    break;
                case LookupKind::ProviderSymbol:
                    r = IixRepo::find_by_provider_symbol(db, req.provider, req.key, req.exchange);
                    break;
                case LookupKind::Current:
                    r = IixRepo::resolve_current(db, req.key);
                    break;
                case LookupKind::Predecessors:
                    r = IixRepo::predecessors(db, req.key);
                    break;
            }
            if (r.is_ok())
                result.found = std::move(r.value());
            else
                result.error = report_error(r.error(), "lookup");
        }
        QMetaObject::invokeMethod(
            this,
            [this, result = std::move(result), ctx, has_context, done]() {
                if (!take_if_current(result.request_id))
                    return;
                if (has_context) {
                    QObject* target = ctx.data();
                    if (!target)
                        return;
                    if (done) {
                        if (target->thread() == QThread::currentThread())
                            done(result);
                        else
                            QMetaObject::invokeMethod(target, [done, result]() { done(result); }, Qt::QueuedConnection);
                    }
                } else if (done) {
                    done(result);
                }
                emit lookup_finished(result);
            },
            Qt::QueuedConnection);
    });
    return id;
}

// ── Learning ────────────────────────────────────────────────────────────────

void InstrumentIndex::learn(const Instrument& in) {
    namespace ci = core::instrument;
    Instrument inst = in;
    inst.symbol = inst.symbol.trimmed();
    if (!inst.symbol.isEmpty()) {
        const QString canon = ci::canonical_symbol(inst.symbol);
        if (!canon.isEmpty())
            inst.symbol = canon;
        const Instrument sk = ci::instrument_skeleton(inst.symbol);
        if (sk.is_valid()) {
            auto fill = [](QString& dst, const QString& v) {
                if (dst.trimmed().isEmpty())
                    dst = v;
            };
            fill(inst.id, sk.id);
            fill(inst.display, sk.display);
            fill(inst.venue, sk.venue);
            fill(inst.mic, sk.mic);
            fill(inst.country, sk.country);
            fill(inst.currency, sk.currency);
            fill(inst.name, sk.name);
            if (inst.asset_class == ci::AssetClass::Unknown)
                inst.asset_class = sk.asset_class;
        }
    }
    if (inst.id.trimmed().isEmpty())
        return; // nothing the index can key (ISIN, name, junk)
    const QByteArray sig = QJsonDocument(inst.to_json()).toJson(QJsonDocument::Compact);
    {
        QMutexLocker lock(&learn_mutex_);
        if (learned_seen_.value(inst.id) == sig)
            return;
        if (learned_seen_.size() >= kIixMaxLearnedSeen)
            learned_seen_.clear();
        learned_seen_.insert(inst.id, sig);
        pending_learn_.push_back(std::move(inst));
        if (learn_flush_scheduled_)
            return;
        learn_flush_scheduled_ = true;
    }
    QMetaObject::invokeMethod(
        this,
        [this]() {
            ensure_started();
            learn_timer_->start();
        },
        Qt::QueuedConnection);
}

void InstrumentIndex::learn_from_quote(const QString& symbol, const QString& name, const QString& exchange,
                                       const QString& quote_type, const QString& currency) {
    namespace ci = core::instrument;
    Instrument inst;
    inst.symbol = symbol.trimmed();
    if (inst.symbol.isEmpty())
        return;
    inst.name = name.trimmed();
    inst.quote_type = quote_type.trimmed();
    inst.currency = currency.trimmed();
    const QString canon = ci::canonical_symbol(inst.symbol);
    inst.asset_class = ci::asset_class_of(canon.isEmpty() ? inst.symbol : canon, inst.quote_type);
    if (!exchange.trimmed().isEmpty()) {
        if (const ci::VenueInfo* v = ci::venue_info_for_code(exchange, ci::CodeSpace::Yahoo)) {
            inst.venue = v->venue;
            inst.mic = v->mic;
        }
    }
    learn(inst);
}

void InstrumentIndex::flush_learned() {
    QVector<Instrument> batch;
    {
        QMutexLocker lock(&learn_mutex_);
        batch = std::exchange(pending_learn_, {});
        learn_flush_scheduled_ = false;
    }
    if (batch.isEmpty())
        return;
    run_write([this, batch = std::move(batch)](QSqlDatabase& db) {
        if (!db.isOpen()) {
            report_error("the database connection is not available on the index worker", "learn");
            return;
        }
        IixRepo::UpsertOptions opt;
        opt.source = IixRepo::Source::Learned;
        auto r = IixRepo::upsert(db, batch, opt);
        if (r.is_err()) {
            report_error(r.error(), "learn");
            return;
        }
        post_changed(r.value().changed(), r.value().changed_ids);
    });
}

// ── Seed and broker masters ─────────────────────────────────────────────────

void InstrumentIndex::seed_task(QSqlDatabase& db) {
    if (!db.isOpen()) {
        report_error("the database connection is not available on the index worker", "seed");
        return;
    }
    const QString version = QString::fromLatin1(kSymbolSeedVersion);
    auto stored = IixRepo::meta_get(db, QLatin1String(IixRepo::kMetaSeedVersion));
    if (stored.is_err()) {
        report_error(stored.error(), "seed");
        return;
    }
    if (stored.value() == version)
        return;
    SymbolSeedStats stats;
    const QVector<Instrument> rows = builtin_symbol_seed(&stats);
    IixRepo::UpsertOptions opt;
    opt.source = IixRepo::Source::Seed;
    opt.former_valid_to = builtin_symbol_seed_former_dates();
    auto r = IixRepo::upsert(db, rows, opt);
    if (r.is_err()) {
        report_error(r.error(), "seed");
        return;
    }
    if (auto m = IixRepo::meta_set(db, QLatin1String(IixRepo::kMetaSeedVersion), version); m.is_err())
        report_error(m.error(), "seed version");
    LOG_DEBUG(kIixTag, QStringLiteral("Seed %1 applied: %2 records (%3 indices, %4 futures, %5 crypto, %6 FX, "
                                     "%7 companies), %8 changed")
                          .arg(version)
                          .arg(rows.size())
                          .arg(stats.indices)
                          .arg(stats.futures)
                          .arg(stats.crypto)
                          .arg(stats.fx)
                          .arg(stats.companies)
                          .arg(r.value().changed()));
    const int changed = r.value().changed();
    const QStringList ids = r.value().changed_ids;
    QMetaObject::invokeMethod(
        this, [this, changed, version]() { emit seed_applied(changed, version); }, Qt::QueuedConnection);
    post_changed(changed, ids);
}

void InstrumentIndex::broker_task(QSqlDatabase& db, const QString& only_broker, bool force) {
    if (!db.isOpen()) {
        report_error("the database connection is not available on the index worker", "broker import");
        return;
    }
    auto state = IixRepo::broker_master_state(db);
    if (state.is_err()) {
        report_error(state.error(), "broker import");
        return;
    }
    const QString only = only_broker.trimmed().toLower();
    for (const IixRepo::BrokerState& s : state.value()) {
        if (shutting_down_.load(std::memory_order_acquire))
            return;
        if (!only.isEmpty() && s.broker_id.toLower() != only)
            continue;
        const QString key = IixRepo::broker_meta_key(s.broker_id);
        auto seen = IixRepo::meta_get(db, key);
        if (seen.is_err()) {
            report_error(seen.error(), "broker import");
            return;
        }
        if (!force && seen.value() == s.last_updated)
            continue;
        QString error;
        const int changed = import_broker_rows(db, s.broker_id, &error);
        if (changed < 0)
            return; // reported; retried on the next check
        if (auto m = IixRepo::meta_set(db, key, s.last_updated); m.is_err())
            report_error(m.error(), "broker import state");
        LOG_INFO(kIixTag, QStringLiteral("Broker master %1 imported into the symbol index: %2 records changed")
                              .arg(s.broker_id)
                              .arg(changed));
        const QString broker = s.broker_id;
        QMetaObject::invokeMethod(
            this, [this, broker, changed]() { emit broker_import_finished(broker, changed); }, Qt::QueuedConnection);
        post_changed(changed, {});
    }
}

int InstrumentIndex::import_broker_rows(QSqlDatabase& db, const QString& broker_id, QString* error) {
    IixRepo::UpsertOptions opt;
    opt.source = IixRepo::Source::Broker;
    opt.source_detail = QStringLiteral("broker:") + broker_id.trimmed().toLower();
    QHash<QString, bool> eq_seen; // id -> the kept row is an EQ-series row
    qint64 after = 0;
    int changed = 0;
    for (;;) {
        if (shutting_down_.load(std::memory_order_acquire))
            return -1;
        auto rows = IixRepo::read_broker_rows(db, broker_id, after, kIixBrokerPage);
        if (rows.is_err()) {
            if (error)
                *error = report_error(rows.error(), "broker import");
            return -1;
        }
        if (rows.value().isEmpty())
            break;
        QVector<Instrument> batch;
        batch.reserve(rows.value().size());
        for (const IixRepo::BrokerRow& row : rows.value()) {
            after = row.rowid;
            Instrument inst = IixRepo::instrument_from_broker_row(broker_id, row);
            if (inst.id.isEmpty())
                continue;
            const bool eq = iix_is_eq_series(row);
            const auto it = eq_seen.constFind(inst.id);
            if (it != eq_seen.cend() && (it.value() || !eq))
                continue;
            eq_seen.insert(inst.id, eq);
            batch.push_back(std::move(inst));
        }
        if (!batch.isEmpty()) {
            auto r = IixRepo::upsert(db, batch, opt); // one transaction per page
            if (r.is_err()) {
                if (error)
                    *error = report_error(r.error(), "broker import");
                return -1;
            }
            changed += r.value().changed();
        }
        if (rows.value().size() < kIixBrokerPage)
            break;
    }
    return changed;
}

void InstrumentIndex::import_broker_instruments(const QString& broker_id) {
    ensure_started();
    const QString broker = broker_id.trimmed();
    if (broker.isEmpty())
        return;
    run_write([this, broker](QSqlDatabase& db) { broker_task(db, broker, true); });
}

void InstrumentIndex::import_all_broker_instruments() {
    ensure_started();
    run_write([this](QSqlDatabase& db) { broker_task(db, QString(), false); });
}

// ── Backend master sync ─────────────────────────────────────────────────────

void InstrumentIndex::apply_master_delta(const QJsonObject& delta, QObject* context, DeltaCallback done) {
    ensure_started();
    const bool has_context = context != nullptr;
    QPointer<QObject> ctx(context);
    run_write([this, delta, ctx, has_context, done = std::move(done)](QSqlDatabase& db) {
        bool ok = false;
        bool has_more = false;
        QString text;
        int changed = 0;
        QStringList ids;
        if (!db.isOpen()) {
            text = report_error("the database connection is not available on the index worker", "master delta");
        } else {
            auto r = IixRepo::apply_master_delta(db, delta);
            if (r.is_ok()) {
                ok = true;
                text = r.value().cursor;
                has_more = r.value().has_more;
                changed = r.value().upserted + r.value().delisted + r.value().renamed;
                ids = r.value().changed_ids;
            } else {
                text = report_error(r.error(), "master delta");
            }
        }
        if (ok)
            post_changed(changed, ids);
        QMetaObject::invokeMethod(
            this,
            [this, ok, text, has_more, changed, ctx, has_context, done]() {
                if (ok)
                    emit master_delta_applied(text, changed, has_more);
                if (!done)
                    return;
                if (!has_context) {
                    done(ok, text, has_more);
                    return;
                }
                QObject* target = ctx.data();
                if (!target)
                    return;
                if (target->thread() == QThread::currentThread())
                    done(ok, text, has_more);
                else
                    QMetaObject::invokeMethod(
                        target, [done, ok, text, has_more]() { done(ok, text, has_more); }, Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
    });
}

void InstrumentIndex::read_sync_cursor(QObject* context, CursorCallback done) {
    ensure_started();
    if (!done)
        return;
    const bool has_context = context != nullptr;
    QPointer<QObject> ctx(context);
    run_read([this, ctx, has_context, done = std::move(done)](QSqlDatabase& db) {
        QString cursor;
        if (db.isOpen()) {
            auto r = IixRepo::meta_get(db, QLatin1String(IixRepo::kMetaMasterCursor));
            if (r.is_ok())
                cursor = r.value();
            else
                report_error(r.error(), "sync cursor");
        }
        QMetaObject::invokeMethod(
            this,
            [cursor, ctx, has_context, done]() {
                if (!has_context) {
                    done(cursor);
                    return;
                }
                QObject* target = ctx.data();
                if (!target)
                    return;
                if (target->thread() == QThread::currentThread())
                    done(cursor);
                else
                    QMetaObject::invokeMethod(target, [done, cursor]() { done(cursor); }, Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::services
