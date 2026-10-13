// src/screens/ai_chat/AiChatScreen_Sessions.cpp
//
// Session list / sidebar interactions: load_sessions, create_new_session,
// load_messages (render window, persisted tool cards, superseded answers),
// on_new/select/rename/delete/attach_file, session context menu, sidebar
// collapse, show_welcome, Markdown export, and the conversation-model notice.
//
// Part of the partial-class split of AiChatScreen.cpp.

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/ChatRepository.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>

namespace fincept::screens {

// File-local logging tag. Renamed to avoid a unity-build collision with the
// anonymous-namespace `TAG` in AiChatScreen.cpp — two `namespace {}` blocks
// in the same TU produce two distinct anonymous namespaces, so an unqualified
// `TAG` reference becomes ambiguous when the unity bucket merges both.
static constexpr const char* TAG_SESSIONS = "AiChatScreen";

// Title text of a session row (the row text also carries the meta line).
static constexpr int kSessionTitleRole = Qt::UserRole + 1;

static QString generate_session_title() {
    static const QStringList prefixes = {"Amber", "Apex", "Atlas", "Echo", "Flux", "Nova", "Slate", "Vector"};
    static const QStringList nouns = {"Brief", "Drift", "Focus", "Ledger", "Macro", "Pulse", "Signal", "Tape"};
    const int pi = QRandomGenerator::global()->bounded(prefixes.size());
    const int ni = QRandomGenerator::global()->bounded(nouns.size());
    const QString sfx =
        QString::number(QRandomGenerator::global()->bounded(0x10000), 16).rightJustified(4, '0').toUpper();
    return QString("%1 %2 %3").arg(prefixes[pi], nouns[ni], sfx);
}

static QString display_session_title(const ChatSession& s) {
    if (!s.title.trimmed().isEmpty() && s.title.trimmed().toLower() != "chat")
        return s.title;
    return QObject::tr("Session %1").arg(s.id.left(4).toUpper());
}

// chat_sessions / chat_messages timestamps are SQLite datetime('now') — UTC.
static QDateTime ai_chat_db_time_local(const QString& stamp) {
    QDateTime dt = QDateTime::fromString(stamp, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (dt.isValid())
        return QDateTime(dt.date(), dt.time(), QTimeZone(QTimeZone::UTC)).toLocalTime();
    dt = QDateTime::fromString(stamp, Qt::ISODate);
    return dt.isValid() ? dt.toLocalTime() : QDateTime();
}

static QString display_session_meta(const ChatSession& s) {
    const QDateTime dt = ai_chat_db_time_local(s.updated_at);
    const QString stamp = dt.isValid() ? dt.toString("MMM d · hh:mm") : QString();
    return s.message_count > 0 ? QObject::tr("%n msg", "", s.message_count) +
                                     (stamp.isEmpty() ? QString() : QStringLiteral("  ·  ") + stamp)
                               : stamp;
}

static QString ai_chat_provider_display(const QString& provider, const QString& model) {
    if (provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0)
        return QObject::tr("Fincept LLM");
    if (provider.isEmpty())
        return model;
    return model.isEmpty() ? provider : provider + QStringLiteral(" / ") + model;
}

void AiChatScreen::on_toggle_sidebar() {
    apply_sidebar_collapsed(!sidebar_collapsed_, /*animate=*/true);
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::apply_sidebar_collapsed(bool collapsed, bool animate) {
    sidebar_collapsed_ = collapsed;
    if (!sidebar_)
        return;

    const int target = collapsed ? 0 : kSidebarExpandedWidth;

    // Lazily create the animation so the helper works even before the first
    // user toggle (e.g. when restore_state runs before the screen is shown).
    if (!sidebar_anim_) {
        sidebar_anim_ = new QPropertyAnimation(sidebar_, "maximumWidth", this);
        sidebar_anim_->setDuration(180);
        sidebar_anim_->setEasingCurve(QEasingCurve::OutCubic);
    }

    sidebar_anim_->stop();
    if (animate) {
        sidebar_anim_->setStartValue(sidebar_->maximumWidth());
        sidebar_anim_->setEndValue(target);
        sidebar_anim_->start();
    } else {
        sidebar_->setMaximumWidth(target);
    }

    if (sidebar_toggle_btn_) {
        sidebar_toggle_btn_->setText(collapsed ? "›" : "‹");
        sidebar_toggle_btn_->setToolTip(collapsed ? tr("Expand sidebar  (Ctrl+B)") : tr("Collapse sidebar  (Ctrl+B)"));
    }
}

void AiChatScreen::on_search_changed(const QString& text) {
    // Matches the conversation title only — the row text also carries the
    // "N msg · date" meta line, so searching "msg" used to match every row.
    const QString filter = text.trimmed();
    for (int i = 0; i < session_list_->count(); ++i) {
        auto* item = session_list_->item(i);
        const QString title = item->data(kSessionTitleRole).toString();
        item->setHidden(!filter.isEmpty() && !title.contains(filter, Qt::CaseInsensitive));
    }
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::load_sessions() {
    session_list_->blockSignals(true);
    session_list_->clear();
    auto result = ChatRepository::instance().list_sessions();
    if (result.is_ok()) {
        for (const auto& s : result.value()) {
            const QString meta = display_session_meta(s);
            const QString title = display_session_title(s);
            const QString label = title + (meta.isEmpty() ? "" : "\n" + meta);
            auto* item = new QListWidgetItem(label);
            item->setData(Qt::UserRole, s.id);
            item->setData(kSessionTitleRole, title);
            item->setToolTip(title + (meta.isEmpty() ? "" : "\n" + meta));
            item->setSizeHint(QSize(0, 52));
            session_list_->addItem(item);
        }
    } else {
        LOG_ERROR(TAG_SESSIONS, "list_sessions failed: " + QString::fromStdString(result.error()));
    }
    session_list_->blockSignals(false);
    // Keep the search filter applied across reloads (rename, new session…).
    if (search_edit_ && !search_edit_->text().trimmed().isEmpty())
        on_search_changed(search_edit_->text());

    if (!active_session_id_.isEmpty()) {
        for (int i = 0; i < session_list_->count(); ++i) {
            if (session_list_->item(i)->data(Qt::UserRole).toString() == active_session_id_) {
                session_list_->blockSignals(true);
                session_list_->setCurrentRow(i);
                session_list_->blockSignals(false);
                return;
            }
        }
    }
    if (session_list_->count() > 0)
        session_list_->setCurrentRow(0);
    else
        create_new_session();
}

void AiChatScreen::create_new_session() {
    if (streaming_)
        return;
    const QString title = generate_session_title();
    auto result = ChatRepository::instance().create_session(title, ai_chat::LlmService::instance().active_provider(),
                                                            ai_chat::LlmService::instance().active_model());
    if (result.is_err()) {
        LOG_ERROR(TAG_SESSIONS, "create_new_session failed: " + QString::fromStdString(result.error()));
        return;
    }
    active_session_id_ = result.value().id;
    active_session_title_ = result.value().title;
    history_.clear();
    total_tokens_ = 0;
    total_messages_ = 0;
    conv_provider_.clear();
    conv_model_.clear();
    model_notice_dismissed_ = false;
    render_limit_ = kDefaultRenderLimit;
    clear_messages();
    load_sessions();
    hdr_session_lbl_->setText(active_session_title_);
    delete_btn_->setEnabled(true);
    rename_btn_->setEnabled(true);
    update_stats();
    show_welcome(true);
}

void AiChatScreen::load_messages(const QString& session_id) {
    clear_messages();
    history_.clear();
    conv_provider_.clear();
    conv_model_.clear();
    total_tokens_ = 0;
    total_messages_ = 0;
    auto result = ChatRepository::instance().get_messages(session_id);
    if (result.is_err()) {
        LOG_ERROR(TAG_SESSIONS, "get_messages failed: " + QString::fromStdString(result.error()));
        add_message_bubble("system", tr("Could not load this conversation: %1")
                                         .arg(QString::fromStdString(result.error())));
        update_stats();
        return;
    }
    const auto& msgs = result.value();
    const int n = int(msgs.size());

    // Regenerate keeps the superseded answer in the database: of consecutive
    // assistant rows, only the LAST is the live answer. The earlier ones are
    // shown dimmed and never sent back to the model.
    QVector<bool> superseded(n, false);
    for (int i = 0; i + 1 < n; ++i)
        superseded[i] = msgs[i].role == QLatin1String("assistant") && msgs[i + 1].role == QLatin1String("assistant");

    for (int i = 0; i < n; ++i) {
        const auto& msg = msgs[i];
        total_tokens_ += msg.tokens_used; // superseded answers were billed too
        if (msg.role == QLatin1String("system") || superseded[i])
            continue;
        if (msg.role == QLatin1String("assistant")) {
            history_.push_back({msg.role, strip_tool_trailer(msg.content, nullptr)});
            if (!msg.provider.isEmpty()) {
                conv_provider_ = msg.provider;
                conv_model_ = msg.model;
            }
        } else {
            history_.push_back({msg.role, msg.content});
        }
    }
    total_messages_ = n;

    // Render window: only the newest render_limit_ messages get widgets.
    const int first = std::max(0, n - render_limit_);
    hidden_message_count_ = first;
    QLabel* newest_body = nullptr;
    QString newest_role;
    for (int i = first; i < n; ++i) {
        const auto& msg = msgs[i];
        if (msg.role == QLatin1String("assistant")) {
            QJsonArray steps;
            const QString content = strip_tool_trailer(msg.content, &steps);
            add_static_tools_card(steps);
            newest_body = add_message_bubble("assistant", content, msg.timestamp,
                                             answer_footer_meta(msg.provider, msg.model, msg.tokens_used));
            if (superseded[i]) {
                if (last_answer_row_) {
                    auto* fx = new QGraphicsOpacityEffect(last_answer_row_);
                    fx->setOpacity(0.5);
                    last_answer_row_->setGraphicsEffect(fx);
                }
                if (last_answer_role_lbl_)
                    last_answer_role_lbl_->setText(tr("AI · superseded — regenerated below"));
            }
        } else {
            newest_body = add_message_bubble(msg.role, msg.content, msg.timestamp);
        }
        newest_role = msg.role;
    }
    if (hidden_message_count_ > 0) {
        load_earlier_btn_->setText(tr("Show %n earlier message(s)", "", hidden_message_count_));
        load_earlier_btn_->show();
    }
    // Regenerate on the newest answer; Retry when the conversation ends on an
    // unanswered message (a failed turn, or the app closed mid-answer).
    if (newest_body && newest_role == QLatin1String("assistant"))
        add_answer_action(newest_body, /*failed=*/false);
    else if (newest_body && newest_role == QLatin1String("user"))
        add_answer_action(newest_body, /*failed=*/true);

    show_welcome(msgs.empty());
    scroll_to_bottom();
    update_stats();
}

void AiChatScreen::on_load_earlier() {
    if (streaming_ || active_session_id_.isEmpty())
        return;
    render_limit_ += kDefaultRenderLimit;
    load_messages(active_session_id_);
    // The user asked for OLDER messages: land at the top, not the bottom.
    stick_to_bottom_ = false;
    QTimer::singleShot(0, this, [this]() {
        if (!scroll_area_ || !scroll_area_->verticalScrollBar())
            return;
        stick_to_bottom_ = false;
        programmatic_scroll_ = true;
        scroll_area_->verticalScrollBar()->setValue(0);
        programmatic_scroll_ = false;
    });
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AiChatScreen::on_new_session() {
    if (streaming_)
        return;
    // An empty conversation is already a new one — don't litter the list with
    // untouched sessions every time New is pressed.
    if (!active_session_id_.isEmpty() && total_messages_ == 0 && history_.empty()) {
        input_box_->setFocus();
        return;
    }
    create_new_session();
    input_box_->setFocus();
}

void AiChatScreen::on_session_selected(int row) {
    if (streaming_ || row < 0 || row >= session_list_->count())
        return;
    auto* item = session_list_->item(row);
    active_session_id_ = item->data(Qt::UserRole).toString();
    active_session_title_ = item->data(kSessionTitleRole).toString();
    delete_btn_->setEnabled(true);
    rename_btn_->setEnabled(true);
    if (export_btn_)
        export_btn_->setEnabled(true);
    hdr_session_lbl_->setText(active_session_title_);
    model_notice_dismissed_ = false;
    render_limit_ = kDefaultRenderLimit;
    load_messages(active_session_id_);
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::on_rename_session() {
    if (streaming_ || active_session_id_.isEmpty())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename Session"), tr("Session name:"), QLineEdit::Normal,
                                               active_session_title_, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    const QString clean = name.trimmed().left(200);
    if (auto r = ChatRepository::instance().update_session_title(active_session_id_, clean); r.is_err()) {
        QMessageBox::warning(this, tr("Rename Session"),
                             tr("Could not rename the session: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    active_session_title_ = clean;
    hdr_session_lbl_->setText(active_session_title_);
    load_sessions();
}

void AiChatScreen::on_delete_session() {
    if (streaming_ || active_session_id_.isEmpty())
        return;
    // Delete sits next to Rename in the sidebar and permanently drops the
    // session and all of its messages from ChatRepository — confirm first.
    const QString label = active_session_title_.isEmpty() ? active_session_id_.left(8) : active_session_title_;
    if (QMessageBox::question(this, tr("Delete Session"),
                              tr("Delete \"%1\" and all of its messages?\n\nThis cannot be undone.").arg(label),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (auto r = ChatRepository::instance().delete_session(active_session_id_); r.is_err()) {
        QMessageBox::warning(this, tr("Delete Session"),
                             tr("Could not delete the session: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    active_session_id_.clear();
    active_session_title_.clear();
    history_.clear();
    total_messages_ = 0;
    total_tokens_ = 0;
    clear_messages();
    delete_btn_->setEnabled(false);
    rename_btn_->setEnabled(false);
    load_sessions();
}

void AiChatScreen::show_session_menu(const QPoint& pos) {
    if (streaming_)
        return;
    auto* item = session_list_->itemAt(pos);
    if (!item)
        return;
    // Act on the row under the cursor: select it first (loads it).
    if (session_list_->currentItem() != item)
        session_list_->setCurrentItem(item);
    auto* menu = new QMenu(session_list_);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("Rename…  (F2)")), &QAction::triggered, this, &AiChatScreen::on_rename_session);
    connect(menu->addAction(tr("Export as Markdown…")), &QAction::triggered, this, &AiChatScreen::on_export_markdown);
    connect(menu->addAction(tr("Copy as Markdown")), &QAction::triggered, this,
            &AiChatScreen::on_copy_conversation_markdown);
    menu->addSeparator();
    connect(menu->addAction(tr("Delete…  (Del)")), &QAction::triggered, this, &AiChatScreen::on_delete_session);
    menu->popup(session_list_->viewport()->mapToGlobal(pos));
}

void AiChatScreen::on_attach_file() {
    // Let user pick from File Manager index or browse disk
    QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Attach File to Message"), QString(),
        tr("All Files (*);;Text Files (*.txt *.md *.csv *.json);;Notebooks (*.ipynb);;PDF (*.pdf)"));
    if (paths.isEmpty())
        return;

    attached_file_path_ = paths.first(); // single attach for now
    QFileInfo fi(attached_file_path_);
    if (attach_badge_) {
        attach_badge_->setText(QStringLiteral("⊕ ") + fi.fileName());
        attach_badge_->setVisible(true);
    }
    if (attach_btn_)
        attach_btn_->setProperty("active", true);
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::show_welcome(bool show) {
    if (welcome_panel_)
        welcome_panel_->setVisible(show);
}

// ── Markdown export ───────────────────────────────────────────────────────────
// Built from the database (the full conversation, not just the rendered
// window), with tool activity listed under each answer and superseded answers
// marked as such.

QString AiChatScreen::conversation_markdown() const {
    auto result = ChatRepository::instance().get_messages(active_session_id_);
    if (result.is_err())
        return {};
    const auto& msgs = result.value();
    const QString title = active_session_title_.isEmpty() ? tr("AI Chat") : active_session_title_;
    QString md = QStringLiteral("# ") + title + QStringLiteral("\n\n");
    md += QStringLiteral("_") +
          tr("Exported from Fincept Terminal · %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")) +
          QStringLiteral("_\n\n");
    for (int i = 0; i < msgs.size(); ++i) {
        const auto& m = msgs[i];
        const QDateTime when = ai_chat_db_time_local(m.timestamp);
        const QString stamp = when.isValid() ? when.toString("yyyy-MM-dd HH:mm") : QString();
        QString who;
        QString body = m.content;
        QJsonArray steps;
        if (m.role == QLatin1String("user")) {
            who = tr("You");
        } else if (m.role == QLatin1String("assistant")) {
            body = strip_tool_trailer(m.content, &steps);
            who = tr("AI");
            const QString via = ai_chat_provider_display(m.provider, m.model);
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
            for (const auto& v : steps) {
                const QJsonObject o = v.toObject();
                const QString st = o.value("s").toString();
                const QString mark = st == QLatin1String("fail")  ? QStringLiteral("✗")
                                     : st == QLatin1String("job") ? QStringLiteral("⧗")
                                                                  : QStringLiteral("✓");
                md += QStringLiteral("- ") + mark + QStringLiteral(" `") + o.value("l").toString() +
                      QStringLiteral("`");
                if (o.contains("ms"))
                    md += QStringLiteral(" — %1 ms").arg(qint64(o.value("ms").toDouble()));
                if (!o.value("d").toString().isEmpty())
                    md += QStringLiteral(" — ") + o.value("d").toString();
                md += QStringLiteral("\n");
            }
            md += QStringLiteral("\n</details>\n\n");
        }
        md += body.trimmed() + QStringLiteral("\n\n");
    }
    return md;
}

void AiChatScreen::on_export_markdown() {
    if (active_session_id_.isEmpty())
        return;
    const QString md = conversation_markdown();
    if (md.isEmpty()) {
        QMessageBox::warning(this, tr("Export Conversation"), tr("Could not read this conversation."));
        return;
    }
    QString base = active_session_title_.isEmpty() ? QStringLiteral("conversation") : active_session_title_;
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
    LOG_INFO(TAG_SESSIONS, QString("Exported conversation to %1").arg(path));
}

void AiChatScreen::on_copy_conversation_markdown() {
    if (active_session_id_.isEmpty())
        return;
    const QString md = conversation_markdown();
    if (!md.isEmpty())
        QApplication::clipboard()->setText(md);
}

// ── Conversation model notice ─────────────────────────────────────────────────

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
    // Fincept's model id is internal — a provider match is a match.
    const bool same_model = conv_model_.isEmpty() || conv_model_ == active_model ||
                            conv_provider_.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0;
    if (same_provider && same_model) {
        model_notice_->hide();
        return;
    }
    const QString was = ai_chat_provider_display(conv_provider_, conv_model_);
    const QString now = ai_chat_provider_display(active_provider, active_model);
    model_notice_lbl_->setText(
        tr("This conversation's last answer came from %1. New messages will go to %2.").arg(was, now));
    // The switch only makes sense while that provider is still configured.
    bool configured = false;
    if (auto r = LlmConfigRepository::instance().list_providers(); r.is_ok())
        for (const auto& c : r.value())
            if (c.provider.compare(conv_provider_, Qt::CaseInsensitive) == 0)
                configured = true;
    model_notice_btn_->setVisible(configured);
    model_notice_btn_->setText(tr("Use %1").arg(was));
    model_notice_btn_->setToolTip(tr("Make %1 the active LLM again (a global setting — it also applies to the Quick "
                                     "Chat bubble and agents that use the default model)")
                                      .arg(was));
    model_notice_->show();
}

void AiChatScreen::on_use_conversation_model() {
    if (streaming_ || conv_provider_.isEmpty())
        return;
    auto r = LlmConfigRepository::instance().list_providers();
    if (r.is_err()) {
        QMessageBox::warning(this, tr("Switch Model"),
                             tr("Could not read LLM settings: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    for (auto cfg : r.value()) {
        if (cfg.provider.compare(conv_provider_, Qt::CaseInsensitive) != 0)
            continue;
        if (!conv_model_.isEmpty() && cfg.model != conv_model_ &&
            cfg.provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) != 0) {
            cfg.model = conv_model_;
            if (auto s = LlmConfigRepository::instance().save_provider(cfg); s.is_err()) {
                QMessageBox::warning(this, tr("Switch Model"),
                                     tr("Could not save the model: %1").arg(QString::fromStdString(s.error())));
                return;
            }
        }
        if (auto a = LlmConfigRepository::instance().set_active(cfg.provider); a.is_err()) {
            QMessageBox::warning(this, tr("Switch Model"),
                                 tr("Could not switch provider: %1").arg(QString::fromStdString(a.error())));
            return;
        }
        LOG_INFO(TAG_SESSIONS, QString("Active LLM switched back to %1 / %2 for this conversation")
                                   .arg(cfg.provider, cfg.model));
        // Same path as the set_active_llm MCP tool: LlmService reloads and every
        // listener (Settings, other chat windows) refreshes.
        ai_chat::LlmService::instance().reload_config();
        EventBus::instance().publish(QStringLiteral("llm.provider_changed"), QVariantMap{{"provider", cfg.provider}});
        update_stats();
        return;
    }
    QMessageBox::information(this, tr("Switch Model"),
                             tr("%1 is no longer configured. Add it in Settings → LLM Configuration.")
                                 .arg(ai_chat_provider_display(conv_provider_, conv_model_)));
}

} // namespace fincept::screens
