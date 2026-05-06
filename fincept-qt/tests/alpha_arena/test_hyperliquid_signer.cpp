// test_hyperliquid_signer — known-vector test for the EIP-712 signer.
//
// CURRENT STATE: HyperliquidSigner is not yet wired (libsecp256k1 + keccak256
// pending FetchContent in Phase 5c follow-up). Tests assert the gated
// stub behaviour today; replace `expect_err` with `expect_eq` against the
// published HL vectors once the real signer lands.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 5c.

#include "trading/exchanges/hyperliquid/HyperliquidSigner.h"

#include <QJsonObject>
#include <QObject>
#include <QTest>

using namespace fincept::trading::hyperliquid;

class TestHyperliquidSigner : public QObject {
    Q_OBJECT

  private slots:

    void signer_not_wired_yet() {
        QVERIFY(!HyperliquidSigner::is_wired());
    }

    void sign_action_returns_err_until_wired() {
        QJsonObject action;
        action[QStringLiteral("type")] = QStringLiteral("order");
        const auto r = HyperliquidSigner::sign_action(action, "0x" + QString(64, QChar('0')), true);
        QVERIFY(r.is_err());
    }

    void canonical_action_struct_hash_is_stable() {
        QJsonObject a;
        a[QStringLiteral("type")] = QStringLiteral("order");
        a[QStringLiteral("nonce")] = 12345;
        const auto h1 = HyperliquidSigner::action_struct_hash(a);
        const auto h2 = HyperliquidSigner::action_struct_hash(a);
        QCOMPARE(h1, h2);
        QCOMPARE(h1.size(), 32); // SHA-256 stand-in is 32 bytes; replace with keccak256 once wired.
    }

    void key_order_does_not_affect_hash() {
        QJsonObject a, b;
        a[QStringLiteral("alpha")] = 1;
        a[QStringLiteral("bravo")] = 2;
        b[QStringLiteral("bravo")] = 2;
        b[QStringLiteral("alpha")] = 1;
        QCOMPARE(HyperliquidSigner::action_struct_hash(a),
                 HyperliquidSigner::action_struct_hash(b));
    }
};

QTEST_MAIN(TestHyperliquidSigner)
#include "test_hyperliquid_signer.moc"
