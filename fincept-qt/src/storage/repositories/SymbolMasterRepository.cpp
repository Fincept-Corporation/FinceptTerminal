// SymbolMasterRepository.cpp — see SymbolMasterRepository.h. Every file-scope name
// carries an smr_ prefix (unity builds concatenate translation units).

#include "storage/repositories/SymbolMasterRepository.h"

#include "core/instrument/VenueSymbols.h"
#include "core/logging/Logger.h"
#include "trading/instruments/InstrumentSearchSql.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

#include <algorithm>
#include <atomic>
#include <utility>

namespace fincept {

namespace {

constexpr const char* kSmrTag = "SymbolMaster";
constexpr int kSmrIdChunk = 400;          // ids per IN (...) list
constexpr int kSmrExactKeyLimit = 400;    // exact alias rows per search
constexpr int kSmrPrefixLimit = 200;      // per prefix / contains pass
constexpr int kSmrFuzzyScanLimit = 20000; // alias rows read by the fuzzy pass (2-char range)
constexpr int kSmrDenseRange = 3000;      // a prefix range at least this large is walked by weight
constexpr int kSmrMaxSuccessorHops = 8;

std::atomic<QThread*> g_smr_forbidden_thread{nullptr};

using SmrInstrument = core::instrument::Instrument;

void smr_check_thread(const char* fn) {
    QThread* forbidden = g_smr_forbidden_thread.load(std::memory_order_relaxed);
    if (forbidden && QThread::currentThread() == forbidden) {
        LOG_ERROR(kSmrTag, QStringLiteral("%1 ran on the GUI thread — symbol index SQL must run on a worker")
                               .arg(QLatin1String(fn)));
        Q_ASSERT_X(false, fn, "symbol index SQL on the GUI thread");
    }
}

// A null QString binds as SQL NULL; every text column is NOT NULL.
QString smr_nn(const QString& s) {
    return s.isNull() ? QStringLiteral("") : s;
}

std::string smr_err(const QSqlQuery& q, const char* what) {
    return std::string(what) + ": " + q.lastError().text().toStdString();
}

/// Bind every value by position (prepared statements reused across rows).
void smr_bind(QSqlQuery& q, const QVariantList& values) {
    for (int i = 0; i < values.size(); ++i)
        q.bindValue(i, values.at(i));
}

QString smr_placeholders(qsizetype n) {
    // "?,?,?" for n bound values. Deliberately not a hand-written append loop: AppleClang 17
    // (arm64, -O2) compiled `for (i..n) s += (i == 0) ? "?" : ",?"` into two appends followed
    // by a trap instruction, so the first query with three or more ids killed the process
    // (macOS only; the 4.5.3 self-tests on the release runner).
    if (n <= 0)
        return QString();
    return QStringList(n, QStringLiteral("?")).join(QLatin1Char(','));
}

// Empty containers skip QJsonDocument entirely (most records have no lists): the JSON
// round trip was a measurable share of a bulk upsert.
QString smr_json_text(const QJsonObject& o) {
    if (o.isEmpty())
        return QStringLiteral("{}");
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString smr_json_text(const QStringList& l) {
    if (l.isEmpty())
        return QStringLiteral("[]");
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(l)).toJson(QJsonDocument::Compact));
}

QStringList smr_json_list(const QString& text) {
    QStringList out;
    if (text.size() <= 2) // "", "[]"
        return out;
    const QJsonArray a = QJsonDocument::fromJson(text.toUtf8()).array();
    for (const auto& v : a) {
        const QString s = v.toString();
        if (!s.isEmpty())
            out.push_back(s);
    }
    return out;
}

QHash<QString, QString> smr_json_map(const QString& text) {
    QHash<QString, QString> out;
    if (text.size() <= 2) // "", "{}"
        return out;
    const QJsonObject o = QJsonDocument::fromJson(text.toUtf8()).object();
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        const QString v = it.value().toString();
        if (!it.key().isEmpty() && !v.isEmpty())
            out.insert(it.key(), v);
    }
    return out;
}

QJsonObject smr_providers_json(const QHash<QString, QString>& p) {
    QJsonObject o;
    for (auto it = p.constBegin(); it != p.constEnd(); ++it)
        o.insert(it.key(), it.value());
    return o;
}

bool smr_all_digits(const QString& s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return false;
    return true;
}

/// "0700" -> "700"; anything else unchanged.
QString smr_strip_zeros(const QString& key) {
    if (!smr_all_digits(key))
        return key;
    qsizetype i = 0;
    while (i < key.size() - 1 && key.at(i) == QLatin1Char('0'))
        ++i;
    return key.mid(i);
}

// ── Rows ────────────────────────────────────────────────────────────────────

const char* const kSmrCols = "id, symbol, display, name, asset_class, quote_type, venue, mic, country, currency, "
                             "isin, figi, status, successor_id, primary_listing, rank_weight, providers_json, "
                             "aliases_json, former_json, source, source_rank";

struct SmrRow {
    SmrInstrument inst;
    QString source;
    int source_rank = 0;
};

SmrRow smr_map_row(const QSqlQuery& q) {
    namespace ci = core::instrument;
    SmrRow r;
    SmrInstrument& i = r.inst;
    i.id = q.value(0).toString();
    i.symbol = q.value(1).toString();
    i.display = q.value(2).toString();
    i.name = q.value(3).toString();
    i.asset_class = ci::asset_class_from_slug(q.value(4).toString());
    i.quote_type = q.value(5).toString();
    i.venue = q.value(6).toString();
    i.mic = q.value(7).toString();
    i.country = q.value(8).toString();
    i.currency = q.value(9).toString();
    i.isin = q.value(10).toString();
    i.figi = q.value(11).toString();
    i.status = ci::instrument_status_from_slug(q.value(12).toString());
    i.successor_id = q.value(13).toString();
    i.primary_listing = q.value(14).toInt() != 0;
    i.rank_weight = q.value(15).toInt();
    i.provider_symbols = smr_json_map(q.value(16).toString());
    i.aliases = smr_json_list(q.value(17).toString());
    i.former_symbols = smr_json_list(q.value(18).toString());
    r.source = q.value(19).toString();
    r.source_rank = q.value(20).toInt();
    return r;
}

/// Stored rows for `ids` (any order; unknown ids skipped).
Result<QHash<QString, SmrRow>> smr_rows_by_ids(QSqlDatabase& db, const QStringList& ids) {
    using R = Result<QHash<QString, SmrRow>>;
    QHash<QString, SmrRow> out;
    for (qsizetype off = 0; off < ids.size(); off += kSmrIdChunk) {
        const QStringList chunk = ids.mid(off, kSmrIdChunk);
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.prepare(QStringLiteral("SELECT %1 FROM symbol_master WHERE id IN (%2)")
                           .arg(QLatin1String(kSmrCols), smr_placeholders(chunk.size()))))
            return R::err(smr_err(q, "prepare rows by id"));
        for (const QString& id : chunk)
            q.addBindValue(id);
        if (!q.exec())
            return R::err(smr_err(q, "rows by id"));
        while (q.next()) {
            SmrRow row = smr_map_row(q);
            out.insert(row.inst.id, std::move(row));
        }
    }
    return R::ok(std::move(out));
}

/// Records for `ids`, in the order given.
Result<QVector<SmrInstrument>> smr_records(QSqlDatabase& db, const QStringList& ids) {
    using R = Result<QVector<SmrInstrument>>;
    auto rows = smr_rows_by_ids(db, ids);
    if (rows.is_err())
        return R::err(rows.error());
    QVector<SmrInstrument> out;
    out.reserve(rows.value().size());
    for (const QString& id : ids) {
        const auto it = rows.value().constFind(id);
        if (it != rows.value().cend())
            out.push_back(it->inst);
    }
    return R::ok(std::move(out));
}

