// CloudSseMapping.cpp — SSE framing, token accounting and the cloud event
// vocabulary -> RunEvent mapping. Qt Core only (linked by tests/tst_agent_task_runs).

#include "services/agent_tasks/CloudSseMapping.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

namespace fincept::agent_tasks {

namespace cloud_sse_mapping_detail {

QString csm_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::CloudSse", text);
}

QString csm_sentence(QString text) {
    text = text.trimmed();
    if (!text.isEmpty() && !text.endsWith(QLatin1Char('.')) && !text.endsWith(QLatin1Char('!')) &&
        !text.endsWith(QLatin1Char('?')))
        text += QLatin1Char('.');
    return text;
}

RunEvent csm_event(RunEventKind kind, const QString& title, const QString& body = {}, const QJsonObject& data = {}) {
    RunEvent e;
    e.kind = kind;
    e.title = title;
    e.body = body;
    e.data = data;
    return e;
}

RunEvent csm_verbose(RunEventKind kind, const QString& title, QJsonObject data = {}) {
    data.insert(QStringLiteral("verbose"), true);
    return csm_event(kind, title, {}, data);
}

QString csm_seconds(int ms) {
    if (ms < 1000)
        return csm_tr("%1 ms").arg(ms);
    return csm_tr("%1 s").arg(QString::number(ms / 1000.0, 'f', ms < 10000 ? 1 : 0));
}

QString csm_number(double v) {
    return QString::number(v, 'g', 12);
}

} // namespace cloud_sse_mapping_detail

using namespace cloud_sse_mapping_detail;

// ── SseParser ────────────────────────────────────────────────────────────────

bool SseParser::feed_line(QByteArray line, Message* out) {
    while (line.endsWith('\n') || line.endsWith('\r'))
        line.chop(1);

    if (line.isEmpty()) {
        // Blank line: dispatch the block (a block without data is dropped, per spec).
        const bool dispatch = has_data_;
        if (dispatch && out) {
            out->event = event_.isEmpty() ? QStringLiteral("message") : event_;
            out->data = data_;
        }
        reset();
        return dispatch;
    }
    if (line.startsWith(':'))
        return false; // comment / keep-alive

    const int colon = line.indexOf(':');
    QByteArray field = colon < 0 ? line : line.left(colon);
    QByteArray value = colon < 0 ? QByteArray() : line.mid(colon + 1);
    if (value.startsWith(' '))
        value.remove(0, 1);

    if (field == "event") {
        // A new block starting while the previous still holds data: servers that
        // omit the blank line between events still get each event delivered.
        bool dispatched = false;
        if (has_data_) {
            if (out) {
                out->event = event_.isEmpty() ? QStringLiteral("message") : event_;
                out->data = data_;
            }
            dispatched = true;
            data_.clear();
            has_data_ = false;
        }
        event_ = QString::fromUtf8(value).trimmed();
        return dispatched;
    }
    if (field == "data") {
        if (has_data_)
            data_ += '\n';
        data_ += value;
        has_data_ = true;
        return false;
    }
    return false; // id:, retry: and unknown fields are not used
}

bool SseParser::flush(Message* out) {
    const bool dispatch = has_data_;
    if (dispatch && out) {
        out->event = event_.isEmpty() ? QStringLiteral("message") : event_;
        out->data = data_;
    }
    reset();
    return dispatch;
}

void SseParser::reset() {
    event_.clear();
    data_.clear();
    has_data_ = false;
}

// ── TokenMeter ───────────────────────────────────────────────────────────────

void TokenMeter::on_step_finish(const QJsonObject& data) {
    const qint64 tokens = static_cast<qint64>(data.value(QStringLiteral("tokensUsed")).toDouble());
    if (tokens <= 0)
        return;
    if (data.contains(QStringLiteral("window"))) {
        // Deep (windowed) mode: the window's running total so far — take the max.
        // step-finish numbers windows from 0, window-start / window-done from 1.
        const int window = data.value(QStringLiteral("window")).toInt() + 1;
        qint64& slot = window_max_[window];
        slot = std::max(slot, tokens);
    } else {
        lite_sum_ += tokens; // lite mode: this graph step's own tokens
    }
}

