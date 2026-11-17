#pragma once
// RunPlanPrompts.h — the prompts CloudPlanExecutor sends and the parser for the
// plan the cloud agent answers with. Header-only, Qt Core only (unit-tested).
//
//   plan_request()       first turn: a fenced JSON plan {"steps":[{title,
//                        instruction, deliverable}]} of at most N steps.
//   step_request()       one turn per step: the step, its deliverable, a short
//                        reminder of the goal and of what earlier steps found.
//   synthesis_request()  last turn: the final report from the step results.
//   parse_plan_reply()   the plan in a reply — fenced or bare JSON, prose around
//                        it, trailing commas, smart quotes — capped and cleaned;
//                        ONE step carrying the goal when nothing usable is found.
//
// The prompts are sent to the agent verbatim and are deliberately not
// translated (their JSON contract must stay exact). Step and synthesis requests
// stay well under kChatMessageMaxChars whatever the plan holds; they are also
// deterministic for the same run and steps, which lets a resumed run find the
// message it sent before an app quit (CloudTurnDriver::find_answer).

#include "services/agent_tasks/TaskRunTypes.h"

#include <QChar>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <initializer_list>

namespace fincept::agent_tasks::run_plan_prompts {

// ── Limits ───────────────────────────────────────────────────────────────────

inline constexpr int kPlanHardMaxSteps = 12;       ///< the plan cap when the run's budget sets none
inline constexpr int kTitleChars = 120;
inline constexpr int kInstructionChars = 2000;
inline constexpr int kDeliverableChars = 500;
inline constexpr int kGoalReminderChars = 1500;    ///< the goal as repeated in step / synthesis turns
inline constexpr int kWholeTaskGoalChars = 4000;   ///< a single-step fallback restates more of the goal
inline constexpr int kContextChars = 2000;         ///< the run's context block in the plan request
inline constexpr int kPriorSummaryChars = 400;     ///< one earlier step's result in a step request
inline constexpr int kPriorTotalChars = 4000;      ///< all earlier steps in a step request
inline constexpr int kSynthesisStepChars = 2500;   ///< one step's result in the synthesis request
inline constexpr int kSynthesisTotalChars = 12000; ///< all step results in the synthesis request
inline constexpr int kSynthesisMaxSections = 30;   ///< steps listed in the synthesis request

// ── Helpers ──────────────────────────────────────────────────────────────────

/// `text` trimmed and cut to `max_chars` (an ellipsis marks the cut).
inline QString clip(const QString& text, int max_chars) {
    const QString t = text.trimmed();
    if (max_chars <= 0 || t.size() <= max_chars)
        return t;
    return t.left(std::max(0, max_chars - 1)).trimmed() + QChar(0x2026);
}

/// One line: whitespace runs collapsed, then clipped.
inline QString clip_line(const QString& text, int max_chars) {
    return clip(text.simplified(), max_chars);
}

/// The run context as "- key: value" lines (internal keys skipped), bounded.
inline QString context_block(const QJsonObject& context) {
    QStringList lines;
    int total = 0;
    for (auto it = context.constBegin(); it != context.constEnd(); ++it) {
        if (it.key() == QLatin1String(kCtxAnswerToPause))
            continue;
        QString value;
        const QJsonValue v = it.value();
        if (v.isString())
            value = v.toString();
        else if (v.isDouble())
            value = QString::number(v.toDouble(), 'g', 12);
        else if (v.isBool())
            value = v.toBool() ? QStringLiteral("yes") : QStringLiteral("no");
        else if (v.isArray())
            value = QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        else if (v.isObject())
            value = QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        value = clip_line(value, 500);
        if (value.isEmpty())
            continue;
        const QString line = QStringLiteral("- %1: %2").arg(it.key(), value);
        if (total + line.size() > kContextChars)
            break;
        total += static_cast<int>(line.size()) + 1;
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

// ── Requests ─────────────────────────────────────────────────────────────────

/// First turn: ask for a plan of at most `max_steps` steps as a fenced JSON block
/// {"steps":[{"title","instruction","deliverable"}]} for `goal` with `context`.
inline QString plan_request(const QString& goal, const QJsonObject& context, int max_steps) {
    const int cap = max_steps > 0 ? max_steps : kPlanHardMaxSteps;
    QString out;
    out += QStringLiteral("Plan the following finance research task as a short sequence of steps. Do not carry it "
                          "out yet — only plan it.\n\n");
    out += QStringLiteral("TASK:\n") + goal.trimmed() + QStringLiteral("\n\n");
    const QString ctx = context_block(context);
    if (!ctx.isEmpty())
        out += QStringLiteral("CONTEXT:\n") + ctx + QStringLiteral("\n\n");
    out += QStringLiteral("Rules for the plan:\n"
                          "- At most %1 steps; use fewer when the task is simple.\n"
                          "- Each step must be doable in one turn with your tools and must produce a concrete "
                          "deliverable (figures, a table, a short finding) that later steps or the final report "
                          "can use.\n"
                          "- The steps run one after another in this conversation; a final turn then combines their "
                          "deliverables into the report, so do not add a separate \"write the report\" step.\n\n")
               .arg(cap);
    out += QStringLiteral("Reply with only the plan as one fenced JSON block in exactly this shape, and nothing "
                          "else:\n"
                          "```json\n"
                          "{\"steps\":[{\"title\":\"short title\",\"instruction\":\"what to do in this step\","
                          "\"deliverable\":\"what the step must produce\"}]}\n"
                          "```");
    return out;
}

/// One turn per step: carry out `step` (index `step.idx` of `step_count`) of
/// `run`; `prior` are the run's steps (earlier ones with their results).
inline QString step_request(const TaskRun& run, const RunStep& step, int step_count, const QVector<RunStep>& prior) {
    const int number = step.idx + 1;
    const int count = std::max(step_count, number);
    const QString goal = run.prompt.trimmed();
    const bool whole_task = step.instruction.trimmed() == goal; // the single-step fallback
    QString out = QStringLiteral("Step %1 of %2 of the planned task.\n\n").arg(number).arg(count);
    if (whole_task) {
        out += QStringLiteral("Carry out the whole task now. It is stated in full earlier in this conversation; in "
                              "short:\n") +
               clip(goal, kWholeTaskGoalChars) + QStringLiteral("\n\n");
    } else {
        out += QStringLiteral("Overall goal (reminder):\n") + clip(goal, kGoalReminderChars) + QStringLiteral("\n\n");

        // Earlier steps, newest first until the budget is spent; older ones are
        // only counted (their full results are in this conversation).
        QStringList lines;
        int total = 0;
        int omitted = 0;
        for (int i = static_cast<int>(prior.size()) - 1; i >= 0; --i) {
            const RunStep& p = prior.at(i);
            if (p.idx >= step.idx)
                continue;
            if (total >= kPriorTotalChars) {
                ++omitted;
                continue;
            }
            const QString title = clip_line(p.title, 80);
            QString line;
            if (p.status == StepStatus::Done)
                line = QStringLiteral("%1. %2 — %3")
                           .arg(p.idx + 1)
                           .arg(title, p.summary.trimmed().isEmpty() ? QStringLiteral("done")
                                                                     : clip_line(p.summary, kPriorSummaryChars));
            else if (p.status == StepStatus::Skipped)
                line = QStringLiteral("%1. %2 — skipped").arg(p.idx + 1).arg(title);
            else if (p.status == StepStatus::Failed)
                line = QStringLiteral("%1. %2 — failed, no result").arg(p.idx + 1).arg(title);
            else
                line = QStringLiteral("%1. %2 — not done").arg(p.idx + 1).arg(title);
            if (total + line.size() > kPriorTotalChars && !lines.isEmpty()) {
                ++omitted;
                total = kPriorTotalChars;
                continue;
            }
            total += static_cast<int>(line.size()) + 1;
            lines.prepend(line);
        }
        if (!lines.isEmpty() || omitted > 0) {
            out += QStringLiteral("Done so far (full results are earlier in this conversation):\n");
            if (omitted > 0)
                out += QStringLiteral("(%1 earlier step(s) not repeated here)\n").arg(omitted);
            out += lines.join(QLatin1Char('\n')) + QStringLiteral("\n\n");
        }
        const QString title = clip_line(step.title, kTitleChars);
        const QString instruction = clip(step.instruction.isEmpty() ? step.title : step.instruction, kInstructionChars);
        out += QStringLiteral("Now do step %1").arg(number);
        out += title.isEmpty() ? QStringLiteral(":\n") : QStringLiteral(" — %1:\n").arg(title);
        out += instruction + QStringLiteral("\n\n");
    }
    const QString deliverable = clip(step.deliverable, kDeliverableChars);
    if (!deliverable.isEmpty())
        out += QStringLiteral("Deliverable: ") + deliverable + QStringLiteral("\n\n");
    out += whole_task ? QStringLiteral("Give the complete answer (use Markdown tables for figures where useful).")
                      : QStringLiteral("Work only on this step; the remaining steps follow in later turns. End your "
                                       "reply with the deliverable itself (use Markdown tables for figures where "
                                       "useful).");
    return out;
}

/// One turn per step (without the earlier steps' results).
inline QString step_request(const TaskRun& run, const RunStep& step, int step_count) {
    return step_request(run, step, step_count, {});
}

/// Final turn: synthesise the steps' results into the run's answer.
inline QString synthesis_request(const TaskRun& run, const QVector<RunStep>& steps) {
    const int n = static_cast<int>(steps.size());
    QString out = QStringLiteral("All %1 step(s) of the planned task are done. Write the final report now.\n\n").arg(n);
    out += QStringLiteral("Task (reminder):\n") + clip(run.prompt, kGoalReminderChars) + QStringLiteral("\n\n");
    out += QStringLiteral("Step results (full versions are earlier in this conversation):\n\n");
    const int per_step = n > 0 ? std::clamp(kSynthesisTotalChars / n, 300, kSynthesisStepChars) : kSynthesisStepChars;
    int total = 0;
    int listed = 0;
    for (const RunStep& s : steps) {
        if (listed >= kSynthesisMaxSections) {
            out += QStringLiteral("(and %1 more step(s), see above)\n\n").arg(n - listed);
            break;
        }
        ++listed;
        const QString head = QStringLiteral("### Step %1 — %2").arg(s.idx + 1).arg(clip_line(s.title, 80));
        QString body;
        if (s.status == StepStatus::Done && !s.summary.trimmed().isEmpty() && total < kSynthesisTotalChars) {
            body = clip(s.summary, std::min(per_step, kSynthesisTotalChars - total));
            total += static_cast<int>(body.size());
        } else if (s.status == StepStatus::Done) {
            body = QStringLiteral("(result earlier in this conversation)");
        } else if (s.status == StepStatus::Skipped) {
            body = QStringLiteral("(skipped)");
        } else {
            body = QStringLiteral("(no result — this step did not complete)");
        }
        out += head + QLatin1Char('\n') + body + QStringLiteral("\n\n");
    }
    out += QStringLiteral("Write the report for the user: a short summary first, then the findings with the key "
                          "numbers and tables (Markdown). Build on the step results and this conversation; say "
                          "plainly what could not be established. Do not start new research.");
    return out;
}

// ── Reading a request back ───────────────────────────────────────────────────

enum class RequestKind { None, Plan, Step, Synthesis };

/// What a stored conversation message is, when CloudPlanExecutor sent it.
struct RequestInfo {
    RequestKind kind = RequestKind::None;
    QString goal;  ///< Plan: the task as the user wrote it
    int step = 0;  ///< Step: 1-based number
    int steps = 0; ///< Step / Synthesis: how many steps the plan has
    QString title; ///< Step: the step's title (may be empty)
};

/// Recognises plan_request() / step_request() / synthesis_request() texts, so a
/// conversation that is shown again can present them as the task and its steps
/// instead of as messages the user typed. Anything else is RequestKind::None.
inline RequestInfo classify_request(const QString& message) {
    RequestInfo info;
    if (message.startsWith(QStringLiteral("Plan the following finance research task as a short sequence of steps."))) {
        const QString task_mark = QStringLiteral("\n\nTASK:\n");
        const int a = static_cast<int>(message.indexOf(task_mark));
        const int rules = static_cast<int>(message.lastIndexOf(QStringLiteral("\n\nRules for the plan:\n")));
        if (a < 0 || rules <= a)
            return info;
        const int from = a + static_cast<int>(task_mark.size());
        int to = rules;
        const int ctx = static_cast<int>(message.lastIndexOf(QStringLiteral("\n\nCONTEXT:\n"), rules));
        if (ctx >= from)
            to = ctx;
        info.kind = RequestKind::Plan;
        info.goal = message.mid(from, to - from).trimmed();
        return info;
    }
    static const QRegularExpression kStep(QStringLiteral("^Step (\\d+) of (\\d+) of the planned task\\.\\n"));
    const QRegularExpressionMatch sm = kStep.match(message);
    if (sm.hasMatch()) {
        info.kind = RequestKind::Step;
        info.step = sm.captured(1).toInt();
        info.steps = sm.captured(2).toInt();
        const QString mark = QStringLiteral("\nNow do step %1 — ").arg(info.step);
        const int t = static_cast<int>(message.indexOf(mark));
        if (t >= 0) {
            const int begin = t + static_cast<int>(mark.size());
            const int end = static_cast<int>(message.indexOf(QStringLiteral(":\n"), begin));
            if (end > begin)
                info.title = message.mid(begin, end - begin).trimmed();
        }
        return info;
    }
    static const QRegularExpression kSynthesis(
        QStringLiteral("^All (\\d+) step\\(s\\) of the planned task are done\\. Write the final report now\\."));
    const QRegularExpressionMatch ym = kSynthesis.match(message);
    if (ym.hasMatch()) {
        info.kind = RequestKind::Synthesis;
        info.steps = ym.captured(1).toInt();
    }
    return info;
}

// ── Plan parsing ─────────────────────────────────────────────────────────────

namespace detail {

/// Contents of ``` fenced blocks, json-labelled ones first.
inline QStringList fenced_blocks(const QString& text) {
    QStringList labelled, plain;
    int pos = 0;
    while (true) {
        const int open = text.indexOf(QStringLiteral("```"), pos);
        if (open < 0)
            break;
        const int line_end = text.indexOf(QLatin1Char('\n'), open + 3);
        if (line_end < 0)
            break;
        const QString label = text.mid(open + 3, line_end - open - 3).trimmed().toLower();
        const int close = text.indexOf(QStringLiteral("```"), line_end + 1);
        const QString body = close < 0 ? text.mid(line_end + 1) : text.mid(line_end + 1, close - line_end - 1);
        (label.startsWith(QLatin1String("json")) ? labelled : plain) << body;
        if (close < 0)
            break;
        pos = close + 3;
    }
    return labelled + plain;
}

/// Balanced {…} / […] spans (string-aware), outermost first, in text order.
inline QStringList balanced_spans(const QString& text) {
    QStringList out;
    const int n = static_cast<int>(text.size());
    for (int start = 0; start < n && out.size() < 16; ++start) {
        const QChar c = text.at(start);
        if (c != QLatin1Char('{') && c != QLatin1Char('['))
            continue;
        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        int end = -1;
        for (int i = start; i < n; ++i) {
            const QChar ch = text.at(i);
            if (in_string) {
                if (escaped)
                    escaped = false;
                else if (ch == QLatin1Char('\\'))
                    escaped = true;
                else if (ch == QLatin1Char('"'))
                    in_string = false;
                continue;
            }
            if (ch == QLatin1Char('"'))
                in_string = true;
            else if (ch == QLatin1Char('{') || ch == QLatin1Char('['))
                ++depth;
            else if (ch == QLatin1Char('}') || ch == QLatin1Char(']')) {
                if (--depth == 0) {
                    end = i;
                    break;
                }
            }
        }
        if (end < 0)
            continue;
        out << text.mid(start, end - start + 1);
        start = end; // the next span starts after this one
    }
    return out;
}

/// JSON as models write it: smart quotes straightened, trailing commas dropped.
inline QString repair_json(QString s) {
    s.replace(QChar(0x201C), QLatin1Char('"'));
    s.replace(QChar(0x201D), QLatin1Char('"'));
    s.replace(QChar(0xFEFF), QString());
    QString out;
    out.reserve(s.size());
    bool in_string = false;
    bool escaped = false;
    for (int i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (in_string) {
            out += ch;
            if (escaped)
                escaped = false;
            else if (ch == QLatin1Char('\\'))
                escaped = true;
            else if (ch == QLatin1Char('"'))
                in_string = false;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            in_string = true;
            out += ch;
            continue;
        }
        if (ch == QLatin1Char(',')) {
            int j = i + 1;
            while (j < s.size() && s.at(j).isSpace())
                ++j;
            if (j < s.size() && (s.at(j) == QLatin1Char('}') || s.at(j) == QLatin1Char(']')))
                continue; // trailing comma
        }
        out += ch;
    }
    return out;
}

/// The step array of a parsed plan document, or an empty array.
inline QJsonArray steps_of(const QJsonDocument& doc) {
    if (doc.isArray())
        return doc.array();
    const QJsonObject o = doc.object();
    for (const char* key : {"steps", "plan", "tasks"}) {
        const QJsonValue v = o.value(QLatin1String(key));
        if (v.isArray())
            return v.toArray();
        if (v.isObject()) {
            const QJsonValue inner = v.toObject().value(QStringLiteral("steps"));
            if (inner.isArray())
                return inner.toArray();
        }
    }
    return {};
}

inline QString first_string(const QJsonObject& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const QJsonValue v = o.value(QLatin1String(k));
        if (v.isString() && !v.toString().trimmed().isEmpty())
            return v.toString().trimmed();
    }
    return {};
}

/// A title for a step that has only an instruction: its first sentence, short.
inline QString title_from(const QString& instruction) {
    static const QRegularExpression kSentenceEnd(QStringLiteral("[.!?](\\s|$)"));
    QString t = instruction.simplified();
    const int stop = static_cast<int>(t.indexOf(kSentenceEnd));
    if (stop > 0)
        t = t.left(stop);
    return clip_line(t, 80);
}

} // namespace detail

/// The plan in `reply` (a fenced or bare JSON object with "steps"), capped at
/// `max_steps` (0 = no cap). When it does not parse: ONE step carrying `goal`,
/// and `error` says why. `proposed_steps` (optional) receives how many usable
/// steps the reply held before the cap (0 on the fallback).
inline QVector<RunStep> parse_plan_reply(const QString& reply, const QString& goal, int max_steps,
                                         QString* error = nullptr, int* proposed_steps = nullptr) {
    const auto tr = [](const char* s) { return QCoreApplication::translate("fincept::agent_tasks::RunPlanPrompts", s); };
    if (error)
        error->clear();
    if (proposed_steps)
        *proposed_steps = 0;

    QStringList candidates = detail::fenced_blocks(reply);
    candidates += detail::balanced_spans(reply);
    QString why = reply.trimmed().isEmpty() ? tr("the agent's reply was empty") : tr("the reply held no JSON plan");

    QVector<RunStep> steps;
    for (const QString& candidate : std::as_const(candidates)) {
        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(candidate.trimmed().toUtf8(), &pe);
        if (pe.error != QJsonParseError::NoError)
            doc = QJsonDocument::fromJson(detail::repair_json(candidate.trimmed()).toUtf8(), &pe);
        if (pe.error != QJsonParseError::NoError) {
            // A fenced block that is not JSON may wrap one: its spans are tried too.
            why = tr("the plan JSON did not parse (%1)").arg(pe.errorString());
            continue;
        }
        const QJsonArray arr = detail::steps_of(doc);
        if (arr.isEmpty()) {
            why = tr("the JSON held no steps");
            continue;
        }
        QSet<QString> seen;
        QVector<RunStep> parsed;
        for (const QJsonValue& v : arr) {
            QString title, instruction, deliverable;
            if (v.isString()) {
                instruction = v.toString().trimmed();
            } else if (v.isObject()) {
                const QJsonObject o = v.toObject();
                title = detail::first_string(o, {"title", "name", "step", "summary"});
                instruction = detail::first_string(o, {"instruction", "instructions", "description", "task", "details",
                                                       "action", "query"});
                deliverable = detail::first_string(o, {"deliverable", "output", "expected_output", "result", "produces"});
            }
            if (title.isEmpty() && instruction.isEmpty())
                continue; // an empty step
            if (title.isEmpty())
                title = detail::title_from(instruction);
            if (instruction.isEmpty())
                instruction = title;
            RunStep s;
            s.title = clip_line(title, kTitleChars);
            s.instruction = clip(instruction, kInstructionChars);
            s.deliverable = clip(deliverable, kDeliverableChars);
            const QString key = (s.title + QLatin1Char('\x1f') + s.instruction).toLower();
            if (seen.contains(key))
                continue; // the same step twice
            seen.insert(key);
            parsed.append(s);
        }
        if (parsed.isEmpty()) {
            why = tr("the plan had no usable steps");
            continue;
        }
        steps = parsed;
        break;
    }

    if (steps.isEmpty()) {
        if (error)
            *error = why;
        RunStep only;
        only.idx = 0;
        only.title = tr("Carry out the task");
        only.instruction = goal.trimmed();
        only.deliverable = tr("The complete answer to the task");
        return {only};
    }
    if (proposed_steps)
        *proposed_steps = static_cast<int>(steps.size());
    if (max_steps > 0 && steps.size() > max_steps)
        steps.resize(max_steps);
    for (int i = 0; i < steps.size(); ++i)
        steps[i].idx = i;
    return steps;
}

} // namespace fincept::agent_tasks::run_plan_prompts
