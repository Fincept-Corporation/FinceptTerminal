#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct LlmConfig {
    QString provider;
    QString api_key;
    QString base_url;
    QString model;
    bool is_active = false;
    bool tools_enabled = true;
    QString created_at;
    QString updated_at;
};

struct LlmGlobalSettings {
    double temperature = 0.7;
    int max_tokens = 4096;
    QString system_prompt;
};

struct LlmModelConfig {
    QString id;
    QString provider;
    QString model_id;
    QString display_name;
    QString api_key;
    QString base_url;
    bool is_enabled = true;
    bool is_default = false;
    QString created_at;
    QString updated_at;
};

class LlmConfigRepository : public BaseRepository<LlmConfig> {
  public:
    static LlmConfigRepository& instance();

    // Provider configs
    Result<QVector<LlmConfig>> list_providers();
    Result<LlmConfig> get_active_provider();
    Result<void> save_provider(const LlmConfig& c);
    Result<void> set_active(const QString& provider);
    Result<void> delete_provider(const QString& provider);

    // Global settings
    Result<LlmGlobalSettings> get_global_settings();
    Result<void> save_global_settings(const LlmGlobalSettings& s);

    // Custom models
    Result<QVector<LlmModelConfig>> list_models(const QString& provider = {});
    Result<void> save_model(const LlmModelConfig& m);
    Result<void> delete_model(const QString& id);

  private:
    LlmConfigRepository() = default;
    static LlmConfig map_config(QSqlQuery& q);
    static LlmModelConfig map_model(QSqlQuery& q);
};

} // namespace fincept