void TokenMeter::on_window_done(const QJsonObject& data) {
    const qint64 tokens = static_cast<qint64>(data.value(QStringLiteral("tokensUsed")).toDouble());
    if (tokens <= 0)
        return;
    const int window = data.value(QStringLiteral("window")).toInt();
    qint64& slot = window_max_[window];
    slot = std::max(slot, tokens);
}

void TokenMeter::on_finish(const QJsonObject& data) {
    // Lite mode's finish carries the turn's total (= the sum of its steps): it
    // replaces the running sum instead of adding to it. Deep mode sends none.
    const qint64 total = static_cast<qint64>(data.value(QStringLiteral("totalTokens")).toDouble());
    if (total > 0)
        finish_total_ = total;
}

qint64 TokenMeter::total() const {
    if (finish_total_ > 0)
        return finish_total_;
    qint64 windows = 0;
    for (auto it = window_max_.cbegin(); it != window_max_.cend(); ++it)
        windows += it.value();
    return lite_sum_ + windows;
}

void TokenMeter::reset() {
    lite_sum_ = 0;
    finish_total_ = -1;
    window_max_.clear();
}

// ── CloudTurnTracker ─────────────────────────────────────────────────────────

void CloudTurnTracker::reset_for_new_turn() {
    const int keep_step = step;
    *this = CloudTurnTracker{};
    step = keep_step;
}

bool is_verbose_event(const RunEvent& e) {
    return e.data.value(QStringLiteral("verbose")).toBool();
}

// ── Mapping ──────────────────────────────────────────────────────────────────

