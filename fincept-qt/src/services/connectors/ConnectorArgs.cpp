// ConnectorArgs.cpp — see ConnectorArgs.h.

#include "services/connectors/ConnectorArgs.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QRegularExpression>

#include <cmath>

namespace fincept::services::connectors {

namespace {

QString ca_tr(const char* text) {
    return QCoreApplication::translate("ConnectorArgs", text);
}

struct HintToken {
    QChar open; ///< '<', '[' or null for a bare word
    QString body;
};

/// Split a hint into bracketed / bare tokens. False when a bracket is not closed or nested.
bool ca_tokenize(const QString& hint, QVector<HintToken>* out) {
    const int n = int(hint.size());
    int i = 0;
    while (i < n) {
        const QChar c = hint.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }
        if (c == QLatin1Char('<') || c == QLatin1Char('[')) {
            const QChar close = c == QLatin1Char('<') ? QLatin1Char('>') : QLatin1Char(']');
            int j = i + 1;
            while (j < n && hint.at(j) != close) {
                if (hint.at(j) == QLatin1Char('<') || hint.at(j) == QLatin1Char('['))
                    return false; // nested — not a shape the form understands
                ++j;
            }
            if (j >= n)
                return false;
            out->append({c, hint.mid(i + 1, j - i - 1).trimmed()});
            i = j + 1;
        } else {
            int j = i;
            while (j < n && !hint.at(j).isSpace())
                ++j;
            out->append({QChar(), hint.mid(i, j - i)});
            i = j;
        }
    }
    return true;
}

bool ca_is_identifier_list(const QString& s) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_.\\-]+(\\|[A-Za-z0-9_.\\-]+)+$"));
    return re.match(s).hasMatch();
}

/// The input kind a field name suggests (only "integer" / "number" are validated).
QString ca_type_for(const QString& name, const QString& default_value) {
    const QString n = name.toLower();
    static const QStringList kInts = {QStringLiteral("limit"),     QStringLiteral("count"),      QStringLiteral("size"),
                                      QStringLiteral("page"),      QStringLiteral("page_size"),  QStringLiteral("per_page"),
                                      QStringLiteral("offset"),    QStringLiteral("max_results"), QStringLiteral("rows"),
                                      QStringLiteral("num"),       QStringLiteral("days"),       QStringLiteral("days_back"),
                                      QStringLiteral("top"),       QStringLiteral("year"),       QStringLiteral("start_year"),
                                      QStringLiteral("end_year"),  QStringLiteral("months"),     QStringLiteral("quarters")};
    static const QStringList kNums = {QStringLiteral("lat"),    QStringLiteral("lon"),    QStringLiteral("lat1"),
                                      QStringLiteral("lon1"),   QStringLiteral("lat2"),   QStringLiteral("lon2"),
                                      QStringLiteral("radius"), QStringLiteral("amount"), QStringLiteral("zoom"),
                                      QStringLiteral("seconds")};
    if (kInts.contains(n))
        return QStringLiteral("integer");
    if (kNums.contains(n))
        return QStringLiteral("number");
    const QString d = default_value.toLower();
    if (d == QLatin1String("true") || d == QLatin1String("false"))
        return QStringLiteral("choice");
    static const QRegularExpression kDate(QStringLiteral("(^|_)(date|start|end|since|until|from|to|date_from|date_to)($|_)"));
    if (n.contains(QLatin1String("date")) || kDate.match(n).hasMatch())
        return QStringLiteral("date");
    if (default_value.contains(QLatin1Char(',')))
        return QStringLiteral("list");
    return QStringLiteral("text");
}

bool ca_valid_value(const ArgField& f, const QString& v, QString* why) {
    if (f.type == QLatin1String("integer")) {
        static const QRegularExpression re(QStringLiteral("^[+-]?\\d+$"));
        if (!re.match(v).hasMatch()) {
            *why = ca_tr("%1 must be a whole number").arg(f.label);
            return false;
        }
    } else if (f.type == QLatin1String("number")) {
        bool ok = false;
        v.toDouble(&ok);
        if (!ok) {
            *why = ca_tr("%1 must be a number").arg(f.label);
            return false;
        }
    } else if (f.type == QLatin1String("choice") && !f.choices.isEmpty() && !f.choices.contains(v)) {
        *why = ca_tr("%1 must be one of: %2").arg(f.label, f.choices.join(QStringLiteral(", ")));
        return false;
    }
    return true;
}

} // namespace

