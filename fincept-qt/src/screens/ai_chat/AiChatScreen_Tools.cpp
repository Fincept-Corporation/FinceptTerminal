// src/screens/ai_chat/AiChatScreen_Tools.cpp
//
// Tool activity of the answer being written (streamed on tool_stream_prefix(): a start line
// per call plus a completion record — LlmToolActivity.h — with outcome and duration), live
// status lines of background jobs (JobRegistry), the tool catalogue for the context rail
// (read off the GUI thread), and the context items the next message carries (linked symbol,
// mentions, portfolio / watchlist references, files) and saved prompts.
//
// Part of the partial-class split of AiChatScreen.cpp.

#include "core/instrument/Instrument.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "mcp/JobRegistry.h"
#include "mcp/McpService.h"
#include "screens/ai_chat/AiChatComposer.h"
#include "screens/ai_chat/AiChatContextRail.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "screens/ai_chat/AiChatStyle.h"
#include "screens/ai_chat/AiChatTranscript.h"
#include "services/llm/LlmToolActivity.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QInputDialog>
#include <QLabel>
#include <QMap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens {

namespace st = aichat::style;

namespace {

/// `w` is `root` or inside it.
bool tls_inside(const QWidget* root, const QWidget* w) {
    for (; w; w = w->parentWidget())
        if (w == root)
            return true;
    return false;
}

bool tls_is_image(const QFileInfo& fi) {
    static const QStringList kImages = {"png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "tif", "tiff", "heic", "ico"};
    return kImages.contains(fi.suffix().toLower());
}

} // namespace

// ── Tool activity ────────────────────────────────────────────────────────────

void AiChatScreen::append_tool_chunk(const QString& text) {
    if (text.isEmpty() || streaming_turn_ == 0)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QString& raw : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        // Completion record → close the oldest open step with the same label (records
        // follow the start lines in the model's order).
        if (const auto rec = ai_chat::decode_tool_done(raw)) {
            for (auto& s : tool_steps_) {
                if (s.state != aichat::ToolStep::State::Running || s.label != rec->label)
                    continue;
                s.elapsed_ms = rec->elapsed_ms;
                s.parallel = rec->parallel;
                s.detail = rec->detail;
                switch (rec->outcome) {
                    case ai_chat::ToolDoneRecord::Outcome::Ok:
                        s.state = aichat::ToolStep::State::Ok;
                        break;
                    case ai_chat::ToolDoneRecord::Outcome::Failed:
                        s.state = aichat::ToolStep::State::Failed;
                        break;
                    case ai_chat::ToolDoneRecord::Outcome::Backgrounded:
                        s.state = aichat::ToolStep::State::Job;
                        track_job(rec->detail);
                        break;
                }
                break;
            }
            continue;
        }
        // simplified(): completion records carry the label whitespace-normalised.
        const QString line = raw.simplified();
        if (line.isEmpty())
            continue;
        aichat::ToolStep s;
        s.label = line;
        s.base = line.section(QStringLiteral(" · "), 0, 0);
        s.started_ms = now;
        // The model polling a background Python job: show that job's live status too.
        if (s.base == QLatin1String("Python status") || s.base == QLatin1String("Python cancel") ||
            s.base == QLatin1String("Python run")) {
            const QString id = line.section(QStringLiteral(" · "), 1, 1).trimmed();
            if (id.startsWith(QLatin1String("job_")))
                track_job(id);
        }
        tool_steps_.append(s);
    }
    // A parallel round arrives as a burst of lines: one repaint per burst.
    if (!tool_render_timer_->isActive())
        tool_render_timer_->start();
}

void AiChatScreen::finalize_tool_steps() {
    tool_render_timer_->stop();
    // Steps that never got a completion record (a provider loop that does not emit them)
    // are finished now — shown neutral, not spinning.
    for (auto& s : tool_steps_)
        if (s.state == aichat::ToolStep::State::Running)
            s.state = aichat::ToolStep::State::Ok;
    if (streaming_turn_ != 0 && !tool_steps_.isEmpty())
        transcript_->set_tool_steps(streaming_turn_, tool_steps_);
}

void AiChatScreen::note_symbols_from_tools() {
    if (streaming_turn_ != 0)
        transcript_->set_tool_steps(streaming_turn_, tool_steps_);
    bool added = false;
    for (const auto& s : std::as_const(tool_steps_)) {
        const QString arg = s.label.section(QStringLiteral(" · "), 1, 1).trimmed().toUpper();
        if (aichat::looks_like_ticker(arg) && !known_symbols_.contains(arg)) {
            known_symbols_.insert(arg);
            added = true;
        }
    }
    if (added)
        transcript_->set_known_symbols(known_symbols_);
}

