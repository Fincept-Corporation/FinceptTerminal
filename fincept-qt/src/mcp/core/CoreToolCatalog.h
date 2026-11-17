#pragma once
// CoreToolCatalog.h — the names, descriptions and input schemas of the UI core
// tools (ui_*), without their handlers: CoreTools.cpp binds the handlers, and
// the unit tests check the catalogue (schema rules for strict clients, the byte
// budget) without the application. Qt Core only.
//
// Descriptions are written for the model and kept short: the whole catalogue
// rides on every request of every turn, so each word is paid for repeatedly.
// A description says what the tool is for and what is not obvious from the
// schema; parameter descriptions never restate their enum.

#include "mcp/McpTypes.h"

#include <QString>

#include <vector>

namespace fincept::mcp::core {

struct UiToolSpec {
    QString name;
    const char* category = nullptr; // kCategory* (CoreTools.h)
    QString description;
    ToolSchema schema;
    int timeout_ms = 15000;
};

/// The nine ui_* tools, in catalogue order (python and report are added by CoreTools.cpp).
std::vector<UiToolSpec> ui_tool_specs();

} // namespace fincept::mcp::core