ArgsForm args_form_from_hint(const QString& hint) {
    ArgsForm form;
    form.hint = hint.trimmed();
    if (form.hint.isEmpty())
        return form;
    QVector<HintToken> tokens;
    if (!ca_tokenize(form.hint, &tokens)) {
        form.structured = false;
        return form;
    }
    for (const HintToken& t : tokens) {
        QString body = t.body;
        if (body.isEmpty()) {
            form.structured = false;
            return form;
        }
        if (t.open.isNull() && body == QLatin1String("...")) {
            if (form.fields.isEmpty() || form.fields.last().named) {
                form.structured = false;
                return form;
            }
            form.fields.last().variadic = true;
            form.fields.last().type = QStringLiteral("list");
            continue;
        }
        ArgField f;
        f.required = t.open == QLatin1Char('<') || t.open.isNull();
        if (body.startsWith(QLatin1String("--"))) {
            // Named option: "--start ...", "--fields=a,b", "--rating aaa|all_ratings", "--all".
            f.named = true;
            f.required = false;
            static const QRegularExpression kSep(QStringLiteral("[\\s=]"));
            const int cut = int(body.indexOf(kSep));
            const QString name = cut < 0 ? body : body.left(cut);
            const QString value = cut < 0 ? QString() : body.mid(cut + 1).trimmed();
            f.named_eq = cut >= 0 && body.at(cut) == QLatin1Char('=');
            f.name = name.mid(2);
            f.label = name;
            if (f.name.isEmpty()) {
                form.structured = false;
                return form;
            }
            if (value.isEmpty()) {
                f.flag = true;
                f.type = QStringLiteral("bool");
            } else if (ca_is_identifier_list(value)) {
                f.type = QStringLiteral("choice");
                f.choices = value.split(QLatin1Char('|'));
            } else {
                f.type = ca_type_for(f.name, QString());
                if (value != QLatin1String("...") && value != QLatin1String("N"))
                    f.placeholder = value;
            }
            form.fields.append(f);
            continue;
        }
        if (body.contains(QLatin1Char(' '))) {
            // "name|postal_code|lat lon": one input cannot say how many arguments it is.
            form.structured = false;
            return form;
        }
        const int eq = int(body.indexOf(QLatin1Char('=')));
        QString name = eq < 0 ? body : body.left(eq);
        if (eq >= 0)
            f.default_value = body.mid(eq + 1);
        if (name.endsWith(QLatin1String("..."))) {
            f.variadic = true;
            name.chop(3);
            if (name.endsWith(QLatin1Char(',')))
                name.chop(1);
        }
        if (name.isEmpty()) {
            form.structured = false;
            return form;
        }
        f.name = name;
        f.label = name;
        if (f.variadic) {
            f.type = QStringLiteral("list");
        } else {
            f.type = ca_type_for(name, f.default_value);
            if (f.type == QLatin1String("choice"))
                f.choices = {QStringLiteral("true"), QStringLiteral("false")};
        }
        f.placeholder = !f.default_value.isEmpty() ? f.default_value
                        : f.type == QLatin1String("date") ? QStringLiteral("YYYY-MM-DD")
                        : f.variadic                     ? ca_tr("one or more, separated by spaces")
                                                         : QString();
        form.fields.append(f);
    }
    // A positional after a variadic one cannot be told apart from it.
    bool seen_variadic = false;
    for (const ArgField& f : form.fields) {
        if (f.named)
            continue;
        if (seen_variadic) {
            form.structured = false;
            form.fields.clear();
            return form;
        }
        seen_variadic = f.variadic;
    }
    return form;
}

// ─────────────────────────────────────────────────────────────────────────────
// Typed params (catalogue contract)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

struct KindName {
    ParamKind kind;
    const char* name;
};
constexpr KindName kKindNames[] = {{ParamKind::String, "string"}, {ParamKind::Int, "int"},       {ParamKind::Float, "float"},
                                   {ParamKind::Date, "date"},     {ParamKind::Enum, "enum"},     {ParamKind::Symbol, "symbol"},
                                   {ParamKind::Country, "country"}, {ParamKind::Bool, "bool"}};

/// The catalogue default as an argv string; *has = false for null (nothing sendable).
QString ca_default_text(const QJsonValue& d, bool* has) {
    *has = true;
    if (d.isString())
        return d.toString();
    if (d.isBool())
        return d.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (d.isDouble()) {
        const double v = d.toDouble();
        if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 1e15)
            return QString::number(static_cast<qint64>(v));
        return float_arg(v);
    }
    *has = false; // null, absent, or a list / object (not one argv item)
    return QString();
}

