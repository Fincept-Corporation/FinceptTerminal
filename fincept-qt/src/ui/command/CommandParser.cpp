#include "ui/command/CommandParser.h"

#include "core/actions/ActionRegistry.h"

#include <QRegularExpression>
#include <QStringList>

namespace fincept::ui {

namespace {

// Tokenise respecting double-quoted strings: `layout switch "Earnings Day"`
// → ["layout", "switch", "Earnings Day"].
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

    // Function-code precedence first. A bare uppercase token (with optional
    // dots/underscores/digits) is more likely a Bloomberg-style code or a
    // ticker than a verb. Verbs typically start lowercase or have a space.
    auto fc = try_function_code_(trimmed);
    if (fc.kind != ParsedCommand::Kind::Unknown && fc.kind != ParsedCommand::Kind::Empty)
        return fc;

    // Else fall through to verb-object.
    return try_verb_object_(trimmed);
}

ParsedCommand CommandParser::try_function_code_(const QString& input) {
    // Match: alphanumeric/digit-led single token, no spaces, no dots either
    // (a dotted form like "layout.switch" is the action-id form, handled
    // by verb-object). Examples that match: AAPL, MSFT, 1, 25, WEI.
    static const QRegularExpression rx(
        R"(^[A-Z0-9][A-Z0-9_]*$)",
        QRegularExpression::CaseInsensitiveOption);
    if (!rx.match(input).hasMatch())
        return ParsedCommand{ParsedCommand::Kind::Unknown, {}, {}, input, {}};

    // Two paths — try ActionRegistry first (for registered numeric/short
    // codes if any), else treat as a symbol publish.
    const QString upper = input.toUpper();
    if (auto* action = ActionRegistry::instance().find(upper.toLower())) {
        ParsedCommand p;
        p.kind = ParsedCommand::Kind::Action;
        p.action_id = action->id;
        return p;
    }

    // Treat as symbol. Caller maps to link.publish_to_group with the active
    // (or first enabled) group.
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

    // Greedy match: try the longest prefix as a dotted action id.
    // "layout switch foo" → try "layout.switch.foo", "layout.switch", "layout".
    // First registered match wins.
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

    // No prefix matched — also try matching aliases. Cheaper to walk all
    // actions once with their full-string display + alias forms.
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
    // Defaults for required slots that weren't supplied. Caller's predicate
    // / handler should detect missing required args.
    for (int i = positionals.size(); i < slot_list.size(); ++i) {
        const ParameterSlot& slot = slot_list.at(i);
        if (slot.default_value.isValid())
            out.insert(slot.name, slot.default_value);
    }
    return out;
}

} // namespace fincept::ui
