#pragma once
#include "core/actions/ActionDef.h"

#include <QString>
#include <QVariantMap>

namespace fincept::ui {

/// Phase 9 / decision 7.2: dual-grammar command parser.
///
/// Two parses tried in priority order:
///   1. **Function-code precedence**: input matching `^[A-Z][A-Z0-9._]*$`
///      or starting with a digit (Bloomberg "1<GO>", "WEI") is treated
///      as a function code. We resolve via ActionRegistry::match() and
///      symbol lookup. (For v1 we treat un-claimed-by-action input as
///      a symbol publish via `link.publish_to_group` to the active group.)
///   2. **Verb-object**: `<verb> [args...]`. The verb is the action id
///      ("layout switch") with spaces collapsed to dots
///      ("layout.switch"); remaining tokens become positional args
///      mapped to the action's ParameterSlot list.
///
/// The parser returns a `Parse` describing which path matched, the
/// resolved action_id, and the bag of args ready to feed `CommandContext`.
/// Callers (CommandBar / CommandPalette) call `ActionRegistry::invoke`.
struct ParsedCommand {
    enum class Kind {
        Empty,
        Action,         ///< matched a registered action; invoke directly
        Symbol,         ///< looks like a ticker — caller may route to link.publish_to_group
        Help,           ///< user typed `?` → caller shows the cheat sheet
        Unknown,
    };
    Kind kind = Kind::Empty;
    QString action_id;
    QVariantMap args;
    QString raw_remainder; ///< text after the verb (or the whole input for Symbol)
    QString error;         ///< populated when kind==Unknown to surface a hint
};

class CommandParser {
  public:
    /// Pure function. Reads ActionRegistry to resolve verb-object matches.
    /// Does NOT invoke the action.
    static ParsedCommand parse(const QString& input);

  private:
    static ParsedCommand try_function_code_(const QString& input);
    static ParsedCommand try_verb_object_(const QString& input);

    /// Map positional tokens to parameter slot names. The first N tokens
    /// fill the first N slots in declaration order.
    static QVariantMap bind_positional_(const QList<ParameterSlot>& slot_list,
                                        const QStringList& positionals);
};

} // namespace fincept::ui
