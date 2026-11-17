#pragma once
// ConnectorArgs — the arguments of one catalogue connector command.
//
// TYPED PARAMS (the catalogue contract, resources/data_connectors.json "convention"):
//   commands[].params = the arguments in argv order; null = unknown (fall back to args_hint),
//   [] = no arguments. A param is {name, required, default, kind, choices?, help?, named?,
//   variadic?}, kind string|int|float|date|enum|symbol|country|bool. `default` is null on a
//   required param; on an optional one a value reproduces the script's default, "" means the
//   script reads an empty argument as "not given", null means there is NO sendable default.
//   argv = <command>, then the positionals in list order — a required one always; an optional one
//   may be left out only together with every positional after it; to send a later one, the
//   earlier one's default goes in its place (an earlier optional with a null default and a later
//   one given is an input error) — then each given named param as one item --<name>=<value> (a
//   named bool: --<name> when true, nothing when false); a variadic param is the last positional
//   and takes several items. Values: int / float plain decimal, bool true/false, date
//   YYYY-MM-DD, enum exactly one of choices, symbol / country as typed.
//   build_argv() is the ONE function that applies these rules (unit-tested).
//
// HINT FALLBACK (params null): args_hint ("<series_id> [start_date] [end_date]",
//   "[country=US] [year=2023]", "[--start ...]", "[countries...]") read as fields, or one
//   free-text box when it has no clear shape (args_form_from_hint / build_args).
//
// Pure (QtCore only); tested in tests/tst_connector_args.cpp.

#include <QDate>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::connectors {

// ── Typed params (catalogue contract) ──────────────────────────────────────

enum class ParamKind { String, Int, Float, Date, Enum, Symbol, Country, Bool };

/// "string" … "bool".
QString param_kind_name(ParamKind kind);
/// Inverse; false (and String) for an unknown name.
bool param_kind_from_name(const QString& name, ParamKind* kind);

struct ConnectorParam {
    QString name;
    bool required = false;
    bool has_default = false; ///< false: the catalogue default is null (nothing sendable)
    QString default_value;    ///< the default as an argv string; "" = the script reads "" as not given
    ParamKind kind = ParamKind::String;
    QStringList choices;      ///< enum only
    QString help;
    bool named = false;       ///< --<name>=<value> after the positionals
    bool variadic = false;    ///< the last positional; several argv items
    /// One argument holding a comma-separated list (help says so, or the default is one).
    bool is_list() const;
};

struct ParamList {
    bool known = false;               ///< false: params null / absent / malformed → use args_hint
    QVector<ConnectorParam> params;
    QString error;                    ///< why a present list was rejected (malformed)
};
/// Parse commands[].params. null / absent → known = false; [] → known, no params.
ParamList parse_param_list(const QJsonValue& value);

/// What the form holds for one param. `given` = false: left empty (date not set, "(not set)").
struct ParamInput {
    bool given = false;
    QString value;
    static ParamInput of(const QString& v) { return ParamInput{!v.trimmed().isEmpty(), v}; }
};

struct ArgvResult {
    bool ok = false;
    QStringList argv;   ///< argv[0] = the command
    QString error;      ///< plain sentence naming the param
    int param = -1;     ///< index of the param to focus
    QStringList args() const { return argv.mid(1); }
};

/// argv for `command` from one input per param (same order as `params`), by the contract rules.
ArgvResult build_argv(const QString& command, const QVector<ConnectorParam>& params, const QVector<ParamInput>& inputs);

/// Validate and normalise one value of `param` (trimmed; "" stays ""). False with *why on error.
bool normalize_param_value(const ConnectorParam& param, const QString& value, QString* out, QString* why);
/// YYYY-MM-DD.
QString date_arg(const QDate& date);
/// Plain decimal, no exponent, no trailing zeros ("1000", "48.8", "-0.25").
QString float_arg(double value);

/// The inputs that reproduce `args` (argv after the command: positionals, then --name=value /
/// --flag) — to show a sample or a remembered run in the form. False when they do not fit.
bool inputs_from_args(const QVector<ConnectorParam>& params, const QStringList& args, QVector<ParamInput>* inputs);

/// True when the command can run with nothing typed: no required param.
bool runs_without_input(const QVector<ConnectorParam>& params);

// ── args_hint fallback (params unknown) ────────────────────────────────────

struct ArgField {
    QString name;          ///< argument name ("series_id", "start", "all")
    QString label;         ///< what the form shows ("series_id", "--start")
    /// "text" | "integer" | "number" | "date" | "bool" | "choice" | "list"
    QString type = QStringLiteral("text");
    bool required = false;
    bool named = false;    ///< "--name value" option (any order) instead of a positional
    bool named_eq = false; ///< written as "--name=value"
    bool flag = false;     ///< a named option without a value ("--all"): present when true
    bool variadic = false; ///< several values ("countries..."): split on spaces / commas
    QString default_value; ///< used when empty and a later positional is set
    QStringList choices;   ///< "a|b|c"
    QString placeholder;   ///< example text for the input
};

struct ArgsForm {
    QVector<ArgField> fields;
    bool structured = true; ///< false: show one free-text box with `hint` as placeholder
    QString hint;           ///< the args_hint it came from
    bool empty() const { return structured && fields.isEmpty(); }
};

/// Fields from an args_hint. An empty hint = a command without arguments.
ArgsForm args_form_from_hint(const QString& hint);

struct BuiltArgs {
    bool ok = false;
    QStringList args;
    QString error;    ///< plain sentence naming the field
    int field = -1;   ///< index of the field to focus on error
};

/// The argument list for `values` (one per field, same order; "" = not set; flags "true"/"").
BuiltArgs build_args(const ArgsForm& form, const QStringList& values);

/// The values that reproduce `args` in `form` (to show default_args / a remembered run in
/// the fields). Returns false when the args do not fit the form (use the text box instead).
bool values_from_args(const ArgsForm& form, const QStringList& args, QStringList* values);

/// Free-text arguments → list: a JSON object / array is one argument; several lines are one
/// argument each; otherwise split like a shell (double quotes group, "" inside quotes = ").
QStringList split_args_text(const QString& text);
/// The inverse for display (quotes arguments that contain spaces or quotes).
QString join_args_text(const QStringList& args);

/// True when a run with no arguments typed can succeed: every required field has a default.
bool runs_without_input(const ArgsForm& form);

} // namespace fincept::services::connectors
