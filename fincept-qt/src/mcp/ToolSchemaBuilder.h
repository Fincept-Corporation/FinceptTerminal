#pragma once
// ToolSchemaBuilder.h — Fluent helper for declaring tool input schemas.
//
// Phase 3 of the MCP refactor. See plans/mcp-refactor-phase-3-schema-validation.md.
//
// Usage:
//   t.input_schema = ToolSchemaBuilder()
//       .string("symbol", "Ticker symbol (e.g. AAPL, BTC-USD)").required().pattern("^[A-Z0-9._-]{1,16}$")
//       .string("category", "News category").default_str("ALL").enums({"ALL","MARKETS","EARNINGS"})
//       .integer("limit", "Max articles").default_int(20).between(1, 100)
//       .build();
//
// The builder targets the structured `params` field of ToolSchema. Tools
// using the legacy `QJsonObject properties` shape continue to work — see
// the comment block in McpTypes.h for the dual-shape contract.

#include "mcp/McpTypes.h"

#include <QString>
#include <QStringList>

namespace fincept::mcp {

class ToolSchemaBuilder {
  public:
    ToolSchemaBuilder() = default;

    // ── Param starters ──────────────────────────────────────────────────
    // Each call adds a new ToolParam keyed by `name` and makes it the active
    // param so subsequent constraint methods (.required(), .enums(), etc.)
    // apply to it.

    ToolSchemaBuilder& string(const QString& name, const QString& description = {}) {
        return add(name, "string", description);
    }
    ToolSchemaBuilder& integer(const QString& name, const QString& description = {}) {
        return add(name, "integer", description);
    }
    ToolSchemaBuilder& number(const QString& name, const QString& description = {}) {
        return add(name, "number", description);
    }
    ToolSchemaBuilder& boolean(const QString& name, const QString& description = {}) {
        return add(name, "boolean", description);
    }
    ToolSchemaBuilder& array(const QString& name, const QString& description = {},
                              const QJsonObject& items = {}) {
        add(name, "array", description);
        if (!items.isEmpty())
            current().items = items;
        return *this;
    }
    ToolSchemaBuilder& object(const QString& name, const QString& description = {}) {
        return add(name, "object", description);
    }

    // ── Constraints (apply to the most recently added param) ────────────

    ToolSchemaBuilder& required() {
        current().required = true;
        return *this;
    }
    ToolSchemaBuilder& default_str(const QString& v) {
        current().default_value = QJsonValue(v);
        return *this;
    }
    ToolSchemaBuilder& default_int(int v) {
        current().default_value = QJsonValue(v);
        return *this;
    }
    ToolSchemaBuilder& default_num(double v) {
        current().default_value = QJsonValue(v);
        return *this;
    }
    ToolSchemaBuilder& default_bool(bool v) {
        current().default_value = QJsonValue(v);
        return *this;
    }
    ToolSchemaBuilder& enums(const QStringList& values) {
        current().enum_values = values;
        return *this;
    }
    ToolSchemaBuilder& between(double min_v, double max_v) {
        current().minimum = min_v;
        current().maximum = max_v;
        return *this;
    }
    ToolSchemaBuilder& min(double v) {
        current().minimum = v;
        return *this;
    }
    ToolSchemaBuilder& max(double v) {
        current().maximum = v;
        return *this;
    }
    ToolSchemaBuilder& length(int min_len, int max_len) {
        current().min_length = min_len;
        current().max_length = max_len;
        return *this;
    }
    ToolSchemaBuilder& pattern(const QString& regex) {
        current().pattern = regex;
        return *this;
    }
    ToolSchemaBuilder& description(const QString& d) {
        current().description = d;
        return *this;
    }

    // ── Finalise ────────────────────────────────────────────────────────

    ToolSchema build() && { return std::move(schema_); }
    ToolSchema build() const& { return schema_; }

  private:
    ToolSchema schema_;
    QString last_key_;

    ToolSchemaBuilder& add(const QString& name, const QString& type, const QString& description) {
        ToolParam p;
        p.type = type;
        p.description = description;
        schema_.params.insert(name, p);
        last_key_ = name;
        return *this;
    }

    ToolParam& current() {
        // Programming error if no param has been added yet — fail fast in
        // debug builds rather than silently mutating a default-constructed
        // sentinel that's never read.
        Q_ASSERT(!last_key_.isEmpty() && schema_.params.contains(last_key_));
        return schema_.params[last_key_];
    }
};

} // namespace fincept::mcp
