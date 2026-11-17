// UiActEdit.cpp — ui_act on native widgets: event synthesis and, per widget
// family, the closure that performs a step (buttons, fields, combos, spin /
// date / slider values, check boxes and radios, tabs, menus, table rows and
// cells, scrolling). Validation happens here, before anything is done.

#include "core/logging/Logger.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiActInternal.h"
#include "mcp/core/UiErrors.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QSpinBox>
#include <QTabBar>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QValidator>

namespace fincept::mcp::core::ui::act {

std::optional<KeyChord> parse_key(const QString& raw) {
    static const QHash<QString, QString> alias = {
        {QStringLiteral("enter"), QStringLiteral("Return")},
        {QStringLiteral("return"), QStringLiteral("Return")},
        {QStringLiteral("escape"), QStringLiteral("Esc")},
        {QStringLiteral("esc"), QStringLiteral("Esc")},
        {QStringLiteral("pageup"), QStringLiteral("PgUp")},
        {QStringLiteral("pagedown"), QStringLiteral("PgDown")},
        {QStringLiteral("page up"), QStringLiteral("PgUp")},
        {QStringLiteral("page down"), QStringLiteral("PgDown")},
        {QStringLiteral("control"), QStringLiteral("Ctrl")},
        {QStringLiteral("ctrl"), QStringLiteral("Ctrl")},
        {QStringLiteral("cmd"), QStringLiteral("Ctrl")},
        {QStringLiteral("command"), QStringLiteral("Ctrl")},
        {QStringLiteral("option"), QStringLiteral("Alt")},
        {QStringLiteral("alt"), QStringLiteral("Alt")},
        {QStringLiteral("shift"), QStringLiteral("Shift")},
        {QStringLiteral("meta"), QStringLiteral("Meta")},
        {QStringLiteral("del"), QStringLiteral("Del")},
        {QStringLiteral("delete"), QStringLiteral("Del")},
        {QStringLiteral("backspace"), QStringLiteral("Backspace")},
        {QStringLiteral("space"), QStringLiteral("Space")},
        {QStringLiteral("tab"), QStringLiteral("Tab")},
        {QStringLiteral("up"), QStringLiteral("Up")},
        {QStringLiteral("down"), QStringLiteral("Down")},
        {QStringLiteral("left"), QStringLiteral("Left")},
        {QStringLiteral("right"), QStringLiteral("Right")},
        {QStringLiteral("home"), QStringLiteral("Home")},
        {QStringLiteral("end"), QStringLiteral("End")},
        {QStringLiteral("ins"), QStringLiteral("Ins")},
    };
    QStringList parts;
    for (const QString& p : raw.split(QLatin1Char('+'), Qt::SkipEmptyParts)) {
        const QString t = p.trimmed();
        parts << alias.value(t.toLower(), t.size() == 1 ? t.toUpper() : t);
    }
    if (parts.isEmpty())
        return std::nullopt;
    const QKeySequence seq = QKeySequence::fromString(parts.join(QLatin1Char('+')), QKeySequence::PortableText);
    if (seq.isEmpty() || seq[0].key() == Qt::Key_unknown)
        return std::nullopt;
    KeyChord k;
    k.key = seq[0].key();
    k.mods = seq[0].keyboardModifiers();
    k.seq = QKeySequence(seq[0]);
    if (!(k.mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        if (k.key >= Qt::Key_Space && k.key <= Qt::Key_AsciiTilde) {
            QChar c(k.key);
            k.text = (k.mods & Qt::ShiftModifier) ? QString(c.toUpper()) : QString(c.toLower());
        } else if (k.key == Qt::Key_Return || k.key == Qt::Key_Enter) {
            k.text = QStringLiteral("\r");
        } else if (k.key == Qt::Key_Tab) {
            k.text = QStringLiteral("\t");
        }
    }
    return k;
}

// ── Event synthesis ─────────────────────────────────────────────────────────

void send_mouse(QWidget* w, QPoint pos, Qt::MouseButton button, bool dbl) {
    QPointer<QWidget> guard(w);
    auto send = [&](QEvent::Type type, Qt::MouseButton b, Qt::MouseButtons held) {
        if (!guard)
            return;
        const QPointF local(pos);
        const QPointF scene = guard->mapTo(guard->window(), pos);
        const QPointF global = guard->mapToGlobal(pos);
        QMouseEvent ev(type, local, scene, global, b, held, Qt::NoModifier);
        QCoreApplication::sendEvent(guard, &ev);
    };
    send(QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
    send(QEvent::MouseButtonPress, button, button);
    send(QEvent::MouseButtonRelease, button, Qt::NoButton);
    if (dbl) {
        send(QEvent::MouseButtonDblClick, button, button);
        send(QEvent::MouseButtonRelease, button, Qt::NoButton);
    }
}

void send_key(QWidget* w, int key, Qt::KeyboardModifiers mods, const QString& text) {
    QPointer<QWidget> guard(w);
    QKeyEvent press(QEvent::KeyPress, key, mods, text);
    QCoreApplication::sendEvent(guard, &press);
    if (!guard)
        return;
    QKeyEvent release(QEvent::KeyRelease, key, mods, text);
    QCoreApplication::sendEvent(guard, &release);
}

/// Trigger an action / QShortcut bound to `seq` in the window around `w`.
bool trigger_shortcut(QWidget* w, const QKeySequence& seq) {
    QWidget* top = w ? w->window() : nullptr;
    if (!top)
        return false;
    QList<QAction*> actions = top->actions();
    actions += top->findChildren<QAction*>();
    for (QAction* a : actions) {
        if (!a->isEnabled())
            continue;
        for (const QKeySequence& s : a->shortcuts()) {
            if (s == seq) {
                a->trigger();
                return true;
            }
        }
    }
    for (QShortcut* sc : top->findChildren<QShortcut*>()) {
        if (!sc->isEnabled())
            continue;
        for (const QKeySequence& s : sc->keys()) {
            if (s == seq) {
                emit sc->activated();
                return true;
            }
        }
    }
    return false;
}

namespace {

/// The entry the caller means among `texts`: an exact (case-insensitive) match
/// first, then the ONLY entry starting with the words, then the ONLY entry
/// containing them. Several equally good entries: -1, and `*ambiguous` lists them.
int pick_text(const QStringList& texts, const QString& wanted, QStringList* ambiguous) {
    const QString w = wanted.trimmed();
    if (w.isEmpty())
        return -1;
    for (int i = 0; i < texts.size(); ++i) {
        if (texts[i].trimmed().compare(w, Qt::CaseInsensitive) == 0)
            return i;
    }
    for (int pass = 0; pass < 2; ++pass) {
        QList<int> hits;
        for (int i = 0; i < texts.size(); ++i) {
            const QString t = texts[i].trimmed();
            if (pass == 0 ? t.startsWith(w, Qt::CaseInsensitive) : t.contains(w, Qt::CaseInsensitive))
                hits << i;
        }
        if (hits.size() == 1)
            return hits.first();
        if (hits.size() > 1) {
            if (ambiguous) {
                ambiguous->clear();
                for (int i : hits.mid(0, 8))
                    *ambiguous << texts[i].trimmed();
                if (hits.size() > 8)
                    *ambiguous << QStringLiteral("+%1 more").arg(hits.size() - 8);
            }
            return -1;
        }
    }
    return -1;
}

QString no_option_error(const QString& what, const QString& wanted, const QStringList& several) {
    if (!several.isEmpty())
        return err_text(Err::Ambiguous,
                        QStringLiteral("\"%1\" fits several options of %2: %3.")
                            .arg(wanted, what, several.join(QStringLiteral(" | "))),
                        QStringLiteral("use the option's full text, or #<index>."));
    return err_text(Err::NotFound, QStringLiteral("%1 has no option \"%2\".").arg(what, wanted),
                    QStringLiteral("ui_read(ref=…) lists its options."));
}

} // namespace

int find_option(QComboBox* cb, const QString& wanted, QStringList* ambiguous) {
    const QString w = wanted.trimmed();
    if (w.startsWith(QLatin1Char('#'))) {
        bool ok = false;
        const int i = w.mid(1).toInt(&ok);
        if (ok && i >= 0 && i < cb->count())
            return i;
    }
    QStringList texts;
    texts.reserve(cb->count());
    for (int i = 0; i < cb->count(); ++i) {
        texts << cb->itemText(i);
        if (texts.last().trimmed().compare(w, Qt::CaseInsensitive) == 0)
            return i;
    }
    // The option's data ("NSE" behind "National Stock Exchange") is as exact a name.
    for (int i = 0; i < cb->count(); ++i) {
        const QVariant d = cb->itemData(i);
        if (d.isValid() && !w.isEmpty() && d.toString().compare(w, Qt::CaseInsensitive) == 0)
            return i;
    }
    return pick_text(texts, w, ambiguous);
}

int find_tab(QTabBar* bar, const QString& wanted, QStringList* ambiguous = nullptr) {
    QStringList texts;
    for (int i = 0; i < bar->count(); ++i)
        texts << strip_mnemonic(bar->tabText(i));
    return pick_text(texts, wanted, ambiguous);
}

QAction* find_menu_action(QWidget* menu_or_bar, const QString& wanted, QStringList* ambiguous = nullptr) {
    QList<QAction*> usable;
    QStringList texts;
    for (QAction* a : menu_or_bar->actions()) {
        if (!a->isVisible() || a->isSeparator())
            continue;
        usable << a;
        texts << strip_mnemonic(a->text());
    }
    const int i = pick_text(texts, wanted, ambiguous);
    return i >= 0 ? usable.value(i) : nullptr;
}

QMenu* menu_showing(QAction* a) {
    if (!a)
        return nullptr;
    for (QObject* o : a->associatedObjects()) {
        if (auto* m = qobject_cast<QMenu*>(o); m && m->isVisible())
            return m;
    }
    return nullptr;
}

void click_view_row(QAbstractItemView* view, int row, bool dbl, Qt::MouseButton button, int column = -1) {
    const QModelIndex idx = view_index(view, row, column >= 0 ? column : visible_columns(view).value(0, 0));
    if (!idx.isValid())
        return;
    view->scrollTo(idx);
    const QRect rect = view->visualRect(idx);
    if (button == Qt::RightButton) {
        if (QItemSelectionModel* sel = view->selectionModel())
            sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QContextMenuEvent ev(QContextMenuEvent::Mouse, rect.center(), view->viewport()->mapToGlobal(rect.center()));
        QCoreApplication::sendEvent(view->viewport(), &ev);
        return;
    }
    send_mouse(view->viewport(), rect.center(), button, dbl);
}

std::optional<Prepared> prepare(const Step& s, const Target& t, QString* error) {
    if (t.web_view)
        return prepare_web(s, t, error);
    QWidget* w = host_widget(t);
    QAction* a = t.action;
    const QString what = describe_target(t);
    QPointer<QWidget> wg(w);
    QPointer<QAction> ag(a);
    QPointer<QWidget> dock(t.dock);
    Prepared p;
    p.note = QStringLiteral("%1 %2").arg(s.action, what);

    // ── Panel-level actions ──
    if (t.role == QLatin1String("panel")) {
        if (s.action == QLatin1String("close")) {
            p.perform = [dock]() {
                if (dock)
                    host::close_panel(dock);
            };
            return p;
        }
        if (s.action == QLatin1String("focus")) {
            p.perform = [dock]() {
                if (dock)
                    host::focus_panel(dock);
            };
            return p;
        }
        if (s.action == QLatin1String("scroll")) {
            QAbstractScrollArea* area = nullptr;
            if (t.dock) {
                for (QAbstractScrollArea* sa : t.dock->findChildren<QAbstractScrollArea*>()) {
                    if (sa->isVisible() && sa->verticalScrollBar() && sa->verticalScrollBar()->maximum() > 0) {
                        area = sa;
                        break;
                    }
                }
            }
            if (!area) {
                *error = err_text(Err::Unsupported, QStringLiteral("Nothing scrolls in %1.").arg(what));
                return std::nullopt;
            }
            QPointer<QAbstractScrollArea> sg(area);
            const int amount = s.amount;
            p.perform = [sg, amount]() {
                if (!sg)
                    return;
                QScrollBar* sb = sg->verticalScrollBar();
                sb->setValue(sb->value() + (amount == 0 ? sb->pageStep() : amount * sb->singleStep() * 3));
            };
            return p;
        }
        *error = err_text(Err::Unsupported, QStringLiteral("%1: a panel can be focused, closed or scrolled.").arg(what),
                          QStringLiteral("act on its elements (ui_snapshot(target=\"%1\")).").arg(t.ref));
        return std::nullopt;
    }

    // ── Menu items ──
    if (a) {
        if (s.action != QLatin1String("click") && s.action != QLatin1String("select") &&
            s.action != QLatin1String("check") && s.action != QLatin1String("uncheck") &&
            s.action != QLatin1String("expand") && s.action != QLatin1String("hover")) {
            *error = err_text(Err::Unsupported,
                              QStringLiteral("%1 is a menu item: click / select / check / uncheck / expand it.").arg(what));
            return std::nullopt;
        }
        if (!a->isEnabled()) {
            *error = err_text(Err::Disabled, QStringLiteral("%1 is disabled.").arg(what));
            return std::nullopt;
        }
        if ((s.action == QLatin1String("check") && a->isChecked()) ||
            (s.action == QLatin1String("uncheck") && !a->isChecked())) {
            p.perform = []() {};
            p.note += QStringLiteral(" (already %1)")
                          .arg(a->isChecked() ? QStringLiteral("checked") : QStringLiteral("unchecked"));
            return p;
        }
        QPointer<QWidget> host(t.widget);
        p.perform = [ag, host]() {
            if (!ag)
                return;
            if (auto* bar = qobject_cast<QMenuBar*>(host.data())) {
                if (ag->menu())
                    bar->setActiveAction(ag); // opens its menu (non-blocking popup)
                else
                    ag->trigger();
                return;
            }
            if (QMenu* menu = menu_showing(ag)) {
                // As a person does it: highlight, then Enter — the menu closes and
                // an exec()'d menu returns this action to its caller.
                menu->setActiveAction(ag);
                if (ag->menu())
                    return; // a submenu opens on activation
                send_key(menu, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
                return;
            }
            ag->trigger();
        };
        return p;
    }

    if (!w) {
        *error = err_text(Err::StaleRef, QStringLiteral("%1 is gone.").arg(what), QStringLiteral("take a fresh ui_snapshot."));
        return std::nullopt;
    }

    // ── Dialog-level ──
    if (s.action == QLatin1String("close")) {
        if (qobject_cast<QMenu*>(w)) {
            p.perform = [wg]() {
                if (wg)
                    wg->close();
            };
            return p;
        }
        QWidget* top = w->window();
        if (auto* dlg = qobject_cast<QDialog*>(top)) {
            QPointer<QDialog> dg(dlg);
            p.perform = [dg]() {
                if (dg)
                    dg->reject();
            };
            p.note = QStringLiteral("close %1").arg(location_label(dlg));
            return p;
        }
        if (top && top->windowType() == Qt::Dialog) {
            QPointer<QWidget> tg(top);
            p.perform = [tg]() {
                if (tg)
                    tg->close();
            };
            return p;
        }
        if (QComboBox* cb = qobject_cast<QComboBox*>(w)) {
            QPointer<QComboBox> cg(cb);
            p.perform = [cg]() {
                if (cg)
                    cg->hidePopup();
            };
            return p;
        }
        *error = err_text(Err::Unsupported, QStringLiteral("%1 is not a dialog, menu or popup.").arg(what),
                          QStringLiteral("close a panel with ui_workspace(action=\"close\")."));
        return std::nullopt;
    }

    const bool needs_enabled =
        s.action != QLatin1String("hover") && s.action != QLatin1String("scroll") && s.action != QLatin1String("focus");
    if (needs_enabled && !w->isEnabled()) {
        *error = err_text(Err::Disabled, QStringLiteral("%1 is disabled.").arg(what),
                          QStringLiteral("something else must be set first, or wait for it: ui_wait(until=\"state\", "
                                         "ref=…, state=\"enabled\")."));
        return std::nullopt;
    }
    bool in_cell = false;
    if (!w->isVisible()) {
        // A control in a table row that is scrolled out of view: bring the row
        // in and go on (clicks, checks, selects and typing reach it either way).
        QModelIndex cell;
        if (QAbstractItemView* owner = cell_widget_owner(w, &cell)) {
            owner->scrollTo(cell);
            in_cell = true;
        } else {
            *error = err_text(Err::Obscured,
                              QStringLiteral("%1 is not visible (another tab, or a hidden section).").arg(what),
                              QStringLiteral("open the tab / section that holds it first."));
            return std::nullopt;
        }
    }
    // A plain control a person would click or type into: in view, and not under
    // a loading overlay or another pane (a point given as x/y is taken as is).
    const bool hands_on = s.action == QLatin1String("click") || s.action == QLatin1String("double_click") ||
                          s.action == QLatin1String("right_click") || s.action == QLatin1String("type") ||
                          s.action == QLatin1String("clear") || s.action == QLatin1String("check") ||
                          s.action == QLatin1String("uncheck") || s.action == QLatin1String("select") ||
                          s.action == QLatin1String("set_value");
    if (hands_on && !in_cell && !t.has_hit && !s.has_xy && t.row < 0 && !qobject_cast<QTabBar*>(w) &&
        !qobject_cast<QAbstractItemView*>(w) && !w->isWindow()) {
        const QString blocked = ensure_operable(w, what);
        if (!blocked.isEmpty()) {
            *error = blocked;
            return std::nullopt;
        }
    }

    // ── Tabs ──
    if (auto* bar = qobject_cast<QTabBar*>(w)) {
        int index = t.sub;
        QStringList several;
        if ((s.action == QLatin1String("select") || index < 0) && !s.text.isEmpty())
            index = find_tab(bar, s.text, &several);
        if (index < 0 || index >= bar->count()) {
            *error = several.isEmpty()
                         ? err_text(Err::NotFound, QStringLiteral("No tab \"%1\" in %2.").arg(s.text, what))
                         : err_text(Err::Ambiguous,
                                    QStringLiteral("\"%1\" fits several tabs of %2: %3.")
                                        .arg(s.text, what, several.join(QStringLiteral(" | "))),
                                    QStringLiteral("use the tab's full name."));
            return std::nullopt;
        }
        if (!bar->isTabEnabled(index)) {
            *error = err_text(Err::Disabled, QStringLiteral("Tab \"%1\" is disabled.").arg(bar->tabText(index)));
            return std::nullopt;
        }
        if (s.action == QLatin1String("close")) {
            *error = QStringLiteral("Tabs are switched, not closed, here.");
            return std::nullopt;
        }
        QPointer<QTabBar> bg(bar);
        p.perform = [bg, index]() {
            if (!bg)
                return;
            emit bg->tabBarClicked(index);
            bg->setCurrentIndex(index);
        };
        p.note = QStringLiteral("select tab %1").arg(common::quoted(bar->tabText(index).remove(QLatin1Char('&'))));
        return p;
    }

    const QString act = s.action;

    // ── Item views (tables, lists, trees) ──
    if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        QPointer<QAbstractItemView> vg(view);
        int row = t.row;
        const bool cell_edit =
            act == QLatin1String("type") || act == QLatin1String("set_value") || act == QLatin1String("clear");
        if (row < 0 && !s.text.isEmpty() && !cell_edit)
            row = view_find_row(view, s.text);
        if (act == QLatin1String("scroll")) {
            const int amount = s.amount;
            p.perform = [vg, amount]() {
                if (!vg)
                    return;
                QScrollBar* sb = vg->verticalScrollBar();
                sb->setValue(sb->value() + (amount == 0 ? sb->pageStep() : amount * sb->singleStep() * 3));
            };
            return p;
        }
        if (act == QLatin1String("focus")) {
            p.perform = [vg]() {
                if (vg)
                    vg->setFocus(Qt::OtherFocusReason);
            };
            return p;
        }
        if (row < 0) {
            *error = s.text.isEmpty() || cell_edit
                         ? err_text(Err::InvalidValue,
                                    QStringLiteral("%1: say which row — a row ref like %2:r0%3.")
                                        .arg(what, t.ref.section(QLatin1Char(':'), 0, 0),
                                             cell_edit ? QStringLiteral(" and the cell's column") : QStringLiteral(", or `text` the row holds")))
                         : err_text(Err::NotFound, QStringLiteral("No row of %1 holds \"%2\".").arg(what, s.text),
                                    QStringLiteral("ui_read(ref=\"%1\", query=…) finds rows.")
                                        .arg(t.ref.section(QLatin1Char(':'), 0, 0)));
            return std::nullopt;
        }
        if (!view_index(view, row, 0).isValid()) {
            *error = err_text(Err::NotFound,
                              QStringLiteral("%1 has no row %2 (it has %3).").arg(what).arg(row).arg(view_row_count(view)));
            return std::nullopt;
        }
        int column = t.column;
        if (!s.column.isEmpty()) {
            column = resolve_column(view, s.column);
            if (column < 0) {
                *error = err_text(Err::NotFound,
                                  QStringLiteral("%1 has no column \"%2\" — columns: %3.")
                                      .arg(what, s.column,
                                           common::clip(column_headers(view, visible_columns(view)).join(QStringLiteral(" | ")),
                                                        400)));
                return std::nullopt;
            }
        }
        // ── A cell's value, edited through the model as its editor would ──
        if (cell_edit) {
            if (column < 0) {
                *error = err_text(Err::InvalidValue, QStringLiteral("%1: say which cell of row r%2 — `column`.").arg(what).arg(row));
                return std::nullopt;
            }
            const QModelIndex idx = view_index(view, row, column);
            if (!idx.isValid() || !(view->model()->flags(idx) & Qt::ItemIsEditable) ||
                view->editTriggers() == QAbstractItemView::NoEditTriggers) {
                *error = err_text(Err::Unsupported, QStringLiteral("That cell of %1 cannot be edited.").arg(what));
                return std::nullopt;
            }
            const QString raw_text = act == QLatin1String("clear")       ? QString()
                                     : act == QLatin1String("set_value") && !s.value.isUndefined() && !s.value.isNull()
                                         ? (s.value.isDouble() ? QString::number(s.value.toDouble(), 'g', 15)
                                                               : s.value.toVariant().toString())
                                         : s.text;
            const QVariant current = idx.data(Qt::EditRole);
            QVariant value = raw_text;
            bool ok = true;
            switch (current.typeId()) {
                case QMetaType::Int:
                case QMetaType::LongLong:
                case QMetaType::UInt:
                case QMetaType::ULongLong: {
                    const qlonglong n = raw_text.trimmed().toLongLong(&ok);
                    value = n;
                    break;
                }
                case QMetaType::Double:
                case QMetaType::Float: {
                    const double d = raw_text.trimmed().toDouble(&ok);
                    value = d;
                    break;
                }
                case QMetaType::Bool: {
                    const QString b = raw_text.trimmed().toLower();
                    ok = b == QLatin1String("true") || b == QLatin1String("false") || b == QLatin1String("1") ||
                         b == QLatin1String("0") || b == QLatin1String("yes") || b == QLatin1String("no");
                    value = b == QLatin1String("true") || b == QLatin1String("1") || b == QLatin1String("yes");
                    break;
                }
                case QMetaType::QDate: {
                    const QDate d = QDate::fromString(raw_text.trimmed(), Qt::ISODate);
                    ok = d.isValid();
                    value = d;
                    break;
                }
                case QMetaType::QDateTime: {
                    const QDateTime d = QDateTime::fromString(raw_text.trimmed(), Qt::ISODate);
                    ok = d.isValid();
                    value = d;
                    break;
                }
                default:
                    break;
            }
            if (!ok) {
                *error = err_text(Err::InvalidValue,
                                  QStringLiteral("\"%1\" does not fit that cell (it holds a %2).")
                                      .arg(common::clip(raw_text, 40), QString::fromLatin1(current.typeName())));
                return std::nullopt;
            }
            const QString header = view->model()->headerData(column, Qt::Horizontal).toString();
            auto failed = std::make_shared<QString>();
            p.failed = failed;
            p.perform = [vg, row, column, value, failed]() {
                if (!vg || !vg->model())
                    return;
                const QModelIndex i = view_index(vg, row, column);
                if (!i.isValid() || !vg->model()->setData(i, value, Qt::EditRole))
                    *failed = QStringLiteral("the table did not accept the value");
            };
            p.note = QStringLiteral("set %1 of row r%2 of %3 to %4")
                         .arg(header.isEmpty() ? QStringLiteral("column %1").arg(column) : common::quoted(header))
                         .arg(row)
                         .arg(what, common::quoted(common::clip(raw_text, 60)));
            return p;
        }
        p.note = QStringLiteral("%1 row r%2%3%4 of %5: %6")
                     .arg(act, QString::number(row),
                          t.listed_row >= 0 ? QStringLiteral(" (listed as r%1; the list moved)").arg(t.listed_row)
                                            : QString(),
                          column >= 0
                              ? QStringLiteral(", column %1")
                                    .arg(common::quoted(view->model()->headerData(column, Qt::Horizontal).toString()))
                              : QString(),
                          what, common::quoted(common::clip(view_row_text(view, row, visible_columns(view), 30), 90)));
        const bool at_point = t.has_hit; // a click at x/y that landed on this row
        const QPoint point = t.hit_pos;
        auto mouse = [vg, row, column, at_point, point](bool dbl, Qt::MouseButton button) {
            if (!vg)
                return;
            if (at_point && button == Qt::LeftButton) {
                send_mouse(vg->viewport(), point, button, dbl);
                return;
            }
            click_view_row(vg, row, dbl, button, column);
        };
        if (act == QLatin1String("click") || act == QLatin1String("select")) {
            p.perform = [mouse]() { mouse(false, Qt::LeftButton); };
            return p;
        }
        if (act == QLatin1String("double_click")) {
            p.perform = [mouse]() { mouse(true, Qt::LeftButton); };
            return p;
        }
        if (act == QLatin1String("right_click")) {
            p.perform = [mouse]() { mouse(false, Qt::RightButton); };
            return p;
        }
        if (act == QLatin1String("expand") || act == QLatin1String("collapse")) {
            auto* tree = qobject_cast<QTreeView*>(view);
            if (!tree) {
                *error = err_text(Err::Unsupported, QStringLiteral("%1 is not a tree.").arg(what));
                return std::nullopt;
            }
            const bool expand = act == QLatin1String("expand");
            QPointer<QTreeView> tg(tree);
            p.perform = [tg, row, expand]() {
                if (!tg)
                    return;
                const QModelIndex idx = view_index(tg, row, 0);
                if (idx.isValid())
                    tg->setExpanded(idx, expand);
            };
            return p;
        }
        if (act == QLatin1String("check") || act == QLatin1String("uncheck")) {
            const QModelIndex idx = view_index(view, row, visible_columns(view).value(0, 0));
            if (!(view->model()->flags(idx) & Qt::ItemIsUserCheckable)) {
                *error = err_text(Err::Unsupported, QStringLiteral("Row r%1 of %2 has no checkbox.").arg(row).arg(what));
                return std::nullopt;
            }
            const Qt::CheckState want = act == QLatin1String("check") ? Qt::Checked : Qt::Unchecked;
            p.perform = [vg, row, want]() {
                if (!vg)
                    return;
                const QModelIndex i = view_index(vg, row, visible_columns(vg).value(0, 0));
                if (i.isValid())
                    vg->model()->setData(i, static_cast<int>(want), Qt::CheckStateRole);
            };
            return p;
        }
        *error = err_text(Err::Unsupported,
                          QStringLiteral("%1: rows can be clicked, double-clicked, right-clicked, selected, checked, "
                                         "expanded, or have a cell set (type / set_value with `column`).")
                              .arg(what));
        return std::nullopt;
    }

    // ── Typing ──
    if (act == QLatin1String("type") || act == QLatin1String("clear")) {
        if (is_secret(w)) {
            *error = err_text(Err::Denied,
                              QStringLiteral("%1 is a secret field (password / API key / token) — agents never type "
                                             "secrets.")
                                  .arg(what),
                              QStringLiteral("ask the user to type it."));
            return std::nullopt;
        }
        const QString text = act == QLatin1String("clear") ? QString() : s.text;
        const bool append = s.append && act == QLatin1String("type");
        const bool submit = s.submit && act == QLatin1String("type");
        QLineEdit* le = qobject_cast<QLineEdit*>(w);
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            if (cb->isEditable())
                le = cb->lineEdit();
            else if (act == QLatin1String("type")) {
                QStringList several;
                const int idx = find_option(cb, text, &several);
                if (idx < 0) {
                    *error = no_option_error(what, text, several);
                    return std::nullopt;
                }
                QPointer<QComboBox> cg(cb);
                p.perform = [cg, idx]() {
                    if (!cg)
                        return;
                    cg->setCurrentIndex(idx);
                    emit cg->activated(idx);
                    emit cg->textActivated(cg->itemText(idx));
                };
                p.note = QStringLiteral("select %1 in %2").arg(common::quoted(cb->itemText(idx)), what);
                return p;
            }
        }
        if (auto* sb = qobject_cast<QAbstractSpinBox*>(w); sb && !le) {
            le = sb->findChild<QLineEdit*>();
        }
        if (le) {
            if (le->isReadOnly()) {
                *error = err_text(Err::Disabled, QStringLiteral("%1 is read-only.").arg(what));
                return std::nullopt;
            }
            // What the field would hold; a validator or a length limit that refuses
            // it is reported now, before anything is typed.
            const QString would_hold = append ? le->text() + text : text;
            if (le->maxLength() > 0 && would_hold.size() > le->maxLength()) {
                *error = err_text(Err::InvalidValue,
                                  QStringLiteral("%1 takes at most %2 characters.").arg(what).arg(le->maxLength()));
                return std::nullopt;
            }
            if (const QValidator* v = le->validator(); v && !would_hold.isEmpty()) {
                QString probe = would_hold;
                int pos = static_cast<int>(probe.size());
                if (v->validate(probe, pos) == QValidator::Invalid) {
                    *error = err_text(Err::InvalidValue,
                                      QStringLiteral("%1 does not accept \"%2\".").arg(what, common::clip(text, 40)),
                                      QStringLiteral("check the field's format (ui_read(ref=…) shows its hint)."));
                    return std::nullopt;
                }
            }
            const bool plain = !qobject_cast<QAbstractSpinBox*>(le->parentWidget()) &&
                               !qobject_cast<QComboBox*>(le->parentWidget());
            bool symbol_box = false;
            for (QWidget* a2 = le->parentWidget(); a2 && !a2->isWindow(); a2 = a2->parentWidget()) {
                if (QByteArray(a2->metaObject()->className()).endsWith("SymbolSearchBox")) {
                    symbol_box = true;
                    break;
                }
            }
            auto failed = std::make_shared<QString>();
            p.failed = failed;
            p.field = le;
            p.symbol_box = symbol_box && submit;
            const bool check_text = plain && !symbol_box;
            QPointer<QLineEdit> lg(le);
            p.perform = [lg, text, append, submit, check_text, would_hold, failed]() {
                if (!lg)
                    return;
                const QString before = lg->text();
                lg->setFocus(Qt::OtherFocusReason);
                if (append)
                    lg->end(false);
                else
                    lg->selectAll();
                if (text.isEmpty() && !append)
                    lg->del();
                else
                    lg->insert(text); // emits textEdited, like typing
                if (!lg)
                    return;
                // A field that silently refused (an input mask, a validator) is a
                // failed step, not a typed one — and is not submitted. A field that
                // reformats what it got ("aapl" → "AAPL", "1000" → "1,000") took it:
                // the answer shows its new value.
                const QString now = lg->text();
                if (!lg->inputMask().isEmpty()) {
                    if (!lg->hasAcceptableInput())
                        *failed = QStringLiteral("the field's format did not accept it (it shows \"%1\")")
                                      .arg(common::clip(lg->displayText(), 40));
                } else if (check_text && now != would_hold) {
                    const bool kept_nothing = now == before && before != would_hold;
                    const bool cut_short = now.size() < would_hold.size() &&
                                           would_hold.startsWith(now, Qt::CaseInsensitive);
                    if (kept_nothing || cut_short)
                        *failed = QStringLiteral("the field holds \"%1\" — it did not take \"%2\"")
                                      .arg(common::clip(now, 40), common::clip(would_hold, 40));
                }
                if (failed->isEmpty() && submit && lg)
                    send_key(lg, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
            };
            p.note = act == QLatin1String("clear") ? QStringLiteral("clear %1").arg(what)
                                                   : QStringLiteral("type %1 into %2%3")
                                                         .arg(common::quoted(common::clip(text, 60)), what,
                                                              submit ? QStringLiteral(" + Enter") : QString());
            return p;
        }
        if (qobject_cast<QTextEdit*>(w) || qobject_cast<QPlainTextEdit*>(w)) {
            auto* te = qobject_cast<QTextEdit*>(w);
            auto* pe = qobject_cast<QPlainTextEdit*>(w);
            if ((te && te->isReadOnly()) || (pe && pe->isReadOnly())) {
                *error = err_text(Err::Disabled, QStringLiteral("%1 is read-only.").arg(what));
                return std::nullopt;
            }
            QPointer<QTextEdit> tg(te);
            QPointer<QPlainTextEdit> pg(pe);
            p.perform = [tg, pg, text, append]() {
                if (tg) {
                    tg->setFocus(Qt::OtherFocusReason);
                    if (append) {
                        tg->moveCursor(QTextCursor::End);
                        tg->insertPlainText(text);
                    } else {
                        tg->setPlainText(text);
                    }
                } else if (pg) {
                    pg->setFocus(Qt::OtherFocusReason);
                    if (append) {
                        pg->moveCursor(QTextCursor::End);
                        pg->insertPlainText(text);
                    } else {
                        pg->setPlainText(text);
                    }
                }
            };
            p.note = QStringLiteral("type %1 chars into %2").arg(text.size()).arg(what);
            return p;
        }
        // Anything else that takes keys: type it key by key — but never into a
        // button (Space clicks it), a dialog (a letter answers "&Yes") or a menu.
        if (qobject_cast<QAbstractButton*>(w) || qobject_cast<QDialog*>(w) || qobject_cast<QMenu*>(w) ||
            qobject_cast<QDialogButtonBox*>(w)) {
            *error = err_text(Err::Unsupported, QStringLiteral("%1 is not a text field.").arg(what),
                              QStringLiteral("use click (or select / press) instead of type."));
            return std::nullopt;
        }
        if (text.size() > 2000) {
            *error = err_text(Err::InvalidValue,
                              QStringLiteral("%1 is not a text field; typing more than 2000 keys into it is refused.")
                                  .arg(what));
            return std::nullopt;
        }
        p.perform = [wg, text, submit]() {
            if (!wg)
                return;
            wg->setFocus(Qt::OtherFocusReason);
            for (const QChar c : text) {
                if (!wg)
                    return;
                send_key(wg, c.toUpper().unicode(), c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier, QString(c));
            }
            if (submit && wg)
                send_key(wg, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
        };
        return p;
    }

    // ── Selecting an option ──
    if (act == QLatin1String("select")) {
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            const QString wanted = !s.text.isEmpty() ? s.text
                                   : s.value.isDouble() ? QString::number(s.value.toDouble(), 'g', 15)
                                                        : s.value.toString();
            QStringList several;
            const int idx = find_option(cb, wanted, &several);
            if (idx < 0) {
                *error = no_option_error(what, wanted, several);
                return std::nullopt;
            }
            QPointer<QComboBox> cg(cb);
            p.perform = [cg, idx]() {
                if (!cg)
                    return;
                cg->setCurrentIndex(idx);
                emit cg->activated(idx);
                emit cg->textActivated(cg->itemText(idx));
            };
            p.note = QStringLiteral("select %1 in %2").arg(common::quoted(cb->itemText(idx)), what);
            return p;
        }
        if (auto* menu = qobject_cast<QMenu*>(w)) {
            QStringList several;
            QAction* found = find_menu_action(menu, s.text, &several);
            if (!found) {
                *error = several.isEmpty()
                             ? err_text(Err::NotFound, QStringLiteral("The menu has no item \"%1\".").arg(s.text))
                             : err_text(Err::Ambiguous,
                                        QStringLiteral("\"%1\" fits several menu items: %2.")
                                            .arg(s.text, several.join(QStringLiteral(" | "))),
                                        QStringLiteral("use the item's full text or its ref."));
                return std::nullopt;
            }
            Target mt;
            mt.action = found;
            mt.widget = menu;
            classify_target(mt);
            Step click = s;
            click.action = QStringLiteral("click");
            return prepare(click, mt, error);
        }
        if (qobject_cast<QRadioButton*>(w) || qobject_cast<QCheckBox*>(w)) {
            Step click = s;
            click.action = QStringLiteral("check");
            return prepare(click, t, error);
        }
        // A radio group: the button named by the text (exact, else the only fit).
        if (!s.text.isEmpty()) {
            QList<QRadioButton*> radios;
            QStringList texts;
            for (QRadioButton* rb : w->findChildren<QRadioButton*>()) {
                if (rb->isVisible()) {
                    radios << rb;
                    texts << strip_mnemonic(rb->text());
                }
            }
            QStringList several;
            const int i = pick_text(texts, s.text, &several);
            if (i >= 0) {
                QRadioButton* rb = radios[i];
                if (!rb->isEnabled()) {
                    *error = err_text(Err::Disabled, QStringLiteral("%1 is disabled.").arg(common::quoted(texts[i])));
                    return std::nullopt;
                }
                QPointer<QRadioButton> rg(rb);
                p.perform = [rg]() {
                    if (rg && !rg->isChecked())
                        rg->click();
                };
                p.note = QStringLiteral("select %1").arg(common::quoted(texts[i]));
                return p;
            }
            if (!several.isEmpty()) {
                *error = err_text(Err::Ambiguous,
                                  QStringLiteral("\"%1\" fits several choices: %2.")
                                      .arg(s.text, several.join(QStringLiteral(" | "))),
                                  QStringLiteral("use the choice's full text."));
                return std::nullopt;
            }
        }
        *error = err_text(Err::Unsupported, QStringLiteral("%1 has no options to select.").arg(what),
                          QStringLiteral("click it, or use type / set_value."));
        return std::nullopt;
    }

    // ── Check / uncheck ──
    if (act == QLatin1String("check") || act == QLatin1String("uncheck")) {
        const bool want = act == QLatin1String("check");
        if (auto* b = qobject_cast<QAbstractButton*>(w)) {
            if (!b->isCheckable()) {
                *error = err_text(Err::Unsupported, QStringLiteral("%1 is not a checkbox or toggle.").arg(what),
                                  QStringLiteral("click it."));
                return std::nullopt;
            }
            if (b->isChecked() == want) {
                p.perform = []() {};
                p.note +=
                    QStringLiteral(" (already %1)").arg(want ? QStringLiteral("checked") : QStringLiteral("unchecked"));
                return p;
            }
            QPointer<QAbstractButton> bg(b);
            p.perform = [bg]() {
                if (bg)
                    bg->click(); // emits clicked + toggled, like a person's click
            };
            return p;
        }
        if (auto* gb = qobject_cast<QGroupBox*>(w); gb && gb->isCheckable()) {
            QPointer<QGroupBox> gg(gb);
            p.perform = [gg, want]() {
                if (gg) {
                    gg->setChecked(want);
                    emit gg->clicked(want);
                }
            };
            return p;
        }
        *error = err_text(Err::Unsupported, QStringLiteral("%1 cannot be checked.").arg(what));
        return std::nullopt;
    }

    // ── Values ──
    if (act == QLatin1String("set_value")) {
        const QString v = s.value.isDouble() ? QString::number(s.value.toDouble(), 'g', 15)
                                             : (s.value.isString() ? s.value.toString() : s.text);
        if (auto* dt = qobject_cast<QDateTimeEdit*>(w)) {
            // A pure date ("2026-11-08") is a date — never a midnight date-time,
            // which a time-zone conversion can move to the previous day.
            const QDate date = QDate::fromString(v.trimmed(), Qt::ISODate);
            const QTime time = date.isValid() ? QTime() : QTime::fromString(v.trimmed(), Qt::ISODate);
            const QDateTime when =
                date.isValid() || time.isValid() ? QDateTime() : QDateTime::fromString(v.trimmed(), Qt::ISODate);
            QPointer<QDateTimeEdit> dg(dt);
            // Out of the field's range: refused, never silently clamped.
            const bool out = (when.isValid() && (when < dt->minimumDateTime() || when > dt->maximumDateTime())) ||
                             (!when.isValid() && date.isValid() &&
                              (date < dt->minimumDate() || date > dt->maximumDate())) ||
                             (!when.isValid() && !date.isValid() && time.isValid() &&
                              (time < dt->minimumTime() || time > dt->maximumTime()));
            if (out) {
                *error = err_text(Err::InvalidValue,
                                  QStringLiteral("%1 takes %2 to %3; %4 is outside that.")
                                      .arg(what, dt->minimumDateTime().toString(Qt::ISODate),
                                           dt->maximumDateTime().toString(Qt::ISODate), v));
                return std::nullopt;
            }
            if (when.isValid()) {
                p.perform = [dg, when]() {
                    if (dg)
                        dg->setDateTime(when);
                };
            } else if (date.isValid()) {
                p.perform = [dg, date]() {
                    if (dg)
                        dg->setDate(date);
                };
            } else if (time.isValid()) {
                p.perform = [dg, time]() {
                    if (dg)
                        dg->setTime(time);
                };
            } else {
                *error = err_text(Err::InvalidValue,
                                  QStringLiteral("\"%1\" is not an ISO date / time (e.g. 2026-11-08, 14:30, "
                                                 "2026-11-08T14:30:00).")
                                      .arg(v));
                return std::nullopt;
            }
            p.note = QStringLiteral("set %1 to %2").arg(what, v);
            return p;
        }
        bool ok = false;
        const double d = v.toDouble(&ok);
        auto out_of_range = [&](double lo, double hi) {
            *error = err_text(Err::InvalidValue,
                              QStringLiteral("%1 takes %2 to %3; %4 is out of range.")
                                  .arg(what, QString::number(lo, 'g', 12), QString::number(hi, 'g', 12), v));
        };
        auto not_number = [&]() {
            *error = err_text(Err::InvalidValue, QStringLiteral("%1 takes a number, not \"%2\".").arg(what, v));
        };
        if (auto* is = qobject_cast<QSpinBox*>(w)) {
            if (!ok) {
                not_number();
                return std::nullopt;
            }
            if (d < is->minimum() || d > is->maximum()) {
                out_of_range(is->minimum(), is->maximum());
                return std::nullopt;
            }
            QPointer<QSpinBox> sg(is);
            const int n = qRound(d);
            p.perform = [sg, n]() {
                if (sg)
                    sg->setValue(n);
            };
            p.note = QStringLiteral("set %1 to %2").arg(what).arg(n);
            return p;
        }
        if (auto* ds = qobject_cast<QDoubleSpinBox*>(w)) {
            if (!ok) {
                not_number();
                return std::nullopt;
            }
            if (d < ds->minimum() || d > ds->maximum()) {
                out_of_range(ds->minimum(), ds->maximum());
                return std::nullopt;
            }
            QPointer<QDoubleSpinBox> sg(ds);
            p.perform = [sg, d]() {
                if (sg)
                    sg->setValue(d);
            };
            // The box keeps `decimals()` places: say what it will really hold.
            const double held = QString::number(d, 'f', ds->decimals()).toDouble();
            p.note = QStringLiteral("set %1 to %2%3")
                         .arg(what, v,
                              held != d ? QStringLiteral(" (it keeps %1 decimals: %2)")
                                              .arg(ds->decimals())
                                              .arg(QString::number(held, 'f', ds->decimals()))
                                        : QString());
            return p;
        }
        if (auto* sl = qobject_cast<QAbstractSlider*>(w)) {
            if (!ok) {
                not_number();
                return std::nullopt;
            }
            if (d < sl->minimum() || d > sl->maximum()) {
                out_of_range(sl->minimum(), sl->maximum());
                return std::nullopt;
            }
            QPointer<QAbstractSlider> sg(sl);
            const int n = qRound(d);
            p.perform = [sg, n]() {
                if (sg)
                    sg->setValue(n);
            };
            p.note = QStringLiteral("set %1 to %2").arg(what).arg(n);
            return p;
        }
        if (qobject_cast<QLineEdit*>(w) || qobject_cast<QComboBox*>(w)) {
            Step type = s;
            type.action = QStringLiteral("type");
            type.text = v;
            return prepare(type, t, error);
        }
        *error = err_text(Err::Unsupported, QStringLiteral("%1 does not hold a value.").arg(what),
                          QStringLiteral("use click / type / select."));
        return std::nullopt;
    }

    // ── Keys ──
    if (act == QLatin1String("press")) {
        const auto chord = parse_key(s.text);
        if (!chord) {
            *error = err_text(Err::InvalidValue, QStringLiteral("\"%1\" is not a key.").arg(s.text),
                              QStringLiteral("e.g. Enter, Escape, Tab, Down, PageDown, Ctrl+S, F5."));
            return std::nullopt;
        }
        const KeyChord k = *chord;
        // The clipboard is the user's: pasting it (or copying a secret field
        // into it) would read whatever they copied — a password, the MCP token.
        const bool ctrl_like = k.mods.testAnyFlags(Qt::ControlModifier | Qt::MetaModifier);
        const bool paste =
            (ctrl_like && k.key == Qt::Key_V) || (k.mods.testFlag(Qt::ShiftModifier) && k.key == Qt::Key_Insert);
        if (paste) {
            *error = err_text(Err::Denied, QStringLiteral("Pasting is not available to agents (the clipboard is the user's)."),
                              QStringLiteral("type the text with action=\"type\"."));
            return std::nullopt;
        }
        if (is_secret(w)) {
            *error = err_text(Err::Denied,
                              QStringLiteral("%1 is a secret field (password / API key / token) — agents never operate it.")
                                  .arg(what));
            return std::nullopt;
        }
        p.perform = [wg, k]() {
            if (!wg)
                return;
            // Shortcuts are matched first, as the window system would.
            if ((k.mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) ||
                (k.key >= Qt::Key_F1 && k.key <= Qt::Key_F35)) {
                if (trigger_shortcut(wg, k.seq))
                    return;
            }
            if (k.key == Qt::Key_Escape) {
                if (auto* menu = qobject_cast<QMenu*>(wg.data())) {
                    menu->close();
                    return;
                }
            }
            send_key(wg, k.key, k.mods, k.text);
        };
        p.note = QStringLiteral("press %1%2").arg(s.text, t.ref.isEmpty() ? QString() : QStringLiteral(" on ") + what);
        return p;
    }

    if (act == QLatin1String("scroll")) {
        const int amount = s.amount;
        QAbstractScrollArea* area = qobject_cast<QAbstractScrollArea*>(w);
        if (!area) {
            for (QWidget* p2 = w; p2 && !area; p2 = p2->parentWidget())
                area = qobject_cast<QAbstractScrollArea*>(p2);
        }
        if (!area) {
            for (QAbstractScrollArea* sa : w->findChildren<QAbstractScrollArea*>()) {
                if (sa->isVisible()) {
                    area = sa;
                    break;
                }
            }
        }
        if (!area) {
            *error = err_text(Err::Unsupported, QStringLiteral("Nothing scrollable at %1.").arg(what));
            return std::nullopt;
        }
        QPointer<QAbstractScrollArea> ag2(area);
        p.perform = [ag2, amount]() {
            if (!ag2)
                return;
            QScrollBar* sb = ag2->verticalScrollBar();
            sb->setValue(sb->value() + (amount == 0 ? sb->pageStep() : amount * sb->singleStep() * 3));
        };
        return p;
    }

    if (act == QLatin1String("focus")) {
        p.perform = [wg]() {
            if (!wg)
                return;
            if (QWidget* d = dock_of(wg))
                host::focus_panel(d);
            if (wg)
                wg->setFocus(Qt::OtherFocusReason);
        };
        return p;
    }

    if (act == QLatin1String("expand") || act == QLatin1String("collapse")) {
        const bool expand = act == QLatin1String("expand");
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            QPointer<QComboBox> cg(cb);
            p.perform = [cg, expand]() {
                if (!cg)
                    return;
                if (expand)
                    cg->showPopup();
                else
                    cg->hidePopup();
            };
            return p;
        }
        if (auto* gb = qobject_cast<QGroupBox*>(w); gb && gb->isCheckable()) {
            QPointer<QGroupBox> gg(gb);
            p.perform = [gg, expand]() {
                if (gg)
                    gg->setChecked(expand);
            };
            return p;
        }
        Step click = s;
        click.action = QStringLiteral("click");
        return prepare(click, t, error); // a collapsible section header toggles on click
    }

    // ── Clicks ──
    if (act == QLatin1String("click") || act == QLatin1String("double_click") || act == QLatin1String("right_click") ||
        act == QLatin1String("hover")) {
        const bool dbl = act == QLatin1String("double_click");
        const bool at_point = t.has_hit || s.has_xy;
        const QPoint pos = t.has_hit ? t.hit_pos : s.has_xy ? QPoint(s.x, s.y) : w->rect().center();
        if (at_point && !w->rect().contains(pos)) {
            *error = err_text(Err::InvalidValue, QStringLiteral("(%1, %2) is outside %3 (%4×%5).")
                                                     .arg(s.x)
                                                     .arg(s.y)
                                                     .arg(what)
                                                     .arg(w->width())
                                                     .arg(w->height()));
            return std::nullopt;
        }
        if (act == QLatin1String("hover")) {
            p.perform = [wg, pos]() {
                if (!wg)
                    return;
                QEnterEvent enter(QPointF(pos), wg->mapTo(wg->window(), QPointF(pos)), wg->mapToGlobal(QPointF(pos)));
                QCoreApplication::sendEvent(wg, &enter);
                if (!wg)
                    return;
                QMouseEvent move(QEvent::MouseMove, QPointF(pos), wg->mapTo(wg->window(), QPointF(pos)),
                                 wg->mapToGlobal(QPointF(pos)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(wg, &move);
            };
            return p;
        }
        if (act == QLatin1String("right_click")) {
            p.perform = [wg, pos]() {
                if (!wg)
                    return;
                QContextMenuEvent ev(QContextMenuEvent::Mouse, pos, wg->mapToGlobal(pos));
                QCoreApplication::sendEvent(wg, &ev);
            };
            return p;
        }
        if (auto* b = qobject_cast<QAbstractButton*>(w); b && !at_point && !dbl) {
            QPointer<QAbstractButton> bg(b);
            auto* tb = qobject_cast<QToolButton*>(b);
            const bool instant_menu = tb && tb->menu() && tb->popupMode() == QToolButton::InstantPopup;
            p.perform = [bg, instant_menu]() {
                if (!bg)
                    return;
                if (instant_menu) {
                    if (auto* t2 = qobject_cast<QToolButton*>(bg.data()))
                        t2->showMenu();
                    return;
                }
                bg->click();
            };
            return p;
        }
        if (auto* cb = qobject_cast<QComboBox*>(w); cb && !at_point) {
            if (!s.text.isEmpty()) {
                Step sel = s;
                sel.action = QStringLiteral("select");
                return prepare(sel, t, error);
            }
            QPointer<QComboBox> cg(cb);
            p.perform = [cg]() {
                if (cg)
                    cg->showPopup();
            };
            p.note += QStringLiteral(" (its options are listed by ui_read)");
            return p;
        }
        if (auto* gb = qobject_cast<QGroupBox*>(w); gb && gb->isCheckable() && !at_point) {
            Step toggle = s;
            toggle.action = gb->isChecked() ? QStringLiteral("uncheck") : QStringLiteral("check");
            return prepare(toggle, t, error);
        }
        if (auto* l = qobject_cast<QLabel*>(w);
            l && !at_point && l->text().contains(QLatin1String("<a "), Qt::CaseInsensitive)) {
            // A link: activate its first anchor the way QLabel does.
            static const QRegularExpression href(QStringLiteral("href\\s*=\\s*['\"]([^'\"]+)['\"]"),
                                                 QRegularExpression::CaseInsensitiveOption);
            const QString link = href.match(l->text()).captured(1);
            QPointer<QLabel> lg(l);
            p.perform = [lg, link]() {
                if (lg)
                    emit lg->linkActivated(link);
            };
            return p;
        }
        p.perform = [wg, pos, dbl]() {
            if (wg)
                send_mouse(wg, pos, Qt::LeftButton, dbl);
        };
        return p;
    }

    *error = err_text(Err::InvalidValue, QStringLiteral("Unknown action \"%1\".").arg(act),
                      QStringLiteral("use one of: %1.").arg(known_actions().join(QStringLiteral(", "))));
    return std::nullopt;
}

} // namespace fincept::mcp::core::ui::act
