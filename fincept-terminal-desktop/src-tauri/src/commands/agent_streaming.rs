// Agent Streaming Module - High-performance streaming agent execution
// Uses Tauri events for real-time token streaming and parallel execution

use crate::utils::python::get_script_path;
use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Emitter};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};

/// Stream chunk sent to frontend
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StreamChunk {
    pub stream_id: String,
    pub chunk_type: String,  // "token", "thinking", "tool_call", "tool_result", "done", "error"
    pub content: String,
    pub metadata: Option<serde_json::Value>,
    pub sequence: u64,
}

/// Stream status for tracking
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StreamStatus {
    pub stream_id: String,
    pub status: String,  // "running", "completed", "error", "cancelled"
    pub tokens_generated: u64,
    pub elapsed_ms: u64,
}

/// Parallel execution result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParallelResult {
    pub task_id: String,
    pub success: bool,
    pub result: Option<serde_json::Value>,
    pub error: Option<String>,
    pub execution_time_ms: u64,
}

/// Batch execution response
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BatchResponse {
    pub batch_id: String,
    pub results: Vec<ParallelResult>,
    pub total_time_ms: u64,
    pub successful_count: usize,
    pub failed_count: usize,
}

// Global stream registry for cancellation
use once_cell::sync::Lazy;
use std::collections::HashMap;
use tokio::sync::RwLock;

static ACTIVE_STREAMS: Lazy<RwLock<HashMap<String, Arc<AtomicBool>>>> =
    Lazy::new(|| RwLock::new(HashMap::new()));

/// Execute agent with streaming - tokens emitted via Tauri events
///
/// Frontend listens to: "agent-stream-{stream_id}"
/// Events: StreamChunk for each token/thinking step
#[tauri::command]
pub async fn execute_agent_streaming(
    app: AppHandle,
    payload: serde_json::Value,
) -> Result<String, String> {
    let stream_id = uuid::Uuid::new_v4().to_string();
    let start_time = std::time::Instant::now();

    // Register stream for cancellation
    let cancel_flag = Arc::new(AtomicBool::new(false));
    {
        let mut streams = ACTIVE_STREAMS.write().await;
        streams.insert(stream_id.clone(), cancel_flag.clone());
    }

    // Add stream flag to payload
    let mut payload = payload;
    if let Some(obj) = payload.as_object_mut() {
        obj.insert("stream".to_string(), serde_json::json!(true));
        obj.insert("stream_id".to_string(), serde_json::json!(stream_id.clone()));
    }

    let script_path = get_script_path(&app, "agents/finagent_core/core_agent_stream.py")?;
    let payload_str = serde_json::to_string(&payload)
        .map_err(|e| format!("Failed to serialize payload: {}", e))?;

    let sequence = Arc::new(AtomicU64::new(0));

    // Spawn streaming task
    let stream_id_for_task = stream_id.clone();
    tokio::spawn(async move {
        let app_clone = app.clone();
        let stream_id_clone = stream_id_for_task.clone();
        let seq = sequence.clone();

        // Clones for the callback closure
        let callback_stream_id = stream_id_clone.clone();
        let callback_app = app_clone.clone();
        let callback_seq = seq.clone();
        let callback_cancel = cancel_flag.clone();

        // Execute with streaming callback
        match execute_with_stream_callback(
            &script_path,
            vec![payload_str],
            move |chunk_type, content, metadata| {
                // Check cancellation
                if callback_cancel.load(Ordering::Relaxed) {
                    return false;
                }

                let chunk = StreamChunk {
                    stream_id: callback_stream_id.clone(),
                    chunk_type: chunk_type.to_string(),
                    content: content.to_string(),
                    metadata,
                    sequence: callback_seq.fetch_add(1, Ordering::Relaxed),
                };

                let event_name = format!("agent-stream-{}", callback_stream_id);
                let _ = callback_app.emit(&event_name, &chunk);
                true
            }
        ).await {
            Ok(final_result) => {
                // Emit done chunk
                let done_chunk = StreamChunk {
                    stream_id: stream_id_clone.clone(),
                    chunk_type: "done".to_string(),
                    content: final_result,
                    metadata: Some(serde_json::json!({
                        "elapsed_ms": start_time.elapsed().as_millis() as u64
                    })),
                    sequence: seq.fetch_add(1, Ordering::Relaxed),
                };
                let event_name = format!("agent-stream-{}", stream_id_clone);
                let _ = app_clone.emit(&event_name, &done_chunk);
            }
            Err(e) => {
                let error_chunk = StreamChunk {
                    stream_id: stream_id_clone.clone(),
                    chunk_type: "error".to_string(),
                    content: e,
                    metadata: None,
                    sequence: seq.fetch_add(1, Ordering::Relaxed),
                };
                let event_name = format!("agent-stream-{}", stream_id_clone);
                let _ = app_clone.emit(&event_name, &error_chunk);
            }
        }

        // Cleanup stream registration
        let mut streams = ACTIVE_STREAMS.write().await;
        streams.remove(&stream_id_clone);
    });

    // Return stream ID immediately
    Ok(serde_json::json!({
        "success": true,
        "stream_id": stream_id,
        "event_name": format!("agent-stream-{}", stream_id)
    }).to_string())
}

/// Cancel an active stream
#[tauri::command]
pub async fn cancel_agent_stream(stream_id: String) -> Result<String, String> {
    let streams = ACTIVE_STREAMS.read().await;

    if let Some(cancel_flag) = streams.get(&stream_id) {
        cancel_flag.store(true, Ordering::Relaxed);
        Ok(serde_json::json!({
            "success": true,
            "stream_id": stream_id,
            "status": "cancelled"
        }).to_string())
    } else {
        Err(format!("Stream {} not found or already completed", stream_id))
    }
}

