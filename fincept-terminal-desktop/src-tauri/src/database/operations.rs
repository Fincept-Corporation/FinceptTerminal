// Database Operations - All CRUD operations with optimized queries
// Part 1: Core operations (Settings, Credentials, LLM, Chat, DataSources)

use crate::database::{pool::get_pool, types::*};
use anyhow::Result;
use rusqlite::{params, OptionalExtension};

// ============================================================================
// Settings Operations
// ============================================================================

pub fn save_setting(key: &str, value: &str, category: Option<&str>) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO settings (setting_key, setting_value, category, updated_at)
         VALUES (?1, ?2, ?3, CURRENT_TIMESTAMP)",
        params![key, value, category],
    )?;

    Ok(())
}

pub fn get_setting(key: &str) -> Result<Option<String>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn
        .query_row(
            "SELECT setting_value FROM settings WHERE setting_key = ?1",
            params![key],
            |row| row.get(0),
        )
        .optional()?;

    Ok(result)
}

pub fn get_all_settings() -> Result<Vec<Setting>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare("SELECT setting_key, setting_value, category, updated_at FROM settings")?;
    let settings = stmt
        .query_map([], |row| {
            Ok(Setting {
                setting_key: row.get(0)?,
                setting_value: row.get(1)?,
                category: row.get(2)?,
                updated_at: row.get(3)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(settings)
}

// ============================================================================
// Credentials Operations
// ============================================================================

pub fn save_credential(cred: &Credential) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO credentials
         (service_name, username, password, api_key, api_secret, additional_data, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, CURRENT_TIMESTAMP)",
        params![
            cred.service_name,
            cred.username,
            cred.password,
            cred.api_key,
            cred.api_secret,
            cred.additional_data,
        ],
    )?;

    Ok(OperationResult {
        success: true,
        message: "Credential saved successfully".to_string(),
    })
}

pub fn get_credentials() -> Result<Vec<Credential>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, service_name, username, password, api_key, api_secret, additional_data, created_at, updated_at
         FROM credentials ORDER BY service_name"
    )?;

    let credentials = stmt
        .query_map([], |row| {
            Ok(Credential {
                id: row.get(0)?,
                service_name: row.get(1)?,
                username: row.get(2)?,
                password: row.get(3)?,
                api_key: row.get(4)?,
                api_secret: row.get(5)?,
                additional_data: row.get(6)?,
                created_at: row.get(7)?,
                updated_at: row.get(8)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(credentials)
}

pub fn get_credential_by_service(service_name: &str) -> Result<Option<Credential>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn
        .query_row(
            "SELECT id, service_name, username, password, api_key, api_secret, additional_data, created_at, updated_at
             FROM credentials WHERE service_name = ?1",
            params![service_name],
            |row| {
                Ok(Credential {
                    id: row.get(0)?,
                    service_name: row.get(1)?,
                    username: row.get(2)?,
                    password: row.get(3)?,
                    api_key: row.get(4)?,
                    api_secret: row.get(5)?,
                    additional_data: row.get(6)?,
                    created_at: row.get(7)?,
                    updated_at: row.get(8)?,
                })
            },
        )
        .optional()?;

    Ok(result)
}

pub fn delete_credential(id: i64) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM credentials WHERE id = ?1", params![id])?;

    Ok(OperationResult {
        success: true,
        message: "Credential deleted successfully".to_string(),
    })
}

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

// ============================================================================
// Chat Operations
// ============================================================================

pub fn create_chat_session(title: &str) -> Result<ChatSession> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let session_uuid = uuid::Uuid::new_v4().to_string();

    conn.execute(
        "INSERT INTO chat_sessions (session_uuid, title) VALUES (?1, ?2)",
        params![session_uuid, title],
    )?;

    let session = conn.query_row(
        "SELECT session_uuid, title, message_count, created_at, updated_at
         FROM chat_sessions WHERE session_uuid = ?1",
        params![session_uuid],
        |row| {
            Ok(ChatSession {
                session_uuid: row.get(0)?,
                title: row.get(1)?,
                message_count: row.get(2)?,
                created_at: row.get(3)?,
                updated_at: row.get(4)?,
            })
        },
    )?;

    Ok(session)
}

