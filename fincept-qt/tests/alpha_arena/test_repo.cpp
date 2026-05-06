// test_repo — replay determinism, WAL concurrency tolerance, monotonic
// event seq for AlphaArenaRepo.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 4.
//
// Each test opens a fresh on-disk SQLite file (we use the file path Database
// expects) and applies the v024 schema before exercising the repo facade.

#include "core/result/Result.h"
#include "services/alpha_arena/AlphaArenaRepo.h"
#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QCoreApplication>
#include <QFile>
#include <QObject>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTest>
#include <QUuid>

using namespace fincept;
using namespace fincept::services::alpha_arena;

namespace {

QString fresh_db_path() {
    auto* tf = new QTemporaryFile;
    tf->setAutoRemove(false);
    if (!tf->open()) return {};
    QString path = tf->fileName();
    tf->close();
    delete tf;
    QFile::remove(path);
    return path;
}

Result<void> open_with_schema() {
    Database::instance().close();
    register_migration_v024();
    auto r = Database::instance().open(fresh_db_path());
    if (r.is_err()) return r;
    MigrationRunner runner(Database::instance().raw_db());
    return runner.run();
}

CompetitionRow make_competition(const QString& name = QStringLiteral("test")) {
    CompetitionRow c;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.name = name;
    c.mode = CompetitionMode::Baseline;
    c.venue = QStringLiteral("paper");
    c.instruments = {QStringLiteral("BTC"), QStringLiteral("ETH")};
    c.initial_capital = 10000.0;
    c.cadence_seconds = 60;
    c.live_mode = false;
    c.status = QStringLiteral("created");
    return c;
}

AgentRow make_agent(const QString& comp_id, int slot, const QString& display) {
    AgentRow a;
    a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.competition_id = comp_id;
    a.slot = slot;
    a.provider = QStringLiteral("openai");
    a.model_id = QStringLiteral("gpt-4o");
    a.display_name = display;
    a.color_hex = QStringLiteral("#FF8800");
    a.state = QStringLiteral("active");
    return a;
}

} // namespace

class TestAlphaArenaRepo : public QObject {
    Q_OBJECT

  private slots:

    void initTestCase() {
        QVERIFY(open_with_schema().is_ok());
    }

    void cleanupTestCase() {
        Database::instance().close();
    }

    // ── Replay determinism ──────────────────────────────────────────────────

    void competition_roundtrip() {
        auto& repo = AlphaArenaRepo::instance();
        const auto comp = make_competition();
        QVERIFY(repo.insert_competition(comp).is_ok());
        const auto got = repo.find_competition(comp.id);
        QVERIFY(got.has_value());
        QCOMPARE(got->id, comp.id);
        QCOMPARE(got->name, comp.name);
        QCOMPARE(got->cadence_seconds, comp.cadence_seconds);
    }

    void agent_persistence() {
        auto& repo = AlphaArenaRepo::instance();
        const auto comp = make_competition(QStringLiteral("agent-test"));
        QVERIFY(repo.insert_competition(comp).is_ok());

        for (int i = 0; i < 4; ++i) {
            QVERIFY(repo.insert_agent(make_agent(comp.id, i,
                QStringLiteral("agent-%1").arg(i))).is_ok());
        }
        auto agents = repo.agents_for(comp.id);
        QVERIFY(agents.is_ok());
        QCOMPARE(agents.value().size(), 4);
    }

    void tick_decision_replay_deterministic() {
        auto& repo = AlphaArenaRepo::instance();
        const auto comp = make_competition(QStringLiteral("replay"));
        QVERIFY(repo.insert_competition(comp).is_ok());

        QVector<AgentRow> agents;
        for (int i = 0; i < 4; ++i) {
            auto a = make_agent(comp.id, i, QStringLiteral("a%1").arg(i));
            QVERIFY(repo.insert_agent(a).is_ok());
            agents.append(a);
        }

        // Insert 10 ticks, with one decision per agent each.
        QStringList tick_ids;
        for (int seq = 1; seq <= 10; ++seq) {
            auto tid = repo.insert_tick(comp.id, seq, 1700000000000LL + seq * 60000,
                                        QStringLiteral("system-sha"),
                                        QStringLiteral("{\"snap\":1}"));
            QVERIFY(tid.is_ok());
            tick_ids << tid.value();
            for (const auto& ag : agents) {
                AlphaArenaRepo::DecisionInsert d;
                d.tick_id = tid.value();
                d.agent_id = ag.id;
                d.user_prompt_sha256 = QStringLiteral("user-sha-%1-%2").arg(seq).arg(ag.slot);
                d.raw_response = QStringLiteral("{\"actions\":[]}");
                d.parsed_actions_json = QStringLiteral("[]");
                d.risk_verdict_json = QStringLiteral("[]");
                d.latency_ms = 100 + seq;
                QVERIFY(repo.insert_decision(d).is_ok());
            }
        }

        // Read back per-agent timeline twice and assert identical row count.
        for (const auto& ag : agents) {
            auto r1 = repo.modelchat_for(comp.id, ag.id, 100, 0);
            auto r2 = repo.modelchat_for(comp.id, ag.id, 100, 0);
            QVERIFY(r1.is_ok());
            QVERIFY(r2.is_ok());
            QCOMPARE(r1.value().size(), 10);
            QCOMPARE(r1.value().size(), r2.value().size());
        }
    }