/// First column of every row, de-duplicated, in result order.
Result<QStringList> smr_ids(QSqlDatabase& db, const QString& sql, const QVariantList& params) {
    QSqlQuery q(db);
    q.setForwardOnly(true);
    if (!q.prepare(sql))
        return Result<QStringList>::err(smr_err(q, "prepare id query"));
    for (const QVariant& v : params)
        q.addBindValue(v);
    if (!q.exec())
        return Result<QStringList>::err(smr_err(q, "id query"));
    QStringList out;
    QSet<QString> seen;
    while (q.next()) {
        const QString id = q.value(0).toString();
        if (!id.isEmpty() && !seen.contains(id)) {
            seen.insert(id);
            out.push_back(id);
        }
    }
    return Result<QStringList>::ok(std::move(out));
}

/// Records heaviest first (rank_weight, then symbol, then id) — the order of the plain lookups.
void smr_sort_by_weight(QVector<SmrInstrument>& v) {
    std::stable_sort(v.begin(), v.end(), [](const SmrInstrument& a, const SmrInstrument& b) {
        if (a.rank_weight != b.rank_weight)
            return a.rank_weight > b.rank_weight;
        if (a.symbol != b.symbol)
            return a.symbol < b.symbol;
        return a.id < b.id;
    });
}

// ── Lookup keys of a record ─────────────────────────────────────────────────

struct SmrKey {
    QString key;  // alias_norm
    QString kind; // code | alias | former | name | lead | isin
    QString text; // as given
};

/// `name_norm` = normalize_name(inst.name), computed once by the caller.
QVector<SmrKey> smr_keys(const SmrInstrument& inst, const QString& name_norm) {
    namespace ci = core::instrument;
    QVector<SmrKey> out;
    QSet<QString> seen; // key + '|' + kind
    auto add = [&out, &seen](const QString& key, const char* kind, const QString& text) {
        if (key.isEmpty() || key.size() > 160)
            return;
        const QString k = QString::fromLatin1(kind);
        const QString tag = key + QLatin1Char('|') + k;
        if (seen.contains(tag))
            return;
        seen.insert(tag);
        out.push_back({key, k, text});
    };
    auto add_code = [&add](const QString& code) {
        const QString key = ci::alias_key(code);
        add(key, "code", code);
        const QString stripped = smr_strip_zeros(key);
        if (stripped != key)
            add(stripped, "code", code);
    };
    const ci::YahooParts p = ci::from_yahoo(inst.symbol);
    if (p.valid()) {
        add_code(p.code);
        add_code(p.base);
    }
    const qsizetype colon = inst.id.indexOf(QLatin1Char(':'));
    if (colon > 0)
        add_code(inst.id.mid(colon + 1));
    if (!inst.display.isEmpty() && !inst.display.contains(QLatin1Char(' ')))
        add_code(inst.display);
    for (auto it = inst.provider_symbols.constBegin(); it != inst.provider_symbols.constEnd(); ++it) {
        if (!it.key().startsWith(QLatin1String("broker:")))
            continue;
        const QStringList parts = it.value().split(QLatin1Char('|'));
        if (parts.size() >= 2)
            add_code(parts.at(1));
    }
    for (const QString& a : inst.aliases)
        add(ci::alias_key(a), "alias", a);
    for (const QString& f : inst.former_symbols)
        add(ci::alias_key(f), "former", f);
    if (!inst.name.isEmpty()) {
        const QString name_key = ci::alias_key(inst.name);
        add(name_key, "name", inst.name);
        const QString lead = name_norm.section(QLatin1Char(' '), 0, 0);
        const QString lead_key = ci::alias_key(lead);
        if (lead_key.size() >= 3 && lead_key != name_key)
            add(lead_key, "lead", lead);
    }
    if (!inst.isin.isEmpty())
        add(inst.isin, "isin", inst.isin);
    return out;
}

// ── Writer ──────────────────────────────────────────────────────────────────

struct SmrWriter {
    explicit SmrWriter(QSqlDatabase& db)
        : ins_master(db), del_alias(db), ins_alias(db), del_prov(db), ins_prov(db), sel_former(db) {}
    QSqlQuery ins_master;
    QSqlQuery del_alias;
    QSqlQuery ins_alias;
    QSqlQuery del_prov;
    QSqlQuery ins_prov;
    QSqlQuery sel_former;

    Result<void> prepare() {
        if (!ins_master.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO symbol_master (id, symbol, display, name, name_norm, asset_class, quote_type, "
                "venue, mic, country, currency, isin, figi, status, successor_id, primary_listing, rank_weight, "
                "providers_json, aliases_json, former_json, source, source_rank, updated_at) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)")))
            return Result<void>::err(smr_err(ins_master, "prepare symbol_master insert"));
        if (!del_alias.prepare(QStringLiteral("DELETE FROM symbol_alias WHERE id = ?")))
            return Result<void>::err(smr_err(del_alias, "prepare symbol_alias delete"));
        if (!ins_alias.prepare(QStringLiteral("INSERT OR IGNORE INTO symbol_alias (alias_norm, id, kind, text, valid_to) "
                                              "VALUES (?,?,?,?,?)")))
            return Result<void>::err(smr_err(ins_alias, "prepare symbol_alias insert"));
        if (!del_prov.prepare(QStringLiteral("DELETE FROM symbol_provider WHERE id = ?")))
            return Result<void>::err(smr_err(del_prov, "prepare symbol_provider delete"));
        if (!ins_prov.prepare(QStringLiteral("INSERT OR REPLACE INTO symbol_provider (id, provider, symbol, exchange, "
                                             "token, native, value) VALUES (?,?,?,?,?,?,?)")))
            return Result<void>::err(smr_err(ins_prov, "prepare symbol_provider insert"));
        if (!sel_former.prepare(QStringLiteral("SELECT alias_norm, valid_to FROM symbol_alias "
                                               "WHERE id = ? AND kind = 'former' AND valid_to <> ''")))
            return Result<void>::err(smr_err(sel_former, "prepare former select"));
        return Result<void>::ok();
    }

    /// Former-symbol dates stored for `id`: alias_norm -> valid_to.
    Result<QHash<QString, QString>> stored_dates(const QString& id) {
        using R = Result<QHash<QString, QString>>;
        QHash<QString, QString> out;
        smr_bind(sel_former, {id});
        if (!sel_former.exec())
            return R::err(smr_err(sel_former, "former select"));
        while (sel_former.next())
            out.insert(sel_former.value(0).toString(), sel_former.value(1).toString());
        sel_former.finish();
        return R::ok(std::move(out));
    }

    /// Write one record and rebuild its lookup rows. `existed`: a row was stored before
    /// (old lookup rows are deleted, former valid_to dates carried over).
    Result<void> write(const SmrInstrument& inst, const QString& source, int source_rank, qint64 now, bool existed,
                       const QHash<QString, QString>& former_dates) {
        namespace ci = core::instrument;
        QHash<QString, QString> dates; // alias_norm -> valid_to
        if (existed && !inst.former_symbols.isEmpty()) {
            auto kept = stored_dates(inst.id);
            if (kept.is_err())
                return Result<void>::err(kept.error());
            dates = std::move(kept.value());
        }
        for (auto it = former_dates.constBegin(); it != former_dates.constEnd(); ++it)
            dates.insert(it.key(), it.value());

        const QString name_norm = ci::normalize_name(inst.name);
        const QVariantList values = {
            inst.id,
            smr_nn(inst.symbol),
            smr_nn(inst.display),
            smr_nn(inst.name),
            smr_nn(name_norm),
            smr_nn(ci::asset_class_slug(inst.asset_class)),
            smr_nn(inst.quote_type),
            smr_nn(inst.venue),
            smr_nn(inst.mic),
            smr_nn(inst.country),
            smr_nn(inst.currency),
            smr_nn(inst.isin),
            smr_nn(inst.figi),
            ci::instrument_status_slug(inst.status),
            smr_nn(inst.successor_id),
            inst.primary_listing ? 1 : 0,
            inst.rank_weight,
            smr_json_text(smr_providers_json(inst.provider_symbols)),
            smr_json_text(inst.aliases),
            smr_json_text(inst.former_symbols),
            smr_nn(source),
            source_rank,
            now,
        };
        smr_bind(ins_master, values);
        if (!ins_master.exec())
            return Result<void>::err(smr_err(ins_master, "symbol_master insert"));

        if (existed) {
            smr_bind(del_alias, {inst.id});
            if (!del_alias.exec())
                return Result<void>::err(smr_err(del_alias, "symbol_alias delete"));
            smr_bind(del_prov, {inst.id});
            if (!del_prov.exec())
                return Result<void>::err(smr_err(del_prov, "symbol_provider delete"));
        }
        for (const SmrKey& k : smr_keys(inst, name_norm)) {
            smr_bind(ins_alias, {k.key, inst.id, k.kind, smr_nn(k.text),
                                 k.kind == QLatin1String("former") ? smr_nn(dates.value(k.key)) : QStringLiteral("")});
            if (!ins_alias.exec())
                return Result<void>::err(smr_err(ins_alias, "symbol_alias insert"));
        }
        for (auto it = inst.provider_symbols.constBegin(); it != inst.provider_symbols.constEnd(); ++it) {
            QString symbol = it.value();
            QString exchange, token, native;
            if (it.key().startsWith(QLatin1String("broker:"))) {
                const QStringList parts = it.value().split(QLatin1Char('|'));
                exchange = parts.value(0);
                symbol = parts.value(1);
                token = parts.value(2);
                native = parts.value(3);
            }
            smr_bind(ins_prov, {inst.id, it.key(), smr_nn(symbol), smr_nn(exchange), smr_nn(token), smr_nn(native),
                                it.value()});
            if (!ins_prov.exec())
                return Result<void>::err(smr_err(ins_prov, "symbol_provider insert"));
        }
        return Result<void>::ok();
    }
};

