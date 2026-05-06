// test_50tick_replay — repo-level replay-determinism smoke test.
//
// The plan's CI-gate is a full 50-tick paper-mode competition with a stub LLM
// subprocess; that requires an injectable ModelDispatcher hook the engine
// doesn't yet expose. Until that lands, this test exercises the persistence
// invariant the integration test will assert: 50 ticks × 4 agents in, the
// leaderboard / modelchat / event stream come back deterministically.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §CI gates.

#include "core/result/Result.h"
#include "services/alpha_arena/AlphaArenaRepo.h"
#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QFile>
#include <QObject>
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

} // namespace

class Test50TickReplay : public QObject {
    Q_OBJECT

  private:
    QString comp_id_;
    QStringList agent_ids_;

  private slots:

    void initTestCase() {
        QVERIFY(open_with_schema().is_ok());
        auto& repo = AlphaArenaRepo::instance();

        CompetitionRow comp;
        comp.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        comp.name = QStringLiteral("50-tick-replay");
        comp.mode = CompetitionMode::Baseline;
        comp.venue = QStringLiteral("paper");
        comp.instruments = kPerpUniverse();
        comp.initial_capital = 10000.0;
        comp.cadence_seconds = 60;
        comp.status = QStringLiteral("running");
        QVERIFY(repo.insert_competition(comp).is_ok());
        comp_id_ = comp.id;

        for (int i = 0; i < 4; ++i) {
            AgentRow a;
            a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            a.competition_id = comp_id_;
            a.slot = i;
            a.provider = QStringLiteral("openai");
            a.model_id = QStringLiteral("stub");
            a.display_name = QStringLiteral("agent-%1").arg(i);
            a.color_hex = QStringLiteral("#%1").arg(0x100000 + i * 0x202020, 6, 16, QChar('0'));
            a.state = QStringLiteral("active");
            QVERIFY(repo.insert_agent(a).is_ok());
            agent_ids_ << a.id;
        }
    }

    void cleanupTestCase() {
        Database::instance().close();
    }

    void run_50_ticks_and_replay() {
        auto& repo = AlphaArenaRepo::instance();

        for (int seq = 1; seq <= 50; ++seq) {
            auto tid = repo.insert_tick(comp_id_, seq, 1700000000000LL + seq * 60000,
                                        QStringLiteral("system-sha"), QStringLiteral("{}"));
            QVERIFY(tid.is_ok());

            for (int slot = 0; slot < agent_ids_.size(); ++slot) {
                AlphaArenaRepo::DecisionInsert d;
                d.tick_id = tid.value();
                d.agent_id = agent_ids_[slot];
                d.user_prompt_sha256 = QStringLiteral("user-sha-%1-%2").arg(seq).arg(slot);
                d.raw_response = QStringLiteral("{\"actions\":[]}");
                d.parsed_actions_json = QStringLiteral("[]");
                d.risk_verdict_json = QStringLiteral("[]");
                d.latency_ms = 100 + seq;
                QVERIFY(repo.insert_decision(d).is_ok());
            }

            // PnL snapshot — tiny equity drift.
            for (int slot = 0; slot < agent_ids_.size(); ++slot) {
                AgentAccountState s;
                s.equity = 10000.0 + slot * 50.0 - seq;
                s.cash = s.equity * 0.5;
                s.return_pct = (s.equity - 10000.0) / 10000.0;
                QVERIFY(repo.insert_pnl_snapshot(tid.value(), agent_ids_[slot], s, 1.0).is_ok());
            }
            QVERIFY(repo.append_event(comp_id_, "", QStringLiteral("tick"),
                                       QJsonObject{{"seq", seq}}).is_ok());
        }

        // Replay 1: read modelchat for every agent; assert size=50.
        for (const auto& a : agent_ids_) {
            auto r = repo.modelchat_for(comp_id_, a, 100, 0);
            QVERIFY(r.is_ok());
            QCOMPARE(r.value().size(), 50);
        }

        // Replay 2: read events; assert seq monotonic and exactly 50 tick events.
        auto evs = repo.events_since(comp_id_, 0, 200);
        QVERIFY(evs.is_ok());
        int tick_evs = 0;
        qint64 last_seq = -1;
        for (const auto& e : evs.value()) {
            if (e.type == QLatin1String("tick")) ++tick_evs;
            QVERIFY(e.seq > last_seq);
            last_seq = e.seq;
        }
        QCOMPARE(tick_evs, 50);

        // Replay 3: leaderboard returns one row per agent.
        auto lb = repo.leaderboard(comp_id_);
        QVERIFY(lb.is_ok());
        QCOMPARE(lb.value().size(), agent_ids_.size());
    }
};

QTEST_MAIN(Test50TickReplay)
#include "test_50tick_replay.moc"
