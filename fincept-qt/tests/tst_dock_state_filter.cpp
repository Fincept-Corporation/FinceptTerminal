// src/app/DockStateFilter.h — header-only, zero app sources. A saved layout with
// a duplicate of a single-instance trading screen restores with one panel.
#include "app/DockStateFilter.h"

#include <QtTest>

using namespace fincept::dock_state;

namespace {
const char* kState = R"(<?xml version="1.0" encoding="UTF-8"?>
<QtAdvancedDockingSystem Version="3" UserVersion="0" Containers="2">
 <Container Floating="0">
  <Splitter Orientation="|" Count="3">
   <Area Tabs="2" Current="equity_trading#dup2">
    <Widget Name="equity_trading" Closed="0"/>
    <Widget Name="equity_trading#dup2" Closed="0"/>
   </Area>
   <Area Tabs="2" Current="watchlist">
    <Widget Name="watchlist" Closed="0"/>
    <Widget Name="watchlist#dup2" Closed="0"/>
   </Area>
   <Area Tabs="1" Current="fno#dup3">
    <Widget Name="fno#dup3" Closed="0">
    </Widget>
   </Area>
  </Splitter>
 </Container>
 <Container Floating="1">
  <Area Tabs="1" Current="crypto_trading#dup2">
   <Widget Name="crypto_trading#dup2" Closed="1"/>
  </Area>
 </Container>
</QtAdvancedDockingSystem>
)";
} // namespace

class TestDockStateFilter : public QObject {
    Q_OBJECT
  private slots:
    void classifies_ids() {
        QVERIFY(is_single_instance_screen(QStringLiteral("equity_trading")));
        QVERIFY(is_single_instance_screen(QStringLiteral("fno#dup2")));
        QVERIFY(!is_single_instance_screen(QStringLiteral("watchlist")));
        QVERIFY(is_single_instance_duplicate(QStringLiteral("equity_trading#dup2")));
        QVERIFY(is_single_instance_duplicate(QStringLiteral("crypto_trading#dup11")));
        QVERIFY(!is_single_instance_duplicate(QStringLiteral("equity_trading")));
        QVERIFY(!is_single_instance_duplicate(QStringLiteral("watchlist#dup2")));
    }

    void strips_plain_xml() {
        const QByteArray in(kState);
        QStringList dropped;
        const QByteArray out = without_single_instance_duplicates(in, &dropped);
        QCOMPARE(dropped, (QStringList{QStringLiteral("equity_trading#dup2"), QStringLiteral("fno#dup3"),
                                       QStringLiteral("crypto_trading#dup2")}));
        QVERIFY(out.startsWith("<?xml"));
        const QStringList names = widget_names(out);
        QCOMPARE(names, (QStringList{QStringLiteral("equity_trading"), QStringLiteral("watchlist"),
                                     QStringLiteral("watchlist#dup2")}));
        // No area names a dropped entry as its current tab any more.
        QVERIFY(out.contains("<Area Tabs=\"2\" Current=\"\">"));
        QVERIFY(out.contains("<Area Tabs=\"2\" Current=\"watchlist\">"));
        QVERIFY(!out.contains("equity_trading#dup2"));
        QVERIFY(!out.contains("fno#dup3"));
        QVERIFY(!out.contains("crypto_trading#dup2"));
        // Everything else is byte-for-byte where it was.
        QVERIFY(out.contains("<Splitter Orientation=\"|\" Count=\"3\">"));
        QVERIFY(out.contains("<Container Floating=\"1\">"));
        QVERIFY(out.contains("</QtAdvancedDockingSystem>"));
    }

    void strips_compressed_blob_and_keeps_it_compressed() {
        const QByteArray in = qCompress(QByteArray(kState));
        QStringList dropped;
        const QByteArray out = without_single_instance_duplicates(in, &dropped);
        QCOMPARE(dropped.size(), 3);
        QVERIFY(!out.startsWith("<?xml"));
        const QByteArray xml = qUncompress(out);
        QVERIFY(xml.startsWith("<?xml"));
        QVERIFY(!xml.contains("equity_trading#dup2"));
        QVERIFY(xml.contains("Name=\"equity_trading\""));
        QCOMPARE(widget_names(out).size(), 3);
    }

    void untouched_when_nothing_to_drop() {
        const QByteArray in = qCompress(QByteArray(R"(<?xml version="1.0"?><QtAdvancedDockingSystem Version="3">)"
                                                   R"(<Container Floating="0"><Area Tabs="1" Current="watchlist">)"
                                                   R"(<Widget Name="watchlist" Closed="0"/></Area></Container>)"
                                                   R"(</QtAdvancedDockingSystem>)"));
        QStringList dropped;
        QCOMPARE(without_single_instance_duplicates(in, &dropped), in); // identical bytes
        QVERIFY(dropped.isEmpty());
    }

    void garbage_is_returned_as_is() {
        const QByteArray junk("not a dock state");
        QCOMPARE(without_single_instance_duplicates(junk), junk);
        QCOMPARE(without_single_instance_duplicates(QByteArray()), QByteArray());
    }
};

QTEST_GUILESS_MAIN(TestDockStateFilter)
#include "tst_dock_state_filter.moc"