/// The upsert itself; the caller owns the transaction.
Result<SymbolMasterRepository::UpsertStats> smr_upsert_in_txn(QSqlDatabase& db, const QVector<SmrInstrument>& batch,
                                                             const SymbolMasterRepository::UpsertOptions& opt) {
    using Repo = SymbolMasterRepository;
    using R = Result<Repo::UpsertStats>;
    Repo::UpsertStats stats;

    // Normalise and merge duplicates inside the batch (same source: later values win).
    QHash<QString, SmrInstrument> incoming;
    QStringList order;
    for (const SmrInstrument& raw : batch) {
        SmrInstrument n = Repo::normalized(raw);
        if (n.id.isEmpty()) {
            ++stats.rejected;
            continue;
        }
        auto it = incoming.find(n.id);
        if (it == incoming.end()) {
            order.push_back(n.id);
            incoming.insert(n.id, std::move(n));
        } else {
            *it = Repo::merge(*it, opt.source, n, opt.source);
        }
    }
    if (order.isEmpty())
        return R::ok(stats);

    auto stored = smr_rows_by_ids(db, order);
    if (stored.is_err())
        return R::err(stored.error());

    SmrWriter w(db);
    if (auto pr = w.prepare(); pr.is_err())
        return R::err(pr.error());

    const qint64 now = opt.now_ms > 0 ? opt.now_ms : QDateTime::currentMSecsSinceEpoch();
    const int incoming_rank = static_cast<int>(opt.source);
    const QString incoming_text = opt.source_detail.isEmpty() ? Repo::source_slug(opt.source) : opt.source_detail;

    for (const QString& id : order) {
        const SmrInstrument& n = incoming.value(id);
        QHash<QString, QString> dates;
        if (!opt.former_valid_to.isEmpty()) {
            const QString prefix = id + QLatin1Char('|');
            for (auto it = opt.former_valid_to.constBegin(); it != opt.former_valid_to.constEnd(); ++it)
                if (it.key().startsWith(prefix) && !it.value().isEmpty())
                    dates.insert(it.key().mid(prefix.size()), it.value());
        }
        const auto sit = stored.value().constFind(id);
        if (sit == stored.value().cend()) {
            if (auto wr = w.write(n, incoming_text, incoming_rank, now, false, dates); wr.is_err())
                return R::err(wr.error());
            ++stats.inserted;
            stats.changed_ids.push_back(id);
            continue;
        }
        const SmrRow& row = sit.value();
        const SmrInstrument merged = Repo::merge(row.inst, Repo::source_from_rank(row.source_rank), n, opt.source);
        const bool takes_over = incoming_rank >= row.source_rank;
        const int rank = takes_over ? incoming_rank : row.source_rank;
        const QString text = takes_over ? incoming_text : row.source;
        bool dates_stored = true; // every incoming former date is already on the alias rows
        if (!dates.isEmpty()) {
            auto have = w.stored_dates(id);
            if (have.is_err())
                return R::err(have.error());
            for (auto it = dates.constBegin(); it != dates.constEnd() && dates_stored; ++it)
                dates_stored = have.value().value(it.key()) == it.value();
        }
        if (merged == row.inst && rank == row.source_rank && text == row.source && dates_stored) {
            ++stats.unchanged;
            continue;
        }
        if (auto wr = w.write(merged, text, rank, now, true, dates); wr.is_err())
            return R::err(wr.error());
        ++stats.updated;
        stats.changed_ids.push_back(id);
    }
    return R::ok(std::move(stats));
}

Result<void> smr_begin(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("BEGIN IMMEDIATE")))
        return Result<void>::err(smr_err(q, "begin"));
    return Result<void>::ok();
}

Result<void> smr_commit(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("COMMIT"))) {
        const std::string e = smr_err(q, "commit");
        QSqlQuery rb(db);
        rb.exec(QStringLiteral("ROLLBACK"));
        return Result<void>::err(e);
    }
    return Result<void>::ok();
}

void smr_rollback(QSqlDatabase& db) {
    QSqlQuery q(db);
    q.exec(QStringLiteral("ROLLBACK"));
}

// ── Search ──────────────────────────────────────────────────────────────────

/// Range bounds for "column starts with `prefix`" (BINARY collation); false = use substr().
bool smr_range(const QString& prefix, QString& lo, QString& hi) {
    return trading::instrument_search_sql::prefix_range(prefix, lo, hi);
}

/// " AND m.asset_class IN (...) AND m.status = 'active'" for the caller's filters.
QString smr_filters(const core::instrument::RankContext& ctx, const QString& alias, QVariantList& params) {
    QString sql;
    if (!ctx.allowed_classes.isEmpty()) {
        QStringList slugs;
        for (const auto c : ctx.allowed_classes) {
            const QString s = core::instrument::asset_class_slug(c);
            if (!slugs.contains(s))
                slugs.push_back(s);
        }
        sql += QStringLiteral(" AND %1asset_class IN (%2)").arg(alias, smr_placeholders(slugs.size()));
        for (const QString& s : slugs)
            params << s;
    }
    if (!ctx.include_inactive)
        sql += QStringLiteral(" AND %1status = 'active'").arg(alias);
    return sql;
}

/// The code the parsed text is compared with as a listing code (InstrumentRank's rule).
QString smr_query_code(const core::instrument::ParsedInput& p) {
    using core::instrument::InputKind;
    switch (p.kind) {
        case InputKind::Ticker:
        case InputKind::CryptoPair:
        case InputKind::YahooSymbol:
        case InputKind::Alias:
            return p.base;
        case InputKind::FxPair:
            return p.base + p.quote;
        case InputKind::Contract:
            return p.search_text.toUpper();
        default:
            break;
    }
    return {};
}

struct SmrGather {
    QSqlDatabase& db;
    const core::instrument::RankContext& ctx;
    QSet<QString> seen;
    QStringList fresh; // ids not yet loaded
    SymbolMasterRepository::GatherStats stats;

    SmrGather(QSqlDatabase& d, const core::instrument::RankContext& c) : db(d), ctx(c) {}

    int add(const QStringList& ids) {
        int n = 0;
        for (const QString& id : ids) {
            if (!seen.contains(id)) {
                seen.insert(id);
                fresh.push_back(id);
                ++n;
            }
        }
        return n;
    }

    Result<int> run(const QString& sql, const QVariantList& params) {
        auto r = smr_ids(db, sql, params);
        if (r.is_err())
            return Result<int>::err(r.error());
        return Result<int>::ok(add(r.value()));
    }

    /// symbol IN (...)
    Result<void> by_symbols(const QStringList& symbols) {
        if (symbols.isEmpty())
            return Result<void>::ok();
        QVariantList params;
        for (const QString& s : symbols)
            params << s;
        auto r = run(QStringLiteral("SELECT id FROM symbol_master WHERE symbol IN (%1)")
                         .arg(smr_placeholders(symbols.size())),
                     params);
        if (r.is_err())
            return Result<void>::err(r.error());
        stats.by_symbol += r.value();
        return Result<void>::ok();
    }

