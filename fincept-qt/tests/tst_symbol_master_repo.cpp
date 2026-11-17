// Unit tests for the local instrument index (symbol system WP2):
//   storage/sqlite/migrations/v060_symbol_master.cpp      schema, idempotence, inspect()
//   storage/repositories/SymbolMasterRepository.{h,cpp}  upsert / merge / lookups / search /
//                                                         master delta / broker import
//   services/instruments/SymbolSeed.{h,cpp}               the bundled seed
//   services/instruments/InstrumentIndex.{h,cpp}          async service end to end
// against temp-file SQLite databases. The ranking cases of tests/data/symbol_vectors.json
// ("rank") run through the index: their instruments are stored, the index gathers the
// candidates with SQL and must return exactly the expected order. A 50k-row index is
// built to measure upsert and search times (printed with qInfo).

#include "core/instrument/Instrument.h"
#include "core/instrument/InstrumentRank.h"
#include "core/instrument/SymbolInput.h"
#include "core/instrument/VenueSymbols.h"
#include "services/instruments/InstrumentIndex.h"
#include "services/instruments/SymbolSeed.h"
#include "storage/repositories/SymbolMasterRepository.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <algorithm>

using namespace fincept::core::instrument;
using fincept::SymbolMasterRepository;
using Repo = fincept::SymbolMasterRepository;
using fincept::services::InstrumentIndex;