/// Items of a list value: commas / semicolons / new lines, and spaces for code-like kinds.
QStringList ca_list_items(const ConnectorParam& p, const QString& value) {
    static const QRegularExpression kCodes(QStringLiteral("[,;\\s]+"));
    static const QRegularExpression kText(QStringLiteral("[,;\\n]+"));
    const bool codes = p.kind != ParamKind::String && p.kind != ParamKind::Country;
    QStringList out;
    for (const QString& part : value.split(codes ? kCodes : kText, Qt::SkipEmptyParts))
        if (!part.trimmed().isEmpty())
            out << part.trimmed();
    return out;
}

} // namespace

QString param_kind_name(ParamKind kind) {
    for (const KindName& k : kKindNames)
        if (k.kind == kind)
            return QString::fromLatin1(k.name);
    return QStringLiteral("string");
}

bool param_kind_from_name(const QString& name, ParamKind* kind) {
    for (const KindName& k : kKindNames) {
        if (name == QLatin1String(k.name)) {
            *kind = k.kind;
            return true;
        }
    }
    *kind = ParamKind::String;
    return false;
}

bool ConnectorParam::is_list() const {
    const QString h = help.toLower();
    return h.contains(QLatin1String("comma-separated")) || h.contains(QLatin1String("comma separated")) ||
           default_value.contains(QLatin1Char(','));
}

ParamList parse_param_list(const QJsonValue& value) {
    ParamList out;
    if (value.isNull() || value.isUndefined())
        return out;
    auto reject = [&out](const QString& why) {
        out.params.clear();
        out.known = false;
        out.error = why;
        return out;
    };
    if (!value.isArray())
        return reject(ca_tr("params is not a list"));
    bool after_variadic = false;
    for (const QJsonValue& v : value.toArray()) {
        if (!v.isObject())
            return reject(ca_tr("a param is not an object"));
        const QJsonObject o = v.toObject();
        ConnectorParam p;
        p.name = o.value(QStringLiteral("name")).toString().trimmed();
        if (p.name.isEmpty())
            return reject(ca_tr("a param has no name"));
        p.required = o.value(QStringLiteral("required")).toBool(false);
        p.default_value = ca_default_text(o.value(QStringLiteral("default")), &p.has_default);
        if (p.required) { // the contract: no default on a required param
            p.has_default = false;
            p.default_value.clear();
        }
        if (!param_kind_from_name(o.value(QStringLiteral("kind")).toString(QStringLiteral("string")), &p.kind))
            return reject(ca_tr("param %1 has an unknown kind").arg(p.name));
        for (const QJsonValue& c : o.value(QStringLiteral("choices")).toArray())
            p.choices << (c.isString() ? c.toString() : c.toVariant().toString());
        if (p.kind == ParamKind::Enum && p.choices.isEmpty())
            return reject(ca_tr("enum param %1 has no choices").arg(p.name));
        p.help = o.value(QStringLiteral("help")).toString();
        p.named = o.value(QStringLiteral("named")).toBool(false);
        p.variadic = !p.named && o.value(QStringLiteral("variadic")).toBool(false);
        if (!p.named) {
            if (after_variadic)
                return reject(ca_tr("param %1 comes after a variadic one").arg(p.name));
            after_variadic = p.variadic;
        }
        out.params.append(p);
    }
    out.known = true;
    return out;
}