// ── Background jobs ──────────────────────────────────────────────────────────
// A slow tool handed to JobRegistry returns a receipt; the work continues after the turn
// may have ended. Each such job gets a status line under the activity row that started it,
// refreshed every second from the registry (in-memory, thread-safe) while the tab is
// visible, until the job reaches a terminal state.

void AiChatScreen::track_job(const QString& job_id) {
    if (job_id.isEmpty() || tool_job_ids_.contains(job_id))
        return;
    tool_job_ids_.append(job_id);
    if (!tracked_job_ids_.contains(job_id))
        tracked_job_ids_.append(job_id);
    refresh_job_lines();
    if (isVisible() && !tracked_job_ids_.isEmpty())
        job_timer_->start();
}

void AiChatScreen::refresh_job_lines() {
    for (int i = int(tracked_job_ids_.size()) - 1; i >= 0; --i) {
        const QString id = tracked_job_ids_[i];
        const auto snap = mcp::JobRegistry::instance().status(id);
        bool finished = true;
        QString text;
        int tone = 3;
        if (!snap) {
            text = tr("%1 — no longer tracked (jobs expire after 15 minutes)").arg(id);
        } else {
            const QString head = id + QStringLiteral(" · ") + snap->tool;
            const QString elapsed = aichat::format_ms(snap->elapsed_ms());
            switch (snap->state) {
                case mcp::JobState::Running: {
                    finished = false;
                    tone = 0;
                    QString status = snap->timeout_ms > 0
                                         ? tr("running %1 of %2").arg(elapsed, aichat::format_ms(snap->timeout_ms))
                                         : tr("running %1").arg(elapsed);
                    if (snap->progress > 0.0)
                        status += QStringLiteral(" · %1%").arg(int(snap->progress * 100.0));
                    text = head + QStringLiteral(" — ") + status;
                    break;
                }
                case mcp::JobState::Succeeded:
                    tone = 1;
                    text = head + QStringLiteral(" — ") + tr("finished in %1").arg(elapsed);
                    break;
                case mcp::JobState::Failed:
                    tone = 2;
                    text = head + QStringLiteral(" — ") + tr("failed after %1").arg(elapsed);
                    break;
                case mcp::JobState::Cancelled:
                    tone = 3;
                    text = head + QStringLiteral(" — ") + tr("cancelled after %1").arg(elapsed);
                    break;
            }
            if (!snap->message.isEmpty())
                text += QStringLiteral(" — ") + snap->message.left(200);
        }
        transcript_->set_job_status(id, text, tone);
        if (finished)
            tracked_job_ids_.removeAt(i);
    }
    if (tracked_job_ids_.isEmpty())
        job_timer_->stop();
}

// ── Tool catalogue (context rail) ────────────────────────────────────────────

void AiChatScreen::load_tool_catalogue() {
    if (tool_catalogue_loading_ || !context_rail_)
        return;
    tool_catalogue_loading_ = true;
    if (!tool_catalogue_loaded_)
        context_rail_->set_tool_groups({}, true); // "loading…" until the first catalogue arrives
    QPointer<AiChatScreen> self = this;
    // McpService's catalogue is shared with the request threads (mutex-guarded): read it on
    // a worker so a slow external server listing never stalls the GUI thread.
    (void)QtConcurrent::run([self]() {
        const std::vector<mcp::UnifiedTool> tools = mcp::McpService::instance().get_all_tools();
        QMap<QString, QStringList> by_category;
        for (const auto& t : tools) {
            const QString cat = !t.category.isEmpty() ? t.category
                                : t.is_internal       ? QStringLiteral("terminal")
                                                      : QStringLiteral("external · ") + t.server_name;
            by_category[cat].append(t.name);
        }
        QVector<aichat::ToolGroup> groups;
        for (auto it = by_category.constBegin(); it != by_category.constEnd(); ++it) {
            QStringList names = it.value();
            names.sort(Qt::CaseInsensitive);
            groups.append({it.key(), names});
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, groups]() {
                if (!self)
                    return;
                self->tool_catalogue_loading_ = false;
                self->tool_catalogue_loaded_ = true;
                if (self->context_rail_)
                    self->context_rail_->set_tool_groups(groups, false);
            },
            Qt::QueuedConnection);
    });
}

// ── Context the next message carries ─────────────────────────────────────────

QVector<aichat::Attachment> AiChatScreen::outgoing_context() const {
    QVector<aichat::Attachment> out;
    if (linked_context_pending_ && linked_symbol_.is_valid()) {
        aichat::Attachment a;
        a.kind = aichat::Attachment::Kind::Symbol;
        a.name = linked_symbol_.symbol;
        a.ref = linked_symbol_.symbol;
        a.detail = linked_symbol_.asset_class.isEmpty() ? QStringLiteral("equity") : linked_symbol_.asset_class;
        a.linked = true;
        out.append(a);
    }
    for (const auto& c : context_) {
        bool dup = false;
        for (const auto& o : std::as_const(out))
            dup = dup || (o.kind == c.kind && o.ref.compare(c.ref, Qt::CaseInsensitive) == 0);
        if (!dup)
            out.append(c);
    }
    return out;
}