namespace {

QJsonObject tsm_load_vectors() {
    QString path = qEnvironmentVariable("FINCEPT_SYMBOL_VECTORS");
    if (path.isEmpty())
        path = QFINDTESTDATA("data/symbol_vectors.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

const QJsonObject& tsm_vectors() {
    static const QJsonObject v = tsm_load_vectors();
    return v;
}

QVector<AssetClass> tsm_classes(const QJsonValue& v) {
    QVector<AssetClass> out;
    for (const auto& x : v.toArray())
        out << asset_class_from_slug(x.toString());
    return out;
}

RankContext tsm_context(const QJsonObject& v) {
    RankContext ctx;
    ctx.home_country = v.value("home").toString();
    ctx.class_priority = tsm_classes(v.value("class_priority"));
    ctx.allowed_classes = tsm_classes(v.value("allowed_classes"));
    ctx.include_inactive = v.value("include_inactive").toBool(true);
    return ctx;
}

QStringList tsm_symbols(const QVector<RankedInstrument>& hits) {
    QStringList out;
    for (const auto& h : hits)
        out << h.instrument.symbol;
    return out;
}

QStringList tsm_symbols(const QVector<Instrument>& v) {
    QStringList out;
    for (const auto& i : v)
        out << i.symbol;
    return out;
}

/// A private connection on a fresh temp-file database with v060 applied (or not).
struct TsmDb {
    QTemporaryDir dir;
    QString name;
    QSqlDatabase db;

    explicit TsmDb(bool migrate = true) {
        name = QStringLiteral("tsm_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(dir.filePath(QStringLiteral("index.db")));
        db.open();
        {
            QSqlQuery q(db);
            q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
            q.finish(); // an unfinished statement would make every later COMMIT fail
        }
        if (migrate) {
            fincept::MigrationRunner runner(db);
            const auto r = runner.run();
            if (r.is_err())
                qWarning("migration failed: %s", r.error().c_str());
        }
    }
    ~TsmDb() {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
};

QStringList tsm_columns(QSqlDatabase& db, const QString& table) {
    QStringList out;
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table));
    while (q.next())
        out << q.value(1).toString();
    return out;
}

const fincept::Migration* tsm_v060() {
    for (const auto& m : fincept::MigrationRunner::all_migrations())
        if (m.version == 60)
            return &m;
    return nullptr;
}

Instrument tsm_full_record() {
    Instrument i;
    i.id = QStringLiteral("XNSE:TMPV");
    i.symbol = QStringLiteral("TMPV.NS");
    i.display = QStringLiteral("TMPV");
    i.name = QStringLiteral("Tata Motors Passenger Vehicles Ltd");
    i.asset_class = AssetClass::Equity;
    i.quote_type = QStringLiteral("EQUITY");
    i.venue = QStringLiteral("NSE");
    i.mic = QStringLiteral("XNSE");
    i.country = QStringLiteral("IN");
    i.currency = QStringLiteral("INR");
    i.isin = QStringLiteral("INE155A01022");
    i.figi = QStringLiteral("BBG000BXPW76");
    i.provider_symbols.insert(QStringLiteral("tv"), QStringLiteral("NSE:TMPV"));
    i.provider_symbols.insert(QStringLiteral("broker:zerodha"), QStringLiteral("NSE|TMPV|3456001"));
    i.aliases = {QStringLiteral("Tata Motors PV")};
    i.former_symbols = {QStringLiteral("TATAMOTORS")};
    i.primary_listing = true;
    i.rank_weight = 8;
    return i;
}

QVector<RankedInstrument> tsm_search(QSqlDatabase& db, const QString& text, const RankContext& ctx = {},
                                     int limit = 12, Repo::GatherStats* stats = nullptr) {
    const auto r = Repo::search(db, parse_symbol_input(text, {}, ctx.home_country), ctx, limit, stats);
    if (r.is_err()) {
        qWarning("search '%s' failed: %s", qPrintable(text), r.error().c_str());
        return {};
    }
    return r.value();
}

RankContext tsm_home(const char* country) {
    RankContext c;
    c.home_country = QString::fromLatin1(country);
    return c;
}

/// Deterministic synthetic listings (codes start with 'X' or are numeric on .T / .HK, so
/// they never shadow the owner-critical names; names reuse common words so prefix and
/// contains scans have real work).
QVector<Instrument> tsm_synthetic(int n) {
    static const char* const kWords[] = {
        "Global", "Holdings", "Industries", "Technologies", "Bank", "Energy", "Motors", "Capital", "Resources",
        "Pharma", "Steel", "Power", "Finance", "Insurance", "Logistics", "Foods", "Retail", "Textiles", "Cement",
        "Chemicals", "Infra", "Realty", "Media", "Telecom", "Software", "Systems", "Networks", "Metals", "Mining",
        "Agro", "Marine", "Aviation", "Healthcare", "Biotech", "Solar", "Green", "United", "National", "Pacific",
        "Atlantic", "Northern", "Southern", "Eastern", "Western", "Royal", "Imperial", "Prime", "Apex", "Summit",
        "Vertex", "Delta", "Omega", "Alpha", "Sigma", "Nova", "Orion", "Titan", "Atlas", "Zenith", "Regal",
        "Republic", "Renewable", "Reliable", "Relay", "Micro", "Macro", "Meridian", "Tata", "Taurus", "Nifty"};
    static const char* const kForms[] = {"Ltd", "Inc.", "plc", "AG", "Corp", "SA", "Limited", "Group"};
    static const char* const kSuffixes[] = {"", ".NS", ".BO", ".L", ".DE", ".TO", ".AX", ".PA", ".T", ".HK"};
    QRandomGenerator rng(42);
    QVector<Instrument> out;
    out.reserve(n);
    QSet<QString> used;
    const int words = int(sizeof(kWords) / sizeof(kWords[0]));
    while (out.size() < n) {
        const QString suffix = QString::fromLatin1(kSuffixes[rng.bounded(10)]);
        QString code;
        if (suffix == QLatin1String(".T"))
            code = QString::number(1000 + rng.bounded(9000));
        else if (suffix == QLatin1String(".HK"))
            code = QStringLiteral("%1").arg(1 + rng.bounded(9999), 4, 10, QLatin1Char('0'));
        else {
            code = QStringLiteral("X");
            const int len = 2 + rng.bounded(5);
            for (int i = 0; i < len; ++i)
                code += QChar(QLatin1Char(char('A' + rng.bounded(26))));
            if (rng.bounded(4) == 0)
                code += QString::number(rng.bounded(10));
        }
        const QString symbol = code + suffix;
        if (used.contains(symbol))
            continue;
        used.insert(symbol);
        Instrument inst = instrument_skeleton(symbol);
        if (inst.id.isEmpty())
            continue;
        QStringList parts;
        const int nw = 1 + rng.bounded(3);
        for (int i = 0; i < nw; ++i)
            parts << QString::fromLatin1(kWords[rng.bounded(words)]);
        parts << QString::fromLatin1(kForms[rng.bounded(8)]);
        inst.name = parts.join(QLatin1Char(' '));
        inst.rank_weight = rng.bounded(7);
        inst.primary_listing = rng.bounded(2) == 0;
        inst.quote_type = QStringLiteral("EQUITY");
        out.push_back(inst);
    }
    return out;
}

double tsm_median_ms(const std::function<void()>& f, int runs = 7) {
    QVector<double> t;
    for (int i = 0; i < runs; ++i) {
        QElapsedTimer timer;
        timer.start();
        f();
        t << timer.nsecsElapsed() / 1e6;
    }
    std::sort(t.begin(), t.end());
    return t.at(t.size() / 2);
}

} // namespace

class TstSymbolMasterRepo : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    void migration_applies_cleanly_twice();
    void migration_passes_inspect();

    void upsert_round_trip_and_idempotent();
    void normalized_fills_id_and_drops_bad_isin();
    void merge_precedence();
    void lookups_each_kind();
    void former_symbol_and_successor_chain();

    void rank_vectors_through_index_data();
    void rank_vectors_through_index();

    void seed_is_valid();
    void seed_owner_critical_searches();

    void master_delta_apply_and_cursor();
    void broker_master_import();

    void index_50k_rows_stays_fast();

    void service_end_to_end();

    void cleanupTestCase();

  private:
    QTemporaryDir service_dir_;
};

void TstSymbolMasterRepo::initTestCase() {
    QCoreApplication::setOrganizationName(QStringLiteral("FinceptTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_symbol_master_repo"));
    fincept::register_migration_v060();
    QVERIFY2(!tsm_vectors().isEmpty(), "tests/data/symbol_vectors.json not found (set FINCEPT_SYMBOL_VECTORS)");
    QVERIFY(tsm_v060() != nullptr);
}

// ── Migration ───────────────────────────────────────────────────────────────

void TstSymbolMasterRepo::migration_applies_cleanly_twice() {
    TsmDb t(false);
    QVERIFY(t.db.isOpen());
    const fincept::Migration* m = tsm_v060();
    for (int pass = 0; pass < 2; ++pass) {
        QSqlQuery(t.db).exec(QStringLiteral("BEGIN"));
        const auto r = m->apply(t.db);
        QVERIFY2(r.is_ok(), r.is_ok() ? "" : r.error().c_str());
        QSqlQuery(t.db).exec(QStringLiteral("COMMIT"));
    }
    const QStringList master = tsm_columns(t.db, QStringLiteral("symbol_master"));
    for (const char* c : {"id", "symbol", "display", "name", "name_norm", "asset_class", "quote_type", "venue", "mic",
                          "country", "currency", "isin", "figi", "status", "successor_id", "primary_listing",
                          "rank_weight", "providers_json", "aliases_json", "former_json", "source", "source_rank",
                          "updated_at"})
        QVERIFY2(master.contains(QLatin1String(c)), c);
    QCOMPARE(tsm_columns(t.db, QStringLiteral("symbol_alias")),
             QStringList({"alias_norm", "id", "kind", "text", "valid_to"}));
    QCOMPARE(tsm_columns(t.db, QStringLiteral("symbol_provider")),
             QStringList({"id", "provider", "symbol", "exchange", "token", "native", "value"}));
    QCOMPARE(tsm_columns(t.db, QStringLiteral("symbol_master_meta")), QStringList({"key", "value", "updated_at"}));
    // The prefix / exact lookups must be index range scans, never table scans.
    QSqlQuery plan(t.db);
    QVERIFY(plan.exec(QStringLiteral("EXPLAIN QUERY PLAN SELECT id FROM symbol_alias WHERE alias_norm >= 'REL' "
                                     "AND alias_norm < 'REM'")));
    QString detail;
    while (plan.next())
        detail += plan.value(3).toString() + QLatin1Char(';');
    QVERIFY2(detail.contains(QLatin1String("SEARCH")), qPrintable(detail));
    QVERIFY(plan.exec(QStringLiteral("EXPLAIN QUERY PLAN SELECT id FROM symbol_master WHERE name_norm >= 'tata' "
                                     "AND name_norm < 'tatb'")));
    detail.clear();
    while (plan.next())
        detail += plan.value(3).toString() + QLatin1Char(';');
    QVERIFY2(detail.contains(QLatin1String("idx_symbol_master_name_norm")), qPrintable(detail));
    // Dense prefix: heaviest-first walk of the weight index, one id probe per instrument.
    QVERIFY(plan.exec(QStringLiteral("EXPLAIN QUERY PLAN SELECT m.id FROM symbol_master m WHERE EXISTS (SELECT 1 FROM "
                                     "symbol_alias a WHERE a.id = m.id AND +a.alias_norm >= 'A' AND "
                                     "+a.alias_norm < 'B') ORDER BY m.rank_weight DESC LIMIT 200")));
    detail.clear();
    while (plan.next())
        detail += plan.value(3).toString() + QLatin1Char(';');
    QVERIFY2(detail.contains(QLatin1String("idx_symbol_master_weight")) &&
                 detail.contains(QLatin1String("idx_symbol_alias_id")) && !detail.contains(QLatin1String("TEMP B-TREE")),
             qPrintable(detail));
    // Name contains: one sequential scan, matches sorted — never a weight-index walk with a
    // random table lookup per row.
    QVERIFY(plan.exec(QStringLiteral("EXPLAIN QUERY PLAN SELECT id FROM symbol_master WHERE instr(name_norm, 'motor') "
                                     "> 0 ORDER BY +rank_weight DESC LIMIT 200")));
    detail.clear();
    while (plan.next())
        detail += plan.value(3).toString() + QLatin1Char(';');
    QVERIFY2(!detail.contains(QLatin1String("idx_symbol_master_weight")), qPrintable(detail));
}

void TstSymbolMasterRepo::migration_passes_inspect() {
    TsmDb t(false);
    {
        fincept::MigrationRunner first(t.db);
        const auto r = first.run();
        QVERIFY2(r.is_ok(), r.is_ok() ? "" : r.error().c_str());
    }
    fincept::MigrationRunner again(t.db);
    const auto ins = again.inspect();
    QVERIFY(ins.is_ok());
    QCOMPARE(int(ins.value().state), int(fincept::SchemaState::UpToDate));
    QCOMPARE(ins.value().current, 60);
    QVERIFY2(ins.value().missing.isEmpty(), qPrintable(ins.value().missing.join(", ")));
    QVERIFY(again.run().is_ok()); // nothing pending: a no-op
    QCOMPARE(again.current_version(), 60);
}

// ── Upsert / merge / lookups ────────────────────────────────────────────────

void TstSymbolMasterRepo::upsert_round_trip_and_idempotent() {
    TsmDb t;
    const Instrument full = tsm_full_record();
    Repo::UpsertOptions opt;
    opt.source = Repo::Source::Master;
    auto r = Repo::upsert(t.db, {full}, opt);
    QVERIFY(r.is_ok());
    QCOMPARE(r.value().inserted, 1);
    auto found = Repo::find_by_ids(t.db, {full.id});
    QVERIFY(found.is_ok());
    QCOMPARE(found.value().size(), 1);
    QCOMPARE(found.value().first(), full); // every field survives the round trip
    auto again = Repo::upsert(t.db, {full}, opt);
    QVERIFY(again.is_ok());
    QCOMPARE(again.value().unchanged, 1);
    QCOMPARE(again.value().changed(), 0);
    QCOMPARE(Repo::count(t.db).value(), 1);
    // Two copies of one instrument in one batch become one row.
    Instrument copy = full;
    copy.name = QStringLiteral("Tata Motors Passenger Vehicles Limited");
    auto dup = Repo::upsert(t.db, {full, copy}, opt);
    QVERIFY(dup.is_ok());
    QCOMPARE(Repo::count(t.db).value(), 1);
    QCOMPARE(Repo::find_by_ids(t.db, {full.id}).value().first().name, copy.name);
    // Empty batch and records without any id are fine.
    QVERIFY(Repo::upsert(t.db, {}, opt).is_ok());
    Instrument junk;
    junk.name = QStringLiteral("no symbol, no id");
    QCOMPARE(Repo::upsert(t.db, {junk}, opt).value().rejected, 1);
}

void TstSymbolMasterRepo::normalized_fills_id_and_drops_bad_isin() {
    Instrument in;
    in.symbol = QStringLiteral(" brk.b ");
    in.isin = QStringLiteral("US0378331006"); // bad check digit
    in.aliases = {QStringLiteral("Berkshire"), QStringLiteral("BERKSHIRE"), QStringLiteral(" ")};
    in.country = QStringLiteral("us");
    const Instrument n = Repo::normalized(in);
    QCOMPARE(n.symbol, QStringLiteral("BRK-B"));
    QCOMPARE(n.id, QStringLiteral("US:BRK-B"));
    QVERIFY(n.isin.isEmpty());
    QCOMPARE(n.aliases, QStringList{QStringLiteral("Berkshire")});
    QCOMPARE(n.country, QStringLiteral("US"));
    Instrument isin_only;
    isin_only.symbol = QStringLiteral("US0378331005"); // an ISIN is never a symbol / id
    QVERIFY(Repo::normalized(isin_only).id.isEmpty());
}

void TstSymbolMasterRepo::merge_precedence() {
    TsmDb t;
    Instrument seed = instrument_skeleton(QStringLiteral("RELIANCE.NS"));
    seed.name = QStringLiteral("Reliance Industries Limited");
    seed.rank_weight = 10;
    seed.primary_listing = true;
    seed.aliases = {QStringLiteral("RIL")};
    Repo::UpsertOptions so;
    so.source = Repo::Source::Seed;
    QVERIFY(Repo::upsert(t.db, {seed}, so).is_ok());

    // Learned (lower): fills the empty quote_type, never replaces the seed's name / weight.
    Instrument learned = instrument_skeleton(QStringLiteral("RELIANCE.NS"));
    learned.name = QStringLiteral("RELIANCE INDUSTRIES");
    learned.quote_type = QStringLiteral("EQUITY");
    learned.aliases = {QStringLiteral("Reliance")};
    Repo::UpsertOptions lo;
    lo.source = Repo::Source::Learned;
    QCOMPARE(Repo::upsert(t.db, {learned}, lo).value().updated, 1);
    Instrument got = Repo::find_by_ids(t.db, {seed.id}).value().first();
    QCOMPARE(got.name, seed.name);
    QCOMPARE(got.rank_weight, 10);
    QCOMPARE(got.quote_type, QStringLiteral("EQUITY"));
    QCOMPARE(got.aliases, QStringList({QStringLiteral("RIL"), QStringLiteral("Reliance")}));

    // Broker (lowest): adds its provider key only.
    Instrument broker = instrument_skeleton(QStringLiteral("RELIANCE.NS"));
    broker.name = QStringLiteral("RELIANCE INDS");
    broker.provider_symbols.insert(QStringLiteral("broker:zerodha"), QStringLiteral("NSE|RELIANCE|738561"));
    Repo::UpsertOptions bo;
    bo.source = Repo::Source::Broker;
    bo.source_detail = QStringLiteral("broker:zerodha");
    QVERIFY(Repo::upsert(t.db, {broker}, bo).is_ok());
    got = Repo::find_by_ids(t.db, {seed.id}).value().first();
    QCOMPARE(got.name, seed.name);
    QCOMPARE(got.provider_symbols.value(QStringLiteral("broker:zerodha")), QStringLiteral("NSE|RELIANCE|738561"));

    // Master (highest, curated): its values win, a status change included.
    Instrument master = got;
    master.name = QStringLiteral("Reliance Industries Ltd.");
    master.status = InstrumentStatus::Delisted;
    master.provider_symbols.clear();
    Repo::UpsertOptions mo;
    mo.source = Repo::Source::Master;
    QVERIFY(Repo::upsert(t.db, {master}, mo).is_ok());
    got = Repo::find_by_ids(t.db, {seed.id}).value().first();
    QCOMPARE(got.name, master.name);
    QCOMPARE(got.status, InstrumentStatus::Delisted);
    QVERIFY(got.provider_symbols.contains(QStringLiteral("broker:zerodha"))); // provider keys are kept

    // A learned "active" record does not resurrect it.
    QVERIFY(Repo::upsert(t.db, {learned}, lo).is_ok());
    QCOMPARE(Repo::find_by_ids(t.db, {seed.id}).value().first().status, InstrumentStatus::Delisted);
}

void TstSymbolMasterRepo::lookups_each_kind() {
    TsmDb t;
    Repo::UpsertOptions opt;
    opt.source = Repo::Source::Seed;
    opt.former_valid_to = fincept::services::builtin_symbol_seed_former_dates();
    QVERIFY(Repo::upsert(t.db, fincept::services::builtin_symbol_seed(), opt).is_ok());

    QCOMPARE(tsm_symbols(Repo::find_by_symbol(t.db, QStringLiteral("reliance.ns")).value()),
             QStringList{QStringLiteral("RELIANCE.NS")});
    QCOMPARE(tsm_symbols(Repo::find_by_symbol(t.db, QStringLiteral("BRK.B")).value()),
             QStringList{QStringLiteral("BRK-B")});
    const QStringList reliance = tsm_symbols(Repo::find_by_listing_code(t.db, QStringLiteral("RELIANCE")).value());
    QVERIFY(reliance.contains(QStringLiteral("RELIANCE.NS")) && reliance.contains(QStringLiteral("RELIANCE.BO")));
    QCOMPARE(tsm_symbols(Repo::find_by_listing_code(t.db, QStringLiteral("700")).value()),
             QStringList{QStringLiteral("0700.HK")});
    QCOMPARE(tsm_symbols(Repo::find_by_alias(t.db, QStringLiteral("nifty 50")).value()).value(0),
             QStringLiteral("^NSEI"));
    QCOMPARE(tsm_symbols(Repo::find_by_alias(t.db, QStringLiteral("S&P 500")).value()).value(0),
             QStringLiteral("^GSPC"));
    QCOMPARE(tsm_symbols(Repo::find_by_isin(t.db, QStringLiteral("INE002A01018")).value()),
             QStringList({QStringLiteral("RELIANCE.NS"), QStringLiteral("RELIANCE.BO")}));
    QCOMPARE(tsm_symbols(Repo::find_by_isin(t.db, QStringLiteral("us0378331005")).value()),
             QStringList{QStringLiteral("AAPL")});
    const QStringList tata = tsm_symbols(Repo::search_prefix(t.db, QStringLiteral("tata mot")).value());
    QVERIFY2(tata.contains(QStringLiteral("TMPV.NS")) && tata.contains(QStringLiteral("TMCV.NS")),
             qPrintable(tata.join(',')));
    const QStringList contains = tsm_symbols(Repo::search_name_contains(t.db, QStringLiteral("motors")).value());
    QVERIFY2(contains.contains(QStringLiteral("GM")) && contains.contains(QStringLiteral("TMPV.NS")),
             qPrintable(contains.join(',')));
    QVERIFY(Repo::search_name_contains(t.db, QStringLiteral("mo")).value().isEmpty()); // < 3 characters
    const QStringList etfs =
        tsm_symbols(Repo::search_prefix(t.db, QStringLiteral("S"), {AssetClass::Etf}, 50).value());
    QVERIFY2(etfs.contains(QStringLiteral("SPY")) && !etfs.contains(QStringLiteral("SAP.DE")), qPrintable(etfs.join(',')));

    // Reverse provider lookup.
    Instrument rel = Repo::find_by_ids(t.db, {QStringLiteral("XNSE:RELIANCE")}).value().first();
    rel.provider_symbols.insert(QStringLiteral("broker:zerodha"), QStringLiteral("NSE|RELIANCE|738561|RELIANCE-EQ"));
    Repo::UpsertOptions bo;
    bo.source = Repo::Source::Broker;
    QVERIFY(Repo::upsert(t.db, {rel}, bo).is_ok());
    QCOMPARE(tsm_symbols(Repo::find_by_provider_symbol(t.db, QStringLiteral("broker:zerodha"), QStringLiteral("RELIANCE"),
                                                       QStringLiteral("NSE"))
                             .value()),
             QStringList{QStringLiteral("RELIANCE.NS")});
    QVERIFY(Repo::find_by_provider_symbol(t.db, QStringLiteral("broker:zerodha"), QStringLiteral("RELIANCE"),
                                          QStringLiteral("BSE"))
                .value()
                .isEmpty());

    // Meta.
    QVERIFY(Repo::meta_get(t.db, QStringLiteral("nothing")).value().isEmpty());
    QVERIFY(Repo::meta_set(t.db, QStringLiteral("k"), QStringLiteral("v")).is_ok());
    QCOMPARE(Repo::meta_get(t.db, QStringLiteral("k")).value(), QStringLiteral("v"));
}

void TstSymbolMasterRepo::former_symbol_and_successor_chain() {
    TsmDb t;
    Repo::UpsertOptions opt;
    opt.source = Repo::Source::Seed;
    opt.former_valid_to = fincept::services::builtin_symbol_seed_former_dates();
    QVERIFY(Repo::upsert(t.db, fincept::services::builtin_symbol_seed(), opt).is_ok());

    // The former code resolves to the ACTIVE instrument, flagged; the old record follows.
    const auto hits = tsm_search(t.db, QStringLiteral("TATAMOTORS"), tsm_home("IN"));
    QVERIFY(hits.size() >= 2);
    QCOMPARE(hits.at(0).instrument.symbol, QStringLiteral("TMPV.NS"));
    QVERIFY(hits.at(0).via_former_symbol);
    QCOMPARE(hits.at(0).match, MatchKind::ExactAlias);
    QCOMPARE(hits.at(1).instrument.symbol, QStringLiteral("TATAMOTORS.NS"));
    QCOMPARE(hits.at(1).instrument.status, InstrumentStatus::Renamed);

    // A stored row holding the dead symbol: the record plus its current successor.
    const auto cur = Repo::resolve_current(t.db, QStringLiteral("TATAMOTORS.NS")).value();
    QCOMPARE(tsm_symbols(cur), QStringList({QStringLiteral("TATAMOTORS.NS"), QStringLiteral("TMPV.NS")}));
    QCOMPARE(tsm_symbols(Repo::resolve_current(t.db, QStringLiteral("XNSE:TMPV")).value()),
             QStringList{QStringLiteral("TMPV.NS")});
    QCOMPARE(tsm_symbols(Repo::predecessors(t.db, QStringLiteral("XNSE:TMPV")).value()),
             QStringList{QStringLiteral("TATAMOTORS.NS")});
    QCOMPARE(tsm_symbols(Repo::find_by_alias(t.db, QStringLiteral("tatamotors")).value()).value(0),
             QStringLiteral("TATAMOTORS.NS")); // its own listing code ranks before the former alias

    // The rename date is kept on the alias row.
    QSqlQuery q(t.db);
    QVERIFY(q.exec(QStringLiteral("SELECT valid_to FROM symbol_alias WHERE id = 'XNSE:TMPV' AND kind = 'former'")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("2025-10-24"));
    // ... and survives a rewrite of the record that does not carry it.
    q.finish();
    Instrument tmpv = Repo::find_by_ids(t.db, {QStringLiteral("XNSE:TMPV")}).value().first();
    tmpv.name = QStringLiteral("Tata Motors Passenger Vehicles Ltd");
    Repo::UpsertOptions mo;
    mo.source = Repo::Source::Master;
    QVERIFY(Repo::upsert(t.db, {tmpv}, mo).is_ok());
    QVERIFY(q.exec(QStringLiteral("SELECT valid_to FROM symbol_alias WHERE id = 'XNSE:TMPV' AND kind = 'former'")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("2025-10-24"));

    // FB -> META, RDS-A -> SHEL.
    QCOMPARE(tsm_search(t.db, QStringLiteral("FB"), tsm_home("US")).value(0).instrument.symbol, QStringLiteral("META"));
    QCOMPARE(tsm_search(t.db, QStringLiteral("RDS-A"), tsm_home("US")).value(0).instrument.symbol,
             QStringLiteral("SHEL"));
}

// ── Shared ranking vectors, through the index ───────────────────────────────

void TstSymbolMasterRepo::rank_vectors_through_index_data() {
    QTest::addColumn<QJsonObject>("v");
    for (const auto& x : tsm_vectors().value("rank").toArray()) {
        const QJsonObject v = x.toObject();
        QTest::newRow(qPrintable(v.value("id").toString())) << v;
    }
}

void TstSymbolMasterRepo::rank_vectors_through_index() {
    QFETCH(QJsonObject, v);
    TsmDb t;
    QVector<Instrument> insts;
    for (const auto& x : v.value("instruments").toArray())
        insts << Instrument::from_json(x.toObject());
    Repo::UpsertOptions opt;
    opt.source = Repo::Source::Master;
    const auto up = Repo::upsert(t.db, insts, opt);
    QVERIFY(up.is_ok());
    const RankContext ctx = tsm_context(v);
    const auto hits = tsm_search(t.db, v.value("query").toString(), ctx, 50);
    QStringList expected;
    for (const auto& x : v.value("expected").toArray())
        expected << x.toString();
    QCOMPARE(tsm_symbols(hits), expected);
    const QJsonObject match = v.value("match").toObject();
    for (auto it = match.constBegin(); it != match.constEnd(); ++it) {
        const auto hit = std::find_if(hits.cbegin(), hits.cend(),
                                      [&it](const RankedInstrument& h) { return h.instrument.symbol == it.key(); });
        QVERIFY2(hit != hits.cend(), qPrintable(it.key()));
        QCOMPARE(match_kind_slug(hit->match), it.value().toString());
    }
    if (v.contains("former")) {
        QStringList former;
        for (const auto& h : hits)
            if (h.via_former_symbol)
                former << h.instrument.symbol;
        QStringList want;
        for (const auto& x : v.value("former").toArray())
            want << x.toString();
        QCOMPARE(former, want);
    }
}

// ── Seed ────────────────────────────────────────────────────────────────────

void TstSymbolMasterRepo::seed_is_valid() {
    fincept::services::SymbolSeedStats st;
    const QVector<Instrument> seed = fincept::services::builtin_symbol_seed(&st);
    qInfo("seed: %d records = %d indices + %d futures + %d crypto + %d FX + %d companies/ETFs (%d table rows skipped)",
          int(seed.size()), st.indices, st.futures, st.crypto, st.fx, st.companies, st.skipped);
    QCOMPARE(int(seed.size()), st.total());
    QVERIFY(st.indices >= 20 && st.futures >= 10 && st.crypto >= 20 && st.fx >= 30 && st.companies >= 250);
    QSet<QString> ids;
    for (const Instrument& i : seed) {
        QVERIFY2(!i.id.isEmpty() && !i.symbol.isEmpty(), qPrintable(i.symbol));
        QVERIFY2(!ids.contains(i.id), qPrintable(i.id));
        ids.insert(i.id);
        QVERIFY2(!i.name.isEmpty(), qPrintable(i.symbol));
        QVERIFY2(i.asset_class != AssetClass::Unknown, qPrintable(i.symbol));
        QVERIFY2(canonical_symbol(i.symbol) == i.symbol, qPrintable(i.symbol)); // stored form is canonical
        QVERIFY2(instrument_id_for(i.symbol) == i.id, qPrintable(i.symbol));
        if (!i.isin.isEmpty())
            QVERIFY2(is_valid_isin(i.isin), qPrintable(i.symbol + QLatin1Char(' ') + i.isin));
        if (i.status == InstrumentStatus::Renamed)
            QVERIFY2(!i.successor_id.isEmpty(), qPrintable(i.symbol));
    }
    // Every successor exists in the seed.
    for (const Instrument& i : seed)
        if (!i.successor_id.isEmpty())
            QVERIFY2(ids.contains(i.successor_id), qPrintable(i.successor_id));
    QVERIFY(fincept::services::builtin_symbol_seed_former_dates().contains(QStringLiteral("XNSE:TMPV|TATAMOTORS")));
    QVERIFY(ids.contains(QStringLiteral("CRYPTO:BTC-USD")));
    QVERIFY(ids.contains(QStringLiteral("FX:EURUSD")));
    QVERIFY(ids.contains(QStringLiteral("FX:USDINR")));
    QVERIFY(ids.contains(QStringLiteral("IDX:NSEI")));
    QVERIFY(ids.contains(QStringLiteral("FUT:GC")));
}

void TstSymbolMasterRepo::seed_owner_critical_searches() {
    TsmDb t;
    Repo::UpsertOptions opt;
    opt.source = Repo::Source::Seed;
    opt.former_valid_to = fincept::services::builtin_symbol_seed_former_dates();
    QElapsedTimer timer;
    timer.start();
    const auto up = Repo::upsert(t.db, fincept::services::builtin_symbol_seed(), opt);
    QVERIFY(up.is_ok());
    qInfo("seed upsert: %d records in %lld ms", up.value().inserted, timer.elapsed());
    const auto again = Repo::upsert(t.db, fincept::services::builtin_symbol_seed(), opt);
    QVERIFY(again.is_ok());
    QVERIFY2(again.value().changed() == 0, qPrintable(again.value().changed_ids.mid(0, 10).join(',')));

    struct Case {
        const char* text;
        const char* home;
        const char* first;
    };
    const Case cases[] = {
        {"BTC", "US", "BTC-USD"},          {"bitcoin", "", "BTC-USD"},      {"apple", "US", "AAPL"},
        {"Apple", "IN", "AAPL"},           {"AAPL", "", "AAPL"},            {"RELIANCE", "IN", "RELIANCE.NS"},
        {"reliance", "IN", "RELIANCE.NS"}, {"RELIANCE.BO", "IN", "RELIANCE.BO"},
        {"NSE:RELIANCE-EQ", "", "RELIANCE.NS"},
        {"TATAMOTORS", "IN", "TMPV.NS"},   {"tata motors", "IN", "TMPV.NS"}, {"NIFTY", "IN", "^NSEI"},
        {"NIFTY 50", "", "^NSEI"},         {"SENSEX", "IN", "^BSESN"},      {"S&P 500", "US", "^GSPC"},
        {"gold", "US", "GC=F"},            {"crude", "", "CL=F"},           {"EUR/USD", "", "EURUSD=X"},
        {"EURUSD", "", "EURUSD=X"},        {"USDINR", "IN", "INR=X"},       {"US0378331005", "US", "AAPL"},
        {"INE002A01018", "IN", "RELIANCE.NS"},
        {"relaince", "IN", "RELIANCE.NS"}, {"microsft", "US", "MSFT"},      {"SAP", "", "SAP.DE"},
        {"SAP", "US", "SAP"},              {"INFY", "IN", "INFY.NS"},       {"INFY", "US", "INFY"},
        {"700", "", "0700.HK"},            {"tencent", "", "0700.HK"},
        {"\xe8\x85\xbe\xe8\xae\xaf", "", "0700.HK"},
        {"Nestl\xc3\xa9", "", "NESN.SW"},  {"BRK.B", "US", "BRK-B"},        {"berkshire", "", "BRK-B"},
        {"M&M", "IN", "M&M.NS"},           {"FB", "US", "META"},            {"ETH", "", "ETH-USD"},
        {"SPY", "US", "SPY"},              {"google", "US", "GOOGL"},
    };
    for (const Case& c : cases) {
        const auto hits = tsm_search(t.db, QString::fromUtf8(c.text), tsm_home(c.home));
        const QString first = hits.isEmpty() ? QString() : hits.first().instrument.symbol;
        QVERIFY2(first == QLatin1String(c.first),
                 qPrintable(QStringLiteral("'%1' (home %2): got [%3], want %4")
                                .arg(QString::fromUtf8(c.text), QLatin1String(c.home),
                                     tsm_symbols(hits).mid(0, 6).join(','), QLatin1String(c.first))));
    }
    // "tata motors": TMPV (its former symbol) and TMCV both before the renamed old record.
    const QStringList tata = tsm_symbols(tsm_search(t.db, QStringLiteral("tata motors"), tsm_home("IN")));
    const qsizetype old_at = tata.indexOf(QStringLiteral("TATAMOTORS.NS"));
    QVERIFY2(old_at > tata.indexOf(QStringLiteral("TMPV.NS")) && old_at > tata.indexOf(QStringLiteral("TMCV.NS")) &&
                 tata.indexOf(QStringLiteral("TMCV.NS")) >= 0,
             qPrintable(tata.join(',')));
}

// ── Master delta ────────────────────────────────────────────────────────────

void TstSymbolMasterRepo::master_delta_apply_and_cursor() {
    TsmDb t;
    Instrument old = instrument_skeleton(QStringLiteral("OLDCO.NS"));
    old.name = QStringLiteral("Old Co Ltd");
    Instrument tmcv = instrument_skeleton(QStringLiteral("NEWCO.NS"));
    tmcv.name = QStringLiteral("New Co Ltd");
    Instrument gone = instrument_skeleton(QStringLiteral("GONE.NS"));
    gone.name = QStringLiteral("Gone Ltd");
    QJsonObject f_old = old.to_json();
    QJsonObject f_new = tmcv.to_json();
    f_new.insert("former_valid_to", QJsonObject{{"PRE", "2024-01-31"}});
    f_new.insert("former_symbols", QJsonArray{"PRE"});
    const QJsonObject page1{
        {"next", 10},
        {"version", "2026-11-16.1"},
        {"changes", QJsonArray{QJsonObject{{"op", "upsert"}, {"instrument", f_old}},
                               QJsonObject{{"op", "upsert"}, {"instrument", f_new}},
                               QJsonObject{{"op", "upsert"}, {"instrument", gone.to_json()}},
                               QJsonObject{{"op", "bogus"}}}},
    };
    auto r1 = Repo::apply_master_delta(t.db, page1);
    QVERIFY2(r1.is_ok(), r1.is_ok() ? "" : r1.error().c_str());
    QCOMPARE(r1.value().upserted, 3);
    QCOMPARE(r1.value().rejected, 1);
    QCOMPARE(r1.value().cursor, QStringLiteral("10"));
    QCOMPARE(Repo::meta_get(t.db, QLatin1String(Repo::kMetaMasterCursor)).value(), QStringLiteral("10"));
    QCOMPARE(Repo::meta_get(t.db, QLatin1String(Repo::kMetaMasterVersion)).value(), QStringLiteral("2026-11-16.1"));

    // Out of order: refused, nothing applied.
    const QJsonObject stale{{"since", "7"}, {"next", "12"},
                            {"changes", QJsonArray{QJsonObject{{"op", "delist"}, {"id", gone.id}}}}};
    QVERIFY(Repo::apply_master_delta(t.db, stale).is_err());
    QCOMPARE(Repo::find_by_ids(t.db, {gone.id}).value().first().status, InstrumentStatus::Active);
    QVERIFY(Repo::apply_master_delta(t.db, QJsonObject{{"changes", QJsonArray{}}}).is_err()); // no "next"

    // In order: rename + delist.
    const QJsonObject page2{
        {"since", "10"},
        {"next", "11"},
        {"has_more", true},
        {"changes", QJsonArray{QJsonObject{{"op", "rename"}, {"id", old.id}, {"successor_id", tmcv.id},
                                           {"valid_to", "2026-11-01"}},
                               QJsonObject{{"op", "delist"}, {"id", gone.id}},
                               QJsonObject{{"op", "delist"}, {"id", "XNSE:NOPE"}}}},
    };
    auto r2 = Repo::apply_master_delta(t.db, page2);
    QVERIFY2(r2.is_ok(), r2.is_ok() ? "" : r2.error().c_str());
    QCOMPARE(r2.value().renamed, 1);
    QCOMPARE(r2.value().delisted, 1);
    QCOMPARE(r2.value().rejected, 1);
    QVERIFY(r2.value().has_more);
    QCOMPARE(Repo::meta_get(t.db, QLatin1String(Repo::kMetaMasterCursor)).value(), QStringLiteral("11"));
    const Instrument o = Repo::find_by_ids(t.db, {old.id}).value().first();
    QCOMPARE(o.status, InstrumentStatus::Renamed);
    QCOMPARE(o.successor_id, tmcv.id);
    const Instrument n = Repo::find_by_ids(t.db, {tmcv.id}).value().first();
    QCOMPARE(n.former_symbols, QStringList({QStringLiteral("PRE"), QStringLiteral("OLDCO")}));
    QCOMPARE(Repo::find_by_ids(t.db, {gone.id}).value().first().status, InstrumentStatus::Delisted);
    const auto hits = tsm_search(t.db, QStringLiteral("OLDCO"), tsm_home("IN"));
    QCOMPARE(tsm_symbols(hits).value(0), QStringLiteral("NEWCO.NS"));
    QVERIFY(hits.value(0).via_former_symbol);
    QSqlQuery q(t.db);
    QVERIFY(q.exec(QStringLiteral("SELECT alias_norm, valid_to FROM symbol_alias WHERE id = '%1' AND kind = 'former' "
                                  "ORDER BY alias_norm")
                       .arg(tmcv.id)));
    QStringList dates;
    while (q.next())
        dates << q.value(0).toString() + QLatin1Char('=') + q.value(1).toString();
    QCOMPARE(dates, QStringList({QStringLiteral("OLDCO=2026-11-01"), QStringLiteral("PRE=2024-01-31")}));
}

// ── Broker masters ──────────────────────────────────────────────────────────

void TstSymbolMasterRepo::broker_master_import() {
    TsmDb t;
    QVERIFY(Repo::broker_master_state(t.db).value().isEmpty()); // no instruments table: nothing
    QSqlQuery q(t.db);
    QVERIFY(q.exec(QStringLiteral(
        "CREATE TABLE instruments (id INTEGER PRIMARY KEY AUTOINCREMENT, instrument_token INTEGER NOT NULL, "
        "exchange_token INTEGER NOT NULL DEFAULT 0, symbol TEXT NOT NULL, brsymbol TEXT NOT NULL, "
        "name TEXT NOT NULL DEFAULT '', exchange TEXT NOT NULL, brexchange TEXT NOT NULL DEFAULT '', "
        "expiry TEXT NOT NULL DEFAULT '', strike REAL NOT NULL DEFAULT 0, lot_size INTEGER NOT NULL DEFAULT 1, "
        "instrument_type TEXT NOT NULL DEFAULT 'EQ', tick_size REAL NOT NULL DEFAULT 0.05, broker_id TEXT NOT NULL, "
        "updated_at TEXT DEFAULT (datetime('now')), broker_token TEXT NOT NULL DEFAULT '')")));
    const char* rows[][6] = {
        // symbol, brsymbol, name, exchange, type, token
        {"RELIANCE", "RELIANCE", "RELIANCE INDUSTRIES", "NSE", "EQ", "738561"},
        {"RELIANCE", "RELIANCE", "RELIANCE INDUSTRIES", "BSE", "EQ", "128083204"},
        {"NIFTY", "NIFTY 50", "NIFTY 50", "NSE_INDEX", "INDEX", "256265"},
        {"NIFTY26JULFUT", "NIFTY26JULFUT", "NIFTY", "NFO", "FUT", "13238786"},
        {"BAJAJ-AUTO", "BAJAJ-AUTO", "BAJAJ AUTO", "NSE", "EQ", "4267265"},
    };
    for (const auto& r : rows) {
        QSqlQuery ins(t.db);
        ins.prepare(QStringLiteral("INSERT INTO instruments (instrument_token, symbol, brsymbol, name, exchange, "
                                   "instrument_type, broker_id) VALUES (?, ?, ?, ?, ?, ?, 'zerodha')"));
        ins.addBindValue(QString::fromLatin1(r[5]).toLongLong());
        ins.addBindValue(QString::fromLatin1(r[0]));
        ins.addBindValue(QString::fromLatin1(r[1]));
        ins.addBindValue(QString::fromLatin1(r[2]));
        ins.addBindValue(QString::fromLatin1(r[3]));
        ins.addBindValue(QString::fromLatin1(r[4]));
        QVERIFY2(ins.exec(), qPrintable(ins.lastError().text()));
    }
    const auto state = Repo::broker_master_state(t.db).value();
    QCOMPARE(state.size(), 1);
    QCOMPARE(state.first().broker_id, QStringLiteral("zerodha"));
    QVERIFY(!state.first().last_updated.isEmpty());

    const auto page = Repo::read_broker_rows(t.db, QStringLiteral("zerodha"), 0, 100).value();
    QCOMPARE(page.size(), 4); // the FUT row is not a cash-market row
    QVector<Instrument> batch;
    for (const auto& row : page)
        batch << Repo::instrument_from_broker_row(QStringLiteral("zerodha"), row);
    QCOMPARE(tsm_symbols(batch),
             QStringList({QStringLiteral("RELIANCE.NS"), QStringLiteral("RELIANCE.BO"), QStringLiteral("^NSEI"),
                          QStringLiteral("BAJAJ-AUTO.NS")}));
    QCOMPARE(batch.at(0).provider_symbols.value(QStringLiteral("broker:zerodha")), QStringLiteral("NSE|RELIANCE|738561"));
    QCOMPARE(batch.at(2).provider_symbols.value(QStringLiteral("broker:zerodha")),
             QStringLiteral("NSE_INDEX|NIFTY|256265|NIFTY 50"));
    QCOMPARE(batch.at(2).name, QStringLiteral("NIFTY 50"));
    // A derivatives row has no Yahoo form: invalid.
    Repo::BrokerRow fut;
    fut.symbol = QStringLiteral("NIFTY26JULFUT");
    fut.exchange = QStringLiteral("NFO");
    fut.instrument_type = QStringLiteral("FUT");
    QVERIFY(Repo::instrument_from_broker_row(QStringLiteral("zerodha"), fut).id.isEmpty());

    Repo::UpsertOptions bo;
    bo.source = Repo::Source::Broker;
    bo.source_detail = QStringLiteral("broker:zerodha");
    QCOMPARE(Repo::upsert(t.db, batch, bo).value().inserted, 4);
    QCOMPARE(tsm_symbols(Repo::find_by_provider_symbol(t.db, QStringLiteral("broker:zerodha"),
                                                       QStringLiteral("BAJAJ-AUTO"), QStringLiteral("NSE"))
                             .value()),
             QStringList{QStringLiteral("BAJAJ-AUTO.NS")});
    // Broker-only instruments are searchable, NSE before BSE for a home-IN user.
    QCOMPARE(tsm_symbols(tsm_search(t.db, QStringLiteral("RELIANCE"), tsm_home("IN"))),
             QStringList({QStringLiteral("RELIANCE.NS"), QStringLiteral("RELIANCE.BO")}));
    QCOMPARE(tsm_symbols(tsm_search(t.db, QStringLiteral("BAJAJ_AUTO"), tsm_home("IN"))).value(0),
             QStringLiteral("BAJAJ-AUTO.NS"));
    // The trading table was only read.
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM instruments")) && q.next());
    QCOMPARE(q.value(0).toInt(), 5);
}

// ── 50k rows ────────────────────────────────────────────────────────────────

void TstSymbolMasterRepo::index_50k_rows_stays_fast() {
    TsmDb t;
    Repo::UpsertOptions so;
    so.source = Repo::Source::Seed;
    so.former_valid_to = fincept::services::builtin_symbol_seed_former_dates();
    QVERIFY(Repo::upsert(t.db, fincept::services::builtin_symbol_seed(), so).is_ok());

    const QVector<Instrument> synthetic = tsm_synthetic(50000);
    Repo::UpsertOptions bo;
    bo.source = Repo::Source::Broker;
    QElapsedTimer timer;
    timer.start();
    const auto up = Repo::upsert(t.db, synthetic, bo);
    const qint64 upsert_ms = timer.elapsed();
    QVERIFY(up.is_ok());
    const int total = Repo::count(t.db).value();
    QSqlQuery cq(t.db);
    cq.exec(QStringLiteral("SELECT COUNT(*) FROM symbol_alias"));
    cq.next();
    qInfo("50k upsert: %d synthetic rows (%d inserted, %d merged) in %lld ms; index now %d records, %d alias rows",
          int(synthetic.size()), up.value().inserted, up.value().updated, upsert_ms, total, cq.value(0).toInt());
    timer.restart();
    QCOMPARE(Repo::upsert(t.db, synthetic, bo).value().changed(), 0);
    qInfo("50k re-upsert (all unchanged): %lld ms", timer.elapsed());

    struct Q {
        const char* text;
        const char* home;
        const char* first; // "" = only timed
    };
    const Q queries[] = {
        {"RELIANCE", "IN", "RELIANCE.NS"}, {"apple", "US", "AAPL"},     {"BTC", "US", "BTC-USD"},
        {"TATAMOTORS", "IN", "TMPV.NS"},   {"NIFTY 50", "", "^NSEI"},   {"US0378331005", "US", "AAPL"},
        {"relaince", "IN", "RELIANCE.NS"}, {"microsft", "US", "MSFT"},  {"EUR/USD", "", "EURUSD=X"},
        {"700", "", "0700.HK"},            {"A", "US", "AAPL"},         {"X", "", ""},
        {"Q", "", ""},                     {"S", "", ""},               {"RE", "IN", ""},
        {"XQ", "", ""},                    {"tata mot", "IN", ""},      {"motors", "", ""},
        {"holdings", "", ""},              {"global hold", "", ""},     {"zzzzqqq", "", ""},
    };
    double worst = 0;
    for (const Q& qd : queries) {
        const QString text = QString::fromUtf8(qd.text);
        const RankContext ctx = tsm_home(qd.home);
        Repo::GatherStats stats;
        QVector<RankedInstrument> hits;
        const double ms = tsm_median_ms([&]() { hits = tsm_search(t.db, text, ctx, 12, &stats); });
        worst = std::max(worst, ms);
        qInfo("search %-14s %7.2f ms  hits %2d  candidates %4d (sym %d, key %d, keypfx %d, namepfx %d, contains %d, "
              "fuzzy %d, stage2 %s)  first %s",
              qPrintable(QLatin1Char('\'') + text + QLatin1Char('\'')), ms, int(hits.size()), stats.loaded,
              stats.by_symbol, stats.by_exact_key, stats.by_key_prefix, stats.by_name_prefix, stats.by_contains,
              stats.by_fuzzy, stats.stage2 ? "yes" : "no",
              qPrintable(hits.isEmpty() ? QStringLiteral("-") : hits.first().instrument.symbol));
        if (*qd.first)
            QVERIFY2(!hits.isEmpty() && hits.first().instrument.symbol == QLatin1String(qd.first),
                     qPrintable(text + QStringLiteral(": ") + tsm_symbols(hits).mid(0, 5).join(',')));
    }
    qInfo("worst median search time over 50k rows: %.2f ms", worst);
    QVERIFY2(worst < 250.0, "a search over 50k rows took longer than 250 ms");
}

// ── Service ─────────────────────────────────────────────────────────────────

void TstSymbolMasterRepo::service_end_to_end() {
    auto& database = fincept::Database::instance();
    QVERIFY(service_dir_.isValid());
    const auto opened = database.open(service_dir_.filePath(QStringLiteral("fincept.db")));
    QVERIFY2(opened.is_ok(), opened.is_ok() ? "" : opened.error().c_str());

    // A broker master is present before the index starts.
    {
        QSqlQuery q(database.raw_db());
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE instruments (id INTEGER PRIMARY KEY AUTOINCREMENT, instrument_token INTEGER NOT NULL, "
            "exchange_token INTEGER NOT NULL DEFAULT 0, symbol TEXT NOT NULL, brsymbol TEXT NOT NULL, "
            "name TEXT NOT NULL DEFAULT '', exchange TEXT NOT NULL, brexchange TEXT NOT NULL DEFAULT '', "
            "expiry TEXT NOT NULL DEFAULT '', strike REAL NOT NULL DEFAULT 0, lot_size INTEGER NOT NULL DEFAULT 1, "
            "instrument_type TEXT NOT NULL DEFAULT 'EQ', tick_size REAL NOT NULL DEFAULT 0.05, broker_id TEXT NOT NULL, "
            "updated_at TEXT DEFAULT (datetime('now')), broker_token TEXT NOT NULL DEFAULT '')")));
        QVERIFY(q.exec(QStringLiteral("INSERT INTO instruments (instrument_token, symbol, brsymbol, name, exchange, "
                                      "instrument_type, broker_id) VALUES (738561, 'RELIANCE', 'RELIANCE', "
                                      "'RELIANCE INDUSTRIES', 'NSE', 'EQ', 'zerodha')")));
        QVERIFY(q.exec(QStringLiteral("INSERT INTO instruments (instrument_token, symbol, brsymbol, name, exchange, "
                                      "instrument_type, broker_id) VALUES (1270529, 'ZYDUSLIFE', 'ZYDUSLIFE', "
                                      "'ZYDUS LIFESCIENCES', 'NSE', 'EQ', 'zerodha')")));
    }

    InstrumentIndex& index = InstrumentIndex::instance();
    QCOMPARE(index.thread(), QCoreApplication::instance()->thread());
    QSignalSpy seeded(&index, &InstrumentIndex::seed_applied);
    QSignalSpy imported(&index, &InstrumentIndex::broker_import_finished);
    QSignalSpy errors(&index, &InstrumentIndex::index_error);
    QElapsedTimer seed_timer;
    seed_timer.start();
    index.start();
    QTRY_COMPARE_WITH_TIMEOUT(seeded.count(), 1, 20000);
    qInfo("service: seed applied asynchronously after %lld ms (%d changed)", seed_timer.elapsed(),
          seeded.first().at(0).toInt());
    QTRY_COMPARE_WITH_TIMEOUT(imported.count(), 1, 20000);
    QCOMPARE(imported.first().at(0).toString(), QStringLiteral("zerodha"));

    // Search: delivered on the context's (this) thread with the owner-critical order.
    QObject receiver;
    InstrumentIndex::SearchResult got;
    bool delivered = false;
    InstrumentIndex::SearchRequest req;
    req.text = QStringLiteral("BTC");
    req.ctx.home_country = QStringLiteral("US");
    const quint64 id = index.search(req, &receiver, [&](const InstrumentIndex::SearchResult& r) {
        QCOMPARE(QThread::currentThread(), receiver.thread());
        got = r;
        delivered = true;
    });
    QVERIFY(id > 0);
    QTRY_VERIFY_WITH_TIMEOUT(delivered, 10000);
    QCOMPARE(got.request_id, id);
    QVERIFY(got.error.isEmpty());
    QCOMPARE(got.hits.value(0).instrument.symbol, QStringLiteral("BTC-USD"));

    // Stale guard: only the newest request of a channel is delivered; a cancelled one never is.
    QStringList delivered_texts;
    for (const char* text : {"R", "RE", "REL", "RELIANCE"}) {
        InstrumentIndex::SearchRequest r;
        r.text = QString::fromLatin1(text);
        r.ctx.home_country = QStringLiteral("IN");
        r.channel = QStringLiteral("box");
        index.search(r, &receiver, [&delivered_texts, t = r.text](const InstrumentIndex::SearchResult& res) {
            delivered_texts << t + QLatin1Char('=') + res.hits.value(0).instrument.symbol;
        });
    }
    InstrumentIndex::SearchRequest cancelled;
    cancelled.text = QStringLiteral("apple");
    bool cancelled_delivered = false;
    const quint64 cid = index.search(cancelled, &receiver, [&](const InstrumentIndex::SearchResult&) {
        cancelled_delivered = true;
    });
    index.cancel(cid);
    QTRY_COMPARE_WITH_TIMEOUT(delivered_texts.size(), 1, 10000);
    QTest::qWait(300);
    QCOMPARE(delivered_texts, QStringList{QStringLiteral("RELIANCE=RELIANCE.NS")});
    QVERIFY(!cancelled_delivered);

    // A destroyed context drops the delivery.
    {
        auto* gone = new QObject;
        bool called = false;
        InstrumentIndex::SearchRequest r;
        r.text = QStringLiteral("SAP");
        index.search(r, gone, [&called](const InstrumentIndex::SearchResult&) { called = true; });
        delete gone;
        QTest::qWait(500);
        QVERIFY(!called);
    }

    // Lookups: renamed record + successor; the broker import's provider key.
    auto lookup = [&](InstrumentIndex::LookupRequest lr) {
        QVector<Instrument> found;
        bool done = false;
        index.lookup(lr, &receiver, [&](const InstrumentIndex::LookupResult& r) {
            found = r.found;
            done = true;
        });
        if (!QTest::qWaitFor([&done]() { return done; }, 10000))
            qWarning("lookup timed out");
        return found;
    };
    InstrumentIndex::LookupRequest cur;
    cur.kind = InstrumentIndex::LookupKind::Current;
    cur.key = QStringLiteral("TATAMOTORS.NS");
    QCOMPARE(tsm_symbols(lookup(cur)), QStringList({QStringLiteral("TATAMOTORS.NS"), QStringLiteral("TMPV.NS")}));
    InstrumentIndex::LookupRequest prov;
    prov.kind = InstrumentIndex::LookupKind::ProviderSymbol;
    prov.provider = QStringLiteral("broker:zerodha");
    prov.key = QStringLiteral("ZYDUSLIFE");
    prov.exchange = QStringLiteral("NSE");
    QCOMPARE(tsm_symbols(lookup(prov)), QStringList{QStringLiteral("ZYDUSLIFE.NS")});
    InstrumentIndex::LookupRequest rel;
    rel.kind = InstrumentIndex::LookupKind::Symbol;
    rel.key = QStringLiteral("RELIANCE.NS");
    const auto rel_found = lookup(rel);
    QCOMPARE(rel_found.size(), 1);
    QCOMPARE(rel_found.first().name, QStringLiteral("Reliance Industries Limited")); // seed name kept
    QVERIFY(rel_found.first().provider_symbols.contains(QStringLiteral("broker:zerodha")));

    // Learning: fills what the seed lacks (the US venue), adds unknown instruments.
    QSignalSpy changed(&index, &InstrumentIndex::index_changed);
    index.learn_from_quote(QStringLiteral("AAPL"), QStringLiteral("Apple Inc"), QStringLiteral("NMS"),
                           QStringLiteral("EQUITY"), QStringLiteral("USD"));
    index.learn_from_quote(QStringLiteral("PLUG"), QStringLiteral("Plug Power Inc."), QStringLiteral("NCM"),
                           QStringLiteral("EQUITY"), QStringLiteral("USD"));
    index.learn_from_quote(QStringLiteral("US0378331005"), QStringLiteral("Apple"), {}, {}, {}); // ignored
    auto learned = [&changed]() {
        for (const auto& args : changed)
            if (args.at(1).toStringList().contains(QStringLiteral("US:AAPL")))
                return true;
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(learned(), 10000);
    InstrumentIndex::LookupRequest aapl;
    aapl.kind = InstrumentIndex::LookupKind::Symbol;
    aapl.key = QStringLiteral("AAPL");
    const auto a = lookup(aapl);
    QCOMPARE(a.size(), 1);
    QCOMPARE(a.first().name, QStringLiteral("Apple Inc.")); // seed wins
    QCOMPARE(a.first().rank_weight, 10);
    QVERIFY2(!a.first().venue.isEmpty(), "the learned Yahoo exchange code should fill the venue");
    InstrumentIndex::LookupRequest plug;
    plug.kind = InstrumentIndex::LookupKind::Symbol;
    plug.key = QStringLiteral("PLUG");
    QCOMPARE(lookup(plug).value(0).name, QStringLiteral("Plug Power Inc."));

    // Master delta through the service + stored cursor.
    bool delta_ok = false;
    QString delta_cursor;
    Instrument extra = instrument_skeleton(QStringLiteral("NEWLIST.NS"));
    extra.name = QStringLiteral("New Listing Ltd");
    index.apply_master_delta(QJsonObject{{"next", "42"},
                                         {"changes", QJsonArray{QJsonObject{{"op", "upsert"},
                                                                            {"instrument", extra.to_json()}}}}},
                             &receiver, [&](bool ok, const QString& c, bool) {
                                 delta_ok = ok;
                                 delta_cursor = c;
                             });
    QTRY_VERIFY_WITH_TIMEOUT(!delta_cursor.isEmpty(), 10000);
    QVERIFY(delta_ok);
    QCOMPARE(delta_cursor, QStringLiteral("42"));
    QString read_back;
    index.read_sync_cursor(&receiver, [&read_back](const QString& c) { read_back = c; });
    QTRY_COMPARE_WITH_TIMEOUT(read_back, QStringLiteral("42"), 10000);

    QCOMPARE(errors.count(), 0);
}

void TstSymbolMasterRepo::cleanupTestCase() {
    fincept::SymbolMasterRepository::set_forbidden_thread(nullptr);
    fincept::Database::instance().close();
}

QTEST_GUILESS_MAIN(TstSymbolMasterRepo)
#include "tst_symbol_master_repo.moc"
