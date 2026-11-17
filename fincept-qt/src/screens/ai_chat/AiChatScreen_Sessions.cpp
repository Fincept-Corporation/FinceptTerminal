// src/screens/ai_chat/AiChatScreen_Sessions.cpp
//
// Sessions: the list (pins, snippets), load_messages (the whole conversation goes to the
// virtualised transcript; superseded answers dimmed), new / select / rename / delete,
// Markdown export, and the conversation-model notice.
//
// Part of the partial-class split of AiChatScreen.cpp.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/ai_chat/AiChatComposer.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "screens/ai_chat/AiChatSessionRail.h"
#include "screens/ai_chat/AiChatStartPage.h"
#include "screens/ai_chat/AiChatTranscript.h"
#include "services/llm/LlmService.h"
#include "services/llm/ModelRouter.h"
#include "storage/repositories/ChatRepository.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace fincept::screens {

namespace {

// File-local logging tag (unique in the directory: unity builds merge files).
constexpr const char* kAicSessionsTag = "AiChatScreen";

QString ses_generate_title() {
    static const QStringList prefixes = {"Amber", "Apex", "Atlas", "Echo", "Flux", "Nova", "Slate", "Vector"};
    static const QStringList nouns = {"Brief", "Drift", "Focus", "Ledger", "Macro", "Pulse", "Signal", "Tape"};
    const int pi = int(QRandomGenerator::global()->bounded(qsizetype(prefixes.size())));
    const int ni = int(QRandomGenerator::global()->bounded(qsizetype(nouns.size())));
    const QString sfx =
        QString::number(QRandomGenerator::global()->bounded(0x10000), 16).rightJustified(4, QLatin1Char('0')).toUpper();
    return prefixes[pi] + QLatin1Char(' ') + nouns[ni] + QLatin1Char(' ') + sfx;
}

QString ses_display_title(const ChatSession& s) {
    if (!s.title.trimmed().isEmpty() && s.title.trimmed().toLower() != QLatin1String("chat"))
        return s.title;
    return QObject::tr("Session %1").arg(s.id.left(4).toUpper());
}

QString ses_provider_display(const QString& provider, const QString& model) {
    aichat::Turn t;
    t.role = aichat::TurnRole::Assistant;
    t.provider = provider;
    t.model = model;
    return aichat::turn_speaker(t);
}

} // namespace

// ── List ────────────────────────────────────────────────────────────────────

void AiChatScreen::load_pins() {
    pinned_.clear();
    const auto r = SettingsRepository::instance().get(QStringLiteral("ai_chat.pinned_sessions"));
    if (r.is_err() || r.value().isEmpty())
        return;
    for (const auto& v : QJsonDocument::fromJson(r.value().toUtf8()).array())
        if (!v.toString().isEmpty())
            pinned_.insert(v.toString());
}

void AiChatScreen::store_pins() const {
    QJsonArray arr;
    for (const QString& id : pinned_)
        arr.append(id);
    const auto r = SettingsRepository::instance().set(QStringLiteral("ai_chat.pinned_sessions"),
                                                      QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)),
                                                      QStringLiteral("ai_chat"));
    if (r.is_err())
        LOG_WARN(kAicSessionsTag, "Could not store the pinned sessions");
}

