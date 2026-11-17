// Unit tests for the Derivatives position book (Qt Core + Qt Sql):
//   src/storage/sqlite/migrations/v065_deriv_book   — idempotent (applied twice), inspect() up to date
//   src/storage/repositories/DerivBookRepository    — positions and saved strategies round trip on an
//                                                      in-memory database
//   src/services/options/BookTypes.h                — legs JSON codec
//   src/services/options/BookMath                   — marks, P&L, Greeks and the aggregation (totals,
//                                                      exposure by underlying, expiry ladder)
// Reference values: Black-Scholes-Merton (Hull, Options, Futures and Other Derivatives, ch. 15;
// Haug 2007 §1.1), recomputed independently in Python/scipy (D:\ft-scratch\deriv2\tools\bookref.py):
//   call S=100 K=105 T=30/365 r=4% q=0 σ=25%: price 1.1676993367, delta 0.2745770958,
//   gamma 0.0465195947, vega 9.5588208298 (per 1.00 σ). Two contracts × 100: value 233.539867,
//   delta 54.915419 shares, dollar delta 5491.541917, vega 19.117642 per vol point,
//   dollar gamma (1 % move) 930.391894.
// The chain / curve fixtures are the recorded ones in tests/data/derivatives/.

#include "services/options/BookMath.h"
#include "services/options/BookTypes.h"
#include "services/options/DerivMarketData.h"
#include "services/options/OptionModels.h"
#include "storage/repositories/DerivBookRepository.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>

using namespace fincept;
using namespace fincept::services::options;
namespace bk = fincept::services::options::book;
namespace mk = fincept::services::options::market;

namespace {

bool tdb_near(double a, double b, double tol) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol;
}

const Migration* tdb_migration(int version) {
    for (const Migration& m : MigrationRunner::all_migrations())
        if (m.version == version)
            return &m;
    return nullptr;
}

QStringList tdb_columns(QSqlDatabase& db, const QString& table) {
    QStringList out;
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table));
    while (q.next())
        out << q.value(1).toString();
    return out;
}

