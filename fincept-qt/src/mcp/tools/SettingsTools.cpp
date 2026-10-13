// SettingsTools.cpp — Settings and LLM config management (Qt port)

#include "mcp/tools/SettingsTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "screens/settings/SettingsSecretMask.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/Database.h"

#include <QJsonArray>
#include <QVariantMap>

#include <algorithm>

namespace fincept::mcp::tools {

static constexpr const char* kSettingsToolsTag = "SettingsTools";

namespace settings_tools_detail {

/// Keys a tool loop must never write. The auth/session blob and the legacy
/// plaintext key are owned by AuthManager; security.* holds the auto-lock and
/// lock-on-minimize switches (an injected prompt must not be able to turn the
/// screen lock off); and secrets belong in SecureStorage via their own Settings
/// pages, never in the plaintext settings table.
QString write_refusal(const QString& key, const QString& category) {
    const QString k = key.trimmed().toLower();
    if (k == QLatin1String("fincept_session") || k == QLatin1String("fincept_api_key") ||
        category.trimmed().compare(QLatin1String("auth"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("'%1' is managed by the sign-in flow and cannot be set by a tool").arg(key);
    if (k.startsWith(QLatin1String("security.")) ||
        category.trimmed().compare(QLatin1String("security"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("'%1' is a security setting — change it in Settings → Security").arg(key);
    if (settings_secret_mask::is_secret_name(key))
        return QStringLiteral("'%1' looks like a credential — store secrets through their Settings page "
                              "(Credentials, Notifications, LLM Config), not as a plain setting")
            .arg(key);
    return {};
}

QJsonObject setting_row(const QString& key, const QString& value, const QString& category, const QString& updated_at,
                        int* masked_count) {
    bool masked = false;
    QJsonObject o{{"key", key},
                  {"value", settings_secret_mask::display_value_for_key(key, value, &masked)},
                  {"category", category},
                  {"updated_at", updated_at}};
    if (masked) {
        o["masked"] = true;
        if (masked_count)
            ++*masked_count;
    }
    return o;
}

} // namespace settings_tools_detail

std::vector<ToolDef> get_settings_tools() {
    std::vector<ToolDef> tools;

    // ── get_setting ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_setting";
        t.description = "Get an application setting by key. Secret-looking values (API keys, tokens, passwords, "
                        "webhook URLs) are withheld and reported as masked.";
        t.category = "settings";
        t.input_schema.properties =
            QJsonObject{{"key", QJsonObject{{"type", "string"}, {"description", "Setting key to retrieve"}}}};
        t.input_schema.required = {"key"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString key = args["key"].toString().trimmed();
            if (key.isEmpty())
                return ToolResult::fail("Missing 'key'");

            auto result = SettingsRepository::instance().get(key);
            if (result.is_err())
                return ToolResult::fail("Could not read setting '" + key + "': " +
                                        QString::fromStdString(result.error()));

            bool masked = false;
            const QString shown = settings_secret_mask::display_value_for_key(key, result.value(), &masked);
            QJsonObject out{{"key", key}, {"value", shown}};
            if (masked) {
                out["masked"] = true;
                return ToolResult::ok(QStringLiteral("Value withheld — it looks like a credential"), out);
            }
            return ToolResult::ok_data(out);
        };
        tools.push_back(std::move(t));
    }

    // ── set_setting ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "set_setting";
        t.description = "Set an application setting. Security settings, sign-in state and credentials cannot be "
                        "set through this tool.";
        t.category = "settings";
        // Phase 6.3: mutates persisted state.
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema.properties = QJsonObject{
            {"key", QJsonObject{{"type", "string"}, {"description", "Setting key"}}},
            {"value", QJsonObject{{"type", "string"}, {"description", "Setting value"}}},
            {"category", QJsonObject{{"type", "string"}, {"description", "Setting category (default: general)"}}}};
        t.input_schema.required = {"key", "value"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString key = args["key"].toString().trimmed();
            const QString value = args["value"].toString();
            const QString cat = args["category"].toString("general");

            if (key.isEmpty())
                return ToolResult::fail("Missing 'key'");
            const QString refusal = settings_tools_detail::write_refusal(key, cat);
            if (!refusal.isEmpty())
                return ToolResult::fail(refusal);

            auto r = SettingsRepository::instance().set(key, value, cat);
            if (r.is_err())
                return ToolResult::fail("Failed to save setting: " + QString::fromStdString(r.error()));

            EventBus::instance().publish("settings.changed", QVariantMap{{"key", key}, {"value", value}});

            // Key only: values never go to the log file (P14).
            LOG_INFO(kSettingsToolsTag, "Setting saved: " + key);
            return ToolResult::ok("Setting saved: " + key);
        };
        tools.push_back(std::move(t));
    }

    // ── get_all_settings ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_all_settings";
        t.description = "List stored application settings (key, value, category, updated_at), optionally for one "
                        "category (e.g. appearance, notifications, keybindings, general). Secret-looking values are "
                        "masked. Results are capped by 'limit'; 'total' reports how many exist.";
        t.category = "settings";
        t.input_schema.properties = QJsonObject{
            {"category", QJsonObject{{"type", "string"}, {"description", "Only this category (default: all)"}}},
            {"limit", QJsonObject{{"type", "integer"},
                                  {"description", "Max settings to return (default 200, max 1000)"},
                                  {"minimum", 1},
                                  {"maximum", 1000}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            // This used to read a fixed list of five categories ("general", "ui",
            // "trading", "market", "network") of which only "general" exists, so
            // "all settings" silently meant one category. Read the table itself.
            const QString category = args["category"].toString().trimmed();
            const int limit = std::clamp(args["limit"].toInt(200), 1, 1000);
            auto q_r = category.isEmpty()
                           ? Database::instance().execute(
                                 "SELECT key, value, category, updated_at FROM settings ORDER BY category, key")
                           : Database::instance().execute("SELECT key, value, category, updated_at FROM settings "
                                                          "WHERE category = ? ORDER BY key",
                                                          {category});
            if (q_r.is_err())
                return ToolResult::fail("Failed to read settings: " + QString::fromStdString(q_r.error()));
            auto& q = q_r.value();

            QJsonArray rows;
            int total = 0;
            int masked = 0;
            while (q.next()) {
                ++total;
                if (rows.size() >= limit)
                    continue; // keep counting so 'total' is real (never a silent cut)
                rows.append(settings_tools_detail::setting_row(q.value(0).toString(), q.value(1).toString(),
                                                               q.value(2).toString(), q.value(3).toString(), &masked));
            }
            const int returned = static_cast<int>(rows.size());
            QJsonObject out{{"settings", rows},
                            {"total", total},
                            {"returned", returned},
                            {"truncated", returned < total},
                            {"masked", masked}};
            if (returned < total)
                return ToolResult::ok(QStringLiteral("Showing %1 of %2 settings — pass a category or a larger limit")
                                          .arg(returned)
                                          .arg(total),
                                      out);
            return ToolResult::ok_data(out);
        };
        tools.push_back(std::move(t));
    }

    // ── get_llm_configs ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_llm_configs";
        t.description = "Get all configured LLM providers and their settings (API keys are not exposed).";
        t.category = "settings";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto configs = LlmConfigRepository::instance().list_providers();
            if (configs.is_err())
                return ToolResult::fail("Failed to load LLM configs: " + QString::fromStdString(configs.error()));

            QJsonArray result;
            for (const auto& c : configs.value()) {
                result.append(QJsonObject{{"provider", c.provider},
                                          {"model", c.model},
                                          {"is_active", c.is_active},
                                          {"has_api_key", !c.api_key.isEmpty()},
                                          {"base_url", c.base_url}});
            }
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── set_active_llm ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "set_active_llm";
        t.description = "Set the active LLM provider. Must be one of the supported providers.";
        t.category = "settings";
        // Phase 6.3: switching LLM provider mid-conversation is surprising;
        // requires explicit confirmation.
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
                             .string("provider", "Id of a CONFIGURED provider — see get_llm_configs (e.g. openai, "
                                                 "anthropic, ollama, groq, google, fincept)")
                             .required()
                             .length(1, 64)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString provider = args["provider"].toString().trimmed();
            if (provider.isEmpty())
                return ToolResult::fail("Missing 'provider'");

            // set_active() clears is_active on EVERY row and then sets it on the one
            // that matches — for a provider with no saved config the second UPDATE
            // matches nothing and still returns ok, leaving the terminal with NO active
            // LLM while this tool reported success. (The old fixed enum made that easy
            // to hit, and also hid every provider outside its six names.) Only switch
            // to a provider that exists.
            auto configs = LlmConfigRepository::instance().list_providers();
            if (configs.is_err())
                return ToolResult::fail("Failed to load LLM configs: " + QString::fromStdString(configs.error()));
            QString canonical;
            QStringList known;
            for (const auto& c : configs.value()) {
                known.append(c.provider);
                if (c.provider.compare(provider, Qt::CaseInsensitive) == 0)
                    canonical = c.provider;
            }
            if (canonical.isEmpty())
                return ToolResult::fail("Provider '" + provider + "' is not configured. Configured providers: " +
                                        (known.isEmpty() ? QStringLiteral("(none)") : known.join(", ")));
            provider = canonical;

            auto r = LlmConfigRepository::instance().set_active(provider);
            if (r.is_err())
                return ToolResult::fail("Failed to set active LLM: " + QString::fromStdString(r.error()));

            EventBus::instance().publish("llm.provider_changed", QVariantMap{{"provider", provider}});

            return ToolResult::ok("Active LLM set to: " + provider);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