    /// alias_norm IN (...)
    Result<void> by_keys(const QStringList& keys) {
        if (keys.isEmpty())
            return Result<void>::ok();
        QVariantList params;
        for (const QString& k : keys)
            params << k;
        params << kSmrExactKeyLimit;
        auto r = run(QStringLiteral("SELECT id FROM symbol_alias WHERE alias_norm IN (%1) LIMIT ?")
                         .arg(smr_placeholders(keys.size())),
                     params);
        if (r.is_err())
            return Result<void>::err(r.error());
        stats.by_exact_key += r.value();
        return Result<void>::ok();
    }

    /// Rows matching `where` in `table` (alias "a" / none), counted up to kSmrDenseRange.
    Result<int> range_rows(const QString& table, const QString& where, const QVariantList& bounds) {
        QSqlQuery q(db);
        if (!q.prepare(QStringLiteral("SELECT COUNT(*) FROM (SELECT 1 FROM %1 WHERE %2 LIMIT ?)").arg(table, where)))
            return Result<int>::err(smr_err(q, "prepare range count"));
        for (const QVariant& v : bounds)
            q.addBindValue(v);
        q.addBindValue(kSmrDenseRange);
        if (!q.exec() || !q.next())
            return Result<int>::err(smr_err(q, "range count"));
        return Result<int>::ok(q.value(0).toInt());
    }

    /// alias_norm starts with `key`, heaviest instruments first. A sparse range is read and
    /// sorted; a dense one ("A", "S") walks idx_symbol_master_weight heaviest first and probes
    /// each instrument's keys (idx_symbol_alias_id), stopping at the cap — sorting tens of
    /// thousands of keys per keystroke is what made one-character searches slow.
    Result<void> by_key_prefix(const QString& key) {
        if (key.isEmpty())
            return Result<void>::ok();
        QVariantList bounds;
        QString where;       // range on the primary key
        QString probe_where; // the same test, kept off the primary key inside the per-row probe
        QString lo, hi;
        if (smr_range(key, lo, hi)) {
            where = QStringLiteral("a.alias_norm >= ? AND a.alias_norm < ?");
            probe_where = QStringLiteral("+a.alias_norm >= ? AND +a.alias_norm < ?");
            bounds << lo << hi;
        } else {
            where = probe_where = QStringLiteral("substr(a.alias_norm, 1, ?) = ?");
            bounds << key.size() << key;
        }
        auto dense = range_rows(QStringLiteral("symbol_alias a"), where, bounds);
        if (dense.is_err())
            return Result<void>::err(dense.error());
        QVariantList params = bounds;
        QString sql;
        if (dense.value() >= kSmrDenseRange) {
            const QString filters = smr_filters(ctx, QStringLiteral("m."), params);
            params << kSmrPrefixLimit;
            sql = QStringLiteral("SELECT m.id FROM symbol_master m WHERE EXISTS (SELECT 1 FROM symbol_alias a "
                                 "WHERE a.id = m.id AND %1)%2 ORDER BY m.rank_weight DESC LIMIT ?")
                      .arg(probe_where, filters);
        } else {
            // "+m.rank_weight": sort the (small) range; never walk the weight index for it.
            const QString filters = smr_filters(ctx, QStringLiteral("m."), params);
            params << kSmrPrefixLimit * 3; // several keys per instrument
            sql = QStringLiteral("SELECT a.id FROM symbol_alias a JOIN symbol_master m ON m.id = a.id "
                                 "WHERE %1%2 ORDER BY +m.rank_weight DESC LIMIT ?")
                      .arg(where, filters);
        }
        auto r = run(sql, params);
        if (r.is_err())
            return Result<void>::err(r.error());
        stats.by_key_prefix += r.value();
        return Result<void>::ok();
    }

    /// name_norm starts with `qname`, heaviest first (dense ranges: weight walk, as above).
    Result<void> by_name_prefix(const QString& qname) {
        if (qname.isEmpty())
            return Result<void>::ok();
        QVariantList bounds;
        QString where;
        QString walk_where;
        QString lo, hi;
        if (smr_range(qname, lo, hi)) {
            where = QStringLiteral("name_norm >= ? AND name_norm < ?");
            walk_where = QStringLiteral("+name_norm >= ? AND +name_norm < ?");
            bounds << lo << hi;
        } else {
            where = walk_where = QStringLiteral("substr(name_norm, 1, ?) = ?");
            bounds << qname.size() << qname;
        }
        auto dense = range_rows(QStringLiteral("symbol_master"), where, bounds);
        if (dense.is_err())
            return Result<void>::err(dense.error());
        QVariantList params = bounds;
        const QString filters = smr_filters(ctx, QString(), params);
        params << kSmrPrefixLimit;
        const bool walk = dense.value() >= kSmrDenseRange;
        auto r = run(QStringLiteral("SELECT id FROM symbol_master WHERE %1%2 ORDER BY %3 DESC LIMIT ?")
                         .arg(walk ? walk_where : where, filters,
                              walk ? QStringLiteral("rank_weight") : QStringLiteral("+rank_weight")),
                     params);
        if (r.is_err())
            return Result<void>::err(r.error());
        stats.by_name_prefix += r.value();
        return Result<void>::ok();
    }

    /// name_norm contains `qname`, heaviest first: one sequential scan, matches sorted
    /// ("+rank_weight" keeps the planner from walking the weight index with a random table
    /// lookup per row, which is several times slower).
    Result<void> by_contains(const QString& qname) {
        QVariantList params{qname};
        const QString filters = smr_filters(ctx, QString(), params);
        params << kSmrPrefixLimit;
        auto r = run(QStringLiteral("SELECT id FROM symbol_master WHERE instr(name_norm, ?) > 0%1 "
                                    "ORDER BY +rank_weight DESC LIMIT ?")
                         .arg(filters),
                     params);
        if (r.is_err())
            return Result<void>::err(r.error());
        stats.by_contains += r.value();
        return Result<void>::ok();
    }

    /// Keys sharing the first two characters with `qkey` and within the ranker's edit
    /// distance (<= 1 for 4-5 characters, <= 2 for 6+).
    Result<void> by_fuzzy(const QString& qkey) {
        const int max = qkey.size() >= 6 ? 2 : 1;
        const QString head = qkey.left(2);
        QString lo, hi;
        if (!smr_range(head, lo, hi))
            return Result<void>::ok();
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.prepare(QStringLiteral("SELECT alias_norm, id FROM symbol_alias WHERE alias_norm >= ? AND alias_norm < ? "
                                      "AND length(alias_norm) BETWEEN ? AND ? "
                                      "AND kind IN ('code', 'alias', 'former', 'lead') LIMIT ?")))
            return Result<void>::err(smr_err(q, "prepare fuzzy scan"));
        q.addBindValue(lo);
        q.addBindValue(hi);
        q.addBindValue(qkey.size() - max);
        q.addBindValue(qkey.size() + max);
        q.addBindValue(kSmrFuzzyScanLimit);
        if (!q.exec())
            return Result<void>::err(smr_err(q, "fuzzy scan"));
        QStringList ids;
        while (q.next()) {
            const QString key = q.value(0).toString();
            if (core::instrument::edit_distance(qkey, key, max) <= max)
                ids.push_back(q.value(1).toString());
        }
        stats.by_fuzzy += add(ids);
        return Result<void>::ok();
    }
};

Result<QVector<SmrInstrument>> smr_load_fresh(SmrGather& g) {
    const QStringList ids = std::exchange(g.fresh, {});
    auto rows = smr_rows_by_ids(g.db, ids);
    if (rows.is_err())
        return Result<QVector<SmrInstrument>>::err(rows.error());
    QVector<SmrInstrument> out;
    out.reserve(rows.value().size());
    for (auto it = rows.value().cbegin(); it != rows.value().cend(); ++it)
        out.push_back(it->inst);
    g.stats.loaded += static_cast<int>(out.size());
    return Result<QVector<SmrInstrument>>::ok(std::move(out));
}

} // namespace

