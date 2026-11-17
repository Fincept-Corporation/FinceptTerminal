// src/ui/navigation/KeyRouting.{h,cpp}: the routing rule (route_key, pure) and
// the application router against real QShortcut / QAction / widgets on the
// offscreen platform.
#include "ui/navigation/KeyRouting.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QShortcut>
#include <QVBoxLayout>
#include <QtTest>

using namespace fincept::ui;

namespace {

/// A panel that handles Esc itself (inline add row / un-maximise).
class EscPanel : public QWidget {
  public:
    int escapes = 0;

  protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape) {
            ++escapes;
            e->accept();
            return;
        }
        QWidget::keyPressEvent(e);
    }
};

struct Rig {
    QMainWindow window;
    EscPanel* screen = nullptr;
    QLineEdit* edit = nullptr;
    QListWidget* list = nullptr;
    QAction* f9_chat = nullptr; // the shell's F9 (WindowShortcut on the window)
    QAction* esc_shell = nullptr;
    QAction* down_shell = nullptr;
    int chat = 0, shell_esc = 0, shell_down = 0;

    Rig() {
        auto* central = new QWidget;
        auto* lay = new QVBoxLayout(central);
        screen = new EscPanel;
        auto* sl = new QVBoxLayout(screen);
        edit = new QLineEdit;
        list = new QListWidget;
        for (int i = 0; i < 5; ++i)
            list->addItem(QStringLiteral("row %1").arg(i));
        list->setCurrentRow(0);
        sl->addWidget(edit);
        sl->addWidget(list);
        lay->addWidget(screen);
        window.setCentralWidget(central);

        f9_chat = new QAction(&window);
        f9_chat->setShortcut(QKeySequence(Qt::Key_F9));
        f9_chat->setShortcutContext(Qt::WindowShortcut);
        window.addAction(f9_chat);
        QObject::connect(f9_chat, &QAction::triggered, [this]() { ++chat; });

        esc_shell = new QAction(&window);
        esc_shell->setShortcut(QKeySequence(Qt::Key_Escape));
        esc_shell->setShortcutContext(Qt::WindowShortcut);
        window.addAction(esc_shell);
        QObject::connect(esc_shell, &QAction::triggered, [this]() { ++shell_esc; });

        down_shell = new QAction(&window);
        down_shell->setShortcut(QKeySequence(Qt::Key_Down));
        down_shell->setShortcutContext(Qt::WindowShortcut);
        window.addAction(down_shell);
        QObject::connect(down_shell, &QAction::triggered, [this]() { ++shell_down; });

        window.resize(400, 300);
        window.show();
        window.activateWindow();
    }
};

} // namespace