void AiChatScreen::load_sessions() {
    sessions_cache_.clear();
    auto result = ChatRepository::instance().list_sessions();
    if (result.is_ok()) {
        sessions_cache_.reserve(result.value().size());
        for (const auto& s : result.value()) {
            aichat::SessionSummary row;
            row.id = s.id;
            row.title = ses_display_title(s);
            row.snippet = snippets_.value(s.id);
            row.provider = s.provider;
            row.model = s.model;
            row.updated = aichat::db_time_local(s.updated_at);
            row.messages = s.message_count;
            row.pinned = pinned_.contains(s.id);
            row.running = streaming_ && s.id == pending_req_session_;
            row.unread = unread_ && s.id == active_session_id_;
            sessions_cache_.append(row);
        }
    } else {
        LOG_ERROR(kAicSessionsTag, "list_sessions failed: " + QString::fromStdString(result.error()));
    }
    if (sessions_) {
        sessions_->set_sessions(sessions_cache_);
        sessions_->set_active(active_session_id_);
    }
    if (start_page_) {
        QVector<aichat::SessionSummary> recent;
        for (const auto& s : std::as_const(sessions_cache_)) {
            if (s.id == active_session_id_ || s.messages == 0)
                continue;
            recent.append(s);
            if (recent.size() >= 5)
                break;
        }
        start_page_->set_recent(recent);
    }

    bool active_listed = false;
    for (const auto& s : std::as_const(sessions_cache_))
        active_listed = active_listed || s.id == active_session_id_;
    if (active_listed)
        return;
    if (!sessions_cache_.isEmpty())
        on_session_selected(sessions_cache_.first().id); // newest first (list_sessions order)
    else
        create_new_session();
}

QString AiChatScreen::session_title(const QString& id) const {
    for (const auto& s : sessions_cache_)
        if (s.id == id)
            return s.title;
    return id.left(8);
}

void AiChatScreen::create_new_session() {
    if (streaming_)
        return;
    const QString title = ses_generate_title();
    auto result = ChatRepository::instance().create_session(title, ai_chat::LlmService::instance().active_provider(),
                                                            ai_chat::LlmService::instance().active_model());
    if (result.is_err()) {
        LOG_ERROR(kAicSessionsTag, "create_new_session failed: " + QString::fromStdString(result.error()));
        return;
    }
    active_session_id_ = result.value().id;
    active_session_title_ = result.value().title;
    history_.clear();
    total_tokens_ = 0;
    total_messages_ = 0;
    run_prompt_tokens_ = run_completion_tokens_ = -1;
    run_cost_usd_ = -1.0;
    last_answer_ms_ = -1;
    last_answer_at_ = QDateTime();
    last_failed_ = false;
    conv_provider_.clear();
    conv_model_.clear();
    model_notice_dismissed_ = false;
    last_answer_turn_ = 0;
    streaming_turn_ = 0;
    tool_steps_.clear();
    if (transcript_)
        transcript_->clear();
    load_sessions();
    if (bar_)
        bar_->set_command_text(active_session_title_);
    show_start_page(true);
    update_stats();
}