QByteArray tdb_fixture(const char* name) {
    QString root = qEnvironmentVariable("FINCEPT_DERIV_TEST_DATA");
    if (root.isEmpty())
        root = QFINDTESTDATA("data/derivatives");
    QFile f(root + QLatin1Char('/') + QLatin1String(name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

bk::BookLeg tdb_option(bool call, double qty, double K, const QDate& expiry, double entry, double iv = 0.25) {
    bk::BookLeg l;
    l.type = call ? bk::LegType::Call : bk::LegType::Put;
    l.quantity = qty;
    l.strike = K;
    l.expiry = expiry;
    l.entry_price = entry;
    l.entry_iv = iv;
    return l;
}

bk::BookPosition tdb_position(const QString& kind, const QString& underlying, double mult, QVector<bk::BookLeg> legs) {
    bk::BookPosition p;
    p.kind = kind;
    p.name = kind + QLatin1Char(' ') + underlying;
    p.underlying = underlying;
    p.multiplier = mult;
    p.entry_date = QDate(2026, 11, 2);
    p.legs = std::move(legs);
    return p;
}

} // namespace

class TestDerivBook : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        register_migration_v065();
        QVERIFY(tdb_migration(65) != nullptr);
        db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("tst_deriv_book_mem"));
        db_.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(db_.open());
    }

    // ── Migration ────────────────────────────────────────────────────────────
    void migration_applies_twice() {
        const Migration* m = tdb_migration(65);
        QVERIFY(m->apply(db_).is_ok());
        QVERIFY(m->apply(db_).is_ok()); // IF NOT EXISTS: the second run changes nothing
        const QStringList pos = tdb_columns(db_, QStringLiteral("deriv_positions"));
        for (const char* c : {"id", "book", "kind", "name", "underlying", "currency", "multiplier", "entry_date",
                              "legs_json", "strategy_id", "notes", "created_at", "updated_at"})
            QVERIFY2(pos.contains(QString::fromLatin1(c)), c);
        const QStringList st = tdb_columns(db_, QStringLiteral("deriv_strategies"));
        for (const char* c : {"id", "name", "underlying", "template_id", "currency", "multiplier", "legs_json", "notes",
                              "created_at", "updated_at"})
            QVERIFY2(st.contains(QString::fromLatin1(c)), c);
    }

    void runner_then_inspect_up_to_date() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            QSqlDatabase f = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("tst_deriv_book_file"));
            f.setDatabaseName(dir.filePath(QStringLiteral("book.db")));
            QVERIFY(f.open());
            MigrationRunner runner(f);
            const auto ran = runner.run();
            QVERIFY2(ran.is_ok(), ran.is_ok() ? "" : ran.error().c_str());
            QCOMPARE(runner.current_version(), 65);
            // Applied once more on the migrated file: still fine.
            QVERIFY(tdb_migration(65)->apply(f).is_ok());
            MigrationRunner again(f);
            const auto inspected = again.inspect();
            QVERIFY2(inspected.is_ok(), inspected.is_ok() ? "" : inspected.error().c_str());
            QCOMPARE(inspected.value().current, 65);
            QCOMPARE(inspected.value().target, 65);
            QCOMPARE(static_cast<int>(inspected.value().state), static_cast<int>(SchemaState::UpToDate));
            QVERIFY2(inspected.value().missing.isEmpty(), qPrintable(inspected.value().missing.join(QStringLiteral(", "))));
            f.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("tst_deriv_book_file"));
    }

    // ── Legs JSON ────────────────────────────────────────────────────────────
    void legs_json_round_trip() {
        bk::BookLeg sw;
        sw.type = bk::LegType::Swap;
        sw.quantity = 1;
        sw.swap_start = 0.5;
        sw.swap_tenor = 5;
        sw.swap_rate = 0.0415;
        sw.swap_freq = 2;
        sw.notional = 1e7;
        sw.payer = false;
        bk::BookLeg fut;
        fut.type = bk::LegType::Future;
        fut.quantity = -2;
        fut.contract = QStringLiteral("CLZ26.NYM");
        fut.expiry = QDate(2026, 12, 1);
        fut.entry_price = 88.35;
        const QVector<bk::BookLeg> legs = {tdb_option(true, 2, 105, QDate(2026, 12, 18), 1.25), sw, fut};
        int skipped = -1;
        const QVector<bk::BookLeg> back = bk::legs_from_json(bk::legs_to_json(legs), &skipped);
        QCOMPARE(skipped, 0);
        QCOMPARE(back.size(), 3);
        QCOMPARE(back[0].type, bk::LegType::Call);
        QCOMPARE(back[0].strike, 105.0);
        QCOMPARE(back[0].expiry, QDate(2026, 12, 18));
        QCOMPARE(back[0].entry_iv, 0.25);
        QCOMPARE(back[1].type, bk::LegType::Swap);
        QCOMPARE(back[1].swap_rate, 0.0415);
        QCOMPARE(back[1].notional, 1e7);
        QCOMPARE(back[1].payer, false);
        QCOMPARE(back[2].contract, QStringLiteral("CLZ26.NYM"));
        QCOMPARE(back[2].quantity, -2.0);
        // Garbage entries are skipped and counted; an option without a strike is not a leg.
        const QVector<bk::BookLeg> partial =
            bk::legs_from_json(QStringLiteral("[{\"t\":\"X\"},{\"t\":\"C\",\"q\":1},{\"t\":\"S\",\"q\":10,\"p\":5}]"), &skipped);
        QCOMPARE(partial.size(), 1);
        QCOMPARE(skipped, 2);
        QCOMPARE(bk::legs_from_json(QStringLiteral("not json")).size(), 0);
    }

    // ── Repository ───────────────────────────────────────────────────────────
    void repository_positions_round_trip() {
        DerivBookRepository repo(db_);
        bk::BookPosition a = tdb_position(QStringLiteral("option"), QStringLiteral("SPY"), 100,
                                          {tdb_option(true, 2, 600, QDate(2026, 12, 18), 12.5)});
        a.notes = QStringLiteral("earnings hedge");
        auto saved = repo.save_position(a);
        QVERIFY2(saved.is_ok(), saved.is_ok() ? "" : saved.error().c_str());
        QVERIFY(!saved.value().id.isEmpty());
        QCOMPARE(saved.value().book, QStringLiteral("Main"));
        QVERIFY(saved.value().created_at > 0);
        bk::BookPosition b = tdb_position(QStringLiteral("strategy"), QStringLiteral("QQQ"), 100,
                                          {tdb_option(true, 1, 500, QDate(2027, 1, 15), 9),
                                           tdb_option(true, -1, 520, QDate(2027, 1, 15), 3.2)});
        b.book = QStringLiteral("Hedges");
        b.currency = QStringLiteral("usd");
        QVERIFY(repo.save_position(b).is_ok());

        auto all = repo.positions();
        QVERIFY(all.is_ok());
        QCOMPARE(all.value().size(), 2);
        const bk::BookPosition& r0 = all.value()[0];
        QCOMPARE(r0.underlying, QStringLiteral("SPY"));
        QCOMPARE(r0.legs.size(), 1);
        QCOMPARE(r0.legs[0].strike, 600.0);
        QCOMPARE(r0.entry_date, QDate(2026, 11, 2));
        QCOMPARE(r0.notes, QStringLiteral("earnings hedge"));
        QCOMPARE(all.value()[1].currency, QStringLiteral("USD")); // normalised
        auto hedges = repo.positions(QStringLiteral("Hedges"));
        QVERIFY(hedges.is_ok());
        QCOMPARE(hedges.value().size(), 1);
        QCOMPARE(hedges.value()[0].legs.size(), 2);
        auto books = repo.books();
        QVERIFY(books.is_ok());
        QCOMPARE(books.value(), (QStringList{QStringLiteral("Hedges"), QStringLiteral("Main")}));

        // Update keeps the id and creation time.
        bk::BookPosition upd = r0;
        upd.legs[0].quantity = 3;
        auto u = repo.save_position(upd);
        QVERIFY(u.is_ok());
        QCOMPARE(u.value().id, r0.id);
        auto again = repo.positions(QStringLiteral("Main"));
        QCOMPARE(again.value().size(), 1);
        QCOMPARE(again.value()[0].legs[0].quantity, 3.0);
        QCOMPARE(again.value()[0].created_at, r0.created_at);

        QVERIFY(repo.remove_position(r0.id).is_ok());
        QCOMPARE(repo.count_positions().value(), 1);
        // Saving a deleted row (edited in another view meanwhile) stores it again.
        QVERIFY(repo.save_position(upd).is_ok());
        QCOMPARE(repo.count_positions().value(), 2);
    }

    void repository_strategies_by_name() {
        DerivBookRepository repo(db_);
        bk::SavedStrategy s;
        s.name = QStringLiteral("SPY condor Dec");
        s.underlying = QStringLiteral("SPY");
        s.template_id = QStringLiteral("iron_condor");
        s.legs = {tdb_option(false, 1, 560, QDate(2026, 12, 18), 2), tdb_option(false, -1, 580, QDate(2026, 12, 18), 4)};
        auto first = repo.save_strategy(s);
        QVERIFY(first.is_ok());
        // Same name, other case: replaces, keeps the id.
        bk::SavedStrategy s2 = s;
        s2.name = QStringLiteral("spy CONDOR dec ");
        s2.legs.append(tdb_option(true, -1, 640, QDate(2026, 12, 18), 3));
        auto second = repo.save_strategy(s2);
        QVERIFY(second.is_ok());
        QCOMPARE(second.value().id, first.value().id);
        QCOMPARE(second.value().created_at, first.value().created_at);
        auto list = repo.strategies();
        QVERIFY(list.is_ok());
        QCOMPARE(list.value().size(), 1);
        QCOMPARE(list.value()[0].legs.size(), 3);
        QCOMPARE(list.value()[0].template_id, QStringLiteral("iron_condor"));
        bk::SavedStrategy unnamed;
        QVERIFY(repo.save_strategy(unnamed).is_err());
        QVERIFY(repo.remove_strategy(first.value().id).is_ok());
        QCOMPARE(repo.strategies().value().size(), 0);
    }

    // ── Valuation ────────────────────────────────────────────────────────────
    void option_marked_from_entry_iv() {
        // No chain: Black-Scholes-Merton at the IV recorded at entry (reference values above).
        bk::MarketView m;
        m.today = QDate(2026, 11, 16);
        bk::UnderlyingQuote q;
        q.spot = 100;
        q.r = 0.04;
        q.q = 0;
        m.underlyings.insert(QStringLiteral("XYZ"), q);
        const bk::BookPosition p = tdb_position(QStringLiteral("option"), QStringLiteral("xyz"), 100,
                                                {tdb_option(true, 2, 105, m.today.addDays(30), 1.0, 0.25)});
        const bk::PositionResult r = bk::value_position(p, m);
        QCOMPARE(r.legs.size(), 1);
        QCOMPARE(r.legs[0].source, bk::MarkSource::ModelEntryIv);
        QVERIFY2(tdb_near(r.total.mark, 1.1676993367, 1e-8), qPrintable(QString::number(r.total.mark, 'g', 12)));
        QVERIFY(tdb_near(r.total.value, 233.539867, 1e-5));
        QVERIFY(tdb_near(r.total.pnl, 233.539867 - 200.0, 1e-5));
        QVERIFY(tdb_near(r.total.delta, 54.915419, 1e-5));
        QVERIFY(tdb_near(r.total.dollar_delta, 5491.541917, 1e-4));
        QVERIFY(tdb_near(r.total.vega, 19.117642, 1e-5));
        QVERIFY(tdb_near(r.total.dollar_gamma, 930.391894, 1e-4));
        QVERIFY(r.total.theta < 0);
        QVERIFY(!r.stale);
        QCOMPARE(r.nearest_expiry, m.today.addDays(30));
    }

    void option_marked_from_chain_mid() {
        const QByteArray raw = tdb_fixture("spy_chain_cboe_sample.json");
        QVERIFY2(!raw.isEmpty(), "tests/data/derivatives/spy_chain_cboe_sample.json not found");
        auto chain = std::make_shared<mk::FreeChain>(mk::parse_free_chain(raw));
        QVERIFY(chain->ok());
        mk::analyse_chain(*chain, {}, 0.04);
        const mk::FreeExpiry& x = chain->expiries[1];
        double mid = 0, iv = 0;
        QVERIFY(mk::find_quote(*chain, 1, x.atm_strike, false, &mid, &iv) && mid > 0 && iv > 0);
        bk::MarketView m;
        m.today = chain->quote_time.date();
        bk::UnderlyingQuote q;
        q.chain = chain;
        q.as_of = chain->quote_time;
        q.source = chain->source_label;
        m.underlyings.insert(QStringLiteral("SPY"), q);
        const bk::BookPosition p = tdb_position(QStringLiteral("option"), QStringLiteral("SPY"), 100,
                                                {tdb_option(false, -3, x.atm_strike, x.date, mid + 1.0, 0.0)});
        const bk::PositionResult r = bk::value_position(p, m);
        QCOMPARE(r.legs[0].source, bk::MarkSource::ChainMid);
        QVERIFY(tdb_near(r.total.mark, mid, 1e-12));
        QVERIFY(tdb_near(r.total.pnl, -300.0 * (mid - (mid + 1.0)), 1e-9)); // short gained 1.00 per share
        const models::Greeks g = models::bsm_full(false, chain->spot, x.atm_strike, x.years, x.rate, x.carry_q, iv);
        QVERIFY(tdb_near(r.total.delta, -300.0 * g.delta, 1e-9));
        QVERIFY(r.total.delta > 0); // short put: long delta
        QCOMPARE(r.as_of, chain->quote_time);
        // A strike the chain does not list: valued with the chain's smile, not the entry IV.
        const bk::BookPosition odd = tdb_position(QStringLiteral("option"), QStringLiteral("^spy"), 100,
                                                  {tdb_option(true, 1, x.atm_strike + 0.37, x.date, 5.0, 0.9)});
        const bk::PositionResult ro = bk::value_position(odd, m);
        QCOMPARE(ro.legs[0].source, bk::MarkSource::ModelChainIv);
        QVERIFY(ro.legs[0].iv > 0.03 && ro.legs[0].iv < 0.6);
    }

    void expired_stock_future_swap() {
        bk::MarketView m;
        m.today = QDate(2026, 11, 16);
        bk::UnderlyingQuote q;
        q.spot = 50;
        m.underlyings.insert(QStringLiteral("ABC"), q);
        // Expired put: intrinsic at today's spot.
        const bk::PositionResult e = bk::value_position(
            tdb_position(QStringLiteral("option"), QStringLiteral("ABC"), 100, {tdb_option(false, 1, 55, QDate(2026, 11, 13), 2.0)}),
            m);
        QVERIFY(e.legs[0].expired);
        QCOMPARE(e.legs[0].source, bk::MarkSource::Intrinsic);
        QVERIFY(tdb_near(e.total.value, 500.0, 1e-12));
        QVERIFY(tdb_near(e.total.pnl, 300.0, 1e-12));
        QVERIFY(e.all_expired);
        QVERIFY(!e.nearest_expiry.isValid());
        // Stock.
        bk::BookLeg st;
        st.type = bk::LegType::Stock;
        st.quantity = 200;
        st.entry_price = 45;
        const bk::PositionResult s = bk::value_position(tdb_position(QStringLiteral("stock"), QStringLiteral("ABC"), 1, {st}), m);
        QVERIFY(tdb_near(s.total.value, 10000.0, 1e-9));
        QVERIFY(tdb_near(s.total.pnl, 1000.0, 1e-9));
        QVERIFY(tdb_near(s.total.delta, 200.0, 1e-12));
        // Future with and without a price.
        bk::BookLeg f;
        f.type = bk::LegType::Future;
        f.quantity = -2;
        f.contract = QStringLiteral("clz26.nym");
        f.expiry = QDate(2026, 12, 1);
        f.entry_price = 88.0;
        const bk::BookPosition fp = tdb_position(QStringLiteral("future"), QStringLiteral("CL"), 1000, {f});
        const bk::PositionResult f0 = bk::value_position(fp, m);
        QVERIFY(f0.stale);
        QCOMPARE(f0.legs[0].source, bk::MarkSource::Entry);
        QVERIFY(tdb_near(f0.total.delta, -2000.0, 1e-12));
        m.futures.insert(QStringLiteral("CLZ26.NYM"), 86.5);
        const bk::PositionResult f1 = bk::value_position(fp, m);
        QCOMPARE(f1.legs[0].source, bk::MarkSource::FuturesLast);
        QVERIFY(tdb_near(f1.total.pnl, 3000.0, 1e-9)); // short 2 × 1,000 bbl, down 1.50
        QVERIFY(tdb_near(f1.total.value, f1.total.pnl, 1e-12));
        QVERIFY(tdb_near(f1.total.dollar_delta, -2000.0 * 86.5, 1e-9));
        // Swap: carried at entry without a curve, re-valued off one.
        bk::BookLeg w;
        w.type = bk::LegType::Swap;
        w.quantity = 1;
        w.swap_start = 0;
        w.swap_tenor = 5;
        w.swap_freq = 2;
        w.notional = 1e7;
        w.payer = true;
        bk::BookPosition sp = tdb_position(QStringLiteral("swap"), QStringLiteral("USD rates"), 1, {w});
        sp.entry_date = m.today;
        QVERIFY(bk::value_position(sp, m).stale);
        const mk::TreasuryCurve tc = mk::parse_treasury_curve(tdb_fixture("treasury_curve_sample.json"));
        QVERIFY(tc.ok());
        m.curve = mk::zero_curve(tc);
        m.curve_date = tc.date;
        const rates::SwapResult par = rates::swap_on_curve(m.curve, 0, 5, 0, 2, 1e7, true);
        sp.legs[0].swap_rate = par.par_rate;
        const bk::PositionResult sw = bk::value_position(sp, m);
        QCOMPARE(sw.legs[0].source, bk::MarkSource::Curve);
        QVERIFY2(std::fabs(sw.total.value) < 1e-4, qPrintable(QString::number(sw.total.value)));
        QVERIFY(sw.total.rho > 0); // pay fixed: gains when rates rise
        QCOMPARE(sw.key, QStringLiteral("RATES:USD"));
        // A year later the 5-year swap has 4 years left.
        bk::MarketView later = m;
        later.today = m.today.addDays(365);
        QVERIFY(std::fabs(bk::value_position(sp, later).legs[0].years - 4.0) < 1e-9);
        sp.entry_date = m.today.addDays(-6 * 365);
        QVERIFY(bk::value_position(sp, m).legs[0].expired);
    }

    void aggregation_sums_rows() {
        bk::MarketView m;
        m.today = QDate(2026, 11, 16);
        for (const auto& [sym, spot] : {std::pair<QString, double>{QStringLiteral("AAA"), 100.0},
                                        std::pair<QString, double>{QStringLiteral("BBB"), 40.0}}) {
            bk::UnderlyingQuote q;
            q.spot = spot;
            m.underlyings.insert(sym, q);
        }
        QVector<bk::BookPosition> book = {
            tdb_position(QStringLiteral("option"), QStringLiteral("AAA"), 100, {tdb_option(true, 1, 100, m.today.addDays(10), 2)}),
            tdb_position(QStringLiteral("strategy"), QStringLiteral("AAA"), 100,
                         {tdb_option(false, 2, 95, m.today.addDays(10), 1), tdb_option(false, -1, 90, m.today.addDays(40), 0.5)}),
            tdb_position(QStringLiteral("option"), QStringLiteral("BBB"), 100, {tdb_option(true, -5, 45, m.today.addDays(200), 1)}),
        };
        const QVector<bk::PositionResult> res = bk::value_book(book, m);
        QCOMPARE(res.size(), 3);
        const bk::BookSummary s = bk::summarise(book, res, m.today);
        double v = 0, pnl = 0, dd = 0, vega = 0, theta = 0;
        for (const auto& r : res) {
            v += r.total.value;
            pnl += r.total.pnl;
            dd += r.total.dollar_delta;
            vega += r.total.vega;
            theta += r.total.theta;
        }
        QVERIFY(tdb_near(s.total.value, v, 1e-9));
        QVERIFY(tdb_near(s.total.pnl, pnl, 1e-9));
        QVERIFY(tdb_near(s.total.dollar_delta, dd, 1e-9));
        QVERIFY(tdb_near(s.total.vega, vega, 1e-9));
        QVERIFY(tdb_near(s.total.theta, theta, 1e-9));
        QCOMPARE(s.total.positions, 3);
        QCOMPARE(s.by_underlying.size(), 2);
        int aaa = s.by_underlying[0].key == QLatin1String("AAA") ? 0 : 1;
        QCOMPARE(s.by_underlying[aaa].positions, 2);
        QVERIFY(tdb_near(s.by_underlying[aaa].delta, res[0].total.delta + res[1].total.delta, 1e-9));
        QVERIFY(tdb_near(s.by_underlying[0].dollar_delta + s.by_underlying[1].dollar_delta, dd, 1e-9));
        // Ladder: three dates, ascending, legs counted, buckets by days.
        QCOMPARE(s.ladder.size(), 3);
        QCOMPARE(s.ladder[0].expiry, m.today.addDays(10));
        QCOMPARE(s.ladder[0].legs, 2);
        QCOMPARE(s.ladder[0].bucket, 2);
        QCOMPARE(s.ladder[1].bucket, 3);
        QCOMPARE(s.ladder[2].bucket, 5);
        double ladder_vega = 0;
        for (const auto& l : s.ladder)
            ladder_vega += l.vega;
        QVERIFY(tdb_near(ladder_vega, vega, 1e-9));
        QCOMPARE(s.currencies, QStringList{QStringLiteral("USD")});
        QCOMPARE(bk::expiry_bucket(-1), 0);
        QCOMPARE(bk::expiry_bucket(7), 1);
        QCOMPARE(bk::expiry_bucket(366), 6);
    }

    void strategy_legs_conversion() {
        bk::BookLeg f;
        f.type = bk::LegType::Future;
        f.quantity = 1;
        bk::BookLeg st;
        st.type = bk::LegType::Stock;
        st.quantity = 100;
        st.entry_price = 50;
        const bk::BookPosition p =
            tdb_position(QStringLiteral("strategy"), QStringLiteral("AAA"), 100,
                         {tdb_option(true, -1, 55, QDate(2026, 12, 16), 1.5, 0.3), st, f});
        int skipped = 0;
        const auto legs = bk::strategy_legs(p, QDate(2026, 11, 16), &skipped);
        QCOMPARE(int(legs.size()), 2);
        QCOMPARE(skipped, 1);
        QCOMPARE(legs[0].kind, strategy::LegKind::Call);
        QVERIFY(tdb_near(legs[0].expiry_years, 30.0 / 365.0, 1e-12));
        QCOMPARE(legs[0].iv, 0.3);
        QCOMPARE(legs[1].kind, strategy::LegKind::Underlying);
        QCOMPARE(bk::underlying_key(QStringLiteral(" ^gspc ")), QStringLiteral("SPX"));
    }

    void cleanupTestCase() {
        db_.close();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(QStringLiteral("tst_deriv_book_mem"));
    }

  private:
    QSqlDatabase db_;
};

QTEST_GUILESS_MAIN(TestDerivBook)
#include "tst_deriv_book.moc"