// ── Public: sources, guard ──────────────────────────────────────────────────

QString SymbolMasterRepository::source_slug(Source s) {
    switch (s) {
        case Source::Broker:
            return QStringLiteral("broker");
        case Source::Learned:
            return QStringLiteral("learned");
        case Source::Seed:
            return QStringLiteral("seed");
        case Source::Master:
            return QStringLiteral("master");
    }
    return QStringLiteral("broker");
}

SymbolMasterRepository::Source SymbolMasterRepository::source_from_rank(int rank) {
    switch (rank) {
        case 2:
            return Source::Learned;
        case 3:
            return Source::Seed;
        case 4:
            return Source::Master;
        default:
            return Source::Broker;
    }
}

void SymbolMasterRepository::set_forbidden_thread(QThread* t) {
    g_smr_forbidden_thread.store(t, std::memory_order_relaxed);
}

QString SymbolMasterRepository::broker_meta_key(const QString& broker_id) {
    return QStringLiteral("broker_import:") + broker_id.trimmed().toLower();
}

// ── Public: pure helpers ────────────────────────────────────────────────────

SymbolMasterRepository::Instrument SymbolMasterRepository::normalized(const Instrument& in) {
    namespace ci = core::instrument;
    Instrument o = in;
    o.id = o.id.trimmed();
    o.symbol = o.symbol.trimmed();
    o.display = o.display.trimmed();
    o.name = o.name.trimmed();
    o.quote_type = o.quote_type.trimmed();
    o.venue = o.venue.trimmed();
    o.mic = o.mic.trimmed().toUpper();
    o.country = o.country.trimmed().toUpper();
    o.currency = o.currency.trimmed();
    o.isin = o.isin.trimmed().toUpper();
    o.figi = o.figi.trimmed();
    o.successor_id = o.successor_id.trimmed();
    if (!o.symbol.isEmpty()) {
        const QString canon = ci::canonical_symbol(o.symbol);
        if (!canon.isEmpty())
            o.symbol = canon;
    }
    if (o.id.isEmpty() && !o.symbol.isEmpty())
        o.id = ci::instrument_id_for(o.symbol);
    if (!o.isin.isEmpty() && !ci::is_valid_isin(o.isin))
        o.isin.clear();
    auto dedupe = [](const QStringList& in_list) {
        QStringList out;
        QSet<QString> keys;
        for (const QString& raw : in_list) {
            const QString s = raw.trimmed();
            const QString k = ci::alias_key(s);
            if (k.isEmpty() || keys.contains(k))
                continue;
            keys.insert(k);
            out.push_back(s);
        }
        return out;
    };
    o.aliases = dedupe(o.aliases);
    o.former_symbols = dedupe(o.former_symbols);
    QHash<QString, QString> providers;
    for (auto it = o.provider_symbols.constBegin(); it != o.provider_symbols.constEnd(); ++it) {
        const QString k = it.key().trimmed();
        const QString v = it.value().trimmed();
        if (!k.isEmpty() && !v.isEmpty())
            providers.insert(k, v);
    }
    o.provider_symbols = providers;
    return o;
}

SymbolMasterRepository::Instrument SymbolMasterRepository::merge(const Instrument& s, Source stored_source,
                                                                 const Instrument& in, Source incoming_source) {
    namespace ci = core::instrument;
    Instrument o = s;
    const bool wins = static_cast<int>(incoming_source) >= static_cast<int>(stored_source);
    const bool curated = incoming_source == Source::Seed || incoming_source == Source::Master;
    auto str = [wins](QString& dst, const QString& v) {
        if (!v.isEmpty() && (wins || dst.isEmpty()))
            dst = v;
    };
    if (o.id.isEmpty())
        o.id = in.id;
    str(o.symbol, in.symbol);
    str(o.display, in.display);
    str(o.name, in.name);
    str(o.quote_type, in.quote_type);
    str(o.venue, in.venue);
    str(o.mic, in.mic);
    str(o.country, in.country);
    str(o.currency, in.currency);
    str(o.isin, in.isin);
    str(o.figi, in.figi);
    if (in.asset_class != ci::AssetClass::Unknown && (wins || o.asset_class == ci::AssetClass::Unknown))
        o.asset_class = in.asset_class;
    if (wins && curated) {
        o.status = in.status;
        o.successor_id = in.successor_id;
        o.primary_listing = in.primary_listing;
    } else if (wins) {
        if (in.status != ci::InstrumentStatus::Active) {
            o.status = in.status;
            o.successor_id = in.successor_id;
        }
        if (in.primary_listing)
            o.primary_listing = true;
    } else if (!o.primary_listing && in.primary_listing) {
        o.primary_listing = true;
    }
    if (in.rank_weight != 0 && (wins || o.rank_weight == 0))
        o.rank_weight = in.rank_weight;
    for (auto it = in.provider_symbols.constBegin(); it != in.provider_symbols.constEnd(); ++it)
        o.provider_symbols.insert(it.key(), it.value());
    auto unite = [](QStringList& dst, const QStringList& add) {
        QSet<QString> keys;
        for (const QString& x : dst)
            keys.insert(ci::alias_key(x));
        for (const QString& x : add) {
            const QString k = ci::alias_key(x);
            if (!k.isEmpty() && !keys.contains(k)) {
                keys.insert(k);
                dst.push_back(x);
            }
        }
    };
    unite(o.aliases, in.aliases);
    unite(o.former_symbols, in.former_symbols);
    return o;
}

// ── Public: writes ──────────────────────────────────────────────────────────

Result<SymbolMasterRepository::UpsertStats> SymbolMasterRepository::upsert(QSqlDatabase& db,
                                                                           const QVector<Instrument>& batch,
                                                                           const UpsertOptions& opt) {
    smr_check_thread("SymbolMasterRepository::upsert");
    using R = Result<UpsertStats>;
    if (batch.isEmpty())
        return R::ok(UpsertStats{});
    if (auto b = smr_begin(db); b.is_err())
        return R::err(b.error());
    auto r = smr_upsert_in_txn(db, batch, opt);
    if (r.is_err()) {
        smr_rollback(db);
        return r;
    }
    if (auto c = smr_commit(db); c.is_err())
        return R::err(c.error());
    return r;
}

// ── Public: reads ───────────────────────────────────────────────────────────

Result<QVector<SymbolMasterRepository::Instrument>> SymbolMasterRepository::find_by_ids(QSqlDatabase& db,
                                                                                       const QStringList& ids) {
    smr_check_thread("SymbolMasterRepository::find_by_ids");
    QStringList clean;
    for (const QString& id : ids) {
        const QString t = id.trimmed();
        if (!t.isEmpty() && !clean.contains(t))
            clean.push_back(t);
    }
    return smr_records(db, clean);
}

Result<QVector<SymbolMasterRepository::Instrument>> SymbolMasterRepository::find_by_symbol(QSqlDatabase& db,
                                                                                          const QString& symbol) {
    smr_check_thread("SymbolMasterRepository::find_by_symbol");
    using R = Result<QVector<Instrument>>;
    QString s = core::instrument::canonical_symbol(symbol);
    if (s.isEmpty())
        s = symbol.trimmed().toUpper();
    if (s.isEmpty())
        return R::ok({});
    auto ids = smr_ids(db, QStringLiteral("SELECT id FROM symbol_master WHERE symbol = ?"), {s});
    if (ids.is_err())
        return R::err(ids.error());
    auto recs = smr_records(db, ids.value());
    if (recs.is_ok())
        smr_sort_by_weight(recs.value());
    return recs;
}