QString date_arg(const QDate& date) {
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

QString float_arg(double value) {
    if (!std::isfinite(value))
        return QString();
    QString s = QString::number(value, 'f', 10);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (s == QLatin1String("-0"))
        s = QStringLiteral("0");
    return s;
}

bool normalize_param_value(const ConnectorParam& p, const QString& value, QString* out, QString* why) {
    const QString s = value.trimmed();
    out->clear();
    if (s.isEmpty())
        return true;
    switch (p.kind) {
        case ParamKind::Int: {
            static const QRegularExpression re(QStringLiteral("^[+-]?\\d+$"));
            if (!re.match(s).hasMatch()) {
                *why = ca_tr("%1 must be a whole number").arg(p.name);
                return false;
            }
            *out = s.startsWith(QLatin1Char('+')) ? s.mid(1) : s;
            return true;
        }
        case ParamKind::Float: {
            static const QRegularExpression plain(QStringLiteral("^[+-]?(\\d+(\\.\\d*)?|\\.\\d+)$"));
            if (plain.match(s).hasMatch()) {
                QString t = s.startsWith(QLatin1Char('+')) ? s.mid(1) : s;
                if (t.startsWith(QLatin1Char('.')))
                    t.prepend(QLatin1Char('0'));
                else if (t.startsWith(QLatin1String("-.")))
                    t.insert(1, QLatin1Char('0'));
                if (t.endsWith(QLatin1Char('.')))
                    t.chop(1);
                *out = t;
                return true;
            }
            bool ok = false;
            const double d = s.toDouble(&ok); // "1e3" -> 1000 (C locale: never "1,5")
            if (!ok || !std::isfinite(d)) {
                *why = ca_tr("%1 must be a number, e.g. 2.5").arg(p.name);
                return false;
            }
            *out = float_arg(d);
            return true;
        }
        case ParamKind::Date: {
            const QDate d = QDate::fromString(s, QStringLiteral("yyyy-MM-dd"));
            if (s.size() != 10 || !d.isValid()) {
                *why = ca_tr("%1 must be a date written YYYY-MM-DD").arg(p.name);
                return false;
            }
            *out = s;
            return true;
        }
        case ParamKind::Enum: {
            if (p.choices.contains(s)) {
                *out = s;
                return true;
            }
            for (const QString& c : p.choices) {
                if (c.compare(s, Qt::CaseInsensitive) == 0) {
                    *out = c;
                    return true;
                }
            }
            *why = ca_tr("%1 must be one of: %2").arg(p.name, p.choices.join(QStringLiteral(", ")));
            return false;
        }
        case ParamKind::Bool: {
            const QString l = s.toLower();
            if (l == QLatin1String("true") || l == QLatin1String("1") || l == QLatin1String("yes") || l == QLatin1String("on") ||
                l == QLatin1String("y")) {
                *out = QStringLiteral("true");
                return true;
            }
            if (l == QLatin1String("false") || l == QLatin1String("0") || l == QLatin1String("no") || l == QLatin1String("off") ||
                l == QLatin1String("n")) {
                *out = QStringLiteral("false");
                return true;
            }
            *why = ca_tr("%1 must be true or false").arg(p.name);
            return false;
        }
        default:
            break; // text kinds below
    }
    // string / symbol / country: as typed; a list is normalised to "a,b,c".
    if (p.is_list() || p.variadic) {
        *out = ca_list_items(p, s).join(QLatin1Char(','));
        return true;
    }
    static const QRegularExpression space(QStringLiteral("\\s"));
    if (p.kind == ParamKind::Symbol && s.contains(space) && !p.help.contains(QLatin1String("free text"), Qt::CaseInsensitive)) {
        *why = ca_tr("%1 takes one symbol, without spaces").arg(p.name);
        return false;
    }
    *out = s;
    return true;
}

ArgvResult build_argv(const QString& command, const QVector<ConnectorParam>& params, const QVector<ParamInput>& inputs) {
    ArgvResult r;
    r.argv << command;
    const int n = int(params.size());
    QVector<QString> norm(n);
    QVector<bool> given(n, false);
    for (int i = 0; i < n; ++i) {
        const ParamInput in = inputs.value(i);
        if (!in.given || in.value.trimmed().isEmpty())
            continue;
        QString why;
        if (!normalize_param_value(params[i], in.value, &norm[i], &why)) {
            r.error = why;
            r.param = i;
            return r;
        }
        given[i] = !norm[i].isEmpty();
    }
    for (int i = 0; i < n; ++i) {
        if (params[i].required && !given[i]) {
            r.error = ca_tr("%1 is required").arg(params[i].name);
            r.param = i;
            return r;
        }
    }
    // Positionals up to the last one that must be sent: required, or given with a value other
    // than its default (sending a default, or leaving it out at the tail, is the same thing).
    QVector<int> pos;
    for (int i = 0; i < n; ++i)
        if (!params[i].named)
            pos << i;
    int last = -1;
    for (int k = 0; k < pos.size(); ++k) {
        const ConnectorParam& p = params[pos[k]];
        const bool differs = !(p.has_default && norm[pos[k]] == p.default_value);
        if (p.required || (given[pos[k]] && differs))
            last = k;
    }
    for (int k = 0; k <= last; ++k) {
        const int i = pos[k];
        const ConnectorParam& p = params[i];
        if (given[i]) {
            if (p.variadic)
                r.argv << ca_list_items(p, norm[i]);
            else
                r.argv << norm[i];
        } else if (p.has_default) {
            r.argv << p.default_value; // may be "" — the script reads an empty argument as "not given"
        } else {
            QString later;
            for (int kk = k + 1; kk <= last && later.isEmpty(); ++kk)
                if (given[pos[kk]] || params[pos[kk]].required)
                    later = params[pos[kk]].name;
            r.error = ca_tr("Set %1 as well: it has no default, and %2 comes after it (the arguments are positional)")
                          .arg(p.name, later);
            r.param = i;
            r.argv = QStringList{command};
            return r;
        }
    }
    // Named params: one item each, after the positionals.
    for (int i = 0; i < n; ++i) {
        const ConnectorParam& p = params[i];
        if (!p.named)
            continue;
        if (p.kind == ParamKind::Bool) {
            if (given[i] && norm[i] == QLatin1String("true"))
                r.argv << QStringLiteral("--") + p.name;
            continue;
        }
        if (!given[i] || (p.has_default && norm[i] == p.default_value))
            continue;
        r.argv << QStringLiteral("--%1=%2").arg(p.name, norm[i]);
    }
    r.ok = true;
    return r;
}

bool inputs_from_args(const QVector<ConnectorParam>& params, const QStringList& args, QVector<ParamInput>* inputs) {
    QVector<ParamInput> out(params.size());
    QVector<int> pos;
    for (int i = 0; i < params.size(); ++i)
        if (!params[i].named)
            pos << i;
    int next = 0;
    for (const QString& a : args) {
        if (a.startsWith(QLatin1String("--"))) {
            const int eq = int(a.indexOf(QLatin1Char('=')));
            const QString name = eq < 0 ? a.mid(2) : a.mid(2, eq - 2);
            int idx = -1;
            for (int i = 0; i < params.size() && idx < 0; ++i)
                if (params[i].named && params[i].name == name)
                    idx = i;
            if (idx < 0)
                return false;
            if (eq < 0) {
                if (params[idx].kind != ParamKind::Bool)
                    return false;
                out[idx] = ParamInput{true, QStringLiteral("true")};
            } else {
                out[idx] = ParamInput::of(a.mid(eq + 1));
            }
            continue;
        }
        if (next >= pos.size())
            return false;
        const int i = pos[next];
        if (params[i].variadic) {
            out[i].value = out[i].value.isEmpty() ? a : out[i].value + QLatin1Char(',') + a;
            out[i].given = !out[i].value.trimmed().isEmpty();
            continue; // the variadic param takes the rest
        }
        out[i] = ParamInput::of(a);
        ++next;
    }
    *inputs = out;
    return true;
}

bool runs_without_input(const QVector<ConnectorParam>& params) {
    for (const ConnectorParam& p : params)
        if (p.required)
            return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// args_hint fallback
// ─────────────────────────────────────────────────────────────────────────────

BuiltArgs build_args(const ArgsForm& form, const QStringList& values) {
    BuiltArgs out;
    if (!form.structured) {
        out.args = split_args_text(values.value(0));
        out.ok = true;
        return out;
    }
    // A missing required value is named first, before any "positional gap" message.
    for (int i = 0; i < form.fields.size(); ++i) {
        const ArgField& f = form.fields[i];
        if (f.required && f.default_value.isEmpty() && values.value(i).trimmed().isEmpty()) {
            out.error = ca_tr("%1 is required").arg(f.label);
            out.field = i;
            return out;
        }
    }
    // Positionals: everything up to the last one that is set or required must be passed.
    int last_needed = -1;
    for (int i = 0; i < form.fields.size(); ++i) {
        const ArgField& f = form.fields[i];
        if (f.named)
            continue;
        if (!values.value(i).trimmed().isEmpty() || f.required)
            last_needed = i;
    }
    for (int i = 0; i <= last_needed; ++i) {
        const ArgField& f = form.fields[i];
        if (f.named)
            continue;
        QString v = values.value(i).trimmed();
        if (v.isEmpty()) {
            if (!f.default_value.isEmpty()) {
                v = f.default_value;
            } else if (f.required) {
                out.error = ca_tr("%1 is required").arg(f.label);
                out.field = i;
                return out;
            } else {
                QString later;
                for (int j = i + 1; j <= last_needed && later.isEmpty(); ++j)
                    if (!form.fields[j].named && !values.value(j).trimmed().isEmpty())
                        later = form.fields[j].label;
                out.error = ca_tr("Set %1 as well: the arguments are positional and %2 comes after it")
                                .arg(f.label, later.isEmpty() ? ca_tr("a later one") : later);
                out.field = i;
                return out;
            }
        } else {
            QString why;
            if (!ca_valid_value(f, v, &why)) {
                out.error = why;
                out.field = i;
                return out;
            }
        }
        if (f.variadic) {
            static const QRegularExpression kSplit(QStringLiteral("[\\s,]+"));
            for (const QString& part : v.split(kSplit, Qt::SkipEmptyParts))
                out.args << part;
        } else {
            out.args << v;
        }
    }
    for (int i = 0; i < form.fields.size(); ++i) {
        const ArgField& f = form.fields[i];
        if (!f.named)
            continue;
        const QString v = values.value(i).trimmed();
        if (f.flag) {
            if (v == QLatin1String("true"))
                out.args << QStringLiteral("--") + f.name;
            continue;
        }
        if (v.isEmpty())
            continue;
        QString why;
        if (!ca_valid_value(f, v, &why)) {
            out.error = why;
            out.field = i;
            return out;
        }
        if (f.named_eq) {
            out.args << QStringLiteral("--%1=%2").arg(f.name, v);
        } else {
            out.args << QStringLiteral("--") + f.name;
            out.args << v;
        }
    }
    out.ok = true;
    return out;
}

bool values_from_args(const ArgsForm& form, const QStringList& args, QStringList* values) {
    QStringList vals;
    for (int i = 0; i < form.fields.size(); ++i)
        vals << QString();
    if (!form.structured) {
        vals = QStringList{join_args_text(args)};
        *values = vals;
        return true;
    }
    QVector<int> positional;
    for (int i = 0; i < form.fields.size(); ++i)
        if (!form.fields[i].named)
            positional << i;
    int next = 0;
    for (int a = 0; a < args.size(); ++a) {
        const QString& arg = args[a];
        if (arg.startsWith(QLatin1String("--"))) {
            const int eq = int(arg.indexOf(QLatin1Char('=')));
            const QString name = (eq < 0 ? arg : arg.left(eq)).mid(2);
            int idx = -1;
            for (int i = 0; i < form.fields.size(); ++i)
                if (form.fields[i].named && form.fields[i].name == name)
                    idx = i;
            if (idx < 0)
                return false;
            if (form.fields[idx].flag) {
                vals[idx] = QStringLiteral("true");
            } else if (eq >= 0) {
                vals[idx] = arg.mid(eq + 1);
            } else {
                if (a + 1 >= args.size())
                    return false;
                vals[idx] = args[++a];
            }
            continue;
        }
        if (next >= positional.size()) {
            if (!positional.isEmpty() && form.fields[positional.last()].variadic) {
                vals[positional.last()] += QLatin1Char(' ') + arg;
                continue;
            }
            return false;
        }
        const int idx = positional[next];
        vals[idx] = arg;
        if (!form.fields[idx].variadic)
            ++next;
    }
    *values = vals;
    return true;
}

QStringList split_args_text(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty())
        return {};
    if (t.startsWith(QLatin1Char('{')) || t.startsWith(QLatin1Char('[')))
        return {t}; // a JSON payload is one argument
    if (t.contains(QLatin1Char('\n'))) {
        QStringList out;
        for (const QString& line : t.split(QLatin1Char('\n')))
            if (!line.trimmed().isEmpty())
                out << line.trimmed();
        return out;
    }
    return QProcess::splitCommand(t);
}

QString join_args_text(const QStringList& args) {
    QStringList quoted;
    for (const QString& a : args) {
        if (a.isEmpty() || a.contains(QLatin1Char(' ')) || a.contains(QLatin1Char('"')))
            quoted << QLatin1Char('"') + QString(a).replace(QLatin1Char('"'), QStringLiteral("\"\"\"")) + QLatin1Char('"');
        else
            quoted << a;
    }
    return quoted.join(QLatin1Char(' '));
}

bool runs_without_input(const ArgsForm& form) {
    if (!form.structured)
        return !form.hint.contains(QLatin1Char('<'));
    for (const ArgField& f : form.fields)
        if (f.required && f.default_value.isEmpty())
            return false;
    return true;
}

} // namespace fincept::services::connectors
