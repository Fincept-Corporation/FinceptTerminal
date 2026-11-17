// src/app/WindowFrame_Actions.cpp
//
// Public action handlers — chat mode toggle, focus mode, command palette,
// debug overlay, component browser, and the periodic dock-layout save.
// These are invoked from the ActionRegistry (builtin_actions.cpp) or
// directly from menu/shortcut hooks.
//
// Part of the partial-class split of WindowFrame.cpp.

#include "app/DockScreenRouter.h"
#include "app/WindowFrame.h"
#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/SessionManager.h"
#include "screens/chat_mode/ChatModeScreen.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/command/CommandPalette.h"
#include "ui/command/QuickCommandBar.h"
#include "ui/components/ComponentBrowserDialog.h"
#include "ui/debug/DebugOverlay.h"
#include "ui/navigation/DockStatusBar.h"
#include "ui/navigation/DockToolBar.h"
#include "ui/notifications/NotificationService.h"
#include "ui/theme/Theme.h"

#include <QBoxLayout>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QStackedWidget>
#include <QToolBar>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

namespace fincept {

void WindowFrame::ensure_chat_mode_screen() {
    if (chat_mode_screen_ || !chat_mode_host_)
        return;
    QElapsedTimer t;
    t.start();
    // Built in its final parent (the stack page), so the finished tree is never
    // re-parented / re-styled.
    chat_mode_screen_ = new chat_mode::ChatModeScreen(chat_mode_host_);
    if (auto* box = qobject_cast<QBoxLayout*>(chat_mode_host_->layout()))
        box->addWidget(chat_mode_screen_);
    connect(chat_mode_screen_, &chat_mode::ChatModeScreen::exit_requested, this, &WindowFrame::toggle_chat_mode);
    // Its draft, conversation and mode are saved on every change (notify_changed)
    // but were never read back: bring them back now.
    ScreenStateManager::instance().restore(chat_mode_screen_);
    LOG_DEBUG("WindowFrame", QString("Chat Mode built on first use in %1 ms").arg(t.elapsed()));
}

void WindowFrame::confirm_logout() {
    static bool asking = false; // the modal box runs an event loop: one question at a time
    if (asking)
        return;
    asking = true;
    const auto answer = QMessageBox::question(
        this, tr("Log out"), tr("Log out of Fincept Terminal? You will need to sign in again to come back."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    asking = false;
    if (answer == QMessageBox::Yes)
        auth::AuthManager::instance().logout();
}

void WindowFrame::toggle_chat_mode() {
    if (locked_)
        return;
    if (!chat_mode_)
        ensure_chat_mode_screen(); // before it is shown (lazy since the window constructor)
    chat_mode_ = !chat_mode_;
    // Toolbar + status bar follow "!focus_mode_ && !chat_mode_" — leaving chat
    // mode must not bring them back while focus mode is still on.
    set_shell_visible(true);

    if (chat_mode_) {
        if (chat_bubble_)
            chat_bubble_->setVisible(false);
        stack_->setCurrentIndex(2);
        LOG_DEBUG("WindowFrame", "Entered Chat Mode");
    } else {
        stack_->setCurrentIndex(1);
        // Restore chat bubble based on setting
        if (chat_bubble_) {
            auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
            const bool show = !r.is_ok() || r.value() != "false";
            chat_bubble_->setVisible(show);
            if (show) {
                chat_bubble_->reposition();
                chat_bubble_->raise();
            }
        }
        LOG_DEBUG("WindowFrame", "Exited Chat Mode");
    }
}

void WindowFrame::toggle_focus_mode() {
    // Don't let focus mode toggle shell visibility while the user is on an
    // auth screen — the toolbar must stay hidden there. Mirrors the gate
    // that lived inside the original inline lambda.
    if (stack_ && stack_->currentIndex() == 0)
        return;
    focus_mode_ = !focus_mode_;
    // set_shell_visible applies "!focus_mode_ && !chat_mode_", so toggling
    // focus mode from chat mode does not show the dock chrome over the chat.
    set_shell_visible(true);
}

void WindowFrame::toggle_chat_mode_action() {
    // Public facade so action handlers in builtin_actions.cpp can call into
    // the existing private implementation without becoming friends of this
    // class. Caller has already gated on is_locked().
    toggle_chat_mode();
}

void WindowFrame::refresh_focused_panel() {
    if (!dock_manager_)
        return;
    auto* focused = dock_manager_->focusedDockWidget();
    if (!focused || !focused->widget())
        return;
    // The screen itself, not the dock widget's content host around it.
    QWidget* panel = dock_router_ ? dock_router_->screen_widget(focused->objectName()) : nullptr;
    if (!panel)
        panel = focused->widget();
    // No top-level screen defines a plain refresh() slot, so the F5 / View ▸ Refresh
    // path used to be a silent no-op everywhere. Several screens do expose their
    // Refresh-button handler as a (private) slot under a conventional name — try
    // those too. Probe before invoking: invokeMethod on a missing method logs a Qt
    // "No such method" warning.
    static const char* const kRefreshSlots[] = {"refresh", "on_refresh", "on_refresh_clicked"};
    const QMetaObject* mo = panel->metaObject();
    for (const char* name : kRefreshSlots) {
        if (mo->indexOfMethod(QMetaObject::normalizedSignature((QByteArray(name) + "()").constData())) >= 0) {
            QMetaObject::invokeMethod(panel, name, Qt::QueuedConnection);
            return;
        }
    }
    LOG_DEBUG("WindowFrame", QString("refresh: '%1' exposes no refresh slot — ignored")
                                 .arg(QString::fromLatin1(mo->className())));
    // F5 / Refresh used to do nothing visible on most screens; say so instead.
    ui::ToastService::instance().post(ui::ToastService::Severity::Info,
                                      tr("This screen has no manual refresh"), "refresh");
}

void WindowFrame::open_component_browser() {
    auto* dlg = new ui::ComponentBrowserDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &ui::ComponentBrowserDialog::component_chosen, this, [this](const QString& id) {
        if (dock_router_ && !id.isEmpty())
            dock_router_->navigate(id);
    });
    dlg->show();
}

void WindowFrame::toggle_debug_overlay() {
    // Lazy construction so users who never enable the overlay don't pay
    // for the QTimer + QLabel cost.
    if (!debug_overlay_) {
        debug_overlay_ = new ui::DebugOverlay(this);
        // Parent it to the main window so resizes auto-trigger reposition
        // via our resizeEvent. The overlay uses
        // WA_TransparentForMouseEvents so it never blocks dock interaction.
    }
    debug_overlay_->toggle_visible();
}

void WindowFrame::toggle_command_bar() {
    // Phase 9: lazy construction. Wraps a `ui::CommandBar` in a bottom
    // QToolBar so QMainWindow lays it out above the status bar.
    if (!command_bar_) {
        auto* bar = new ui::QuickCommandBar(this);
        auto* host = new QToolBar(this);
        host->setObjectName("commandBarHost");
        host->setMovable(false);
        host->setFloatable(false);
        host->setAllowedAreas(Qt::BottomToolBarArea);
        host->addWidget(bar);
        addToolBar(Qt::BottomToolBarArea, host);
        host->setVisible(false);
        host->setProperty("commandBarInner", QVariant::fromValue(static_cast<QWidget*>(bar)));
        command_bar_ = host;
    }
    if (command_bar_) {
        const bool was_visible = command_bar_->isVisible();
        command_bar_->setVisible(!was_visible);
        if (!was_visible) {
            if (auto* inner = command_bar_->property("commandBarInner").value<QWidget*>()) {
                if (auto* cb = qobject_cast<ui::QuickCommandBar*>(inner))
                    cb->surface();
            }
        }
    }
}

void WindowFrame::open_command_palette() {
    ui::CommandPalette::show_for(this);
}

void WindowFrame::schedule_dock_layout_save() {
    if (suppress_layout_save_ || !dock_layout_save_timer_)
        return;
    dock_layout_save_timer_->start();
}

} // namespace fincept
