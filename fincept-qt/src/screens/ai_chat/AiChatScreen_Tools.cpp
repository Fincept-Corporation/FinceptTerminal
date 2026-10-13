// src/screens/ai_chat/AiChatScreen_Tools.cpp
//
// The two per-turn side cards of the AI Chat tab:
//   • Thinking — chain-of-thought streamed on think_stream_prefix(); collapsed.
//   • Tools    — tool activity streamed on tool_stream_prefix(): a start line
//                per call plus a completion record (LlmToolActivity.h) with the
//                outcome and duration. Background jobs (a slow tool handed to
//                JobRegistry) get a live status line until they finish.
// Plus the tool "trailer" persisted with each answer, so a reloaded
// conversation still shows which tools produced it.
//
// Part of the partial-class split of AiChatScreen.cpp.

#include "core/logging/Logger.h"
#include "mcp/JobRegistry.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "services/llm/LlmToolActivity.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace fnt = fincept::ui::fonts;
namespace col = fincept::ui::colors;

namespace {

constexpr int kAiChatToolsCardMaxWidth = 680;
constexpr int kAiChatThinkFlushMs = 66;            // ≤ ~15 repaints / s, like the answer bubble
constexpr qsizetype kAiChatThinkMaxChars = 200000; // keep the tail of runaway reasoning
constexpr int kAiChatMaxPersistedSteps = 300;

const QString& ai_chat_trailer_prefix() {
    static const QString p = QStringLiteral("<!--fincept-tools:");
    return p;
}

QString ai_chat_fmt_ms(qint64 ms) {
    if (ms < 0)
        return {};
    if (ms < 1000)
        return QStringLiteral("%1 ms").arg(ms);
    if (ms < 60000)
        return QStringLiteral("%1 s").arg(ms / 1000.0, 0, 'f', 1);
    return QStringLiteral("%1m %2s").arg(ms / 60000).arg((ms % 60000) / 1000);
}

// One card frame (Thinking or Tools) with a toggle header and a body label.
// One stylesheet per card, scoped by object name, so the frame's border does
// not cascade onto the header and body (it used to draw a box inside the box).
QFrame* ai_chat_make_card(QPushButton** header_out, QLabel** body_out, const QString& header_text, bool expanded,
                          bool italic_body) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("aiCard"));
    card->setMaximumWidth(kAiChatToolsCardMaxWidth);
    card->setStyleSheet(QString("QFrame#aiCard{background:%1;border:1px solid %2;}"
                                "QPushButton#aiCardHdr{background:transparent;color:%3;border:none;font-size:%4px;"
                                "font-weight:600;text-align:left;padding:0;}"
                                "QPushButton#aiCardHdr:hover{color:%5;}"
                                "QLabel{background:transparent;border:none;color:%6;font-size:%4px;}"
                                "QLabel#aiCardBody[italic=\"true\"]{font-style:italic;}"
                                "QLabel#aiCardJob{color:%5;}")
                            .arg(col::BG_SURFACE(), col::BORDER_DIM(), col::TEXT_TERTIARY())
                            .arg(fnt::SMALL)
                            .arg(col::TEXT_SECONDARY(), col::TEXT_DIM()));
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 6, 10, 6);
    vl->setSpacing(4);

    auto* header = new QPushButton(header_text);
    header->setObjectName(QStringLiteral("aiCardHdr"));
    header->setCursor(Qt::PointingHandCursor);
    vl->addWidget(header);

    auto* body = new QLabel;
    body->setObjectName(QStringLiteral("aiCardBody"));
    body->setProperty("italic", italic_body);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setVisible(expanded);
    vl->addWidget(body);

    // The body text is kept on the label as "full_text" and painted only while
    // expanded — a collapsed card never pays for re-laying out long text.
    QObject::connect(header, &QPushButton::clicked, card, [header, body]() {
        const bool show = !body->isVisible();
        if (show)
            body->setText(body->property("full_text").toString());
        body->setVisible(show);
        QString t = header->text();
        if (!t.isEmpty())
            t.replace(0, 1, show ? QChar(0x25BE) : QChar(0x25B8));
        header->setText(t);
    });
    *header_out = header;
    *body_out = body;
    return card;
}

