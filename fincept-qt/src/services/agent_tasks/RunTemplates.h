#pragma once
// RunTemplates.h — finance task templates for NEW RUN and schedules (WP6):
// equity deep-dive, earnings preview / review, portfolio review, sector / peer
// comparison, macro / market wrap, watchlist screen, news digest for holdings,
// portfolio risk check — each with typed parameters, a goal prompt, a plan hint,
// the artefacts it is expected to produce and a default budget. Users save their
// own templates (RunTemplateLibrary keeps them; this file only (de)serializes).
//
// Prompts carry <PLACEHOLDERS> (the idiom of AgentWorkbenchTemplates.cpp),
// filled from the run's context: a placeholder named like a context key (case-
// insensitive, spaces / underscores equal) is replaced by its value; a line whose
// placeholder has an EMPTY value is dropped (optional parameters); a placeholder
// with no key at all stays. Qt Core only (tests/tst_run_scheduling).

#include "services/agent_tasks/TaskRunTypes.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::agent_tasks {

/// What a template parameter holds, and the context keys it fills:
///   Symbol    → "<key>" (one ticker, e.g. "symbol")
///   Symbols   → "<key>" (tickers joined with ", ")
///   Portfolio → "portfolio_id", "portfolio_name"   (holdings are appended at start)
///   Watchlist → "watchlist_id", "watchlist_name"   (symbols are appended at start)
///   DateRange → "date_range" (a schedule_logic code), "date_from", "date_to", "period"
///   Text / LongText / Choice → "<key>"
enum class TemplateParamType { Symbol, Symbols, Portfolio, Watchlist, DateRange, Text, LongText, Choice };

QString param_type_id(TemplateParamType t);
std::optional<TemplateParamType> param_type_from_id(const QString& id);

struct RunTemplateParam {
    QString key;   ///< context key (Symbol / Symbols / Text / LongText / Choice); fixed keys for the others
    QString label; ///< translated
    TemplateParamType type = TemplateParamType::Text;
    bool required = false;
    QString default_value; ///< Choice: a value of `choices`; DateRange: a date_range code
    QString hint;          ///< placeholder / help text
    QStringList choices;       ///< Choice: stored values
    QStringList choice_labels; ///< Choice: what the user sees (same order)
};

struct RunTemplate {
    QString id;          ///< stable id stored in agent_task_runs.template_id ("user:<uuid>" for user templates)
    QString title;       ///< translated
    QString description; ///< translated, one or two sentences
    QString prompt;      ///< with <PLACEHOLDERS>
    QString engine = QString::fromLatin1(kEngineCloud);
    QString mode = QStringLiteral("deep");
    bool plan_first = true;
    RunBudget budget;               ///< suggested budget
    QStringList context_keys;       ///< context the template asks for ("symbol", "portfolio_id" …)
    // ── WP6 ──
    QVector<RunTemplateParam> params;
    QString title_pattern;          ///< the run's title, with placeholders ("Deep-dive: <SYMBOL>")
    QString plan_hint;              ///< suggested approach, appended to the goal
    QStringList expected_artefacts; ///< what the run should leave behind (shown, and asked for)
    QString category;               ///< "Equities", "Portfolio", "Markets", "Mine"
    bool user_defined = false;
    QString based_on;               ///< the built-in a user template was made from
    qint64 created_at_ms = 0;
    qint64 updated_at_ms = 0;
};

/// The built-in templates (translated at call time), in display order.
QVector<RunTemplate> run_templates();
/// A built-in template by id (user templates: RunTemplateLibrary::find).
std::optional<RunTemplate> find_run_template(const QString& id);

/// `prompt` with each <PLACEHOLDER> whose name matches a key of `context`
/// (case-insensitive, spaces / underscores equal) replaced by that value; lines
/// whose placeholder value is empty are dropped. Unresolved placeholders stay.
QString fill_run_template(const QString& prompt, const QJsonObject& context);
/// The <PLACEHOLDERS> still present in `prompt`, in order, without duplicates.
QStringList run_template_placeholders(const QString& prompt);
/// A start request for `t` with `context` (prompt filled, plan hint and expected
/// output appended, title, budget, engine, mode). Relative dates are NOT expanded
/// here (schedule_logic::expand_relative_dates, at start time).
RunRequest request_from_template(const RunTemplate& t, const QJsonObject& context);

/// The parameters `context` lacks or gets wrong, as plain sentences (empty: OK).
/// Also reports placeholders of the prompt that nothing fills.
QStringList validate_template_params(const RunTemplate& t, const QJsonObject& context);
/// The context keys a parameter writes (see TemplateParamType).
QStringList param_context_keys(const RunTemplateParam& p);
/// A context with every parameter's default value.
QJsonObject template_default_context(const RunTemplate& t);

/// User templates are stored as JSON (agent_run_templates.template_json).
QJsonObject run_template_to_json(const RunTemplate& t);
RunTemplate run_template_from_json(const QJsonObject& o);

} // namespace fincept::agent_tasks