Result<QVector<SymbolMasterRepository::Instrument>>
SymbolMasterRepository::find_by_listing_code(QSqlDatabase& db, const QString& code, int limit) {
    smr_check_thread("SymbolMasterRepository::find_by_listing_code");
    using R = Result<QVector<Instrument>>;
    const QString key = core::instrument::alias_key(code);
    if (key.isEmpty())
        return R::ok({});
    QStringList keys{key};
    const QString stripped = smr_strip_zeros(key);
    if (stripped != key)
        keys << stripped;
    QVariantList params;
    for (const QString& k : keys)
        params << k;
    params << kSmrExactKeyLimit;
    auto ids = smr_ids(db,
                       QStringLiteral("SELECT id FROM symbol_alias WHERE alias_norm IN (%1) AND kind = 'code' LIMIT ?")
                           .arg(smr_placeholders(keys.size())),
                       params);
    if (ids.is_err())
        return R::err(ids.error());
    auto recs = smr_records(db, ids.value());
    if (recs.is_err())
        return recs;
    smr_sort_by_weight(recs.value());
    if (limit > 0 && recs.value().size() > limit)
        recs.value().resize(limit);
    return recs;
}

Result<QVector<SymbolMasterRepository::Instrument>> SymbolMasterRepository::find_by_alias(QSqlDatabase& db,
                                                                                         const QString& text,
                                                                                         int limit) {
    smr_check_thread("SymbolMasterRepository::find_by_alias");
    using R = Result<QVector<Instrument>>;
    const QString key = core::instrument::alias_key(text);
    if (key.isEmpty())
        return R::ok({});
    QSqlQuery q(db);
    q.setForwardOnly(true);
    if (!q.prepare(QStringLiteral("SELECT id, kind FROM symbol_alias WHERE alias_norm = ? LIMIT ?")))
        return R::err(smr_err(q, "prepare alias lookup"));
    q.addBindValue(key);
    q.addBindValue(kSmrExactKeyLimit);
    if (!q.exec())
        return R::err(smr_err(q, "alias lookup"));
    static const QStringList kKindOrder = {QStringLiteral("code"), QStringLiteral("former"), QStringLiteral("alias"),
                                           QStringLiteral("isin"), QStringLiteral("name"), QStringLiteral("lead")};
    QHash<QString, int> best_kind;
    while (q.next()) {
        const QString id = q.value(0).toString();
        const int k = static_cast<int>(kKindOrder.indexOf(q.value(1).toString()));
        const int rank = k < 0 ? 99 : k;
        const auto it = best_kind.find(id);
        if (it == best_kind.end() || rank < *it)
            best_kind.insert(id, rank);
    }
    auto recs = smr_records(db, best_kind.keys());
    if (recs.is_err())
        return recs;
    std::stable_sort(recs.value().begin(), recs.value().end(), [&best_kind](const Instrument& a, const Instrument& b) {
        const int ka = best_kind.value(a.id);
        const int kb = best_kind.value(b.id);
        if (ka != kb)
            return ka < kb;
        if (a.rank_weight != b.rank_weight)
            return a.rank_weight > b.rank_weight;
        return a.id < b.id;
    });
    if (limit > 0 && recs.value().size() > limit)
        recs.value().resize(limit);
    return recs;
}

Result<QVector<SymbolMasterRepository::Instrument>> SymbolMasterRepository::find_by_isin(QSqlDatabase& db,
                                                                                        const QString& isin) {
    smr_check_thread("SymbolMasterRepository::find_by_isin");
    using R = Result<QVector<Instrument>>;
    const QString key = isin.trimmed().toUpper();
    if (key.isEmpty())
        return R::ok({});
    auto ids = smr_ids(db, QStringLiteral("SELECT id FROM symbol_alias WHERE alias_norm = ? AND kind = 'isin'"), {key});
    if (ids.is_err())
        return R::err(ids.error());
    auto recs = smr_records(db, ids.value());
    if (recs.is_ok()) {
        // Primary listing first, then the heaviest.
        std::stable_sort(recs.value().begin(), recs.value().end(), [](const Instrument& a, const Instrument& b) {
            if (a.primary_listing != b.primary_listing)
                return a.primary_listing;
            if (a.rank_weight != b.rank_weight)
                return a.rank_weight > b.rank_weight;
            return a.id < b.id;
        });
    }
    return recs;
}

Result<QVector<SymbolMasterRepository::Instrument>>
SymbolMasterRepository::find_by_provider_symbol(QSqlDatabase& db, const QString& provider, const QString& symbol,
                                                const QString& exchange) {
    smr_check_thread("SymbolMasterRepository::find_by_provider_symbol");
    using R = Result<QVector<Instrument>>;
    const QString prov = provider.trimmed();
    const QString sym = symbol.trimmed();
    if (prov.isEmpty() || sym.isEmpty())
        return R::ok({});
    QString sql = QStringLiteral("SELECT id FROM symbol_provider WHERE provider = ? AND symbol = ?");
    QVariantList params{prov, sym};
    if (!exchange.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND exchange = ?");
        params << exchange.trimmed().toUpper();
    }
    auto ids = smr_ids(db, sql, params);
    if (ids.is_err())
        return R::err(ids.error());
    auto recs = smr_records(db, ids.value());
    if (recs.is_ok())
        smr_sort_by_weight(recs.value());
    return recs;
}

Result<QVector<SymbolMasterRepository::Instrument>>
SymbolMasterRepository::search_prefix(QSqlDatabase& db, const QString& text, const QVector<AssetClass>& classes,
                                      int limit) {
    smr_check_thread("SymbolMasterRepository::search_prefix");
    using R = Result<QVector<Instrument>>;
    RankContext ctx;
    ctx.allowed_classes = classes;
    SmrGather g(db, ctx);
    if (auto r = g.by_key_prefix(core::instrument::alias_key(text)); r.is_err())
        return R::err(r.error());
    const QString qname = core::instrument::normalize_name(text);
    if (qname.size() >= 2) {
        if (auto r = g.by_name_prefix(qname); r.is_err())
            return R::err(r.error());
    }
    auto recs = smr_load_fresh(g);
    if (recs.is_err())
        return recs;
    smr_sort_by_weight(recs.value());
    if (limit > 0 && recs.value().size() > limit)
        recs.value().resize(limit);
    return recs;
}

Result<QVector<SymbolMasterRepository::Instrument>>
SymbolMasterRepository::search_name_contains(QSqlDatabase& db, const QString& text, const QVector<AssetClass>& classes,
                                             int limit) {
    smr_check_thread("SymbolMasterRepository::search_name_contains");
    using R = Result<QVector<Instrument>>;
    const QString qname = core::instrument::normalize_name(text);
    if (qname.size() < 3)
        return R::ok({});
    RankContext ctx;
    ctx.allowed_classes = classes;
    SmrGather g(db, ctx);
    if (auto r = g.by_contains(qname); r.is_err())
        return R::err(r.error());
    auto recs = smr_load_fresh(g);
    if (recs.is_err())
        return recs;
    smr_sort_by_weight(recs.value());
    if (limit > 0 && recs.value().size() > limit)
        recs.value().resize(limit);
    return recs;
}

Result<QVector<SymbolMasterRepository::Instrument>> SymbolMasterRepository::predecessors(QSqlDatabase& db,
                                                                                        const QString& id) {
    smr_check_thread("SymbolMasterRepository::predecessors");
    using R = Result<QVector<Instrument>>;
    if (id.trimmed().isEmpty())
        return R::ok({});
    auto ids = smr_ids(db, QStringLiteral("SELECT id FROM symbol_master WHERE successor_id = ?"), {id.trimmed()});
    if (ids.is_err())
        return R::err(ids.error());
    return smr_records(db, ids.value());
}

Result<QVector<SymbolMasterRepository::Instrument>> SymbolMasterRepository::resolve_current(QSqlDatabase& db,
                                                                                           const QString& key) {
    smr_check_thread("SymbolMasterRepository::resolve_current");
    using R = Result<QVector<Instrument>>;
    const QString k = key.trimmed();
    if (k.isEmpty())
        return R::ok({});
    auto first = smr_records(db, {k});
    if (first.is_err())
        return first;
    if (first.value().isEmpty()) {
        auto by_symbol = find_by_symbol(db, k);
        if (by_symbol.is_err())
            return by_symbol;
        if (by_symbol.value().isEmpty())
            return R::ok({});
        first.value() = {by_symbol.value().first()};
    }
    QVector<Instrument> out = first.value();
    Instrument cur = out.first();
    QSet<QString> visited{cur.id};
    bool moved = false;
    for (int hop = 0; hop < kSmrMaxSuccessorHops; ++hop) {
        if (cur.status != core::instrument::InstrumentStatus::Renamed || cur.successor_id.isEmpty() ||
            visited.contains(cur.successor_id))
            break;
        auto next = smr_records(db, {cur.successor_id});
        if (next.is_err())
            return next;
        if (next.value().isEmpty())
            break;
        cur = next.value().first();
        visited.insert(cur.id);
        moved = true;
    }
    if (moved)
        out.push_back(cur);
    return R::ok(std::move(out));
}

