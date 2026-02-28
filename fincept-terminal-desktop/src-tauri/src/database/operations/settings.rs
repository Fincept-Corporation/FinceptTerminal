// Settings, Credentials, and Data Source Operations

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
