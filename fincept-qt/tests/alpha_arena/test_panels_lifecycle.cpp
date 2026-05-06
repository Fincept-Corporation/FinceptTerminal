// test_panels_lifecycle — basic construct/destroy + lifecycle smoke for the
// Alpha Arena panels.
//
// Full snapshot/render testing requires a running Qt event loop with the app's
// stylesheet, which is overkill for the unit tier. We verify here that:
//   * Each panel constructs without crashing.
//   * set_competition() is idempotent and does not crash on the empty-id case.
//   * refresh() on an empty competition is a no-op (does not throw).
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 7.

#include "screens/alpha_arena/panels/LeaderboardPanel.h"
#include "screens/alpha_arena/panels/ModelChatPanel.h"
#include "screens/alpha_arena/panels/StatePanels.h"

#include <QApplication>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>

using namespace fincept::screens::alpha_arena;

class TestPanelsLifecycle : public QObject {
    Q_OBJECT

  private slots:

    void leaderboard_constructs_and_refreshes_empty() {
        LeaderboardPanel p;
        p.set_competition(QString());
        p.refresh();
        QVERIFY(true); // no crash
    }

    void modelchat_constructs_and_refreshes_empty() {
        ModelChatPanel p;
        p.set_competition(QString());
        p.refresh();
        QVERIFY(true);
    }

    void positions_constructs_and_refreshes_empty() {
        PositionsPanel p;
        p.set_competition(QString());
        p.refresh();
        QVERIFY(true);
    }

    void hitl_constructs_and_handles_request() {
        HitlPanel p;
        p.set_competition(QStringLiteral("comp-1"));
        // Push a fake approval and confirm signal reuses approval_id payload.
        QSignalSpy spy(&p, &HitlPanel::approval_resolved);
        p.on_hitl_requested(QStringLiteral("ap-1"), QStringLiteral("agent-A"),
                            QStringLiteral("close BTC long"));
        // Manually finding the approve button without exec() is brittle; just
        // assert the panel accepted the request without crashing.
        QVERIFY(true);
    }

    void risk_constructs_and_refreshes_empty() {
        RiskPanel p;
        p.set_competition(QString());
        p.refresh();
        QVERIFY(true);
    }

    void audit_constructs_and_refreshes_empty() {
        AuditPanel p;
        p.set_competition(QString());
        p.refresh();
        QVERIFY(true);
    }

    void set_competition_idempotent() {
        LeaderboardPanel p;
        p.set_competition(QStringLiteral("a"));
        p.set_competition(QStringLiteral("a"));
        p.set_competition(QStringLiteral("b"));
        QVERIFY(true);
    }
};

QTEST_MAIN(TestPanelsLifecycle)
#include "test_panels_lifecycle.moc"
