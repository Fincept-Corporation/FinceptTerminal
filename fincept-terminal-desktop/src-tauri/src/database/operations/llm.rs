// LLM Config and Model Config Operations

use crate::database::{pool::get_pool, types::*};
use anyhow::Result;
use rusqlite::params;

// ============================================================================
// LLM Config Operations
// ============================================================================

pub fn get_llm_configs() -> Result<Vec<LLMConfig>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT provider, api_key, base_url, model, is_active, created_at, updated_at
         FROM llm_configs"
    )?;

    let configs = stmt
        .query_map([], |row| {
            Ok(LLMConfig {
                provider: row.get(0)?,
                api_key: row.get(1)?,
                base_url: row.get(2)?,
                model: row.get(3)?,
                is_active: row.get::<_, i32>(4)? != 0,
                created_at: row.get(5)?,
                updated_at: row.get(6)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(configs)
}

pub fn save_llm_config(config: &LLMConfig) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO llm_configs (provider, api_key, base_url, model, is_active, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, CURRENT_TIMESTAMP)",
        params![
            config.provider,
            config.api_key,
            config.base_url,
            config.model,
            if config.is_active { 1 } else { 0 },
        ],
    )?;

    Ok(())
}

pub fn get_llm_global_settings() -> Result<LLMGlobalSettings> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn.query_row(
        "SELECT temperature, max_tokens, system_prompt FROM llm_global_settings WHERE id = 1",
        [],
        |row| {
            Ok(LLMGlobalSettings {
                temperature: row.get(0)?,
                max_tokens: row.get(1)?,
                system_prompt: row.get(2)?,
            })
        },
    )?;

    Ok(result)
}

pub fn save_llm_global_settings(settings: &LLMGlobalSettings) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "UPDATE llm_global_settings SET temperature = ?1, max_tokens = ?2, system_prompt = ?3 WHERE id = 1",
        params![settings.temperature, settings.max_tokens, settings.system_prompt],
    )?;

    Ok(())
}

pub fn set_active_llm_provider(provider: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("UPDATE llm_configs SET is_active = 0", [])?;

    conn.execute(
        "UPDATE llm_configs SET is_active = 1 WHERE provider = ?1",
        params![provider],
    )?;

    Ok(())
}

// ============================================================================
// LLM Model Config Operations
// ============================================================================

pub fn get_llm_model_configs() -> Result<Vec<LLMModelConfig>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, provider, model_id, display_name, api_key, base_url, is_enabled, is_default, created_at, updated_at
         FROM llm_model_configs"
    )?;

    let configs = stmt
        .query_map([], |row| {
            Ok(LLMModelConfig {
                id: row.get(0)?,
                provider: row.get(1)?,
                model_id: row.get(2)?,
                display_name: row.get(3)?,
                api_key: row.get(4)?,
                base_url: row.get(5)?,
                is_enabled: row.get::<_, i32>(6)? != 0,
                is_default: row.get::<_, i32>(7)? != 0,
                created_at: row.get(8)?,
                updated_at: row.get(9)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(configs)
}

pub fn save_llm_model_config(config: &LLMModelConfig) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO llm_model_configs (id, provider, model_id, display_name, api_key, base_url, is_enabled, is_default, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, CURRENT_TIMESTAMP)",
        params![
            config.id,
            config.provider,
            config.model_id,
            config.display_name,
            config.api_key,
            config.base_url,
            if config.is_enabled { 1 } else { 0 },
            if config.is_default { 1 } else { 0 },
        ],
    )?;

    Ok(OperationResult {
        success: true,
        message: "Model configuration saved".to_string(),
    })
}

pub fn delete_llm_model_config(id: &str) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let rows_affected = conn.execute(
        "DELETE FROM llm_model_configs WHERE id = ?1",
        params![id],
    )?;

    if rows_affected > 0 {
        Ok(OperationResult { success: true, message: "Model configuration deleted".to_string() })
    } else {
        Ok(OperationResult { success: false, message: "Model configuration not found".to_string() })
    }
}

pub fn toggle_llm_model_config_enabled(id: &str) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let rows_affected = conn.execute(
        "UPDATE llm_model_configs SET is_enabled = NOT is_enabled, updated_at = CURRENT_TIMESTAMP WHERE id = ?1",
        params![id],
    )?;

    if rows_affected > 0 {
        Ok(OperationResult { success: true, message: "Model enabled state toggled".to_string() })
    } else {
        Ok(OperationResult { success: false, message: "Model configuration not found".to_string() })
    }
}

pub fn update_llm_model_id(id: &str, new_model_id: &str) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let rows_affected = conn.execute(
        "UPDATE llm_model_configs SET model_id = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2",
        params![new_model_id, id],
    )?;

    if rows_affected > 0 {
        Ok(OperationResult { success: true, message: format!("Model ID updated to: {}", new_model_id) })
    } else {
        Ok(OperationResult { success: false, message: "Model configuration not found".to_string() })
    }
}

pub fn fix_google_model_ids() -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let rows_affected = conn.execute(
        "UPDATE llm_model_configs
         SET model_id = REPLACE(REPLACE(REPLACE(model_id, 'gemini/', ''), 'google/', ''), 'models/', ''),
             updated_at = CURRENT_TIMESTAMP
         WHERE (provider = 'google' OR provider = 'gemini')
         AND (model_id LIKE 'gemini/%' OR model_id LIKE 'google/%' OR model_id LIKE 'models/%')",
        [],
    )?;

    conn.execute(
        "UPDATE llm_model_configs
         SET model_id = 'gemini-1.5-flash',
             updated_at = CURRENT_TIMESTAMP
         WHERE (provider = 'google' OR provider = 'gemini')
         AND model_id NOT LIKE 'gemini-%'
         AND model_id != ''",
        [],
    )?;

    Ok(OperationResult {
        success: true,
        message: format!("Fixed {} Google model configurations", rows_affected),
    })
}
