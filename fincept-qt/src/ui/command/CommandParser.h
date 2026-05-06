#pragma once
#include "core/actions/ActionDef.h"

#include <QString>
#include <QVariantMap>

namespace fincept::ui {

/// Dual-grammar command parser.
/// Tries function-code (^[A-Z][A-Z0-9._]*$ or leading digit) first, then verb-object ("layout switch" → "layout.switch").
/// Pure — does not invoke. Caller dispatches via ActionRegistry::invoke.
struct ParsedCommand {
    enum class Kind {
        Empty,
        Action,
        Symbol,    ///< ticker-shaped — caller may route to link.publish_to_group
        Help,      ///< user typed `?`
        Unknown,
    };
    Kind kind = Kind::Empty;
    QString action_id;
    QVariantMap args;
    QString raw_remainder;
    QString error;
};

class CommandParser {
  public:
    static ParsedCommand parse(const QString& input);

  private:
    static ParsedCommand try_function_code_(const QString& input);
    static ParsedCommand try_verb_object_(const QString& input);

    /// First N positionals fill the first N slots in declaration order.
    static QVariantMap bind_positional_(const QList<ParameterSlot>& slot_list,
                                        const QStringList& positionals);
};

} // namespace fincept::ui
