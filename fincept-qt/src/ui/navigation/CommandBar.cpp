// src/ui/navigation/CommandBar.cpp — the header search box (see CommandBar.h). Wiring only:
// the field, the popup, the controller; key / mouse paths; focus give-back; Ctrl+K and "/".
#include "ui/navigation/CommandBar.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/keys/KeyConfigManager.h"
#include "core/symbol/SymbolDragSource.h"
#include "ui/navigation/search/SearchActions.h"
#include "ui/navigation/search/SearchController.h"
#include "ui/navigation/search/SearchField.h"
#include "ui/navigation/search/SearchModel.h"
#include "ui/navigation/search/SearchPopup.h"
#include "ui/navigation/search/SearchSources.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QHBoxLayout>
#include <QListView>
#include <QMenu>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QTextEdit>

#include <algorithm>
#include <cstdlib>

namespace fincept::ui {

namespace {

constexpr char kBarProperty[] = "fincept_header_search";

QString cb_menu_css() {
    return QStringLiteral("QMenu{background:%1;color:%2;border:1px solid %3;padding:3px 0;}"
                          "QMenu::item{padding:4px 18px 4px 12px;}"
                          "QMenu::item:selected{background:%4;}"
                          "QMenu::separator{background:%3;height:1px;margin:3px 8px;}")
        .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_MED(), colors::BG_RAISED());
}

bool cb_is_text_input(QWidget* w) {
    if (!w)
        return false;
    if (qobject_cast<QLineEdit*>(w) || qobject_cast<QTextEdit*>(w) || qobject_cast<QPlainTextEdit*>(w) ||
        qobject_cast<QAbstractSpinBox*>(w))
        return true;
    auto* combo = qobject_cast<QComboBox*>(w);
    return combo && combo->isEditable();
}

} // namespace

CommandBar::CommandBar(QWidget* parent) : QWidget(parent) {
    setProperty(kBarProperty, true);
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    field_ = new search::SearchField(this);
    hl->addWidget(field_, 0, Qt::AlignVCenter);

    model_ = new search::SearchModel(this);
    popup_ = new search::SearchPopup(model_, field_);
    controller_ = new search::SearchController(this);
    popup_->set_legend(tr("↑↓ move · Enter open · Tab or → more actions · Ctrl+Enter new panel · Esc close"));

    // Results can be dragged onto any panel or the pins strip like any other symbol.
    symbol_dnd::installDragSource(popup_->view()->viewport(), [this]() -> SymbolRef {
        QListView* v = popup_->view();
        const QModelIndex idx = v->indexAt(v->viewport()->mapFromGlobal(QCursor::pos()));
        const auto* row = model_->row(idx.row());
        if (!row || row->symbol.isEmpty())
            return {};
        SymbolRef ref;
        ref.symbol = row->symbol;
        ref.asset_class = row->symbol_class;
        return ref;
    });

    connect(field_, &QLineEdit::textChanged, this, &CommandBar::on_text_changed);
    connect(field_, &search::SearchField::navigate, this, [this](int step) {
        if (!popup_->isVisible()) {
            open_popup();
            return;
        }
        move_selection(step);
    });
    connect(field_, &search::SearchField::accept, this, [this](Qt::KeyboardModifiers mods) {
        if (!popup_->isVisible() && field_->text().trimmed().isEmpty())
            return;
        controller_->accept(current_key(), mods, user_moved_);
    });
    connect(field_, &search::SearchField::more_actions, this, [this]() { open_menu_for_current(); });
    connect(field_, &search::SearchField::escape, this, [this]() { close_popup(true); });
    connect(field_, &search::SearchField::focus_entered, this, [this](Qt::FocusReason reason) {
        if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason)
            open_popup();
    });
    connect(field_, &search::SearchField::focus_left, this, [this](Qt::FocusReason reason) {
        if (reason == Qt::PopupFocusReason || menu_open_)
            return; // our own action menu
        popup_->dismiss();
        field_->set_popup_open(false);
        controller_->cancel_all();
    });
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* old, QWidget* now) {
        if (now == field_ && old && old != field_ && !isAncestorOf(old) && old->window() == window())
            previous_focus_ = old;
    });

    connect(controller_, &search::SearchController::rows_changed, this, &CommandBar::on_rows);
    connect(controller_, &search::SearchController::navigate_requested, this, &CommandBar::navigate_to);
    connect(controller_, &search::SearchController::dock_requested, this, &CommandBar::dock_command);
    connect(controller_, &search::SearchController::toolbar_action_requested, this, &CommandBar::toolbar_action);
    connect(controller_, &search::SearchController::status, this, [this](const QString& text) {
        if (popup_->isVisible())
            popup_->show_status(text);
    });
    connect(controller_, &search::SearchController::fill_requested, this, [this](const QString& text) {
        field_->setText(text);
        field_->setCursorPosition(static_cast<int>(text.size()));
        field_->setFocus(Qt::OtherFocusReason);
    });
    connect(controller_, &search::SearchController::finished, this, [this]() {
        close_popup(false);
        field_->clearFocus();
    });

    connect(popup_, &search::SearchPopup::row_activated, this, [this](int row, int action, Qt::KeyboardModifiers mods) {
        const auto* r = model_->row(row);
        if (!r || !r->selectable())
            return;
        controller_->activate(r->key, action, mods);
    });
    connect(popup_, &search::SearchPopup::row_context_menu, this,
            [this](int row, const QPoint& pos) { open_menu(row, pos); });

    // Widths / heights follow the font; colours the theme — re-fit once the change is in.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, &CommandBar::refit);
    refit();
}

