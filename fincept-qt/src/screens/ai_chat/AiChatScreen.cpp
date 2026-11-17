// AiChatScreen.cpp — the AI Chat tab: lifecycle, state, keys, breakpoints, overlays,
// theme and symbol-group linking. See AiChatScreen.h for the split.

#include "screens/ai_chat/AiChatScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "screens/ai_chat/AiChatComposer.h"
#include "screens/ai_chat/AiChatContextRail.h"
#include "screens/ai_chat/AiChatSessionRail.h"
#include "screens/ai_chat/AiChatStartPage.h"
#include "screens/ai_chat/AiChatStyle.h"
#include "screens/ai_chat/AiChatTranscript.h"
#include "services/llm/LlmService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStyle>
#include <QTimer>

namespace fincept::screens {

namespace st = aichat::style;

namespace {
// Breakpoints of the three zones (logical px of the screen's width).
constexpr int kAicWideMin = 1500;
constexpr int kAicMediumMin = 1100;

// A rail's object name selects its look (docked rail vs drawer): re-polish only on a change.
void aic_set_look(QWidget* w, const char* name) {
    if (!w || w->objectName() == QLatin1String(name))
        return;
    w->setObjectName(QString::fromLatin1(name));
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}
} // namespace

AiChatScreen::AiChatScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QString::fromLatin1(st::kRoot));
    setAttribute(Qt::WA_StyledBackground, true);
    // The one selector sheet for the whole tree, set before any child exists.
    st::apply_sheet(this);

    // Stop grace period: if a cancelled turn hasn't come back by then it is stuck inside a
    // tool — stop waiting for it (see detach_stopped_request).
    stop_grace_timer_ = new QTimer(this);
    stop_grace_timer_->setSingleShot(true);
    stop_grace_timer_->setInterval(kStopGraceMs);
    connect(stop_grace_timer_, &QTimer::timeout, this, &AiChatScreen::detach_stopped_request);

    // Background-job status lines. Started by track_job() and showEvent, stopped in
    // hideEvent and once no tracked job is running.
    job_timer_ = new QTimer(this);
    job_timer_->setInterval(1000);
    connect(job_timer_, &QTimer::timeout, this, &AiChatScreen::refresh_job_lines);

    // Tool steps arrive in bursts (a parallel round = many lines): one repaint per burst.
    tool_render_timer_ = new QTimer(this);
    tool_render_timer_->setSingleShot(true);
    tool_render_timer_->setInterval(st::kStreamFlushMs);
    connect(tool_render_timer_, &QTimer::timeout, this, [this]() {
        if (streaming_turn_ != 0)
            note_symbols_from_tools();
    });

    load_pins();
    saved_prompts_ = aichat::load_saved_prompts();
    build_ui();
    load_sessions();
    refresh_context_ui();
    update_stats();

    // Never inside theme_changed (restyling / re-fitting there stalls a theme switch): once
    // the appearance has settled, and each sheet only when its text changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
}

AiChatScreen::~AiChatScreen() = default;

void AiChatScreen::refresh_theme() {
    st::apply_sheet(this);
    apply_zones(/*force=*/true); // rail widths follow the font
    update_stats();
}

void AiChatScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::finished_streaming, this,
            &AiChatScreen::on_streaming_done, Qt::UniqueConnection);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            &AiChatScreen::on_provider_changed, Qt::UniqueConnection);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::route_notice, this, &AiChatScreen::on_route_notice,
            Qt::UniqueConnection);
    // Rehydrate the linked symbol from SymbolContext on every show: link state is
    // shell-scoped (per group), so the group's latest symbol is the source of truth.
    if (link_group_ != fincept::SymbolGroup::None) {
        const fincept::SymbolRef rehydrated = fincept::SymbolContext::instance().group_symbol(link_group_);
        if (rehydrated.is_valid())
            linked_symbol_ = rehydrated;
    }
    if (unread_) {
        unread_ = false;
        if (sessions_)
            sessions_->set_unread(active_session_id_, false);
    }
    apply_zones(/*force=*/true);
    refresh_context_ui();
    update_stats();
    subscribe_mcp_events();
    load_tool_catalogue();
    if (status_)
        status_->start_clock();
    if (!tracked_job_ids_.isEmpty()) {
        refresh_job_lines();
        if (!tracked_job_ids_.isEmpty())
            job_timer_->start();
    }
}

void AiChatScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    unsubscribe_mcp_events();
    job_timer_->stop();
    if (status_)
        status_->stop_clock();
    close_overlay();
}

void AiChatScreen::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_zones();
    place_overlay();
}

// ── MCP-driven UI sync ──────────────────────────────────────────────────────

void AiChatScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty())
        return; // idempotent

    QPointer<AiChatScreen> self = this;
    auto on_provider_event = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (self)
                    ai_chat::LlmService::instance().reload_config();
            },
            Qt::QueuedConnection);
    };
    auto on_session_created = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (self)
                    self->load_sessions();
            },
            Qt::QueuedConnection);
    };
    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe(this, "llm.provider_changed", on_provider_event));
    mcp_event_subs_.append(bus.subscribe(this, "ai_chat.session_created", on_session_created));
}

void AiChatScreen::unsubscribe_mcp_events() {
    auto& bus = EventBus::instance();
    for (auto id : mcp_event_subs_)
        bus.unsubscribe(id);
    mcp_event_subs_.clear();
}

void AiChatScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

// ── Zones and overlays ──────────────────────────────────────────────────────

void AiChatScreen::apply_zones(bool force) {
    if (!body_ || !sessions_ || !context_rail_)
        return;
    const int w = width();
    const Zones z = w >= kAicWideMin ? Zones::Wide : w >= kAicMediumMin ? Zones::Medium : Zones::Narrow;
    if (z == zones_ && !force)
        return;
    const bool changed = z != zones_;
    zones_ = z;
    if (changed)
        close_overlay();

    auto* hl = qobject_cast<QHBoxLayout*>(body_->layout());
    if (!hl)
        return;

    // Sessions rail: docked (Wide / Medium) unless the user collapsed it; a drawer (Narrow).
    if (overlay_ != Overlay::Sessions) {
        if (z == Zones::Narrow) {
            if (hl->indexOf(sessions_) >= 0)
                hl->removeWidget(sessions_);
            sessions_->hide();
        } else {
            if (sessions_->parentWidget() != body_)
                sessions_->setParent(body_);
            if (hl->indexOf(sessions_) < 0)
                hl->insertWidget(0, sessions_);
            aic_set_look(sessions_, st::kRailLeft);
            sessions_->setFixedWidth(sessions_->preferred_width());
            sessions_->setVisible(!sidebar_collapsed_);
        }
    }
    // Context rail: full (Wide) or an icon strip.
    if (overlay_ != Overlay::Context) {
        const auto mode = z == Zones::Wide ? aichat::AicContextRail::Mode::Full : aichat::AicContextRail::Mode::Icons;
        if (context_rail_->parentWidget() != body_)
            context_rail_->setParent(body_);
        if (hl->indexOf(context_rail_) < 0)
            hl->addWidget(context_rail_);
        aic_set_look(context_rail_, st::kRailRight);
        context_rail_->set_mode(mode);
        context_rail_->setFixedWidth(context_rail_->preferred_width(mode));
        context_rail_->show();
    }
    if (bar_)
        bar_->set_action_checked(QStringLiteral("sessions"),
                                 z == Zones::Narrow ? overlay_ == Overlay::Sessions : !sidebar_collapsed_);
    update_tab_order();
}