/// Execute multiple agent tasks in parallel
///
/// Each task runs concurrently, results aggregated
#[tauri::command]
pub async fn execute_agents_parallel(
    app: AppHandle,
    tasks: Vec<serde_json::Value>,
    max_concurrent: Option<usize>,
) -> Result<String, String> {
    let batch_id = uuid::Uuid::new_v4().to_string();
    let start_time = std::time::Instant::now();
    let max_concurrent = max_concurrent.unwrap_or(4);

    let script_path = get_script_path(&app, "agents/finagent_core/core_agent.py")?;

    // Create semaphore for concurrency control
    let semaphore = Arc::new(tokio::sync::Semaphore::new(max_concurrent));

    // Spawn all tasks
    let mut handles = Vec::new();

    for (idx, task) in tasks.into_iter().enumerate() {
        let task_id = format!("{}-{}", batch_id, idx);
        let script_path = script_path.clone();
        let semaphore = semaphore.clone();
        let task_start = std::time::Instant::now();

        let handle = tokio::spawn(async move {
            // Acquire semaphore permit
            let _permit = semaphore.acquire().await.unwrap();

            let payload_str = serde_json::to_string(&task)
                .map_err(|e| format!("Failed to serialize: {}", e))?;

            match crate::python_runtime::execute_python_script_async(&script_path, vec![payload_str]).await {
                Ok(result) => {
                    let parsed: serde_json::Value = serde_json::from_str(&result)
                        .unwrap_or(serde_json::json!({"raw": result}));

                    Ok::<ParallelResult, String>(ParallelResult {
                        task_id,
                        success: true,
                        result: Some(parsed),
                        error: None,
                        execution_time_ms: task_start.elapsed().as_millis() as u64,
                    })
                }
                Err(e) => {
                    Ok(ParallelResult {
                        task_id,
                        success: false,
                        result: None,
                        error: Some(e),
                        execution_time_ms: task_start.elapsed().as_millis() as u64,
                    })
                }
            }
        });

        handles.push(handle);
    }

    // Collect all results
    let mut results = Vec::new();
    for handle in handles {
        match handle.await {
            Ok(Ok(result)) => results.push(result),
            Ok(Err(e)) => results.push(ParallelResult {
                task_id: "unknown".to_string(),
                success: false,
                result: None,
                error: Some(e),
                execution_time_ms: 0,
            }),
            Err(e) => results.push(ParallelResult {
                task_id: "unknown".to_string(),
                success: false,
                result: None,
                error: Some(format!("Task panicked: {}", e)),
                execution_time_ms: 0,
            }),
        }
    }

    let successful_count = results.iter().filter(|r| r.success).count();
    let failed_count = results.len() - successful_count;

    let response = BatchResponse {
        batch_id,
        results,
        total_time_ms: start_time.elapsed().as_millis() as u64,
        successful_count,
        failed_count,
    };

    serde_json::to_string(&response)
        .map_err(|e| format!("Failed to serialize response: {}", e))
}

/// Execute agent with priority (for urgent queries)
#[tauri::command]
pub async fn execute_agent_priority(
    app: AppHandle,
    payload: serde_json::Value,
    priority: u8,  // 0=HIGH, 1=NORMAL, 2=LOW
) -> Result<String, String> {
    let script_path = get_script_path(&app, "agents/finagent_core/core_agent.py")?;
    let payload_str = serde_json::to_string(&payload)
        .map_err(|e| format!("Failed to serialize payload: {}", e))?;

    crate::python_runtime::execute_python_script_async_with_priority(
        &script_path,
        vec![payload_str],
        priority
    ).await
}

/// Get status of all active agent streams
#[tauri::command]
pub async fn get_active_agent_streams() -> Result<String, String> {
    let streams = ACTIVE_STREAMS.read().await;
    let stream_ids: Vec<String> = streams.keys().cloned().collect();

    Ok(serde_json::json!({
        "success": true,
        "active_streams": stream_ids,
        "count": stream_ids.len()
    }).to_string())
}

/// Internal: Execute with streaming callback (simulated for now)
/// In production, this would use a named pipe or socket to get streaming output
async fn execute_with_stream_callback<F>(
    script_path: &std::path::PathBuf,
    args: Vec<String>,
    mut callback: F,
) -> Result<String, String>
where
    F: FnMut(&str, &str, Option<serde_json::Value>) -> bool + Send + 'static,
{
    // For now, execute normally and emit result as single chunk
    // Future: Use named pipes or Unix sockets for true streaming

    // Emit thinking indicator
    callback("thinking", "Processing your request...", None);

    let result = crate::python_runtime::execute_python_script_async(script_path, args).await?;

    // Parse and emit tokens (simulated chunking for now)
    if let Ok(parsed) = serde_json::from_str::<serde_json::Value>(&result) {
        if let Some(response) = parsed.get("response").and_then(|r| r.as_str()) {
            // Emit in chunks for smoother UI
            for chunk in response.chars().collect::<Vec<_>>().chunks(50) {
                let chunk_str: String = chunk.iter().collect();
                if !callback("token", &chunk_str, None) {
                    return Err("Stream cancelled".to_string());
                }
                tokio::time::sleep(tokio::time::Duration::from_millis(10)).await;
            }
        }
    }

    Ok(result)
}
