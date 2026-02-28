// Chat Session and Message Operations

use crate::database::{pool::get_pool, types::*};
use anyhow::Result;
use rusqlite::params;

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