void AiChatScreen::refresh_context_ui() {
    if (!composer_ || !context_rail_)
        return;
    const QVector<aichat::Attachment> ctx = outgoing_context();
    composer_->set_chips(ctx, files_);
    context_rail_->set_linked_symbol(linked_symbol_.is_valid() ? linked_symbol_.symbol : QString(),
                                     linked_symbol_.asset_class, linked_context_pending_ && linked_symbol_.is_valid());
    context_rail_->set_context(context_, files_);

    // The exact lines the context adds — files by their header line (contents are read on Send).
    QStringList preview;
    const QString lines = aichat::context_lines(ctx);
    if (!lines.isEmpty())
        preview << lines;
    qint64 file_tokens = 0;
    for (const auto& f : std::as_const(files_)) {
        const bool cut = f.bytes > aichat::kAttachMaxChars;
        preview << (cut ? QStringLiteral("[Attached file: %1 — truncated to the first %2 characters]")
                              .arg(f.name, QString::number(aichat::kAttachMaxChars))
                        : QStringLiteral("[Attached file: ") + f.name + QStringLiteral("]"));
        if (f.bytes > 0)
            file_tokens += std::min<qint64>(f.bytes, aichat::kAttachMaxChars) / 4;
    }
    context_rail_->set_wire_preview(preview.join(QLatin1Char('\n')));
    composer_->set_attachment_tokens(int(std::min<qint64>(file_tokens, 1000000)));
    QString hint;
    if (linked_symbol_.is_valid())
        hint = linked_symbol_.symbol;
    for (const auto& c : std::as_const(context_))
        if (hint.isEmpty() && c.kind == aichat::Attachment::Kind::Symbol)
            hint = c.ref;
    composer_->set_symbol_hint(hint);
}

void AiChatScreen::add_files(const QStringList& paths) {
    QStringList refused;
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        if (!fi.exists() || !fi.isFile())
            continue;
        if (tls_is_image(fi)) {
            refused << fi.fileName();
            continue;
        }
        const QString abs = fi.absoluteFilePath();
        bool dup = false;
        for (const auto& f : std::as_const(files_))
            dup = dup || f.ref == abs;
        if (dup)
            continue;
        aichat::Attachment a;
        a.kind = aichat::Attachment::Kind::File;
        a.name = fi.fileName();
        a.ref = abs;
        a.bytes = fi.size(); // metadata only — the contents are read on Send
        files_.append(a);
    }
    if (!refused.isEmpty() && status_)
        status_->show_notice(tr("Not attached: %1 — images can't be sent; this chat sends text only.")
                                 .arg(refused.join(QStringLiteral(", "))),
                             QString(), {}, 6000, ui::ScreenStatusBar::Tone::Warn);
    refresh_context_ui();
    ScreenStateManager::instance().notify_changed(this);
}

void AiChatScreen::pick_files() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Attach Files to Message"), QString(),
        tr("Text files (*.txt *.md *.csv *.json *.ipynb *.py *.sql *.log *.xml *.yaml *.yml);;All Files (*)"));
    if (!paths.isEmpty())
        add_files(paths);
}

