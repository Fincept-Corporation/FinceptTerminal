// Type definitions for all database entities

use serde::{Deserialize, Serialize};

// ============================================================================
// Common Result Types
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OperationResult {
    pub success: bool,
    pub message: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OperationResultWithId {
    pub success: bool,
    pub message: String,
    pub id: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToggleResult {
    pub success: bool,
    pub message: String,
    pub enabled: bool,
}

// ============================================================================
// Credentials & API Keys
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Credential {
    pub id: Option<i64>,
    pub service_name: String,
    pub username: Option<String>,
    pub password: Option<String>,
    pub api_key: Option<String>,
    pub api_secret: Option<String>,
    pub additional_data: Option<String>,
    pub created_at: Option<String>,
    pub updated_at: Option<String>,
}

// ============================================================================
// Settings
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Setting {
    pub setting_key: String,
    pub setting_value: String,
    pub category: Option<String>,
    pub updated_at: Option<String>,
}

// ============================================================================
// LLM Configuration
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LLMConfig {
    pub provider: String,
    pub api_key: Option<String>,
    pub base_url: Option<String>,
    pub model: String,
    pub is_active: bool,
    pub created_at: Option<String>,
    pub updated_at: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LLMGlobalSettings {
    pub temperature: f64,
    pub max_tokens: i64,
    pub system_prompt: String,
}

// ============================================================================
// Chat
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatSession {
    pub session_uuid: String,
    pub title: String,
    pub message_count: i64,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatMessage {
    pub id: String,
    pub session_uuid: String,
    pub role: String,
    pub content: String,
    pub timestamp: String,
    pub provider: Option<String>,
    pub model: Option<String>,
    pub tokens_used: Option<i64>,
}

// ============================================================================
// Data Sources
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DataSource {
    pub id: String,
    pub alias: String,
    pub display_name: String,
    pub description: Option<String>,
    #[serde(rename = "type")]
    pub ds_type: String,
    pub provider: String,
    pub category: Option<String>,
    pub config: String,
    pub enabled: bool,
    pub tags: Option<String>,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WSProviderConfig {
    pub id: i64,
    pub provider_name: String,
    pub enabled: bool,
    pub api_key: Option<String>,
    pub api_secret: Option<String>,
    pub endpoint: Option<String>,
    pub config_data: Option<String>,
    pub created_at: String,
    pub updated_at: String,
}

// ============================================================================
// MCP Servers
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MCPServer {
    pub id: String,
    pub name: String,
    pub description: String,
    pub command: String,
    pub args: String,
    pub env: Option<String>,
    pub category: String,
    pub icon: String,
    pub enabled: bool,
    pub auto_start: bool,
    pub status: String,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MCPTool {
    pub id: String,
    pub server_id: String,
    pub name: String,
    pub description: Option<String>,
    pub input_schema: Option<String>,
    pub created_at: String,
}

// ============================================================================
// Context Recorder
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RecordedContext {
    pub id: String,
    pub tab_name: String,
    pub data_type: String,
    pub label: Option<String>,
    pub raw_data: String,
    pub metadata: Option<String>,
    pub data_size: i64,
    pub created_at: String,
    pub tags: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RecordingSession {
    pub id: String,
    pub tab_name: String,
    pub is_active: bool,
    pub auto_record: bool,
    pub filters: Option<String>,
    pub started_at: String,
    pub ended_at: Option<String>,
}

// ============================================================================
// Backtesting
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BacktestingProvider {
    pub id: String,
    pub name: String,
    pub adapter_type: String,
    pub config: String,
    pub enabled: bool,
    pub is_active: bool,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BacktestingStrategy {
    pub id: String,
    pub name: String,
    pub description: Option<String>,
    pub version: String,
    pub author: Option<String>,
    pub provider_type: String,
    pub strategy_type: String,
    pub strategy_definition: String,
    pub tags: Option<String>,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BacktestRun {
    pub id: String,
    pub strategy_id: Option<String>,
    pub provider_name: String,
    pub config: String,
    pub results: Option<String>,
    pub status: String,
    pub performance_metrics: Option<String>,
    pub error_message: Option<String>,
    pub created_at: String,
    pub completed_at: Option<String>,
    pub duration_seconds: Option<i64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OptimizationRun {
    pub id: String,
    pub strategy_id: Option<String>,
    pub provider_name: String,
    pub parameter_grid: String,
    pub objective: String,
    pub best_parameters: Option<String>,
    pub best_result: Option<String>,
    pub all_results: Option<String>,
    pub iterations: Option<i64>,
    pub status: String,
    pub error_message: Option<String>,
    pub created_at: String,
    pub completed_at: Option<String>,
    pub duration_seconds: Option<i64>,
}

// ============================================================================
// Watchlist
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Watchlist {
    pub id: String,
    pub name: String,
    pub description: Option<String>,
    pub color: String,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WatchlistStock {
    pub id: String,
    pub watchlist_id: String,
    pub symbol: String,
    pub added_at: String,
    pub notes: Option<String>,
}

// ============================================================================
// Notes
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Note {
    pub id: Option<i64>,
    pub title: String,
    pub content: String,
    pub category: String,
    pub priority: String,
    pub tags: Option<String>,
    pub tickers: Option<String>,
    pub sentiment: String,
    pub is_favorite: bool,
    pub is_archived: bool,
    pub color_code: String,
    pub attachments: String,
    pub created_at: String,
    pub updated_at: String,
    pub reminder_date: Option<String>,
    pub word_count: i64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NoteTemplate {
    pub id: Option<i64>,
    pub name: String,
    pub description: String,
    pub content: String,
    pub category: String,
    pub created_at: String,
}

// ============================================================================
// Excel
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExcelFile {
    pub id: Option<i64>,
    pub file_name: String,
    pub file_path: String,
    pub sheet_count: i64,
    pub last_opened: String,
    pub last_modified: String,
    pub created_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExcelSnapshot {
    pub id: Option<i64>,
    pub file_id: i64,
    pub snapshot_name: String,
    pub sheet_data: String,
    pub created_at: String,
}