pub fn get_chat_sessions(limit: Option<i64>) -> Result<Vec<ChatSession>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let query = if let Some(lim) = limit {
        format!(
            "SELECT session_uuid, title, message_count, created_at, updated_at
             FROM chat_sessions ORDER BY updated_at DESC LIMIT {}",
            lim
        )
    } else {
        "SELECT session_uuid, title, message_count, created_at, updated_at
         FROM chat_sessions ORDER BY updated_at DESC"
            .to_string()
    };

    let mut stmt = conn.prepare(&query)?;
    let sessions = stmt
        .query_map([], |row| {
            Ok(ChatSession {
                session_uuid: row.get(0)?,
                title: row.get(1)?,
                message_count: row.get(2)?,
                created_at: row.get(3)?,
                updated_at: row.get(4)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(sessions)
}

pub fn add_chat_message(msg: &ChatMessage) -> Result<ChatMessage> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT INTO chat_messages (id, session_uuid, role, content, provider, model, tokens_used)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
        params![
            msg.id,
            msg.session_uuid,
            msg.role,
            msg.content,
            msg.provider,
            msg.model,
            msg.tokens_used,
        ],
    )?;

    // Update message count
    conn.execute(
        "UPDATE chat_sessions SET message_count = message_count + 1, updated_at = CURRENT_TIMESTAMP
         WHERE session_uuid = ?1",
        params![msg.session_uuid],
    )?;

    let result = conn.query_row(
        "SELECT id, session_uuid, role, content, timestamp, provider, model, tokens_used
         FROM chat_messages WHERE id = ?1",
        params![msg.id],
        |row| {
            Ok(ChatMessage {
                id: row.get(0)?,
                session_uuid: row.get(1)?,
                role: row.get(2)?,
                content: row.get(3)?,
                timestamp: row.get(4)?,
                provider: row.get(5)?,
                model: row.get(6)?,
                tokens_used: row.get(7)?,
            })
        },
    )?;

    Ok(result)
}

pub fn get_chat_messages(session_uuid: &str) -> Result<Vec<ChatMessage>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, session_uuid, role, content, timestamp, provider, model, tokens_used
         FROM chat_messages WHERE session_uuid = ?1 ORDER BY timestamp ASC"
    )?;

    let messages = stmt
        .query_map(params![session_uuid], |row| {
            Ok(ChatMessage {
                id: row.get(0)?,
                session_uuid: row.get(1)?,
                role: row.get(2)?,
                content: row.get(3)?,
                timestamp: row.get(4)?,
                provider: row.get(5)?,
                model: row.get(6)?,
                tokens_used: row.get(7)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(messages)
}

pub fn delete_chat_session(session_uuid: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "DELETE FROM chat_sessions WHERE session_uuid = ?1",
        params![session_uuid],
    )?;

    Ok(())
}

// ============================================================================
// Data Sources Operations
// ============================================================================

pub fn save_data_source(source: &DataSource) -> Result<OperationResultWithId> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO data_sources
         (id, alias, display_name, description, type, provider, category, config, enabled, tags, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, CURRENT_TIMESTAMP)",
        params![
            source.id,
            source.alias,
            source.display_name,
            source.description,
            source.ds_type,
            source.provider,
            source.category,
            source.config,
            if source.enabled { 1 } else { 0 },
            source.tags,
        ],
    )?;

    Ok(OperationResultWithId {
        success: true,
        message: "Data source saved successfully".to_string(),
        id: Some(source.id.clone()),
    })
}

pub fn get_all_data_sources() -> Result<Vec<DataSource>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, alias, display_name, description, type, provider, category, config, enabled, tags, created_at, updated_at
         FROM data_sources ORDER BY display_name"
    )?;

    let sources = stmt
        .query_map([], |row| {
            Ok(DataSource {
                id: row.get(0)?,
                alias: row.get(1)?,
                display_name: row.get(2)?,
                description: row.get(3)?,
                ds_type: row.get(4)?,
                provider: row.get(5)?,
                category: row.get(6)?,
                config: row.get(7)?,
                enabled: row.get::<_, i32>(8)? != 0,
                tags: row.get(9)?,
                created_at: row.get(10)?,
                updated_at: row.get(11)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(sources)
}

pub fn delete_data_source(id: &str) -> Result<OperationResult> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM data_sources WHERE id = ?1", params![id])?;

    Ok(OperationResult {
        success: true,
        message: "Data source deleted successfully".to_string(),
    })
}