void AiChatScreen::load_messages(const QString& session_id) {
    transcript_->clear();
    history_.clear();
    conv_provider_.clear();
    conv_model_.clear();
    total_tokens_ = 0;
    total_messages_ = 0;
    run_prompt_tokens_ = run_completion_tokens_ = -1;
    run_cost_usd_ = -1.0;
    last_answer_ms_ = -1;
    last_answer_at_ = QDateTime();
    last_failed_ = false;
    last_answer_turn_ = 0;
    streaming_turn_ = 0;
    tool_steps_.clear();

    auto result = ChatRepository::instance().get_messages(session_id);
    if (result.is_err()) {
        LOG_ERROR(kAicSessionsTag, "get_messages failed: " + QString::fromStdString(result.error()));
        show_start_page(false);
        add_notice(aichat::NoticeKind::Info,
                   tr("Could not load this conversation: %1").arg(QString::fromStdString(result.error())));
        update_stats();
        return;
    }
    const auto& msgs = result.value();
    const int n = int(msgs.size());

    // Regenerate keeps the superseded answer in the database: of consecutive assistant
    // rows, only the LAST is the live answer. The earlier ones are shown dimmed and never
    // sent back to the model.
    QVector<bool> superseded(n, false);
    for (int i = 0; i + 1 < n; ++i)
        superseded[i] = msgs[i].role == QLatin1String("assistant") && msgs[i + 1].role == QLatin1String("assistant");

    QVector<aichat::Turn> turns;
    turns.reserve(n);
    int last_live = -1; // index in `turns` of the newest non-system message
    QString last_role;
    for (int i = 0; i < n; ++i) {
        const auto& msg = msgs[i];
        total_tokens_ += msg.tokens_used; // superseded answers were billed too
        aichat::Turn t;
        t.when = aichat::db_time_local(msg.timestamp);
        if (msg.role == QLatin1String("system")) {
            t.role = aichat::TurnRole::Notice;
            t.notice = aichat::NoticeKind::Info;
            t.text = msg.content;
            turns.append(t);
            continue;
        }
        if (msg.role == QLatin1String("assistant")) {
            QVector<aichat::ToolStep> steps;
            const QString content = aichat::strip_tool_trailer(msg.content, &steps);
            t.role = aichat::TurnRole::Assistant;
            t.state = superseded[i]                                   ? aichat::TurnState::Superseded
                      : content.trimmed().endsWith(QLatin1String("*(stopped)*")) ? aichat::TurnState::Stopped
                                                                                 : aichat::TurnState::Done;
            t.text = content;
            t.tools = steps;
            t.provider = msg.provider;
            t.model = msg.model;
            t.tokens = msg.tokens_used;
            for (const auto& s : std::as_const(steps)) {
                const QString arg = s.label.section(QStringLiteral(" · "), 1, 1).trimmed().toUpper();
                if (aichat::looks_like_ticker(arg))
                    known_symbols_.insert(arg);
            }
            if (!superseded[i]) {
                history_.push_back({msg.role, content});
                if (!msg.provider.isEmpty()) {
                    conv_provider_ = msg.provider;
                    conv_model_ = msg.model;
                }
            }
        } else {
            history_.push_back({msg.role, msg.content});
            const aichat::UserEnvelope env = aichat::parse_user_wire(msg.content);
            t.role = aichat::TurnRole::User;
            t.text = env.typed;
            t.attachments = env.chips;
            for (const auto& a : env.chips)
                if (a.kind == aichat::Attachment::Kind::Symbol && !a.ref.isEmpty())
                    known_symbols_.insert(a.ref.toUpper());
        }
        turns.append(t);
        last_live = int(turns.size()) - 1;
        last_role = msg.role;
    }
    total_messages_ = n;

    transcript_->set_known_symbols(known_symbols_);
    const QVector<quint64> ids = transcript_->set_turns(turns);
    // REGENERATE on the newest answer; RETRY when the conversation ends on an unanswered
    // message (a failed turn, or the app closed mid-answer).
    if (last_live >= 0 && last_live < ids.size()) {
        if (last_role == QLatin1String("assistant")) {
            last_answer_turn_ = ids[last_live];
            transcript_->set_live_turn(last_answer_turn_);
        } else if (last_role == QLatin1String("user")) {
            const quint64 id = add_notice(aichat::NoticeKind::Error,
                                          tr("This message has no answer — the request failed or the app closed "
                                             "before it finished. RETRY sends it again."));
            transcript_->set_live_turn(id);
        }
    }
    if (n > 0) {
        const auto& last = msgs[n - 1];
        snippets_[session_id] = aichat::snippet_of(last.role, last.content);
        if (sessions_)
            sessions_->set_snippet(session_id, snippets_.value(session_id));
    }
    show_start_page(n == 0);
    LOG_DEBUG(kAicSessionsTag, QString("Loaded a conversation: %1 messages").arg(n));
    update_stats();
}