void AiChatScreen::pick_symbol() {
    // A small window with the terminal's symbol search (its resolver): the picked instrument
    // becomes a context item — never typed text taken as a symbol. Not a Qt::Popup: the search
    // box shows its result list in a window of its own, and a click there would first close
    // a pop-up. It closes on Esc, when the focus moves outside it, or when the app goes inactive.
    auto* popup = new QFrame(this, Qt::Tool | Qt::FramelessWindowHint);
    popup->setObjectName(QString::fromLatin1(st::kDrawer));
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setAttribute(Qt::WA_StyledBackground, true);
    st::apply_sheet(popup);
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), popup);
    esc->setContext(Qt::WindowShortcut);
    connect(esc, &QShortcut::activated, popup, &QWidget::close);
    QPointer<QFrame> guard(popup);
    connect(qApp, &QApplication::focusChanged, popup, [guard](QWidget*, QWidget* now) {
        if (guard && now && !tls_inside(guard, now))
            guard->close();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, popup, [guard](Qt::ApplicationState state) {
        if (guard && state != Qt::ApplicationActive)
            guard->close();
    });
    auto* vl = new QVBoxLayout(popup);
    vl->setContentsMargins(st::u(2), st::u(2), st::u(2), st::u(2));
    vl->setSpacing(st::u(1));
    auto* title = new QLabel(tr("ATTACH A SYMBOL"), popup);
    title->setObjectName(QString::fromLatin1(st::kSectionTitle));
    vl->addWidget(title);
    auto* box = new ui::SymbolSearchBox(popup);
    box->set_placeholder(tr("Symbol or company…"));
    box->setAccessibleName(tr("Symbol to attach"));
    vl->addWidget(box);
    auto* note = new QLabel(tr("Sent with your next message as a context line."), popup);
    note->setObjectName(QString::fromLatin1(st::kDim));
    vl->addWidget(note);
    connect(box, &ui::SymbolSearchBox::instrument_picked, this,
            [this, popup](const core::instrument::Instrument& inst) {
                aichat::Attachment a;
                a.kind = aichat::Attachment::Kind::Symbol;
                a.ref = inst.symbol.isEmpty() ? inst.display : inst.symbol;
                a.name = inst.display.isEmpty() ? a.ref : inst.display;
                a.detail = core::instrument::asset_class_slug(inst.asset_class);
                add_context(a);
                popup->close();
            });
    popup->setFixedWidth(std::max(st::avg_char(0) * 40, 320));
    popup->adjustSize();
    const QWidget* anchor = context_rail_ && context_rail_->isVisible() ? static_cast<QWidget*>(context_rail_)
                                                                         : static_cast<QWidget*>(composer_);
    QPoint at = anchor->mapToGlobal(QPoint(0, 0));
    if (anchor == context_rail_)
        at.rx() -= popup->width() - anchor->width();
    else
        at.ry() -= popup->height();
    popup->move(at);
    popup->show();
    popup->raise();
    popup->activateWindow();
    box->focus_and_select();
}

void AiChatScreen::add_context(const aichat::Attachment& item) {
    if (item.kind == aichat::Attachment::Kind::File) {
        add_files({item.ref});
        return;
    }
    if (item.ref.isEmpty() && item.name.isEmpty())
        return;
    for (const auto& c : std::as_const(context_))
        if (c.kind == item.kind && c.ref.compare(item.ref, Qt::CaseInsensitive) == 0)
            return;
    context_.append(item);
    if (item.kind == aichat::Attachment::Kind::Symbol && !item.ref.isEmpty()) {
        known_symbols_.insert(item.ref.toUpper());
        transcript_->set_known_symbols(known_symbols_);
    }
    refresh_context_ui();
}

void AiChatScreen::remove_context(const aichat::Attachment& item) {
    if (item.kind == aichat::Attachment::Kind::Symbol && item.linked) {
        linked_context_pending_ = false;
    } else if (item.kind == aichat::Attachment::Kind::File || item.kind == aichat::Attachment::Kind::Image) {
        files_.removeIf([&item](const aichat::Attachment& f) { return f.ref == item.ref; });
        ScreenStateManager::instance().notify_changed(this);
    } else {
        context_.removeIf([&item](const aichat::Attachment& c) {
            return c.kind == item.kind && c.ref.compare(item.ref, Qt::CaseInsensitive) == 0;
        });
    }
    refresh_context_ui();
}

// ── Saved prompts ────────────────────────────────────────────────────────────

void AiChatScreen::save_draft_prompt() {
    const QString body = composer_->text().trimmed();
    if (body.isEmpty()) {
        status_->show_notice(tr("Type a prompt in the composer first, then SAVE DRAFT keeps it here."), QString(), {},
                             5000);
        return;
    }
    bool ok = false;
    const QString title = QInputDialog::getText(this, tr("Save Prompt"), tr("Name:"), QLineEdit::Normal,
                                                body.simplified().left(40), &ok);
    if (!ok)
        return;
    aichat::PromptTemplate p;
    p.title = title.trimmed().isEmpty() ? body.simplified().left(40) : title.trimmed().left(80);
    p.body = body;
    p.saved = true;
    saved_prompts_.append(p);
    if (!aichat::store_saved_prompts(saved_prompts_)) {
        status_->show_notice(tr("Could not save the prompt."), QString(), {}, 5000, ui::ScreenStatusBar::Tone::Danger);
        saved_prompts_ = aichat::load_saved_prompts();
        return;
    }
    saved_prompts_ = aichat::load_saved_prompts();
    context_rail_->set_saved_prompts(saved_prompts_);
    composer_->set_saved_prompts(saved_prompts_);
    status_->show_notice(tr("Prompt saved — type / in the composer to use it."), QString(), {}, 4000);
}

void AiChatScreen::delete_saved_prompt(int index) {
    if (index < 0 || index >= saved_prompts_.size())
        return;
    saved_prompts_.removeAt(index);
    aichat::store_saved_prompts(saved_prompts_);
    saved_prompts_ = aichat::load_saved_prompts();
    context_rail_->set_saved_prompts(saved_prompts_);
    composer_->set_saved_prompts(saved_prompts_);
}

} // namespace fincept::screens