Result<int> SymbolMasterRepository::count(QSqlDatabase& db) {
    smr_check_thread("SymbolMasterRepository::count");
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM symbol_master")) || !q.next())
        return Result<int>::err(smr_err(q, "count"));
    return Result<int>::ok(q.value(0).toInt());
}

Result<QVector<SymbolMasterRepository::RankedInstrument>>
SymbolMasterRepository::search(QSqlDatabase& db, const ParsedInput& p, const RankContext& ctx_in, int limit,
                               GatherStats* stats) {
    smr_check_thread("SymbolMasterRepository::search");
    namespace ci = core::instrument;
    using R = Result<QVector<RankedInstrument>>;
    if (p.kind == ci::InputKind::Empty || p.kind == ci::InputKind::Invalid)
        return R::ok({});
    RankContext ctx = ctx_in;
    ctx.limit = limit > 0 ? limit : (ctx.limit > 0 ? ctx.limit : 50);

    SmrGather g(db, ctx);
    auto finish = [&g, stats](QVector<RankedInstrument> hits) {
        if (stats)
            *stats = g.stats;
        return R::ok(std::move(hits));
    };

    // ── Stage 1: exact symbols, exact keys, key prefix, name prefix ──
    QStringList symbols = p.candidates;
    if (!p.canonical.isEmpty() && !symbols.contains(p.canonical))
        symbols.push_back(p.canonical);
    if (auto r = g.by_symbols(symbols); r.is_err())
        return R::err(r.error());

    QStringList keys;
    auto add_key = [&keys](const QString& k) {
        if (!k.isEmpty() && !keys.contains(k))
            keys.push_back(k);
        const QString s = smr_strip_zeros(k);
        if (!s.isEmpty() && !keys.contains(s))
            keys.push_back(s);
    };
    const QString qkey = ci::alias_key(p.search_text);
    const QString code_key = ci::alias_key(smr_query_code(p));
    if (p.kind == ci::InputKind::Isin) {
        add_key(p.isin);
    } else {
        add_key(qkey);
        add_key(code_key);
    }
    if (auto r = g.by_keys(keys); r.is_err())
        return R::err(r.error());

    const QString qname = ci::normalize_name(p.search_text);
    if (p.kind != ci::InputKind::Isin) {
        if (auto r = g.by_key_prefix(qkey); r.is_err())
            return R::err(r.error());
        if (code_key != qkey) {
            if (auto r = g.by_key_prefix(code_key); r.is_err())
                return R::err(r.error());
        }
        if (qname.size() >= 2) {
            if (auto r = g.by_name_prefix(qname); r.is_err())
                return R::err(r.error());
        }
    }

    auto loaded = smr_load_fresh(g);
    if (loaded.is_err())
        return R::err(loaded.error());
    QVector<Instrument> candidates = std::move(loaded.value());
    QVector<RankedInstrument> hits = ci::rank_instruments(candidates, p, ctx);
    if (hits.size() >= ctx.limit || p.kind == ci::InputKind::Isin)
        return finish(std::move(hits));

    // ── Stage 2: name contains; fuzzy when nothing matched exactly ──
    // A fully specified symbol ("RELIANCE.NS", "EUR/USD", an OCC contract) is not a name
    // fragment: the (linear) contains scan cannot add anything useful for it.
    g.stats.stage2 = true;
    const bool specified = p.kind == ci::InputKind::YahooSymbol || p.kind == ci::InputKind::FxPair ||
                           p.kind == ci::InputKind::Contract ||
                           (p.kind == ci::InputKind::CryptoPair && !p.quote.isEmpty());
    if (qname.size() >= 3 && !specified) {
        if (auto r = g.by_contains(qname); r.is_err())
            return R::err(r.error());
    }
    const bool any_exact = std::any_of(hits.cbegin(), hits.cend(),
                                       [](const RankedInstrument& h) { return ci::is_exact_match(h.match); });
    if (!any_exact && qkey.size() >= 4) {
        if (auto r = g.by_fuzzy(qkey); r.is_err())
            return R::err(r.error());
        // Typos past the first word of a name ("tata motrs"): every name sharing the first word.
        const QString first_word = qname.section(QLatin1Char(' '), 0, 0);
        if (qname.contains(QLatin1Char(' ')) && first_word.size() >= 2) {
            if (auto r = g.by_name_prefix(first_word + QLatin1Char(' ')); r.is_err())
                return R::err(r.error());
        }
    }
    if (g.fresh.isEmpty())
        return finish(std::move(hits));
    auto more = smr_load_fresh(g);
    if (more.is_err())
        return R::err(more.error());
    candidates += more.value();
    return finish(ci::rank_instruments(candidates, p, ctx));
}

// ── Public: meta ────────────────────────────────────────────────────────────

Result<QString> SymbolMasterRepository::meta_get(QSqlDatabase& db, const QString& key) {
    smr_check_thread("SymbolMasterRepository::meta_get");
    QSqlQuery q(db);
    if (!q.prepare(QStringLiteral("SELECT value FROM symbol_master_meta WHERE key = ?")))
        return Result<QString>::err(smr_err(q, "prepare meta get"));
    q.addBindValue(key);
    if (!q.exec())
        return Result<QString>::err(smr_err(q, "meta get"));
    return Result<QString>::ok(q.next() ? q.value(0).toString() : QString());
}

Result<void> SymbolMasterRepository::meta_set(QSqlDatabase& db, const QString& key, const QString& value) {
    smr_check_thread("SymbolMasterRepository::meta_set");
    QSqlQuery q(db);
    if (!q.prepare(QStringLiteral("INSERT OR REPLACE INTO symbol_master_meta (key, value, updated_at) VALUES (?, ?, ?)")))
        return Result<void>::err(smr_err(q, "prepare meta set"));
    q.addBindValue(key);
    q.addBindValue(smr_nn(value));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!q.exec())
        return Result<void>::err(smr_err(q, "meta set"));
    return Result<void>::ok();
}

// ── Public: master delta ────────────────────────────────────────────────────

