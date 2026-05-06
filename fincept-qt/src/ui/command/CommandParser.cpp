#include "ui/command/CommandParser.h"

#include "core/actions/ActionRegistry.h"

#include <QRegularExpression>
#include <QStringList>

namespace fincept::ui {

namespace {

/// Splits on whitespace but preserves double-quoted strings as single tokens.
QStringList tokenise(const QString& s) {
    QStringList out;
    QString cur;
    bool in_quotes = false;
    for (QChar c : s) {
        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (c.isSpace() && !in_quotes) {
            if (!cur.isEmpty()) {
                out.append(cur);
                cur.clear();
            }
        } else {
            cur.append(c);
        }
    }
    if (!cur.isEmpty())
        out.append(cur);
    return out;
}

} // namespace

ParsedCommand CommandParser::parse(const QString& input) {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        ParsedCommand p;
        p.kind = ParsedCommand::Kind::Empty;
        return p;
    }
    if (trimmed == QLatin1String("?")) {
        ParsedCommand p;
        p.kind = ParsedCommand::Kind::Help;
        return p;
    }

    auto fc = try_function_code_(trimmed);
    if (fc.kind != ParsedCommand::Kind::Unknown && fc.kind != ParsedCommand::Kind::Empty)
        return fc;

    return try_verb_object_(trimmed);
}

ParsedCommand CommandParser::try_function_code_(const QString& input) {
    // Single token without dots — dotted forms go through verb-object.
    static const QRegularExpression rx(
        R"(^[A-Z0-9][A-Z0-9_]*$)",
        QRegularExpression::CaseInsensitiveOption);
    if (!rx.match(input).hasMatch())
        return ParsedCommand{ParsedCommand::Kind::Unknown, {}, {}, input, {}};

    const QString upper = input.toUpper();
    if (auto* action = ActionRegistry::instance().find(upper.toLower())) {
        ParsedCommand p;
        p.kind = ParsedCommand::Kind::Action;
        p.action_id = action->id;
        return p;
    }

    ParsedCommand p;
    p.kind = ParsedCommand::Kind::Symbol;
    p.raw_remainder = upper;
    p.args.insert(QStringLiteral("symbol"), upper);
    return p;
}

ParsedCommand CommandParser::try_verb_object_(const QString& input) {
    QStringList tokens = tokenise(input);
    if (tokens.isEmpty()) {
        return ParsedCommand{ParsedCommand::Kind::Empty, {}, {}, {}, {}};
    }

    // Greedy: longest prefix wins. "layout switch foo" → tries "layout.switch.foo", then "layout.switch", then "layout".
    auto& reg = ActionRegistry::instance();
    for (int n = tokens.size(); n > 0; --n) {
        QStringList prefix = tokens.mid(0, n);
        const QString candidate = prefix.join('.').toLower();
        if (auto* action = reg.find(candidate)) {
            ParsedCommand p;
            p.kind = ParsedCommand::Kind::Action;
            p.action_id = action->id;
            const QStringList rest = tokens.mid(n);
            p.args = bind_positional_(action->parameter_slots, rest);
            p.raw_remainder = rest.join(' ');
            return p;
        }
    }

    for (const QString& id : reg.all_ids()) {
        const ActionDef* def = reg.find(id);
        if (!def) continue;
        for (const QString& alias : def->aliases) {
            if (input.startsWith(alias, Qt::CaseInsensitive)) {
                const QStringList rest = tokenise(input.mid(alias.size()).trimmed());
                ParsedCommand p;
                p.kind = ParsedCommand::Kind::Action;
                p.action_id = id;
                p.args = bind_positional_(def->parameter_slots, rest);
                p.raw_remainder = rest.join(' ');
                return p;
            }
        }
    }

    ParsedCommand p;
    p.kind = ParsedCommand::Kind::Unknown;
    p.raw_remainder = input;
    p.error = QString("Unknown command: '%1'").arg(input);
    return p;
}

QVariantMap CommandParser::bind_positional_(const QList<ParameterSlot>& slot_list,
                                            const QStringList& positionals) {
    QVariantMap out;
    for (int i = 0; i < slot_list.size() && i < positionals.size(); ++i) {
        const ParameterSlot& slot = slot_list.at(i);
        out.insert(slot.name, positionals.at(i));
    }
    // Apply slot defaults. Handler validates required slots.
    for (int i = positionals.size(); i < slot_list.size(); ++i) {
        const ParameterSlot& slot = slot_list.at(i);
        if (slot.default_value.isValid())
            out.insert(slot.name, slot.default_value);
    }
    return out;
}

} // namespace fincept::ui