void AiChatScreen::show_start_page(bool show) {
    if (!start_page_ || !transcript_)
        return;
    if (show)
        start_page_->refresh_setup();
    start_page_->setVisible(show);
    transcript_->setVisible(!show);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void AiChatScreen::on_new_session() {
    if (streaming_)
        return;
    if (overlay_ == Overlay::Sessions)
        close_overlay();
    // An empty conversation is already a new one — don't litter the list with untouched
    // sessions every time New is pressed.
    if (!active_session_id_.isEmpty() && total_messages_ == 0 && history_.empty()) {
        show_start_page(true);
        composer_->focus_editor();
        return;
    }
    create_new_session();
    composer_->focus_editor();
}

void AiChatScreen::on_session_selected(const QString& id) {
    if (streaming_ || id.isEmpty())
        return;
    active_session_id_ = id;
    active_session_title_ = session_title(id);
    model_notice_dismissed_ = false;
    if (sessions_)
        sessions_->set_active(id);
    if (bar_)
        bar_->set_command_text(active_session_title_);
    load_messages(id);
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::begin_rename() {
    if (streaming_ || active_session_id_.isEmpty() || !bar_)
        return;
    bar_->set_command_text(active_session_title_);
    bar_->focus_command_field(true);
}

void AiChatScreen::rename_session(const QString& id, const QString& title) {
    if (streaming_ || id.isEmpty())
        return;
    const QString clean = title.trimmed().left(200);
    if (clean.isEmpty() || clean == session_title(id)) {
        if (bar_)
            bar_->set_command_text(active_session_title_);
        return;
    }
    if (auto r = ChatRepository::instance().update_session_title(id, clean); r.is_err()) {
        QMessageBox::warning(this, tr("Rename Session"),
                             tr("Could not rename the session: %1").arg(QString::fromStdString(r.error())));
        if (bar_)
            bar_->set_command_text(active_session_title_);
        return;
    }
    if (id == active_session_id_)
        active_session_title_ = clean;
    if (bar_)
        bar_->set_command_text(active_session_title_);
    load_sessions();
    status_->show_notice(tr("Session renamed"), QString(), {}, 3000);
}

void AiChatScreen::delete_session(const QString& id) {
    if (streaming_ || id.isEmpty())
        return;
    // Permanently drops the session and all of its messages from ChatRepository — confirm.
    const QString label = session_title(id);
    if (QMessageBox::question(this, tr("Delete Session"),
                              tr("Delete \"%1\" and all of its messages?\n\nThis cannot be undone.").arg(label),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (auto r = ChatRepository::instance().delete_session(id); r.is_err()) {
        QMessageBox::warning(this, tr("Delete Session"),
                             tr("Could not delete the session: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    snippets_.remove(id);
    if (pinned_.remove(id))
        store_pins();
    if (id == active_session_id_) {
        active_session_id_.clear();
        active_session_title_.clear();
        history_.clear();
        total_messages_ = 0;
        total_tokens_ = 0;
        transcript_->clear();
    }
    load_sessions(); // selects the newest remaining session (or creates one)
}

void AiChatScreen::set_pinned(const QString& id, bool pinned) {
    if (id.isEmpty())
        return;
    if (pinned)
        pinned_.insert(id);
    else
        pinned_.remove(id);
    store_pins();
    for (auto& s : sessions_cache_)
        if (s.id == id)
            s.pinned = pinned;
    if (sessions_) {
        sessions_->set_sessions(sessions_cache_);
        sessions_->set_active(active_session_id_);
    }
}

// ── Markdown export ─────────────────────────────────────────────────────────
// Built from the database (the full conversation), with tool activity listed under each
// answer and superseded answers marked as such.

QString AiChatScreen::conversation_markdown(const QString& session_id) const {
    if (session_id.isEmpty())
        return {};
    auto result = ChatRepository::instance().get_messages(session_id);
    if (result.is_err())
        return {};
    const auto& msgs = result.value();
    const QString title = session_id == active_session_id_ && !active_session_title_.isEmpty()
                              ? active_session_title_
                              : session_title(session_id);
    QString md = QStringLiteral("# ") + title + QStringLiteral("\n\n");
    md += QStringLiteral("_") +
          tr("Exported from Fincept Terminal · %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")) +
          QStringLiteral("_\n\n");
    for (int i = 0; i < msgs.size(); ++i) {
        const auto& m = msgs[i];
        const QDateTime when = aichat::db_time_local(m.timestamp);
        const QString stamp = when.isValid() ? when.toString("yyyy-MM-dd HH:mm") : QString();
        QString who;
        QString body = m.content;
        QVector<aichat::ToolStep> steps;
        if (m.role == QLatin1String("user")) {
            who = tr("You");
        } else if (m.role == QLatin1String("assistant")) {
            body = aichat::strip_tool_trailer(m.content, &steps);
            who = tr("AI");
            const QString via = ses_provider_display(m.provider, m.model);
            if (!via.isEmpty())
                who += QStringLiteral(" (") + via + QStringLiteral(")");
            if (i + 1 < msgs.size() && msgs[i + 1].role == QLatin1String("assistant"))
                who += QStringLiteral(" — ") + tr("superseded by the regenerated answer below");
        } else {
            who = tr("System");
        }
        md += QStringLiteral("---\n\n**") + who + QStringLiteral("**");
        if (!stamp.isEmpty())
            md += QStringLiteral(" · ") + stamp;
        if (m.tokens_used > 0)
            md += QStringLiteral(" · ") + tr("%1 tokens").arg(m.tokens_used);
        md += QStringLiteral("\n\n");
        if (!steps.isEmpty()) {
            md += QStringLiteral("<details><summary>") + tr("Tools used (%1)").arg(steps.size()) +
                  QStringLiteral("</summary>\n\n");
            for (const auto& s : std::as_const(steps)) {
                const QString mark = s.state == aichat::ToolStep::State::Failed ? QStringLiteral("✗")
                                     : s.state == aichat::ToolStep::State::Job  ? QStringLiteral("⧗")
                                                                                : QStringLiteral("✓");
                md += QStringLiteral("- ") + mark + QStringLiteral(" `") + s.label + QStringLiteral("`");
                if (s.elapsed_ms >= 0)
                    md += QStringLiteral(" — ") + aichat::format_ms(s.elapsed_ms);
                if (!s.detail.isEmpty())
                    md += QStringLiteral(" — ") + s.detail;
                md += QStringLiteral("\n");
            }
            md += QStringLiteral("\n</details>\n\n");
        }
        md += body.trimmed() + QStringLiteral("\n\n");
    }
    return md;
}

void AiChatScreen::export_markdown(const QString& session_id) {
    if (session_id.isEmpty())
        return;
    const QString md = conversation_markdown(session_id);
    if (md.isEmpty()) {
        QMessageBox::warning(this, tr("Export Conversation"), tr("Could not read this conversation."));
        return;
    }
    QString base = session_title(session_id);
    if (base.isEmpty())
        base = QStringLiteral("conversation");
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9 _.-]")), QStringLiteral("_"));
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Conversation"),
                                                      QDir(dir).filePath(base.trimmed() + QStringLiteral(".md")),
                                                      tr("Markdown (*.md);;Text (*.txt)"));
    if (path.isEmpty())
        return;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text) || f.write(md.toUtf8()) < 0 || !f.commit()) {
        QMessageBox::warning(this, tr("Export Conversation"),
                             tr("Could not write \"%1\": %2").arg(QFileInfo(path).fileName(), f.errorString()));
        return;
    }
    LOG_INFO(kAicSessionsTag, QString("Exported a conversation (%1 characters)").arg(md.size()));
    status_->show_notice(tr("Saved %1").arg(QFileInfo(path).fileName()), QString(), {}, 4000);
}

void AiChatScreen::copy_markdown(const QString& session_id) {
    if (session_id.isEmpty())
        return;
    const QString md = conversation_markdown(session_id);
    if (md.isEmpty())
        return;
    QApplication::clipboard()->setText(md);
    status_->show_notice(tr("Copied the conversation (%1 characters)").arg(md.size()), QString(), {}, 4000);
}

// ── Conversation model notice ───────────────────────────────────────────────

void AiChatScreen::update_model_notice() {
    if (!model_notice_)
        return;
    auto& llm = ai_chat::LlmService::instance();
    if (conv_provider_.isEmpty() || model_notice_dismissed_ || streaming_ || !llm.is_configured()) {
        model_notice_->hide();
        return;
    }
    const QString active_provider = llm.active_provider();
    const QString active_model = llm.active_model();
    const bool same_provider = conv_provider_.compare(active_provider, Qt::CaseInsensitive) == 0;
    // Fincept's model id is internal — a provider match is a match. A harness reports its
    // full model name ("claude-sonnet-…") for an alias ("sonnet").
    const bool same_model = conv_model_.isEmpty() || conv_model_ == active_model ||
                            conv_provider_.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0 ||
                            ai_chat::provider_is_harness(conv_provider_);
    if (same_provider && same_model) {
        model_notice_->hide();
        return;
    }
    const QString was = ses_provider_display(conv_provider_, conv_model_);
    const QString now = ses_provider_display(active_provider, active_model);
    model_notice_lbl_->setText(
        tr("This conversation's last answer came from %1. New messages will go to %2.").arg(was, now));
    // The switch only makes sense while that model can still answer.
    bool configured = false;
    if (ai_chat::provider_is_harness(conv_provider_)) {
        const QString hid = conv_provider_.mid(8);
        for (const auto& h : ai_chat::ModelRouter::instance().harness_states())
            configured = configured || (h.id == hid && h.eligibility.usable);
    } else if (conv_provider_.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0) {
        configured = true;
    } else if (auto r = LlmConfigRepository::instance().list_providers(); r.is_ok()) {
        for (const auto& c : r.value())
            if (c.provider.compare(conv_provider_, Qt::CaseInsensitive) == 0)
                configured = true;
    }
    model_notice_btn_->setVisible(configured);
    model_notice_btn_->setText(ui::literal_ampersands(tr("USE %1").arg(was.toUpper())));
    model_notice_btn_->setAccessibleName(tr("Use %1 for this conversation").arg(was));
    model_notice_btn_->setToolTip(tr("Answer AI Chat with %1 again (your choice for AI Chat — the model badge in the "
                                     "command bar shows it; Auto restores the default rule)")
                                      .arg(was));
    model_notice_->show();
}

void AiChatScreen::on_use_conversation_model() {
    if (streaming_ || conv_provider_.isEmpty())
        return;
    // AI Chat's model is chosen per feature (ModelRouter): store the conversation's model
    // as AI Chat's explicit choice instead of switching the global active provider.
    ai_chat::ModelChoice c;
    if (ai_chat::provider_is_harness(conv_provider_)) {
        const QString hid = conv_provider_.mid(8);
        c.kind = ai_chat::ModelChoice::Kind::Harness;
        c.harness_id = hid;
        c.provider = conv_provider_;
        c.model = conv_model_;
        for (const auto& h : ai_chat::ModelRouter::instance().harness_states()) {
            if (h.id == hid) {
                c.vendor = h.vendor;
                c.effort = h.default_effort;
                c.label = ai_chat::ModelRouter::harness_label(h.display_name, conv_model_, h.default_effort);
            }
        }
    } else if (conv_provider_.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0) {
        c.kind = ai_chat::ModelChoice::Kind::Fincept;
        c.provider = QStringLiteral("fincept");
        c.label = QStringLiteral("Fincept LLM");
    } else {
        bool configured = false;
        if (auto r = LlmConfigRepository::instance().list_providers(); r.is_ok())
            for (const auto& row : r.value())
                configured = configured || row.provider.compare(conv_provider_, Qt::CaseInsensitive) == 0;
        if (!configured) {
            QMessageBox::information(this, tr("Switch Model"),
                                     tr("%1 is no longer configured. Add it in Settings → LLM Configuration.")
                                         .arg(ses_provider_display(conv_provider_, conv_model_)));
            return;
        }
        c.kind = ai_chat::ModelChoice::Kind::Api;
        c.provider = conv_provider_.toLower();
        c.model = conv_model_;
        c.label = ses_provider_display(conv_provider_, conv_model_);
    }
    QString error;
    if (!ai_chat::ModelRouter::instance().set_choice(QString::fromLatin1(ai_chat::kFeatureAiChat), {}, c, &error)) {
        QMessageBox::warning(this, tr("Switch Model"), tr("Could not switch the model: %1").arg(error));
        return;
    }
    LOG_INFO(kAicSessionsTag, QString("AI Chat model switched back to %1 for this conversation").arg(c.label));
    update_stats();
}

} // namespace fincept::screens
