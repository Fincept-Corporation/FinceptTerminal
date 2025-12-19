/**
 * SQLite Database Types
 * Centralized type definitions for all SQLite database entities
 */

import Database from '@tauri-apps/plugin-sql';

// ============================================================================
// Credentials & API Keys
// ============================================================================

export interface Credential {
    id?: number;
    service_name: string;
    username: string;
    password: string;
    api_key?: string;
    api_secret?: string;
    additional_data?: string;
    created_at?: string;
    updated_at?: string;
}

export interface ApiKeys {
    FRED_API_KEY?: string;
    POLYGON_API_KEY?: string;
    ALPHA_VANTAGE_API_KEY?: string;
    OPENAI_API_KEY?: string;
    ANTHROPIC_API_KEY?: string;
    COINGECKO_API_KEY?: string;
    NASDAQ_API_KEY?: string;
    FINANCIAL_MODELING_PREP_API_KEY?: string;
    [key: string]: string | undefined;
}

export const PREDEFINED_API_KEYS = [
    { key: 'FRED_API_KEY', label: 'FRED API Key', description: 'Federal Reserve Economic Data' },
    { key: 'POLYGON_API_KEY', label: 'Polygon.io API Key', description: 'Stock market data' },
    { key: 'ALPHA_VANTAGE_API_KEY', label: 'Alpha Vantage API Key', description: 'Stock & crypto data' },
    { key: 'OPENAI_API_KEY', label: 'OpenAI API Key', description: 'GPT models' },
    { key: 'ANTHROPIC_API_KEY', label: 'Anthropic API Key', description: 'Claude models' },
    { key: 'COINGECKO_API_KEY', label: 'CoinGecko API Key', description: 'Cryptocurrency data' },
    { key: 'NASDAQ_API_KEY', label: 'NASDAQ API Key', description: 'NASDAQ market data' },
    { key: 'FINANCIAL_MODELING_PREP_API_KEY', label: 'Financial Modeling Prep', description: 'Financial statements & ratios' },
] as const;

// ============================================================================
// Settings
// ============================================================================

export interface Setting {
    setting_key: string;
    setting_value: string;
    category?: string;
    updated_at?: string;
}

// ============================================================================
// LLM Configuration
// ============================================================================

export interface LLMConfig {
    provider: string;
    api_key?: string;
    base_url?: string;
    model: string;
    is_active: boolean;
    created_at?: string;
    updated_at?: string;
}

export interface LLMGlobalSettings {
    temperature: number;
    max_tokens: number;
    system_prompt: string;
}

// ============================================================================
// Chat
// ============================================================================

export interface ChatSession {
    session_uuid: string;
    title: string;
    message_count: number;
    created_at: string;
    updated_at: string;
}

export interface ChatMessage {
    id: string;
    session_uuid: string;
    role: 'user' | 'assistant';
    content: string;
    timestamp: string;
    provider?: string;
    model?: string;
    tokens_used?: number;
}

// ============================================================================
// Data Sources
// ============================================================================

export interface DataSource {
    id: string;
    alias: string;
    display_name: string;
    description?: string;
    type: 'websocket' | 'rest_api';
    provider: string;
    category?: string;
    config: string;
    enabled: boolean;
    tags?: string;
    created_at: string;
    updated_at: string;
}

export interface WSProviderConfig {
    id: number;
    provider_name: string;
    enabled: boolean;
    api_key: string | null;
    api_secret: string | null;
    endpoint: string | null;
    config_data: string | null;
    created_at: string;
    updated_at: string;
}

// ============================================================================
// MCP Servers
// ============================================================================

export interface MCPServer {
    id: string;
    name: string;
    description: string;
    command: string;
    args: string;
    env?: string | null;
    category: string;
    icon: string;
    enabled: boolean;
    auto_start: boolean;
    status: string;
    created_at: string;
    updated_at: string;
}

// ============================================================================
// Context Recorder
// ============================================================================

export interface RecordedContext {
    id: string;
    tab_name: string;
    data_type: string;
    label?: string;
    raw_data: string;
    metadata?: string;
    data_size: number;
    created_at: string;
    tags?: string;
}

export interface RecordingSession {
    id: string;
    tab_name: string;
    is_active: boolean;
    auto_record: boolean;
    filters?: string;
    started_at: string;
    ended_at?: string;
}

export interface ChatContextLink {
    chat_session_uuid: string;
    context_id: string;
    added_at: string;
    is_active: boolean;
}

// ============================================================================
// Backtesting
// ============================================================================

export interface BacktestingProvider {
    id: string;
    name: string;
    adapter_type: string;
    config: string;
    enabled: boolean;
    is_active: boolean;
    created_at: string;
    updated_at: string;
}

export interface BacktestingStrategy {
    id: string;
    name: string;
    description?: string;
    version: string;
    author?: string;
    provider_type: string;
    strategy_type: 'code' | 'visual' | 'template';
    strategy_definition: string;
    tags?: string;
    created_at: string;
    updated_at: string;
}

export interface BacktestRun {
    id: string;
    strategy_id?: string;
    provider_name: string;
    config: string;
    results?: string;
    status: 'completed' | 'failed' | 'running' | 'cancelled';
    performance_metrics?: string;
    error_message?: string;
    created_at: string;
    completed_at?: string;
    duration_seconds?: number;
}

export interface OptimizationRun {
    id: string;
    strategy_id?: string;
    provider_name: string;
    parameter_grid: string;
    objective: string;
    best_parameters?: string;
    best_result?: string;
    all_results?: string;
    iterations?: number;
    status: 'completed' | 'failed' | 'running' | 'cancelled';
    error_message?: string;
    created_at: string;
    completed_at?: string;
    duration_seconds?: number;
}

// ============================================================================
// Common Result Types
// ============================================================================

export interface OperationResult {
    success: boolean;
    message: string;
}

export interface OperationResultWithId extends OperationResult {
    id?: string;
}

export interface ToggleResult extends OperationResult {
    enabled: boolean;
}

// ============================================================================
// Database Instance Type
// ============================================================================

export type DatabaseInstance = Database;