CommandBar::~CommandBar() {
    delete popup_; // a top-level window: not our child
}

void CommandBar::refit() {
    field_->refit();
    popup_->refit();
    field_->set_shortcut_hint(KeyConfigManager::instance().key(KeyAction::BrowseComponents).toString(QKeySequence::NativeText));
    setMinimumHeight(field_->height());
}

void CommandBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        popup_->set_legend(tr("↑↓ move · Enter open · Tab or → more actions · Ctrl+Enter new panel · Esc close"));
        refit();
        if (popup_->isVisible())
            controller_->refresh();
    }
    QWidget::changeEvent(event);
}

void CommandBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    controller_->set_window(window());
    if (!slash_ && window() != this) {
        // "/" focuses the box when the focused widget does not type it (text fields accept "/" themselves;
        // KeyRouting: a printable key in a text input stays with it).
        slash_ = new QShortcut(QKeySequence(Qt::Key_Slash), window());
        slash_->setContext(Qt::WindowShortcut);
        connect(slash_, &QShortcut::activated, this, [this]() {
            if (!isVisible() || cb_is_text_input(QApplication::focusWidget()))
                return;
            activate();
        });
    }
}

void CommandBar::activate() {
    QWidget* now = QApplication::focusWidget();
    if (now && now != field_ && !isAncestorOf(now) && now->window() == window())
        previous_focus_ = now;
    window()->activateWindow();
    field_->setFocus(Qt::ShortcutFocusReason); // opens the results (focus_entered) …
    field_->selectAll();
    if (!popup_->isVisible())
        open_popup(); // … or the box already had focus
}

bool CommandBar::activate_in(QWidget* window) {
    if (!window)
        return false;
    const auto bars = window->findChildren<CommandBar*>();
    for (CommandBar* bar : bars) {
        if (bar->isVisible() && bar->window() == window) {
            bar->activate();
            return true;
        }
    }
    return false;
}

void CommandBar::open_component_browser() {
    search::actions::Ctx ctx;
    ctx.window = window();
    search::actions::invoke_action(ctx, QStringLiteral("frame.component_browser"), {});
}

void CommandBar::open_popup() {
    controller_->set_window(window());
    search::sources::refresh_saved();
    user_moved_ = false;
    controller_->rerun(); // the field and the controller hold the same text
    popup_->present();
    field_->set_popup_open(true);
}

