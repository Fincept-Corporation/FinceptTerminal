// AiChatScreen.cpp — Fincept AI Chat, Obsidian design system.
//
// Core lifecycle: ctor, refresh_theme, show/hide/resizeEvent, MCP event
// subscriptions, eventFilter, save_state/restore_state, set_group +
// on_group_symbol_changed. Other concerns:
//   - AiChatScreen_Layout.cpp    — build_ui + build_* UI pieces
//   - AiChatScreen_Sessions.cpp  — session list / sidebar / load_messages
//   - AiChatScreen_Messaging.cpp — typing / on_send / streaming / bubbles / stats

#include "screens/ai_chat/AiChatScreen.h"

#include "screens/ai_chat/ChatBubbleFactory.h"
#include "services/llm/LlmService.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "mcp/McpService.h"
#include "storage/repositories/ChatRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QPointer>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QtConcurrent/QtConcurrent>

#include <cmath>
#include <memory>


namespace fincept::screens {

// TAG declared in the split TUs that use it (AiChatScreen_Sessions.cpp etc).

namespace fnt = fincept::ui::fonts;
namespace col = fincept::ui::colors;

AiChatScreen::AiChatScreen(QWidget* parent) : QWidget(parent) {
    typing_timer_ = new QTimer(this);
    typing_timer_->setInterval(400);
    connect(typing_timer_, &QTimer::timeout, this, &AiChatScreen::on_typing_indicator_tick);

    build_ui();
    load_sessions();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
}

void AiChatScreen::refresh_theme() {
    // Root
    setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));

    // Sidebar
    if (sidebar_)
        sidebar_->setStyleSheet(
            QString("background:%1;border-right:1px solid %2;").arg(col::BG_SURFACE(), col::BORDER_DIM()));

    // Chat area
    if (chat_widget_)
        chat_widget_->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));
    if (messages_container_)
        messages_container_->setStyleSheet(QString("background:%1;").arg(col::BG_BASE()));

    // Scroll area
    if (scroll_area_)
        scroll_area_->setStyleSheet(
            QString("QScrollArea{background:%1;border:none;}"
                    "QScrollBar:vertical{background:transparent;width:5px;margin:2px;}"
                    "QScrollBar::handle:vertical{background:%2;border-radius:0px;min-height:30px;}"
                    "QScrollBar::handle:vertical:hover{background:%3;}"
                    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                .arg(col::BG_BASE(), col::BORDER_MED(), col::BORDER_BRIGHT()));

    // Input box
    if (input_box_)
        input_box_->setStyleSheet(QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;"
                                          "border-radius:0px;padding:8px 14px;font-size:%4px;}"
                                          "QPlainTextEdit:focus{border-color:%5;}")
                                      .arg(col::BG_BASE(), col::TEXT_PRIMARY(), col::BORDER_MED())
                                      .arg(fnt::BODY)
                                      .arg(col::AMBER()));

    // Send button
    if (send_btn_)
        send_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;border-radius:0px;"
                                         "font-size:%3px;font-weight:700;}"
                                         "QPushButton:hover:enabled{background:%4;}"
                                         "QPushButton:disabled{background:%5;color:%6;}")
                                     .arg(col::AMBER(), col::BG_BASE())
                                     .arg(fnt::SMALL)
                                     .arg(col::ORANGE(), col::BG_RAISED(), col::TEXT_DIM()));

    // Attach button
    if (attach_btn_)
        attach_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                                           "border-radius:0px;font-size:20px;font-weight:700;}"
                                           "QPushButton:hover{background:rgba(217,119,6,0.15);border-color:%3;}")
                                       .arg(col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER()));

    // Header bar elements
    if (hdr_session_lbl_)
        hdr_session_lbl_->setStyleSheet(
            QString("color:%1;font-size:%2px;font-weight:600;").arg(col::TEXT_PRIMARY()).arg(fnt::BODY));
    if (hdr_tokens_lbl_)
        hdr_tokens_lbl_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(col::TEXT_DIM()).arg(fnt::TINY));

    // Update provider/model stats to pick up new colors
    update_stats();
}

void AiChatScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::finished_streaming, this,
            &AiChatScreen::on_streaming_done, Qt::UniqueConnection);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            &AiChatScreen::on_provider_changed, Qt::UniqueConnection);
    // Phase 7: rehydrate the linked symbol from SymbolContext on every
    // show. linked_symbol_ isn't part of save_state — link state is
    // shell-scoped (per-group, not per-panel), so the most recent group
    // symbol is the source of truth. Without this, a layout reload would
    // leave linked_symbol_ empty until the publisher next changes it.
    if (link_group_ != fincept::SymbolGroup::None) {
        const fincept::SymbolRef rehydrated =
            fincept::SymbolContext::instance().group_symbol(link_group_);
        if (rehydrated.is_valid())
            linked_symbol_ = rehydrated;
    }
    update_stats();
    subscribe_mcp_events();
}

void AiChatScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    unsubscribe_mcp_events();
}