Result<SymbolMasterRepository::DeltaStats> SymbolMasterRepository::apply_master_delta(QSqlDatabase& db,
                                                                                     const QJsonObject& delta,
                                                                                     qint64 now_ms) {
    smr_check_thread("SymbolMasterRepository::apply_master_delta");
    namespace ci = core::instrument;
    using R = Result<DeltaStats>;
    auto text_of = [](const QJsonValue& v) {
        if (v.isString())
            return v.toString().trimmed();
        if (v.isDouble())
            return QString::number(static_cast<qint64>(v.toDouble()));
        return QString();
    };
    DeltaStats stats;
    stats.cursor = text_of(delta.value(QStringLiteral("next")));
    if (stats.cursor.isEmpty())
        return R::err("master delta has no \"next\" cursor; nothing applied");
    stats.version = text_of(delta.value(QStringLiteral("version")));
    stats.has_more = delta.value(QStringLiteral("has_more")).toBool(false);
    const QString since = text_of(delta.value(QStringLiteral("since")));

    if (auto b = smr_begin(db); b.is_err())
        return R::err(b.error());
    auto fail = [&db](const std::string& e) {
        smr_rollback(db);
        return R::err(e);
    };

    if (!since.isEmpty()) {
        auto stored = meta_get(db, QLatin1String(kMetaMasterCursor));
        if (stored.is_err())
            return fail(stored.error());
        if (!stored.value().isEmpty() && stored.value() != since)
            return fail("master delta continues from cursor " + since.toStdString() + " but the stored cursor is " +
                        stored.value().toStdString() + "; nothing applied");
    }

    UpsertOptions opt;
    opt.source = Source::Master;
    opt.now_ms = now_ms;
    auto upsert_one = [&db, &opt, &stats](const Instrument& inst, const QHash<QString, QString>& dates) {
        UpsertOptions o = opt;
        o.former_valid_to = dates;
        auto r = smr_upsert_in_txn(db, {inst}, o);
        if (r.is_ok()) {
            stats.changed_ids += r.value().changed_ids;
            stats.rejected += r.value().rejected;
        }
        return r;
    };

    const QJsonArray changes = delta.value(QStringLiteral("changes")).toArray();
    for (const auto& cv : changes) {
        const QJsonObject c = cv.toObject();
        const QString op = c.value(QStringLiteral("op")).toString().trimmed().toLower();
        if (op == QLatin1String("upsert")) {
            const QJsonObject o = c.value(QStringLiteral("instrument")).toObject();
            if (o.isEmpty()) {
                ++stats.rejected;
                continue;
            }
            const Instrument inst = normalized(Instrument::from_json(o));
            QHash<QString, QString> dates;
            const QJsonObject fv = o.value(QStringLiteral("former_valid_to")).toObject();
            for (auto it = fv.constBegin(); it != fv.constEnd(); ++it)
                dates.insert(inst.id + QLatin1Char('|') + ci::alias_key(it.key()), it.value().toString().trimmed());
            auto r = upsert_one(inst, dates);
            if (r.is_err())
                return fail(r.error());
            stats.upserted += r.value().changed();
            stats.unchanged += r.value().unchanged;
            continue;
        }
        if (op == QLatin1String("delist") || op == QLatin1String("rename")) {
            const QString id = c.value(QStringLiteral("id")).toString().trimmed();
            auto rows = smr_rows_by_ids(db, {id});
            if (rows.is_err())
                return fail(rows.error());
            if (id.isEmpty() || !rows.value().contains(id)) {
                ++stats.rejected;
                continue;
            }
            Instrument old = rows.value().value(id).inst;
            if (op == QLatin1String("delist")) {
                old.status = ci::InstrumentStatus::Delisted;
                auto r = upsert_one(old, {});
                if (r.is_err())
                    return fail(r.error());
                ++stats.delisted;
                continue;
            }
            const QString successor = c.value(QStringLiteral("successor_id")).toString().trimmed();
            if (successor.isEmpty() || successor == id) {
                ++stats.rejected;
                continue;
            }
            old.status = ci::InstrumentStatus::Renamed;
            old.successor_id = successor;
            auto r = upsert_one(old, {});
            if (r.is_err())
                return fail(r.error());
            ++stats.renamed;
            // The old listing code becomes a former symbol of the successor.
            auto srows = smr_rows_by_ids(db, {successor});
            if (srows.is_err())
                return fail(srows.error());
            if (!srows.value().contains(successor))
                continue;
            QString code = ci::from_yahoo(old.symbol).code;
            if (code.isEmpty()) {
                const qsizetype colon = id.indexOf(QLatin1Char(':'));
                code = colon > 0 ? id.mid(colon + 1) : id;
            }
            Instrument next = srows.value().value(successor).inst;
            next.former_symbols.push_back(code);
            QHash<QString, QString> dates;
            const QString valid_to = c.value(QStringLiteral("valid_to")).toString().trimmed();
            if (!valid_to.isEmpty())
                dates.insert(successor + QLatin1Char('|') + ci::alias_key(code), valid_to);
            auto sr = upsert_one(next, dates);
            if (sr.is_err())
                return fail(sr.error());
            continue;
        }
        ++stats.rejected;
    }

    if (auto r = meta_set(db, QLatin1String(kMetaMasterCursor), stats.cursor); r.is_err())
        return fail(r.error());
    if (!stats.version.isEmpty()) {
        if (auto r = meta_set(db, QLatin1String(kMetaMasterVersion), stats.version); r.is_err())
            return fail(r.error());
    }
    if (auto c = smr_commit(db); c.is_err())
        return R::err(c.error());
    stats.changed_ids.removeDuplicates();
    return R::ok(std::move(stats));
}

// ── Public: broker masters ──────────────────────────────────────────────────

Result<QVector<SymbolMasterRepository::BrokerState>> SymbolMasterRepository::broker_master_state(QSqlDatabase& db) {
    smr_check_thread("SymbolMasterRepository::broker_master_state");
    using R = Result<QVector<BrokerState>>;
    {
        QSqlQuery t(db);
        if (!t.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'instruments'")))
            return R::err(smr_err(t, "instruments table probe"));
        if (!t.next())
            return R::ok({});
    }
    QSqlQuery q(db);
    q.setForwardOnly(true);
    if (!q.exec(QStringLiteral("SELECT broker_id, MAX(updated_at) FROM instruments GROUP BY broker_id")))
        return R::err(smr_err(q, "broker master state"));
    QVector<BrokerState> out;
    while (q.next()) {
        BrokerState s;
        s.broker_id = q.value(0).toString();
        s.last_updated = q.value(1).toString();
        if (!s.broker_id.isEmpty())
            out.push_back(s);
    }
    return R::ok(std::move(out));
}

Result<QVector<SymbolMasterRepository::BrokerRow>>
SymbolMasterRepository::read_broker_rows(QSqlDatabase& db, const QString& broker_id, qint64 after_rowid, int limit) {
    smr_check_thread("SymbolMasterRepository::read_broker_rows");
    using R = Result<QVector<BrokerRow>>;
    QSqlQuery q(db);
    q.setForwardOnly(true);
    if (!q.prepare(QStringLiteral("SELECT id, symbol, brsymbol, name, exchange, instrument_type, instrument_token, "
                                  "broker_token FROM instruments WHERE broker_id = ? AND id > ? "
                                  "AND instrument_type IN ('EQ', 'INDEX') ORDER BY id LIMIT ?")))
        return R::err(smr_err(q, "prepare broker rows"));
    q.addBindValue(broker_id);
    q.addBindValue(after_rowid);
    q.addBindValue(limit > 0 ? limit : 2000);
    if (!q.exec())
        return R::err(smr_err(q, "broker rows"));
    QVector<BrokerRow> out;
    while (q.next()) {
        BrokerRow r;
        r.rowid = q.value(0).toLongLong();
        r.symbol = q.value(1).toString();
        r.brsymbol = q.value(2).toString();
        r.name = q.value(3).toString();
        r.exchange = q.value(4).toString();
        r.instrument_type = q.value(5).toString();
        const QString broker_token = q.value(7).toString();
        r.token = broker_token.isEmpty() ? q.value(6).toString() : broker_token;
        out.push_back(r);
    }
    return R::ok(std::move(out));
}

SymbolMasterRepository::Instrument SymbolMasterRepository::instrument_from_broker_row(const QString& broker_id,
                                                                                     const BrokerRow& row) {
    namespace ci = core::instrument;
    const QString exchange = row.exchange.trimmed().toUpper();
    const QString symbol = row.symbol.trimmed().toUpper();
    const bool index = row.instrument_type.trimmed().toUpper() == QLatin1String("INDEX");
    if (exchange.isEmpty() || symbol.isEmpty())
        return {};
    const QString yahoo = ci::to_yahoo(symbol, exchange, index ? ci::AssetClass::Index : ci::AssetClass::Equity);
    if (yahoo.isEmpty())
        return {};
    Instrument inst = ci::instrument_skeleton(yahoo);
    if (!inst.is_valid() || inst.id.isEmpty())
        return {};
    if (index && inst.asset_class != ci::AssetClass::Index)
        return {}; // an index code that does not map to an index (never store it as a stock)
    const QString name = row.name.trimmed();
    if (inst.name.isEmpty() && !name.isEmpty() && name.compare(symbol, Qt::CaseInsensitive) != 0)
        inst.name = name;
    inst.quote_type = index ? QStringLiteral("INDEX") : QStringLiteral("EQ");
    QString value = exchange + QLatin1Char('|') + symbol + QLatin1Char('|') + row.token.trimmed();
    const QString brsymbol = row.brsymbol.trimmed();
    if (!brsymbol.isEmpty() && brsymbol != symbol)
        value += QLatin1Char('|') + brsymbol;
    inst.provider_symbols.insert(QStringLiteral("broker:") + broker_id.trimmed().toLower(), value);
    return inst;
}

} // namespace fincept
