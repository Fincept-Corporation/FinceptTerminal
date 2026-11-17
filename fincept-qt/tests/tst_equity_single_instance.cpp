// tst_equity_single_instance — the Equity Trading screen is built once per panel.
//
// Reproduces the shell sequence behind "two complete Equity Trading screens stacked
// one above the other" (see screens/equity_trading/EquitySingleInstance.h): a router
// builds the screen inside a dock host, adds it to the host's QVBoxLayout, shows it,
// and records it only after that; the screen's first show makes the window re-show
// everything, which re-enters the router before the screen is recorded.
//   * without the guard the host ends up holding TWO live screens (the defect);
//   * with it the second build is an inert twin: hidden, no space taken, no
//     subscription of its own, and it forwards to the first screen.
// Offscreen widgets, no application sources.

#include "screens/equity_trading/EquitySingleInstance.h"

#include <QApplication>
#include <QPointer>
#include <QShowEvent>
#include <QTest>
#include <QVBoxLayout>

#include <functional>

using fincept::screens::equity::eq_first_live_twin;
using fincept::screens::equity::kEqInertTwinProperty;

namespace {

int g_subscriptions = 0; // what a real screen starts on show (streams, hub topics, tickets)
bool g_window_recreated = false; // the window is re-created once: the first texture widget's show

/// The router's materialise step, with the same order of operations as
/// DockScreenRouter::materialize_screen: factory(host) -> layout -> show() -> record.
struct FakeRouter {
    QWidget* host = nullptr;
    QWidget* recorded = nullptr;
    std::function<QWidget*(QWidget*)> factory{};
    int factory_calls = 0;

    void materialize() {
        if (recorded)
            return;
        ++factory_calls;
        QWidget* screen = factory(host);
        static_cast<QVBoxLayout*>(host->layout())->addWidget(screen, 1);
        screen->show();
        recorded = screen; // only now does the router know the screen
    }
};

FakeRouter* g_router = nullptr;

/// Stand-in for the real screen: its first show re-enters the router, as the
/// top-level window re-creation does in the app.
class TstEqScreen : public QWidget {
    Q_OBJECT
  public:
    TstEqScreen(QWidget* parent, bool guarded) : QWidget(parent) {
        if (guarded) {
            if (QWidget* twin = eq_first_live_twin(parent, this, staticMetaObject.className())) {
                primary = twin;
                setProperty(kEqInertTwinProperty, true);
                setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                setMaximumSize(0, 0);
                QMetaObject::invokeMethod(this, &QWidget::hide, Qt::QueuedConnection);
                return;
            }
        }
        setMinimumHeight(100);
    }
    bool inert() const { return property(kEqInertTwinProperty).toBool(); }
    QPointer<QWidget> primary;
    int subscribed = 0;

  protected:
    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        if (inert())
            return; // a twin never starts anything
        ++subscribed;
        ++g_subscriptions;
        if (!g_window_recreated) {
            g_window_recreated = true;
            g_router->materialize(); // the window re-show -> visibilityChanged(true)
        }
    }
};

QList<TstEqScreen*> live_screens(QWidget* host) {
    QList<TstEqScreen*> out;
    for (auto* s : host->findChildren<TstEqScreen*>(Qt::FindDirectChildrenOnly))
        if (!s->inert())
            out << s;
    return out;
}

} // namespace

class TstEquitySingleInstance : public QObject {
    Q_OBJECT
  private slots:
    void init() {
        g_subscriptions = 0;
        g_window_recreated = false;
    }

    void reentrant_router_builds_two_screens_without_the_guard() {
        QWidget host;
        new QVBoxLayout(&host);
        host.resize(800, 600);
        host.show();
        FakeRouter router{&host};
        g_router = &router;
        router.factory = [](QWidget* parent) { return new TstEqScreen(parent, /*guarded=*/false); };
        router.materialize();
        QCoreApplication::processEvents();
        QCOMPARE(router.factory_calls, 2);           // the defect: the factory ran twice
        QCOMPARE(live_screens(&host).size(), 2);      // two screens in one panel ...
        QCOMPARE(g_subscriptions, 2);                 // ... each with its own subscriptions
    }

    void guard_makes_the_second_build_an_inert_twin() {
        QWidget host;
        new QVBoxLayout(&host);
        host.resize(800, 600);
        host.show();
        FakeRouter router{&host};
        g_router = &router;
        router.factory = [](QWidget* parent) { return new TstEqScreen(parent, /*guarded=*/true); };
        router.materialize();
        QCoreApplication::processEvents();
        QCOMPARE(router.factory_calls, 2); // the shell still calls twice (its defect) ...
        const auto live = live_screens(&host);
        QCOMPARE(live.size(), 1);          // ... but only one screen is real
        QCOMPARE(g_subscriptions, 1);      // and only it subscribed
        TstEqScreen* twin = nullptr;
        for (auto* s : host.findChildren<TstEqScreen*>(Qt::FindDirectChildrenOnly))
            if (s->inert())
                twin = s;
        QVERIFY(twin);
        QCOMPARE(twin->primary.data(), static_cast<QWidget*>(live.first()));
        QVERIFY(!twin->isVisible());                 // hidden ...
        QCOMPARE(twin->maximumSize(), QSize(0, 0));  // ... and never takes layout space
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
        QCOMPARE(live.first()->height(), host.contentsRect().height() -
                                             host.layout()->contentsMargins().top() -
                                             host.layout()->contentsMargins().bottom());
    }

    void twin_lookup_rules() {
        QWidget host;
        auto* a = new TstEqScreen(&host, true);
        QVERIFY(!a->inert());
        QCOMPARE(eq_first_live_twin(&host, a, TstEqScreen::staticMetaObject.className()), nullptr); // self excluded
        auto* b = new TstEqScreen(&host, true);
        QVERIFY(b->inert());
        QCOMPARE(b->primary.data(), static_cast<QWidget*>(a));
        // A third build still finds the original, never the inert twin.
        auto* c = new TstEqScreen(&host, true);
        QVERIFY(c->inert());
        QCOMPARE(c->primary.data(), static_cast<QWidget*>(a));
        // Another host is independent (a panel in another window is its own screen).
        QWidget other;
        auto* d = new TstEqScreen(&other, true);
        QVERIFY(!d->inert());
        // Widgets of another class and a null host never count.
        new QWidget(&other);
        QCOMPARE(eq_first_live_twin(&other, nullptr, TstEqScreen::staticMetaObject.className()),
                 static_cast<QWidget*>(d));
        QCOMPARE(eq_first_live_twin(nullptr, nullptr, TstEqScreen::staticMetaObject.className()), nullptr);
        // Once the original is gone, the next build is a real screen again.
        delete a;
        auto* e = new TstEqScreen(&host, true);
        QVERIFY(!e->inert());
    }
};

QTEST_MAIN(TstEquitySingleInstance)
#include "tst_equity_single_instance.moc"