// ── MCP-driven UI sync ──────────────────────────────────────────────────────
// MCP set_active_llm publishes llm.provider_changed when the LLM or Finagent
// switches the active provider. We force a LlmService::reload_config() so
// the next outgoing message uses the new provider. reload_config emits
// LlmService::config_changed which is already wired (above) to
// on_provider_changed for the header label refresh.

void AiChatScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty()) return; // idempotent

    QPointer<AiChatScreen> self = this;
    auto on_provider_event = [self](const QVariantMap&) {
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self]() {
            if (!self) return;
            ai_chat::LlmService::instance().reload_config();
        }, Qt::QueuedConnection);
    };

    auto on_session_created = [self](const QVariantMap&) {
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self]() {
            if (!self) return;
            self->load_sessions();
        }, Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe("llm.provider_changed",   on_provider_event));
    mcp_event_subs_.append(bus.subscribe("ai_chat.session_created", on_session_created));
}

void AiChatScreen::unsubscribe_mcp_events() {
    auto& bus = EventBus::instance();
    for (auto id : mcp_event_subs_)
        bus.unsubscribe(id);
    mcp_event_subs_.clear();
}

void AiChatScreen::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
}

// ── Build UI ──────────────────────────────────────────────────────────────────

bool AiChatScreen::eventFilter(QObject* obj, QEvent* event) {
    if (obj == input_box_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier)
                return false;
            ke->accept();
            on_send();
            return true;
        }
        if (ke->key() == Qt::Key_N && (ke->modifiers() & Qt::ControlModifier)) {
            on_new_session();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}


QVariantMap AiChatScreen::save_state() const {
    QVariantMap s{{"session_id", active_session_id_}};

    if (input_box_)
        s.insert("draft", input_box_->toPlainText());
    if (search_edit_)
        s.insert("search", search_edit_->text());
    if (scroll_area_ && scroll_area_->verticalScrollBar())
        s.insert("scroll", scroll_area_->verticalScrollBar()->value());
    if (!attached_file_path_.isEmpty())
        s.insert("attached_file", attached_file_path_);
    s.insert("sidebar_collapsed", sidebar_collapsed_);

    return s;
}

void AiChatScreen::restore_state(const QVariantMap& state) {
    const QString sid = state.value("session_id").toString();

    // Session selection — find the matching row and select it.
    if (!sid.isEmpty()) {
        for (int i = 0; i < session_list_->count(); ++i) {
            auto* item = session_list_->item(i);
            if (item && item->data(Qt::UserRole).toString() == sid) {
                session_list_->setCurrentRow(i);
                on_session_selected(i);
                break;
            }
        }
        // Session not found (may have been deleted) — leave default selection
    }

    // Search box — apply before (or after) session restore; filter text is independent.
    const QString search_text = state.value("search").toString();
    if (search_edit_ && !search_text.isEmpty())
        search_edit_->setText(search_text);

    // Draft composer text — restore what the user was typing.
    const QString draft = state.value("draft").toString();
    if (input_box_ && !draft.isEmpty())
        input_box_->setPlainText(draft);

    // Attached file — restore the badge only if the file still exists on disk.
    const QString attached = state.value("attached_file").toString();
    if (!attached.isEmpty() && QFile::exists(attached)) {
        attached_file_path_ = attached;
        if (attach_badge_) {
            attach_badge_->setText(QFileInfo(attached).fileName());
            attach_badge_->setVisible(true);
        }
    }

    // Sidebar collapsed state — apply without animation on restore so the
    // user lands on the previously chosen layout immediately.
    if (state.contains("sidebar_collapsed"))
        apply_sidebar_collapsed(state.value("sidebar_collapsed").toBool(), /*animate=*/false);

    // Scroll position — defer until after message_layout has laid out,
    // otherwise the scrollbar max is still 0.
    const int scroll = state.value("scroll", -1).toInt();
    if (scroll >= 0 && scroll_area_) {
        QPointer<AiChatScreen> self = this;
        QTimer::singleShot(0, this, [self, scroll]() {
            if (!self || !self->scroll_area_)
                return;
            if (auto* bar = self->scroll_area_->verticalScrollBar())
                bar->setValue(scroll);
        });
    }
}

// ── IGroupLinked (Phase 7) ─────────────────────────────────────────────────

void AiChatScreen::set_group(fincept::SymbolGroup g) {
    if (link_group_ == g)
        return;
    link_group_ = g;
    // Drop the cached symbol when leaving a group — otherwise the next send
    // would still tag a context the user can't see was inherited.
    if (g == fincept::SymbolGroup::None)
        linked_symbol_ = {};
}

void AiChatScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    // Consumer-only. Cache the inbound symbol so the next user-typed
    // message is sent with a "[Context: SYM]" prefix on the wire (see
    // on_send). UI is intentionally untouched — the bubble shows what
    // the user typed; the prefix is only on the LLM payload.
    if (!ref.is_valid()) {
        linked_symbol_ = {};
        return;
    }
    linked_symbol_ = ref;
}

} // namespace fincept::screens
