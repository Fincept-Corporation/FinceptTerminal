// Settings MCP Tools — settings and LLM config management

#include "settings_tools.h"
#include "../../storage/database.h"
#include "../../core/event_bus.h"
#include "../../core/logger.h"

namespace fincept::mcp::tools {

static constexpr const char* TAG_SETTINGS = "SettingsTools";

std::vector<ToolDef> get_settings_tools() {
    std::vector<ToolDef> tools;

    // ── get_setting ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_setting";
        t.description = "Get an application setting by key.";
        t.category = "settings";
        t.input_schema.properties = {
            {"key", {
                {"type", "string"},
                {"description", "Setting key to retrieve"}
            }}
        };
        t.input_schema.required = {"key"};
        t.handler = [](const json& args) -> ToolResult {
            std::string key = args.value("key", "");
            if (key.empty()) return ToolResult::fail("Missing 'key'");

            auto val = db::ops::get_setting(key);
            if (!val.has_value()) {
                return ToolResult::fail("Setting not found: " + key);
            }
            return ToolResult::ok_data({{"key", key}, {"value", *val}});
        };
        tools.push_back(std::move(t));
    }

    // ── set_setting ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "set_setting";
        t.description = "Set an application setting.";
        t.category = "settings";
        t.input_schema.properties = {
            {"key", {
                {"type", "string"},
                {"description", "Setting key"}
            }},
            {"value", {
                {"type", "string"},
                {"description", "Setting value"}
            }},
            {"category", {
                {"type", "string"},
                {"description", "Setting category (default: general)"}
            }}
        };
        t.input_schema.required = {"key", "value"};
        t.handler = [](const json& args) -> ToolResult {
            std::string key = args.value("key", "");
            std::string value = args.value("value", "");
            std::string category = args.value("category", "general");

            if (key.empty()) return ToolResult::fail("Missing 'key'");

            db::ops::save_setting(key, value, category);

            core::EventBus::instance().publish("settings.changed", {
                {"key", key}, {"value", value}
            });

            LOG_INFO(TAG_SETTINGS, "Setting saved: %s = %s", key.c_str(), value.c_str());
            return ToolResult::ok("Setting saved: " + key);
        };
        tools.push_back(std::move(t));
    }

    // ── get_all_settings ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_all_settings";
        t.description = "Get all application settings.";
        t.category = "settings";
        t.handler = [](const json&) -> ToolResult {
            auto settings = db::ops::get_all_settings();
            json result = json::array();
            for (const auto& s : settings) {
                result.push_back({
                    {"key", s.key},
                    {"value", s.value},
                    {"category", s.category},
                    {"updated_at", s.updated_at}
                });
            }
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── get_llm_configs ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_llm_configs";
        t.description = "Get all configured LLM providers and their settings.";
        t.category = "settings";
        t.handler = [](const json&) -> ToolResult {
            auto configs = db::ops::get_llm_configs();
            json result = json::array();
            for (const auto& c : configs) {
                result.push_back({
                    {"provider", c.provider},
                    {"model", c.model},
                    {"is_active", c.is_active},
                    {"has_api_key", c.api_key.has_value() && !c.api_key->empty()},
                    {"base_url", c.base_url.value_or("")}
                });
            }
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── set_active_llm ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "set_active_llm";
        t.description = "Set the active LLM provider.";
        t.category = "settings";
        t.input_schema.properties = {
            {"provider", {
                {"type", "string"},
                {"description", "Provider name (openai, anthropic, ollama, groq, google)"}
            }}
        };
        t.input_schema.required = {"provider"};
        t.handler = [](const json& args) -> ToolResult {
            std::string provider = args.value("provider", "");
            if (provider.empty()) return ToolResult::fail("Missing 'provider'");

            db::ops::set_active_llm_provider(provider);

            core::EventBus::instance().publish("llm.provider_changed", {
                {"provider", provider}
            });

            return ToolResult::ok("Active LLM set to: " + provider);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