void CommandBar::close_popup(bool restore_focus) {
    popup_->dismiss();
    field_->set_popup_open(false);
    controller_->cancel_all();
    if (!field_->text().isEmpty()) {
        closing_ = true; // the clear must not reopen the results
        field_->clear();
        closing_ = false;
    }
    if (restore_focus) {
        if (previous_focus_ && previous_focus_->isVisible() && previous_focus_->isEnabled())
            previous_focus_->setFocus(Qt::OtherFocusReason);
        else
            field_->clearFocus();
    }
    previous_focus_.clear();
}

void CommandBar::on_text_changed(const QString& text) {
    user_moved_ = false;
    if (closing_) {
        controller_->set_query(text);
        return;
    }
    controller_->set_window(window());
    controller_->set_query(text);
    if (field_->hasFocus() && !popup_->isVisible()) {
        popup_->present();
        field_->set_popup_open(true);
    }
}

void CommandBar::on_rows(const QList<search::SearchRow>& rows, const QString& preselect) {
    const QString before = current_key();
    model_->set_rows(rows);
    // The user's own choice stays while it is still listed; otherwise the top hit is current.
    int target = -1;
    if (user_moved_ && !before.isEmpty())
        target = model_->row_of(before);
    if (target < 0) {
        user_moved_ = false;
        target = model_->row_of(preselect);
    }
    popup_->set_current(target);
    if (popup_->isVisible())
        popup_->present(); // height follows the rows
}

QString CommandBar::current_key() const {
    const auto* r = model_->row(popup_->current());
    return r ? r->key : QString();
}

void CommandBar::move_selection(int step) {
    const int n = model_->size();
    if (n == 0)
        return;
    int cur = popup_->current();
    const int dir = step < 0 ? -1 : 1;
    int moves = std::abs(step);
    int next = cur;
    while (moves-- > 0) {
        int probe = model_->next_selectable(next < 0 ? (dir > 0 ? 0 : n - 1) : next + dir, dir);
        if (probe < 0) {
            if (std::abs(step) > 1)
                break; // page keys stop at the ends
            probe = model_->next_selectable(dir > 0 ? 0 : n - 1, dir); // arrows wrap
        }
        if (probe < 0)
            break;
        next = probe;
    }
    if (next != cur && next >= 0) {
        user_moved_ = true;
        popup_->set_current(next);
    }
}

void CommandBar::open_menu_for_current(const QPoint& global_pos) {
    const int row = popup_->current();
    if (row < 0)
        return;
    QPoint pos = global_pos;
    if (pos.isNull()) {
        const QRect r = popup_->view()->visualRect(model_->index(row));
        pos = popup_->view()->viewport()->mapToGlobal(layoutDirection() == Qt::RightToLeft ? r.bottomLeft() : r.bottomRight() -
                                                                                                  QPoint(r.width() / 3, 0));
    }
    open_menu(row, pos);
}

void CommandBar::open_menu(int row, const QPoint& global_pos) {
    const auto* r = model_->row(row);
    if (!r || !r->selectable())
        return;
    const QString key = r->key;
    const auto entries = controller_->menu_entries(key);
    if (entries.isEmpty())
        return;
    popup_->set_current(row);
    QMenu menu(this);
    menu.setStyleSheet(cb_menu_css());
    menu.setAccessibleName(tr("Actions for %1").arg(r->lead.isEmpty() ? r->title : r->lead));
    for (int i = 0; i < entries.size(); ++i) {
        QAction* a = menu.addAction(QString(entries[i].label).replace(QLatin1Char('&'), QStringLiteral("&&")));
        a->setData(i);
        if (!entries[i].agent_risk.isEmpty())
            a->setProperty(core::agent::kAgentRiskProperty, entries[i].agent_risk);
        if (i == 0)
            menu.setDefaultAction(a);
    }
    menu_open_ = true;
    QAction* chosen = menu.exec(global_pos);
    menu_open_ = false;
    field_->setFocus(Qt::OtherFocusReason);
    if (chosen)
        controller_->activate_menu(key, chosen->data().toInt(), QApplication::keyboardModifiers());
}

} // namespace fincept::ui