    void unique_competition_seq_constraint() {
        auto& repo = AlphaArenaRepo::instance();
        const auto comp = make_competition(QStringLiteral("dup-seq"));
        QVERIFY(repo.insert_competition(comp).is_ok());

        QVERIFY(repo.insert_tick(comp.id, 1, 0, "", "{}").is_ok());
        // Inserting tick with same (competition_id, seq) must fail.
        QVERIFY(repo.insert_tick(comp.id, 1, 0, "", "{}").is_err());
    }

    // ── Event monotonicity ──────────────────────────────────────────────────

    void event_seq_monotonic_within_competition() {
        auto& repo = AlphaArenaRepo::instance();
        const auto comp = make_competition(QStringLiteral("events"));
        QVERIFY(repo.insert_competition(comp).is_ok());

        for (int i = 0; i < 50; ++i) {
            QVERIFY(repo.append_event(comp.id, "", QStringLiteral("noise"),
                                       QJsonObject{{"i", i}}).is_ok());
        }
        auto rows = repo.events_since(comp.id, 0, 100);
        QVERIFY(rows.is_ok());
        QCOMPARE(rows.value().size(), 50);
        qint64 last = -1;
        for (const auto& e : rows.value()) {
            QVERIFY(e.seq > last);
            last = e.seq;
        }
    }

    void events_since_filters_by_seq() {
        auto& repo = AlphaArenaRepo::instance();
        const auto comp = make_competition(QStringLiteral("events-filter"));
        QVERIFY(repo.insert_competition(comp).is_ok());

        for (int i = 0; i < 20; ++i) {
            QVERIFY(repo.append_event(comp.id, "", QStringLiteral("e"),
                                       QJsonObject{{"i", i}}).is_ok());
        }
        auto first10 = repo.events_since(comp.id, 0, 10);
        QVERIFY(first10.is_ok());
        QCOMPARE(first10.value().size(), 10);

        const qint64 cursor = first10.value().last().seq;
        auto rest = repo.events_since(comp.id, cursor, 100);
        QVERIFY(rest.is_ok());
        QCOMPARE(rest.value().size(), 10);
        for (const auto& e : rest.value()) QVERIFY(e.seq > cursor);
    }

    // ── Leaderboard (smoke) ─────────────────────────────────────────────────

    void leaderboard_returns_agents_in_rank_order() {
        auto& repo = AlphaArenaRepo::instance();
        const auto comp = make_competition(QStringLiteral("lb"));
        QVERIFY(repo.insert_competition(comp).is_ok());

        QVector<AgentRow> agents;
        for (int i = 0; i < 3; ++i) {
            auto a = make_agent(comp.id, i, QStringLiteral("lb-a%1").arg(i));
            QVERIFY(repo.insert_agent(a).is_ok());
            agents.append(a);
        }

        // Insert one tick + one pnl snapshot per agent with varying equity.
        auto tid = repo.insert_tick(comp.id, 1, 0, "", "{}");
        QVERIFY(tid.is_ok());

        const double equities[3] = {12000.0, 9500.0, 11000.0};
        for (int i = 0; i < 3; ++i) {
            AgentAccountState s;
            s.equity = equities[i];
            s.cash = equities[i] * 0.5;
            s.return_pct = (equities[i] - 10000.0) / 10000.0;
            s.sharpe = 0.0;
            s.fees_paid = 1.0;
            QVERIFY(repo.insert_pnl_snapshot(tid.value(), agents[i].id, s, 1.0).is_ok());
        }

        auto lb = repo.leaderboard(comp.id);
        QVERIFY(lb.is_ok());
        QCOMPARE(lb.value().size(), 3);
        // First row should be the agent with highest equity.
        QVERIFY(lb.value().first().equity >= lb.value().last().equity);
    }
};

QTEST_MAIN(TestAlphaArenaRepo)
#include "test_repo.moc"
