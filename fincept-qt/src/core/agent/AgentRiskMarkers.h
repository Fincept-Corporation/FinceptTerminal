#pragma once
// AgentRiskMarkers.h — how a screen tells AI agents' UI tools what a control
// commits. Names only (header-only, Qt Core): screens set these, the MCP UI
// engine reads them (src/mcp/core/UiRisk.h documents the kinds and rules).
//
//   control->setProperty(kAgentRiskProperty, "order");              // this control
//   pane->setProperty(kAgentRiskProperty, "spend");                  // a pane …
//   pane->setProperty(kAgentRiskInheritProperty, true);              // … covering every control in it
//   editor->setProperty(kAgentRiskKeysProperty, "Ctrl+Return=code;Delete=delete");
//   view->setProperty(kAgentRiskColumnsProperty, "Action=order;5=delete");
//   model data(index, kAgentRiskRole) == "order"                     // one cell
//
// Kinds: order, money, spend, delete (= destructive), account, code, file.

#include <QtCore/qnamespace.h>

namespace fincept::core::agent {

/// The control's kind (string). On a QWidget, a QAction or a QShortcut.
inline constexpr const char* kAgentRiskProperty = "fincept_agent_risk";
/// true on a marked container: its kind also covers every unmarked control inside it.
inline constexpr const char* kAgentRiskInheritProperty = "fincept_agent_risk_inherit";
/// Keys handled in keyPressEvent: "Ctrl+Return=code;Delete=delete;F5=spend" (or a QVariantMap / QStringList of
/// the same pairs), on the widget or an ancestor of the focused widget.
inline constexpr const char* kAgentRiskKeysProperty = "fincept_agent_risk_keys";
/// Risky columns of an item view: "3=order;Action=delete" — logical column number or header text.
inline constexpr const char* kAgentRiskColumnsProperty = "fincept_agent_risk_columns";
/// Model data role: a cell's kind string (overrides nothing; adds risk to that cell).
inline constexpr int kAgentRiskRole = Qt::UserRole + 0x7A11;

} // namespace fincept::core::agent
