// src/app/WindowFrame_Actions.cpp
//
// Public action handlers — chat mode toggle, focus mode, command palette,
// debug overlay, component browser, and the periodic dock-layout save.
// These are invoked from the ActionRegistry (builtin_actions.cpp) or
// directly from menu/shortcut hooks.
//
// Part of the partial-class split of WindowFrame.cpp.

#include "app/WindowFrame.h"

#include "app/DockScreenRouter.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "screens/chat_mode/ChatModeScreen.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/command/CommandPalette.h"
#include "ui/command/QuickCommandBar.h"
#include "ui/components/ComponentBrowserDialog.h"
#include "ui/debug/DebugOverlay.h"
#include "ui/navigation/DockStatusBar.h"
#include "ui/navigation/DockToolBar.h"
#include "ui/theme/Theme.h"

#include <QStackedWidget>
#include <QToolBar>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

namespace fincept {

void WindowFrame::toggle_chat_mode() {
    if (locked_) return;
    chat_mode_ = !chat_mode_;

    if (chat_mode_) {
        if (dock_toolbar_)
            dock_toolbar_->setVisible(false);
        if (dock_status_bar_)
            dock_status_bar_->setVisible(false);
        if (chat_bubble_)
            chat_bubble_->setVisible(false);
        stack_->setCurrentIndex(2);
        LOG_INFO("WindowFrame", "Entered Chat Mode");
    } else {
        stack_->setCurrentIndex(1);
        if (dock_toolbar_)
            dock_toolbar_->setVisible(true);
        if (dock_status_bar_)
            dock_status_bar_->setVisible(true);
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
        LOG_INFO("WindowFrame", "Exited Chat Mode");
    }
}


void WindowFrame::toggle_focus_mode() {
    // Don't let focus mode toggle shell visibility while the user is on an
    // auth screen — the toolbar must stay hidden there. Mirrors the gate
    // that lived inside the original inline lambda.
    if (stack_ && stack_->currentIndex() == 0)
        return;
    focus_mode_ = !focus_mode_;
    if (dock_toolbar_)
        dock_toolbar_->setVisible(!focus_mode_);
    if (dock_status_bar_)
        dock_status_bar_->setVisible(!focus_mode_);
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
    if (focused && focused->widget())
        QMetaObject::invokeMethod(focused->widget(), "refresh", Qt::QueuedConnection);
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
    if (dock_layout_save_timer_)
        dock_layout_save_timer_->start();
}

} // namespace fincept