void AiChatScreen::toggle_sessions() {
    if (zones_ == Zones::Narrow) {
        if (overlay_ == Overlay::Sessions)
            close_overlay();
        else
            open_overlay(Overlay::Sessions);
        return;
    }
    sidebar_collapsed_ = !sidebar_collapsed_;
    apply_zones(/*force=*/true);
    if (!sidebar_collapsed_)
        sessions_->focus_list();
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::open_overlay(Overlay which, int section) {
    Q_UNUSED(section)
    if (which == Overlay::None || overlay_ == which)
        return;
    close_overlay();
    overlay_ = which;
    if (!scrim_) {
        scrim_ = new QWidget(this);
        scrim_->setObjectName(QStringLiteral("aicScrim"));
        scrim_->setAttribute(Qt::WA_StyledBackground, true);
        scrim_->setStyleSheet(QStringLiteral("QWidget#aicScrim{background:%1;}").arg(ui::colors::scrim(0.35)));
        scrim_->installEventFilter(this);
    }
    QWidget* rail = which == Overlay::Sessions ? static_cast<QWidget*>(sessions_) : static_cast<QWidget*>(context_rail_);
    if (auto* hl = qobject_cast<QHBoxLayout*>(body_->layout()); hl && hl->indexOf(rail) >= 0)
        hl->removeWidget(rail);
    rail->setParent(this);
    aic_set_look(rail, st::kDrawer);
    if (which == Overlay::Context) {
        context_rail_->set_mode(aichat::AicContextRail::Mode::Full);
        context_rail_->setFixedWidth(context_rail_->preferred_width(aichat::AicContextRail::Mode::Full));
    } else {
        sessions_->setFixedWidth(sessions_->preferred_width());
    }
    place_overlay();
    scrim_->show();
    scrim_->raise();
    rail->show();
    rail->raise();
    if (which == Overlay::Sessions)
        sessions_->focus_list();
    else
        context_rail_->setFocus(Qt::PopupFocusReason);
    if (bar_ && which == Overlay::Sessions)
        bar_->set_action_checked(QStringLiteral("sessions"), true);
}

void AiChatScreen::close_overlay() {
    if (overlay_ == Overlay::None)
        return;
    const Overlay was = overlay_;
    overlay_ = Overlay::None;
    if (scrim_)
        scrim_->hide();
    QWidget* rail = was == Overlay::Sessions ? static_cast<QWidget*>(sessions_) : static_cast<QWidget*>(context_rail_);
    rail->hide();
    rail->setParent(body_);
    apply_zones(/*force=*/true);
    if (was == Overlay::Sessions && transcript_)
        transcript_->setFocus(Qt::OtherFocusReason);
}

void AiChatScreen::place_overlay() {
    if (overlay_ == Overlay::None || !body_)
        return;
    const QRect area(body_->mapTo(this, QPoint(0, 0)), body_->size());
    if (scrim_)
        scrim_->setGeometry(area);
    if (overlay_ == Overlay::Sessions)
        sessions_->setGeometry(area.left(), area.top(), sessions_->width(), area.height());
    else
        context_rail_->setGeometry(area.right() + 1 - context_rail_->width(), area.top(), context_rail_->width(),
                                   area.height());
}

void AiChatScreen::update_tab_order() {
    // sessions → transcript → composer (editor → attach → send)
    if (!sessions_ || !transcript_ || !composer_)
        return;
    QWidget* first = sessions_->isVisible() ? sessions_->list_widget() : nullptr;
    if (first && sessions_->search_widget() && sessions_->search_widget()->window() == window())
        setTabOrder(sessions_->search_widget(), first);
    if (first && first->window() == transcript_->window())
        setTabOrder(first, transcript_);
    if (composer_->editor())
        setTabOrder(transcript_, composer_->editor());
}

// ── Keys ────────────────────────────────────────────────────────────────────

void AiChatScreen::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        if (overlay_ != Overlay::None) {
            close_overlay();
            e->accept();
            return;
        }
        if (streaming_) {
            on_stop_requested();
            e->accept();
            return;
        }
    }
    QWidget::keyPressEvent(e);
}

bool AiChatScreen::eventFilter(QObject* obj, QEvent* event) {
    if (obj == scrim_ && event->type() == QEvent::MouseButtonPress) {
        close_overlay();
        return true;
    }
    if (bar_ && obj == bar_->command_field()) {
        // The command field is the session title, edited in place: leaving it without
        // Enter restores the saved title.
        if (event->type() == QEvent::FocusOut)
            QTimer::singleShot(0, this, [this]() {
                if (bar_ && bar_->command_field() && !bar_->command_field()->hasFocus())
                    bar_->set_command_text(active_session_title_);
            });
    }
    return QWidget::eventFilter(obj, event);
}

// ── State ───────────────────────────────────────────────────────────────────

QVariantMap AiChatScreen::save_state() const {
    QVariantMap s{{"session_id", active_session_id_}};
    if (composer_)
        s.insert("draft", composer_->text());
    if (sessions_)
        s.insert("search", sessions_->search_text());
    if (transcript_)
        s.insert("scroll", transcript_->following_tail() ? -1 : transcript_->scroll_position());
    QStringList paths;
    for (const auto& f : files_)
        paths << f.ref;
    if (!paths.isEmpty())
        s.insert("attached_files", paths);
    s.insert("sidebar_collapsed", sidebar_collapsed_);
    s.insert("tools_on", tools_on_);
    s.insert("nav_on", nav_on_);
    return s;
}