void ai_chat_set_card_body(QLabel* body, const QString& text) {
    if (!body)
        return;
    body->setProperty("full_text", text);
    if (body->isVisible())
        body->setText(text);
}

QString ai_chat_header(bool expanded, const QString& icon, const QString& text) {
    return QChar(expanded ? 0x25BE : 0x25B8) + QStringLiteral("  ") + icon + QStringLiteral(" ") + text;
}

QString ai_chat_tool_icon() {
    return QString::fromUtf8("\xF0\x9F\x94\xA7");
}

QString ai_chat_think_icon() {
    return QString::fromUtf8("\xF0\x9F\x92\xAD");
}

} // namespace

// ── Thinking (chain-of-thought) card ────────────────────────────────────────────

void AiChatScreen::create_thinking_card() {
    if (thinking_card_)
        return;
    QPushButton* header = nullptr;
    QLabel* body = nullptr;
    auto* card = ai_chat_make_card(&header, &body, ai_chat_header(false, ai_chat_think_icon(), tr("Thinking…")),
                                   /*expanded=*/false, /*italic_body=*/true);
    // Inserted before the trailing stretch so it sits above the answer bubble
    // (which is created lazily on the first answer chunk and lands below it).
    insert_row(card, Qt::AlignLeft);
    thinking_card_ = card;
    thinking_header_ = header;
    thinking_body_ = body;
}

void AiChatScreen::append_thinking_chunk(const QString& text) {
    if (text.isEmpty())
        return;
    show_typing(false); // the card now signals activity
    if (!thinking_card_)
        create_thinking_card();
    thinking_text_ += text;
    if (thinking_text_.size() > kAiChatThinkMaxChars)
        thinking_text_ = QStringLiteral("…") + thinking_text_.right(kAiChatThinkMaxChars);
    // Coalesced like the answer bubble: reasoning models stream thousands of
    // tiny deltas, and a setText per delta re-laid out the whole card each time.
    if (thinking_flush_armed_)
        return;
    thinking_flush_armed_ = true;
    QTimer::singleShot(kAiChatThinkFlushMs, this, [this]() { flush_thinking(); });
}

void AiChatScreen::flush_thinking() {
    thinking_flush_armed_ = false;
    ai_chat_set_card_body(thinking_body_, thinking_text_);
    if (thinking_body_ && thinking_body_->isVisible())
        scroll_to_bottom();
}

void AiChatScreen::finalize_thinking_card() {
    flush_thinking();
    if (thinking_header_) {
        const bool expanded = thinking_body_ && thinking_body_->isVisible();
        thinking_header_->setText(ai_chat_header(expanded, ai_chat_think_icon(), tr("Thoughts")));
    }
    reset_thinking_state();
}

void AiChatScreen::reset_thinking_state() {
    // Stops tracking the current card (the widget itself remains in the
    // transcript); the next message creates a fresh one.
    thinking_card_ = nullptr;
    thinking_header_ = nullptr;
    thinking_body_ = nullptr;
    thinking_text_.clear();
}

// ── Tools card ──────────────────────────────────────────────────────────────────

void AiChatScreen::create_tools_card() {
    if (tools_card_)
        return;
    QPushButton* header = nullptr;
    QLabel* body = nullptr;
    // Expanded while the turn runs — tool progress is the only sign a long
    // multi-round turn is advancing — and collapsed into a summary on finish.
    auto* card = ai_chat_make_card(&header, &body, ai_chat_header(true, ai_chat_tool_icon(), tr("Using tools…")),
                                   /*expanded=*/true, /*italic_body=*/false);
    body->setTextFormat(Qt::RichText);
    insert_row(card, Qt::AlignLeft);
    tools_card_ = card;
    tools_header_ = header;
    tools_body_ = body;
}