CloudEventOutcome apply_cloud_event(CloudTurnTracker& t, const QString& name, const QJsonObject& data) {
    CloudEventOutcome out;
    ++t.events_seen;

    if (name == QLatin1String("text-delta")) {
        const QString text = data.value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) {
            t.text += text;
            out.text_delta = text;
        }
        return out;
    }

    if (name == QLatin1String("heartbeat")) {
        out.heartbeat = true;
        return out;
    }

    if (name == QLatin1String("session-meta")) {
        // {session_id, new_title}; a resumed run adds {resumed: true, mode}.
        t.session_uuid = data.value(QStringLiteral("session_id")).toString();
        t.new_title = data.value(QStringLiteral("new_title")).toString();
        QJsonObject d;
        if (!t.session_uuid.isEmpty())
            d["session_uuid"] = t.session_uuid;
        if (!t.new_title.isEmpty())
            d["new_title"] = t.new_title;
        if (data.value(QStringLiteral("resumed")).toBool()) {
            d["resumed"] = true;
            d["mode"] = data.value(QStringLiteral("mode")).toString();
        }
        out.events.append(csm_verbose(RunEventKind::Status,
                                      data.value(QStringLiteral("resumed")).toBool()
                                          ? csm_tr("The paused run continues")
                                          : csm_tr("Connected to the agent"),
                                      d));
        return out;
    }

    if (name == QLatin1String("thinking")) {
        const QString content = data.value(QStringLiteral("content")).toString();
        if (!content.trimmed().isEmpty())
            out.events.append(csm_event(RunEventKind::Thinking, csm_tr("Thinking"), content));
        return out;
    }

    if (name == QLatin1String("planning")) {
        out.events.append(
            csm_event(RunEventKind::Plan, csm_tr("Plan"), data.value(QStringLiteral("summary")).toString().trimmed()));
        return out;
    }

    if (name == QLatin1String("step-start")) {
        QJsonObject d;
        d["stepNumber"] = data.value(QStringLiteral("stepNumber"));
        out.events.append(csm_verbose(
            RunEventKind::Note, csm_tr("Agent step %1").arg(data.value(QStringLiteral("stepNumber")).toInt()), d));
        return out;
    }

    if (name == QLatin1String("step-finish")) {
        t.tokens.on_step_finish(data);
        out.tokens_changed = true;
        QJsonObject d;
        d["tokens_total"] = static_cast<double>(t.tokens.total());
        const QJsonArray tools = data.value(QStringLiteral("toolsUsed")).toArray();
        if (!tools.isEmpty())
            d["toolsUsed"] = tools;
        out.events.append(
            csm_verbose(RunEventKind::Usage, csm_tr("%1 tokens so far").arg(t.tokens.total()), d));
        return out;
    }

    if (name == QLatin1String("window-start")) {
        const int w = data.value(QStringLiteral("window")).toInt();
        const int max_w = data.value(QStringLiteral("maxWindows")).toInt();
        QJsonObject d = data;
        d["sub"] = QStringLiteral("window");
        out.events.append(csm_verbose(RunEventKind::StepStart,
                                      max_w > 0 ? csm_tr("Work window %1 of %2").arg(w).arg(max_w)
                                                : csm_tr("Work window %1").arg(w),
                                      d));
        return out;
    }

    if (name == QLatin1String("window-done")) {
        t.tokens.on_window_done(data);
        out.tokens_changed = true;
        QJsonObject d = data;
        d["sub"] = QStringLiteral("window");
        d["tokens_total"] = static_cast<double>(t.tokens.total());
        out.events.append(csm_verbose(
            RunEventKind::StepEnd, csm_tr("Work window %1 done").arg(data.value(QStringLiteral("window")).toInt()), d));
        return out;
    }

    if (name == QLatin1String("llm-call-start")) {
        out.events.append(csm_verbose(
            RunEventKind::Note, csm_tr("Model call #%1").arg(data.value(QStringLiteral("callNumber")).toInt()), data));
        return out;
    }

    if (name == QLatin1String("llm-call-end")) {
        out.events.append(csm_verbose(RunEventKind::Note,
                                      csm_tr("Model call #%1 answered (%2 tokens)")
                                          .arg(data.value(QStringLiteral("callNumber")).toInt())
                                          .arg(data.value(QStringLiteral("tokens")).toInt()),
                                      data));
        return out;
    }

    if (name == QLatin1String("tool-start")) {
        const QString tool = data.value(QStringLiteral("tool")).toString();
        const QString ref = QStringLiteral("tool-%1").arg(++t.tool_seq);
        t.open_tools[tool].append(ref);
        RunEvent e = csm_event(RunEventKind::ToolCall, tool, data.value(QStringLiteral("preview")).toString());
        e.ref_id = ref;
        e.data["tool"] = tool;
        out.events.append(e);
        return out;
    }

    if (name == QLatin1String("tool-end")) {
        const QString tool = data.value(QStringLiteral("tool")).toString();
        QString ref;
        auto it = t.open_tools.find(tool);
        if (it != t.open_tools.end() && !it->isEmpty()) {
            ref = it->takeFirst();
            if (it->isEmpty())
                t.open_tools.erase(it);
        } else {
            ref = QStringLiteral("tool-%1").arg(++t.tool_seq); // an end without a start (older server)
        }
        // "success" is sent as true on every tool-end today; treat a missing one as success.
        const bool ok = data.value(QStringLiteral("success")).toBool(true);
        RunEvent e = csm_event(RunEventKind::ToolResult, tool, data.value(QStringLiteral("outputPreview")).toString());
        e.ref_id = ref;
        e.is_error = !ok;
        e.data["tool"] = tool;
        e.data["durationMs"] = data.value(QStringLiteral("durationMs")).toInt();
        e.data["success"] = ok;
        out.events.append(e);
        return out;
    }

    if (name == QLatin1String("tool-discover")) {
        out.events.append(csm_verbose(
            RunEventKind::Note, csm_tr("Looking up tools: %1").arg(data.value(QStringLiteral("query")).toString()),
            data));
        return out;
    }

    if (name == QLatin1String("delegation-start")) {
        // {specialists: [..], taskCount, queries: {specialist: query}, skills, dependencies}
        QStringList specialists;
        for (const QJsonValue& v : data.value(QStringLiteral("specialists")).toArray())
            specialists << v.toString();
        const QJsonObject queries = data.value(QStringLiteral("queries")).toObject();
        QStringList lines;
        for (const QString& s : std::as_const(specialists)) {
            const QString q = queries.value(s).toString().trimmed();
            lines << (q.isEmpty() ? s : QStringLiteral("%1: %2").arg(s, q));
        }
        const int count = data.value(QStringLiteral("taskCount")).toInt(static_cast<int>(specialists.size()));
        const QString ref = QStringLiteral("delegation-%1").arg(++t.tool_seq);
        t.open_delegations.append(ref);
        RunEvent e = csm_event(RunEventKind::Delegation,
                               count == 1 ? csm_tr("Delegating to 1 specialist")
                                          : csm_tr("Delegating to %1 specialists").arg(count),
                               lines.join(QLatin1Char('\n')), data);
        e.data["phase"] = QStringLiteral("start");
        e.ref_id = ref;
        out.events.append(e);
        return out;
    }

    if (name == QLatin1String("delegation-end")) {
        const QString ref = t.open_delegations.isEmpty() ? QStringLiteral("delegation-%1").arg(++t.tool_seq)
                                                         : t.open_delegations.takeFirst();
        const int ms = data.value(QStringLiteral("durationMs")).toInt();
        RunEvent e = csm_event(RunEventKind::Delegation, csm_tr("Specialists finished in %1").arg(csm_seconds(ms)),
                               data.value(QStringLiteral("outputPreview")).toString());
        e.data["phase"] = QStringLiteral("end");
        e.data["durationMs"] = ms;
        e.ref_id = ref;
        out.events.append(e);
        return out;
    }

    if (name == QLatin1String("interrupt")) {
        // {question, mode, resumable}; older servers send only {question}.
        t.interrupted = true;
        t.question = data.value(QStringLiteral("question")).toString().trimmed();
        t.question_mode = data.value(QStringLiteral("mode")).toString();
        t.resumable = data.value(QStringLiteral("resumable")).toBool();
        QJsonObject d;
        d["resumable"] = t.resumable;
        if (!t.question_mode.isEmpty())
            d["mode"] = t.question_mode;
        out.events.append(csm_event(RunEventKind::Question, csm_tr("The agent asks"),
                                    t.question.isEmpty() ? csm_tr("The agent needs more information to continue.")
                                                         : t.question,
                                    d));
        return out;
    }

    if (name == QLatin1String("approval-required")) {
        // {proposal_id, action, symbol, quantity, price, total_value, portfolio_id, portfolio_name, notes}
        const QString action = data.value(QStringLiteral("action")).toString().trimmed().toUpper();
        const QString symbol = data.value(QStringLiteral("symbol")).toString().trimmed();
        RunEvent e = csm_event(RunEventKind::Approval,
                               csm_tr("Trade approval: %1 %2 %3")
                                   .arg(action, csm_number(data.value(QStringLiteral("quantity")).toDouble()), symbol)
                                   .simplified(),
                               data.value(QStringLiteral("notes")).toString(), data);
        e.ref_id = data.value(QStringLiteral("proposal_id")).toString();
        out.events.append(e);
        return out;
    }

    if (name == QLatin1String("proposal-decided")) {
        // {proposal_id, status, approved, message, decided_by, decided_at}
        const QString id = data.value(QStringLiteral("proposal_id")).toString();
        const QString status = data.value(QStringLiteral("status")).toString();
        if (id.isEmpty() || status.isEmpty())
            return out;
        RunEvent e = csm_event(RunEventKind::ApprovalDecided, csm_tr("Trade proposal %1").arg(status),
                               data.value(QStringLiteral("message")).toString(), data);
        e.ref_id = id;
        out.events.append(e);
        return out;
    }

    if (name == QLatin1String("trade-executed")) {
        out.events.append(csm_event(RunEventKind::Note, csm_tr("A trade was executed in the Fincept portfolio ledger"),
                                    {}, data));
        return out;
    }

    if (name == QLatin1String("history-compressed")) {
        out.events.append(csm_event(RunEventKind::Note,
                                    csm_tr("Conversation history compressed (%1 → %2 tokens)")
                                        .arg(data.value(QStringLiteral("beforeTokens")).toInt())
                                        .arg(data.value(QStringLiteral("afterTokens")).toInt()),
                                    {}, data));
        return out;
    }

    if (name == QLatin1String("rate-limit-retry")) {
        out.events.append(csm_event(RunEventKind::Note,
                                    csm_tr("The model is busy — retry %1 in %2 s")
                                        .arg(data.value(QStringLiteral("attempt")).toInt())
                                        .arg(data.value(QStringLiteral("backoffSecs")).toInt()),
                                    {}, data));
        return out;
    }

    if (name == QLatin1String("follow-ups")) {
        QStringList qs;
        for (const QJsonValue& v : data.value(QStringLiteral("questions")).toArray()) {
            const QString q = v.toString().trimmed();
            if (!q.isEmpty())
                qs << q;
        }
        t.follow_ups = qs;
        if (!qs.isEmpty()) {
            QJsonObject d;
            d["follow_ups"] = QJsonArray::fromStringList(qs);
            out.events.append(csm_event(RunEventKind::Note, csm_tr("Suggested follow-ups"), qs.join(QLatin1Char('\n')), d));
        }
        return out;
    }

    if (name == QLatin1String("critique")) {
        QJsonObject d = data;
        d["critique"] = true;
        out.events.append(csm_event(RunEventKind::Note,
                                    csm_tr("Self-check (confidence %1)").arg(data.value(QStringLiteral("confidence")).toVariant().toString()),
                                    data.value(QStringLiteral("summary")).toString(), d));
        return out;
    }

    if (name == QLatin1String("finish")) {
        // {totalTokens?, elapsedMs, windows?, toolsUsed?, interrupted?,
        //  credits_charged?, credit_balance?} (chat.py _finish_with_billing)
        t.finished = true;
        if (data.value(QStringLiteral("interrupted")).toBool())
            t.interrupted = true;
        t.tokens.on_finish(data);
        out.tokens_changed = true;
        const QJsonValue charged = data.value(QStringLiteral("credits_charged"));
        const QJsonValue balance = data.value(QStringLiteral("credit_balance"));
        QJsonObject d;
        d["tokens_total"] = static_cast<double>(t.tokens.total());
        d["turn_end"] = true;
        if (charged.isDouble()) {
            t.credits_known = true;
            t.credits_charged = std::max(0.0, charged.toDouble());
            d["credits_charged"] = t.credits_charged;
        }
        if (balance.isDouble()) {
            t.balance_known = true;
            t.credit_balance = std::max(0.0, balance.toDouble());
            d["credit_balance"] = t.credit_balance;
        }
        if (data.contains(QStringLiteral("elapsedMs")))
            d["elapsedMs"] = data.value(QStringLiteral("elapsedMs"));
        const QString title = t.credits_known
                                  ? csm_tr("Reply cost %1 credits · %2 tokens")
                                        .arg(csm_number(t.credits_charged))
                                        .arg(t.tokens.total())
                                  : csm_tr("Reply finished · %1 tokens").arg(t.tokens.total());
        out.events.append(csm_event(RunEventKind::Usage, title, {}, d));
        out.terminal = true;
        return out;
    }

    if (name == QLatin1String("error")) {
        // {message | error, code?, retry_after?}: "agent_busy" / "agent_unavailable"
        // (with retry_after), "not_waiting" (a resumed run's pause was gone).
        QString msg = data.value(QStringLiteral("message")).toString();
        if (msg.isEmpty())
            msg = data.value(QStringLiteral("error")).toString();
        if (msg.trimmed().isEmpty())
            msg = csm_tr("The agent reported an error.");
        const QString code = data.value(QStringLiteral("code")).toString().trimmed().toLower();
        const int retry_after = data.value(QStringLiteral("retry_after")).toInt();
        if (retry_after > 0)
            msg = csm_sentence(msg) + QLatin1Char(' ') + csm_tr("You can retry in about %1 s.").arg(retry_after);
        t.errored = true;
        t.error_message = msg.trimmed();
        t.error_code = code;
        RunEvent e = csm_event(RunEventKind::Error, csm_tr("The agent stopped with an error"), t.error_message);
        e.is_error = true;
        if (!code.isEmpty())
            e.data["code"] = code;
        if (retry_after > 0)
            e.data["retry_after"] = retry_after;
        out.events.append(e);
        out.terminal = true;
        return out;
    }

    QJsonObject d = data;
    d["event"] = name;
    out.events.append(csm_verbose(RunEventKind::Note, csm_tr("Server event: %1").arg(name), d));
    return out;
}

} // namespace fincept::agent_tasks