bool AiChatScreen::open_section(const QString& section, const QString& focus) {
    // The New Chat button (on_new_session) and a click in the session list
    // (on_session_selected). Validated before anything changes.
    const QString raw = section.trimmed();
    const QString key = raw.toLower();
    if (key == QLatin1String("new")) {
        on_new_session();
        return true;
    }
    QString sid;
    if (key.startsWith(QLatin1String("session:"))) {
        sid = raw.mid(8).trimmed();
        if (sid.isEmpty())
            return false;
    } else if (key.isEmpty() || key == QLatin1String("sessions")) {
        sid = focus.trimmed();
        if (sid.isEmpty())
            return true;
    } else {
        return false;
    }
    for (const auto& s : std::as_const(sessions_cache_)) {
        if (s.id != sid)
            continue;
        if (active_session_id_ != sid)
            on_session_selected(sid);
        return true;
    }
    return false; // no such session: nothing changes
}

void AiChatScreen::restore_state(const QVariantMap& state) {
    const QString sid = state.value("session_id").toString();
    if (!sid.isEmpty() && sid != active_session_id_) {
        for (const auto& s : std::as_const(sessions_cache_)) {
            if (s.id == sid) {
                on_session_selected(sid);
                break;
            }
        }
        // Session not found (may have been deleted) — leave the default selection.
    }

    const QString search_text = state.value("search").toString();
    if (sessions_ && !search_text.isEmpty())
        sessions_->set_search_text(search_text);

    const QString draft = state.value("draft").toString();
    if (composer_ && !draft.isEmpty())
        composer_->set_text(draft);

    // Attached files — restored only while they still exist on disk (metadata only).
    QStringList paths = state.value("attached_files").toStringList();
    const QString legacy = state.value("attached_file").toString(); // v2 / v3: one file
    if (!legacy.isEmpty())
        paths << legacy;
    QStringList existing;
    for (const QString& p : std::as_const(paths))
        if (!p.isEmpty() && QFile::exists(p))
            existing << p;
    if (!existing.isEmpty())
        add_files(existing);

    if (state.contains("tools_on"))
        tools_on_ = state.value("tools_on").toBool();
    if (state.contains("nav_on"))
        nav_on_ = state.value("nav_on").toBool();
    if (bar_) {
        bar_->set_action_checked(QStringLiteral("tools"), tools_on_);
        bar_->set_action_checked(QStringLiteral("nav"), nav_on_);
        bar_->set_action_enabled(QStringLiteral("nav"), tools_on_);
    }
    if (context_rail_)
        context_rail_->set_tool_policy(tools_on_, nav_on_);

    if (state.contains("sidebar_collapsed")) {
        sidebar_collapsed_ = state.value("sidebar_collapsed").toBool();
        apply_zones(/*force=*/true);
    }

    // Scroll position — after the transcript has laid out (-1 / absent = follow the tail).
    const int scroll = state.value("scroll", -1).toInt();
    if (scroll >= 0 && transcript_) {
        QPointer<AiChatScreen> self = this;
        QTimer::singleShot(0, this, [self, scroll]() {
            if (self && self->transcript_)
                self->transcript_->set_scroll_position(scroll);
        });
    }
}

// ── IGroupLinked ────────────────────────────────────────────────────────────

void AiChatScreen::set_group(fincept::SymbolGroup g) {
    if (link_group_ == g)
        return;
    link_group_ = g;
    // Drop the cached symbol when leaving a group — the next send must not carry a context
    // the user can't see was inherited.
    if (g == fincept::SymbolGroup::None) {
        linked_symbol_ = {};
        linked_context_pending_ = false;
    }
    refresh_context_ui();
}

void AiChatScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    // Consumer only. The symbol is shown as an attached context item (rail + composer chip,
    // with DETACH) and goes out as "[Context: SYM (class)]" with the next message only.
    if (!ref.is_valid()) {
        linked_symbol_ = {};
        linked_context_pending_ = false;
    } else {
        linked_symbol_ = ref;
        linked_context_pending_ = true;
        known_symbols_.insert(ref.symbol.toUpper());
        if (transcript_)
            transcript_->set_known_symbols(known_symbols_);
    }
    refresh_context_ui();
}

} // namespace fincept::screens