void AiChatScreen::append_tool_chunk(const QString& text) {
    if (text.isEmpty())
        return;
    show_typing(false); // the card now signals activity
    if (!tools_card_)
        create_tools_card();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QString& raw : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        // Completion record → close the oldest open step with the same label
        // (records follow the start lines in the model's order).
        if (const auto rec = ai_chat::decode_tool_done(raw)) {
            for (auto& s : tool_steps_) {
                if (s.state != ToolStep::State::Running || s.label != rec->label)
                    continue;
                s.elapsed_ms = rec->elapsed_ms;
                s.parallel = rec->parallel;
                s.detail = rec->detail;
                switch (rec->outcome) {
                    case ai_chat::ToolDoneRecord::Outcome::Ok:
                        s.state = ToolStep::State::Ok;
                        break;
                    case ai_chat::ToolDoneRecord::Outcome::Failed:
                        s.state = ToolStep::State::Failed;
                        break;
                    case ai_chat::ToolDoneRecord::Outcome::Backgrounded:
                        s.state = ToolStep::State::Job;
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
        ToolStep s;
        s.label = line;
        s.base = line.section(QStringLiteral(" · "), 0, 0);
        s.started_ms = now;
        // The model polling a background job: show that job's live status too.
        if (s.base == QLatin1String("job_status") || s.base == QLatin1String("job_result") ||
            s.base == QLatin1String("job_cancel")) {
            const QString id = line.section(QStringLiteral(" · "), 1, 1).trimmed();
            if (id.startsWith(QLatin1String("job_")))
                track_job(id);
        }
        tool_steps_.append(s);
    }
    render_tools_card();
}

// One line per step, consecutive steps of the same tool collapsed into a run:
//   ✓ get_quote · AAPL — 1.2 s
//   ✓ set_excel_cell ×45 — 3.4 s
//   ✗ create_portfolio — 0.3 s — failed: Not signed in
//   ⧗ run_backtest · SPY — background job job_00002a
QString AiChatScreen::render_tool_lines(const QVector<ToolStep>& steps) {
    QStringList lines;
    int i = 0;
    while (i < steps.size()) {
        int j = i + 1;
        while (j < steps.size() && steps[j].base == steps[i].base)
            ++j;
        const int n = j - i;
        int failed = 0, running = 0, jobs = 0, unknown = 0;
        qint64 total_ms = 0;
        bool any_parallel = false, any_ms = false;
        QString first_failure, job_id, note;
        for (int k = i; k < j; ++k) {
            const auto& s = steps[k];
            if (s.state == ToolStep::State::Failed) {
                ++failed;
                if (first_failure.isEmpty())
                    first_failure = s.detail;
            } else if (s.state == ToolStep::State::Running) {
                ++running;
            } else if (s.state == ToolStep::State::Job) {
                ++jobs;
                job_id = s.detail;
            } else if (s.elapsed_ms < 0) {
                ++unknown;
            }
            if (s.state == ToolStep::State::Ok && !s.detail.isEmpty() && note.isEmpty())
                note = s.detail;
            if (s.elapsed_ms >= 0) {
                any_ms = true;
                any_parallel = any_parallel || s.parallel;
                // Calls in one parallel round each report the round's wall
                // time; summing them would multiply it.
                total_ms = s.parallel ? std::max(total_ms, s.elapsed_ms) : total_ms + s.elapsed_ms;
            }
        }
        QString glyph, color;
        if (running > 0) {
            glyph = QStringLiteral("…");
            color = col::AMBER();
        } else if (failed > 0) {
            glyph = QStringLiteral("✗");
            color = col::NEGATIVE();
        } else if (jobs > 0) {
            glyph = QStringLiteral("⧗");
            color = col::WARNING();
        } else if (unknown == n) {
            glyph = QStringLiteral("•"); // no completion record from this provider's loop
            color = col::TEXT_DIM();
        } else {
            glyph = QStringLiteral("✓");
            color = col::POSITIVE();
        }
        QString line = QStringLiteral("<span style='color:%1'>%2</span> ").arg(color, glyph);
        line += n == 1 ? steps[i].label.toHtmlEscaped()
                       : steps[i].base.toHtmlEscaped() + QStringLiteral(" ×") + QString::number(n);
        if (any_ms && running == 0)
            line += QStringLiteral(" <span style='color:%1'>— %2%3</span>")
                        .arg(col::TEXT_TERTIARY(), ai_chat_fmt_ms(total_ms),
                             any_parallel ? AiChatScreen::tr(" (parallel round)") : QString());
        if (failed > 0) {
            const QString why = first_failure.isEmpty() ? AiChatScreen::tr("no reason given") : first_failure;
            line += QStringLiteral(" <span style='color:%1'>— %2</span>")
                        .arg(col::NEGATIVE(),
                             (n > 1 ? AiChatScreen::tr("%1 of %2 failed: %3").arg(failed).arg(n).arg(why)
                                    : AiChatScreen::tr("failed: %1").arg(why))
                                 .toHtmlEscaped());
        }
        if (jobs > 0)
            line += QStringLiteral(" <span style='color:%1'>— %2</span>")
                        .arg(col::WARNING(), AiChatScreen::tr("background job %1").arg(job_id).toHtmlEscaped());
        if (failed == 0 && jobs == 0 && !note.isEmpty())
            line += QStringLiteral(" <span style='color:%1'>— %2</span>").arg(col::TEXT_TERTIARY(), note.toHtmlEscaped());
        lines << line;
        i = j;
    }
    return lines.join(QStringLiteral("<br>"));
}

QString AiChatScreen::tools_summary(const QVector<ToolStep>& steps, bool finished) {
    int failed = 0, jobs = 0;
    qint64 total_ms = 0, round_max = 0;
    bool any_ms = false;
    for (const auto& s : steps) {
        if (s.state == ToolStep::State::Failed)
            ++failed;
        if (s.state == ToolStep::State::Job)
            ++jobs;
        if (s.elapsed_ms >= 0) {
            any_ms = true;
            if (s.parallel) {
                round_max = std::max(round_max, s.elapsed_ms);
            } else {
                total_ms += round_max + s.elapsed_ms;
                round_max = 0;
            }
        }
    }
    total_ms += round_max;
    QString out = finished ? AiChatScreen::tr("%n tool call(s)", "", int(steps.size()))
                           : AiChatScreen::tr("Using tools… (%1)").arg(steps.size());
    if (failed > 0)
        out += QStringLiteral(" · ") + AiChatScreen::tr("%n failed", "", failed);
    if (jobs > 0)
        out += QStringLiteral(" · ") + AiChatScreen::tr("%n in background", "", jobs);
    if (finished && any_ms)
        out += QStringLiteral(" · ") + ai_chat_fmt_ms(total_ms);
    return out;
}

void AiChatScreen::render_tools_card() {
    if (!tools_body_)
        return;
    ai_chat_set_card_body(tools_body_, render_tool_lines(tool_steps_));
    if (tools_header_) {
        const bool expanded = tools_body_->isVisible();
        tools_header_->setText(ai_chat_header(expanded, ai_chat_tool_icon(), tools_summary(tool_steps_, false)));
    }
}

void AiChatScreen::finalize_tools_card() {
    if (tools_header_) {
        // Steps that never got a completion record (a provider loop that does
        // not emit them) are finished now — show them neutral, not spinning.
        for (auto& s : tool_steps_)
            if (s.state == ToolStep::State::Running)
                s.state = ToolStep::State::Ok;
        ai_chat_set_card_body(tools_body_, render_tool_lines(tool_steps_));
        // Collapse once the answer is in — the detail stays one click away —
        // unless something failed: then the reason stays in view.
        int failed = 0;
        for (const auto& s : tool_steps_)
            failed += s.state == ToolStep::State::Failed ? 1 : 0;
        const bool keep_open = failed > 0 && tools_body_ && tools_body_->isVisible();
        if (tools_body_ && !keep_open)
            tools_body_->setVisible(false);
        tools_header_->setText(ai_chat_header(keep_open, ai_chat_tool_icon(), tools_summary(tool_steps_, true)));
    }
    reset_tools_state();
}

void AiChatScreen::reset_tools_state() {
    tools_card_ = nullptr;
    tools_header_ = nullptr;
    tools_body_ = nullptr;
    tool_steps_.clear();
    tool_job_ids_.clear();
}

// ── Background jobs ─────────────────────────────────────────────────────────────
// A slow tool handed to JobRegistry returns a receipt; the work continues after
// the turn may have ended. Each such job gets a line in the card that started it,
// refreshed every second from the registry (in-memory, thread-safe) while the
// screen is visible, until the job reaches a terminal state.

void AiChatScreen::track_job(const QString& job_id) {
    if (job_id.isEmpty() || tool_job_ids_.contains(job_id))
        return;
    tool_job_ids_.append(job_id);
    if (job_labels_.contains(job_id) || !tools_card_)
        return;
    auto* lbl = new QLabel;
    lbl->setObjectName(QStringLiteral("aiCardJob"));
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);
    if (auto* vl = qobject_cast<QVBoxLayout*>(tools_card_->layout()))
        vl->addWidget(lbl);
    job_labels_.insert(job_id, lbl);
    if (!tracked_job_ids_.contains(job_id))
        tracked_job_ids_.append(job_id);
    refresh_job_lines();
    if (isVisible() && !tracked_job_ids_.isEmpty())
        job_timer_->start();
}

void AiChatScreen::refresh_job_lines() {
    for (auto it = job_labels_.begin(); it != job_labels_.end();) {
        const QString id = it.key();
        QLabel* lbl = it.value();
        if (!lbl) { // its card was cleared (session switch / trim)
            tracked_job_ids_.removeAll(id);
            it = job_labels_.erase(it);
            continue;
        }
        const auto snap = mcp::JobRegistry::instance().status(id);
        bool finished = true;
        QString text;
        if (!snap) {
            text = QStringLiteral("<span style='color:%1'>⧗</span> %2 — %3")
                       .arg(col::TEXT_DIM(), id.toHtmlEscaped(), tr("no longer tracked (jobs expire after 15 minutes)"));
        } else {
            const QString head = id.toHtmlEscaped() + QStringLiteral(" · ") + snap->tool.toHtmlEscaped();
            const QString elapsed = ai_chat_fmt_ms(snap->elapsed_ms());
            switch (snap->state) {
                case mcp::JobState::Running: {
                    finished = false;
                    QString status = snap->timeout_ms > 0
                                         ? tr("running %1 of %2").arg(elapsed, ai_chat_fmt_ms(snap->timeout_ms))
                                         : tr("running %1").arg(elapsed);
                    if (snap->progress > 0.0)
                        status += QStringLiteral(" · %1%").arg(int(snap->progress * 100.0));
                    text = QStringLiteral("<span style='color:%1'>⏳</span> %2 — %3")
                               .arg(col::AMBER(), head, status.toHtmlEscaped());
                    break;
                }
                case mcp::JobState::Succeeded:
                    text = QStringLiteral("<span style='color:%1'>✓</span> %2 — %3")
                               .arg(col::POSITIVE(), head, tr("finished in %1").arg(elapsed).toHtmlEscaped());
                    break;
                case mcp::JobState::Failed:
                    text = QStringLiteral("<span style='color:%1'>✗</span> %2 — %3")
                               .arg(col::NEGATIVE(), head, tr("failed after %1").arg(elapsed).toHtmlEscaped());
                    break;
                case mcp::JobState::Cancelled:
                    text = QStringLiteral("<span style='color:%1'>⊘</span> %2 — %3")
                               .arg(col::TEXT_DIM(), head, tr("cancelled after %1").arg(elapsed).toHtmlEscaped());
                    break;
            }
            if (!snap->message.isEmpty())
                text += QStringLiteral("<br><span style='color:%1'>&nbsp;&nbsp;%2</span>")
                            .arg(col::TEXT_TERTIARY(), snap->message.left(200).toHtmlEscaped());
        }
        lbl->setText(text);
        if (finished) {
            tracked_job_ids_.removeAll(id);
            it = job_labels_.erase(it);
        } else {
            ++it;
        }
    }
    if (tracked_job_ids_.isEmpty())
        job_timer_->stop();
}

// ── Persisted tool activity ─────────────────────────────────────────────────────
// chat_messages has no tool column and its role CHECK admits only
// user/assistant/system, so a turn's tool steps travel with the answer as an
// HTML-comment trailer (base64 JSON). It is stripped before the answer is shown
// or sent back to the model, and rendered as a collapsed Tools card on reload.

QString AiChatScreen::tool_trailer() const {
    if (tool_steps_.isEmpty())
        return {};
    QJsonArray arr;
    const int start = std::max(0, int(tool_steps_.size()) - kAiChatMaxPersistedSteps);
    for (int i = start; i < tool_steps_.size(); ++i) {
        const auto& s = tool_steps_[i];
        QString state = QStringLiteral("done");
        if (s.state == ToolStep::State::Ok)
            state = s.elapsed_ms >= 0 ? QStringLiteral("ok") : QStringLiteral("done");
        else if (s.state == ToolStep::State::Failed)
            state = QStringLiteral("fail");
        else if (s.state == ToolStep::State::Job)
            state = QStringLiteral("job");
        QJsonObject o{{"l", s.label}, {"s", state}};
        if (s.elapsed_ms >= 0)
            o["ms"] = double(s.elapsed_ms);
        if (s.parallel)
            o["p"] = true;
        if (!s.detail.isEmpty())
            o["d"] = s.detail;
        arr.append(o);
    }
    return QStringLiteral("\n\n") + ai_chat_trailer_prefix() +
           QString::fromLatin1(QJsonDocument(arr).toJson(QJsonDocument::Compact).toBase64()) + QStringLiteral("-->");
}

QString AiChatScreen::strip_tool_trailer(const QString& content, QJsonArray* steps) {
    static const QRegularExpression rx(QStringLiteral("\\s*<!--fincept-tools:([A-Za-z0-9+/=]*)-->\\s*$"));
    const auto m = rx.match(content);
    if (!m.hasMatch())
        return content;
    if (steps)
        *steps = QJsonDocument::fromJson(QByteArray::fromBase64(m.captured(1).toLatin1())).array();
    return content.left(m.capturedStart());
}

void AiChatScreen::add_static_tools_card(const QJsonArray& steps) {
    if (steps.isEmpty())
        return;
    QVector<ToolStep> list;
    list.reserve(steps.size());
    for (const auto& v : steps) {
        const QJsonObject o = v.toObject();
        ToolStep s;
        s.label = o.value("l").toString();
        s.base = s.label.section(QStringLiteral(" · "), 0, 0);
        const QString st = o.value("s").toString();
        s.state = st == QLatin1String("fail")  ? ToolStep::State::Failed
                  : st == QLatin1String("job") ? ToolStep::State::Job
                                               : ToolStep::State::Ok;
        s.elapsed_ms = o.contains("ms") ? qint64(o.value("ms").toDouble()) : -1;
        s.parallel = o.value("p").toBool();
        s.detail = o.value("d").toString();
        list.append(s);
    }
    QPushButton* header = nullptr;
    QLabel* body = nullptr;
    auto* card = ai_chat_make_card(&header, &body, ai_chat_header(false, ai_chat_tool_icon(), tools_summary(list, true)),
                                   /*expanded=*/false, /*italic_body=*/false);
    body->setTextFormat(Qt::RichText);
    ai_chat_set_card_body(body, render_tool_lines(list));
    insert_row(card, Qt::AlignLeft);
}

} // namespace fincept::screens
