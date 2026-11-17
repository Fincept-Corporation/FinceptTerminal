// tests/tst_empty_state_tab_chain.cpp — ui::EmptyState's keyboard order, offscreen.
// An EmptyState built BEFORE it is added to a layout (no parent) with every focusable part —
// DETAILS, clickable detail rows, primary, secondary, suggestion rows — must chain its tab order
// without any "QWidget::setTabOrder: 'first' and 'second' must be in the same window" warning,
// and once parented and shown the order must read DETAILS → rows → primary → secondary → suggestions.
#include "ui/widgets/EmptyState.h"

#include <QAbstractButton>
#include <QApplication>
#include <QPushButton>
#include <QStringList>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QStringList g_tab_order_warnings;
QtMessageHandler g_previous = nullptr;

void capture(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    if (msg.contains(QLatin1String("setTabOrder")))
        g_tab_order_warnings << msg;
    if (g_previous)
        g_previous(type, ctx, msg);
}

fincept::ui::EmptyState::Spec full_spec() {
    using fincept::ui::EmptyState;
    EmptyState::Spec spec = EmptyState::error_spec(QStringLiteral("3 of 12 symbols failed."), QStringLiteral("Could not load"),
                                                   QStringLiteral("RETRY"), QStringLiteral("Traceback: …"));
    spec.secondary = QStringLiteral("OPEN LOG");
    spec.details = {{QStringLiteral("AAPL"), QStringLiteral("timeout")}, {QStringLiteral("MSFT"), QStringLiteral("no data")}};
    spec.details_heading = QStringLiteral("FAILED");
    spec.details_clickable = true;
    spec.suggestions = {{QStringLiteral("Bulk carriers"), {}, {}}, {QStringLiteral("Tankers"), {}, {}}};
    return spec;
}

/// The EmptyState's focusable parts in focus-chain order, by a readable name.
QStringList chain_names(QWidget* es) {
    QStringList out;
    QWidget* w = es;
    for (int guard = 0; guard < 200; ++guard) {
        w = w->nextInFocusChain();
        if (!w || w == es)
            break;
        if (!es->isAncestorOf(w))
            continue;
        const QString on = w->objectName();
        if (on == QLatin1String("feEsLink"))
            out << QStringLiteral("DETAILS");
        else if (on == QLatin1String("feEsPrimary"))
            out << QStringLiteral("PRIMARY");
        else if (on == QLatin1String("feEsSecondary"))
            out << QStringLiteral("SECONDARY");
        else if (qobject_cast<QAbstractButton*>(w) && !qobject_cast<QPushButton*>(w) && !w->accessibleName().isEmpty() &&
                 !w->isHidden())
            out << w->accessibleName(); // detail rows and suggestion rows
    }
    return out;
}

const QStringList kExpected = {QStringLiteral("DETAILS"), QStringLiteral("AAPL"), QStringLiteral("MSFT"),
                               QStringLiteral("PRIMARY"), QStringLiteral("SECONDARY"), QStringLiteral("Bulk carriers"),
                               QStringLiteral("Tankers")};

} // namespace

class TstEmptyStateTabChain : public QObject {
    Q_OBJECT
  private slots:
    void init() { g_tab_order_warnings.clear(); }

    void unparented_then_parented_and_shown() {
        auto* es = new fincept::ui::EmptyState(); // built before it is added to a layout
        es->apply(full_spec());
        QVERIFY2(g_tab_order_warnings.isEmpty(), qPrintable(g_tab_order_warnings.join('\n')));

        QWidget host;
        host.resize(900, 700);
        auto* lay = new QVBoxLayout(&host);
        lay->addWidget(es); // parent change
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QCoreApplication::processEvents();
        QVERIFY2(g_tab_order_warnings.isEmpty(), qPrintable(g_tab_order_warnings.join('\n')));
        QCOMPARE(chain_names(es), kExpected);

        // Re-applying with new rows while in the window: still no warning, same reading order.
        auto spec = full_spec();
        spec.details = {{QStringLiteral("IBM"), QStringLiteral("rate limited")}};
        es->apply(spec);
        QCoreApplication::processEvents();
        QVERIFY2(g_tab_order_warnings.isEmpty(), qPrintable(g_tab_order_warnings.join('\n')));
        QCOMPARE(chain_names(es), QStringList({QStringLiteral("DETAILS"), QStringLiteral("IBM"), QStringLiteral("PRIMARY"),
                                               QStringLiteral("SECONDARY"), QStringLiteral("Bulk carriers"),
                                               QStringLiteral("Tankers")}));
    }

    void unparented_shown_as_its_own_window() {
        fincept::ui::EmptyState es;
        es.apply(full_spec());
        es.resize(900, 700);
        es.show(); // never parented: chained on the first show
        QVERIFY(QTest::qWaitForWindowExposed(&es));
        QCoreApplication::processEvents();
        QVERIFY2(g_tab_order_warnings.isEmpty(), qPrintable(g_tab_order_warnings.join('\n')));
        QCOMPARE(chain_names(&es), kExpected);
    }

    void parented_from_the_start() {
        QWidget host;
        host.resize(900, 700);
        auto* lay = new QVBoxLayout(&host);
        auto* es = new fincept::ui::EmptyState(&host);
        lay->addWidget(es);
        es->apply(full_spec());
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QVERIFY2(g_tab_order_warnings.isEmpty(), qPrintable(g_tab_order_warnings.join('\n')));
        QCOMPARE(chain_names(es), kExpected);
    }

    void moved_between_windows() {
        QWidget a;
        QWidget b;
        auto* la = new QVBoxLayout(&a);
        auto* lb = new QVBoxLayout(&b);
        auto* es = new fincept::ui::EmptyState(&a);
        la->addWidget(es);
        es->apply(full_spec());
        a.show();
        lb->addWidget(es); // re-parented into another window, then re-applied there
        auto spec = full_spec();
        spec.secondary.clear();
        es->apply(spec);
        b.show();
        QCoreApplication::processEvents();
        QVERIFY2(g_tab_order_warnings.isEmpty(), qPrintable(g_tab_order_warnings.join('\n')));
    }
};

int main(int argc, char** argv) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    g_previous = qInstallMessageHandler(capture);
    TstEmptyStateTabChain t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_empty_state_tab_chain.moc"