class TestKeyRouting : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() { install_shell_key_router(qApp); }

    // ── Pure rule ─────────────────────────────────────────────────────────────
    void rule_function_keys_are_reserved() {
        KeyRouteInput in;
        in.key = Qt::Key_F9;
        in.narrow_matches = 1;
        in.broad_matches = 1;
        QCOMPARE(route_key(in), KeyRoute::Broadest);
        in.claimed = true;
        QCOMPARE(route_key(in), KeyRoute::Narrowest);
        in.narrow_matches = 0;
        QCOMPARE(route_key(in), KeyRoute::Widget);
        KeyRouteInput only_shell;
        only_shell.key = Qt::Key_F9;
        only_shell.broad_matches = 1;
        only_shell.focus = KeyFocusKind::TextInput;
        QCOMPARE(route_key(only_shell), KeyRoute::Qt); // F-keys are not "typing keys"
    }

    void rule_modifier_shortcuts_stay_global() {
        KeyRouteInput in;
        in.key = Qt::Key_L;
        in.command_modifier = true;
        in.narrow_matches = 1;
        in.broad_matches = 1;
        in.focus = KeyFocusKind::TextInput;
        QCOMPARE(route_key(in), KeyRoute::Broadest);
        in.narrow_matches = 0;
        QCOMPARE(route_key(in), KeyRoute::Qt);
    }

    void rule_popup_owns_navigation() {
        KeyRouteInput in;
        in.key = Qt::Key_Escape;
        in.popup_open = true;
        in.narrow_matches = 2;
        in.broad_matches = 1;
        QCOMPARE(route_key(in), KeyRoute::Widget);
    }

    void rule_narrowest_beats_shell() {
        KeyRouteInput in;
        in.key = Qt::Key_Escape;
        in.narrow_matches = 1;
        in.broad_matches = 1;
        QCOMPARE(route_key(in), KeyRoute::Narrowest);
        in.broad_matches = 0;
        QCOMPARE(route_key(in), KeyRoute::Narrowest);
    }

    void rule_inputs_and_lists_keep_their_keys() {
        KeyRouteInput in;
        in.broad_matches = 1;
        in.focus = KeyFocusKind::TextInput;
        for (int k : {int(Qt::Key_Escape), int(Qt::Key_Up), int(Qt::Key_Down), int(Qt::Key_Return), int(Qt::Key_Home)}) {
            in.key = k;
            QCOMPARE(route_key(in), KeyRoute::Widget);
        }
        in.key = Qt::Key_J;
        in.printable = true;
        QCOMPARE(route_key(in), KeyRoute::Widget);
        in.printable = false;
        in.focus = KeyFocusKind::ItemView;
        in.key = Qt::Key_Down;
        QCOMPARE(route_key(in), KeyRoute::Widget);
        in.focus = KeyFocusKind::Other;
        QCOMPARE(route_key(in), KeyRoute::Qt); // a button: the shell's Down
        in.focus = KeyFocusKind::TextInput;
        in.broad_matches = 0;
        QCOMPARE(route_key(in), KeyRoute::Qt); // nothing to resolve
    }

    // ── The router with real widgets ─────────────────────────────────────────
    void f9_is_chat_even_with_a_screen_f9() {
        Rig rig;
        QVERIFY(QTest::qWaitForWindowActive(&rig.window));
        int screen_f9 = 0;
        auto* s = new QShortcut(QKeySequence(Qt::Key_F9), rig.screen);
        s->setContext(Qt::WidgetWithChildrenShortcut);
        connect(s, &QShortcut::activated, [&]() { ++screen_f9; });
        rig.list->setFocus();
        QTest::keyClick(rig.list, Qt::Key_F9);
        QCOMPARE(rig.chat, 1); // Qt alone: ambiguous, neither fired
        QCOMPARE(screen_f9, 0);

        claim_keys(rig.screen, {QKeySequence(Qt::Key_F9)}); // the screen claims F9
        QTest::keyClick(rig.list, Qt::Key_F9);
        QCOMPARE(screen_f9, 1);
        QCOMPARE(rig.chat, 1);
    }

    void esc_in_input_reaches_the_panel_not_the_shell() {
        Rig rig;
        QVERIFY(QTest::qWaitForWindowActive(&rig.window));
        rig.edit->setFocus();
        QTest::keyClick(rig.edit, Qt::Key_Escape);
        QCOMPARE(rig.screen->escapes, 1);
        QCOMPARE(rig.shell_esc, 0);
    }

    void narrow_widget_shortcut_on_input_wins() {
        Rig rig;
        QVERIFY(QTest::qWaitForWindowActive(&rig.window));
        int clears = 0;
        auto* s = new QShortcut(QKeySequence(Qt::Key_Escape), rig.edit);
        s->setContext(Qt::WidgetShortcut);
        connect(s, &QShortcut::activated, [&]() { ++clears; });
        rig.edit->setFocus();
        QTest::keyClick(rig.edit, Qt::Key_Escape);
        QCOMPARE(clears, 1);
        QCOMPARE(rig.shell_esc, 0);
        QCOMPARE(rig.screen->escapes, 0);
    }

    void down_in_list_moves_the_list() {
        Rig rig;
        QVERIFY(QTest::qWaitForWindowActive(&rig.window));
        rig.list->setFocus();
        QCOMPARE(rig.list->currentRow(), 0);
        QTest::keyClick(rig.list, Qt::Key_Down);
        QCOMPARE(rig.list->currentRow(), 1);
        QCOMPARE(rig.shell_down, 0);
    }

    void widget_that_accepts_override_keeps_the_key() {
        Rig rig;
        QVERIFY(QTest::qWaitForWindowActive(&rig.window));
        // A pop-up-list owner: accepts the override itself (SymbolSearchBox does).
        struct Accepter : QObject {
            bool eventFilter(QObject*, QEvent* e) override {
                if (e->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent*>(e)->key() == Qt::Key_F9) {
                    e->accept();
                    return true;
                }
                return false;
            }
        } accepter;
        rig.edit->installEventFilter(&accepter);
        rig.edit->setFocus();
        QTest::keyClick(rig.edit, Qt::Key_F9);
        QCOMPARE(rig.chat, 0);
        rig.edit->removeEventFilter(&accepter);
    }

    void shell_key_still_works_elsewhere() {
        Rig rig;
        QVERIFY(QTest::qWaitForWindowActive(&rig.window));
        auto* button_like = new QWidget(rig.screen);
        button_like->setFocusPolicy(Qt::StrongFocus);
        button_like->show();
        button_like->setFocus();
        QTest::keyClick(button_like, Qt::Key_Down);
        QCOMPARE(rig.shell_down, 1);
        QTest::keyClick(button_like, Qt::Key_F9);
        QCOMPARE(rig.chat, 1);
    }
};

QTEST_MAIN(TestKeyRouting)
#include "tst_key_routing.moc"
