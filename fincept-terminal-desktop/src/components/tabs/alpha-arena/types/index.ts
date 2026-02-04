/**
 * Alpha Arena TypeScript Types
 *
 * Type definitions matching the Python Pydantic models.
 */

// Enums
export type CompetitionMode = 'baseline' | 'monk' | 'situational' | 'max_leverage';
export type CompetitionStatus = 'created' | 'running' | 'paused' | 'completed' | 'failed';
export type TradeAction = 'buy' | 'sell' | 'hold' | 'short';
export type ModelProvider = 'openai' | 'anthropic' | 'google' | 'deepseek' | 'groq' | 'openrouter' | 'ollama' | 'fincept';

// Response Events
export type StreamResponseEvent =
  | 'message_chunk'
  | 'decision_started'
  | 'decision_completed'
  | 'trade_executed'
  | 'reasoning_started'
  | 'reasoning'
  | 'reasoning_completed'
  | 'market_data'
  | 'error';

export type SystemResponseEvent =
  | 'competition_started'
  | 'competition_paused'
  | 'competition_completed'
  | 'cycle_started'
  | 'cycle_completed'
  | 'system_failed'
  | 'done';

export type CommonResponseEvent =
  | 'component_generator'
  | 'leaderboard_update'
  | 'portfolio_update';

// Core Models
export interface AgentConfig {
  name: string;
  provider: ModelProvider;
  model_id: string;
  api_key?: string;
  temperature?: number;
  max_tokens?: number;
}

export interface CompetitionModel {
  name: string;
  provider: string;
  model_id: string;
  api_key?: string;
  initial_capital: number;
  trading_style?: string;  // Trading style ID (e.g., 'aggressive', 'conservative')
  metadata?: Record<string, unknown>;  // Additional metadata including style config
  capital?: number;
  total_pnl?: number;
}

export interface PositionInfo {
  symbol: string;
  quantity: number;
  entry_price: number;
  side: 'long' | 'short';
  margin_reserved?: number;
  unrealized_pnl?: number;
}

export interface PortfolioState {
  model_name: string;
  cash: number;
  portfolio_value: number;
  positions: Record<string, PositionInfo>;
  total_pnl: number;
  unrealized_pnl: number;
  trades_count: number;
}

export interface MarketData {
  symbol: string;
  price: number;
  bid: number;
  ask: number;
  high_24h: number;
  low_24h: number;
  volume_24h: number;
  timestamp: string;
}

export interface TradeResult {
  status: 'executed' | 'rejected' | 'partial';
  action: TradeAction;
  symbol: string;
  quantity: number;
  price: number;
  cost?: number;
  proceeds?: number;
  pnl?: number;
  margin_reserved?: number;
  reason?: string;
  timestamp: string;
}

export interface ModelDecision {
  competition_id: string;
  model_name: string;
  cycle_number: number;
  action: TradeAction;
  symbol: string;
  quantity: number;
  confidence: number;
  reasoning: string;
  trade_executed?: TradeResult;
  timestamp: string;
  price_at_decision?: number;
  portfolio_value_before?: number;
  portfolio_value_after?: number;
}

export interface LeaderboardEntry {
  rank: number;
  model_name: string;
  portfolio_value: number;
  total_pnl: number;
  return_pct: number;
  trades_count: number;
  cash: number;
  positions_count: number;
  win_rate?: number;
  sharpe_ratio?: number;
}

export interface CompetitionConfig {
  competition_id: string;
  competition_name: string;
  models: CompetitionModel[];
  symbols: string[];
  initial_capital: number;
  mode: CompetitionMode;
  cycle_interval_seconds: number;
  exchange_id: string;
  max_cycles?: number;
  custom_prompt?: string;
}

export interface Competition {
  config: CompetitionConfig;
  status: CompetitionStatus;
  cycle_count: number;
  start_time?: string;
  end_time?: string;
  portfolios: Record<string, PortfolioState>;
  leaderboard: LeaderboardEntry[];
}

export interface PerformanceSnapshot {
  id: string;
  competition_id: string;
  cycle_number: number;
  model_name: string;
  portfolio_value: number;
  cash: number;
  pnl: number;
  return_pct: number;
  positions_count: number;
  trades_count: number;
  timestamp: string;
}

// Streaming Responses
export interface StreamResponse {
  content?: string;
  event: StreamResponseEvent | SystemResponseEvent | CommonResponseEvent;
  metadata?: Record<string, unknown>;
}

export interface CycleEvent {
  event: string;
  content?: string;
  metadata?: Record<string, unknown>;
}

// Agent Card
export interface AgentCapabilities {
  streaming: boolean;
  trading: boolean;
  analysis: boolean;
  notifications: boolean;
  short_selling: boolean;
  leverage: boolean;
}

export interface AgentCard {
  name: string;
  description: string;
  version: string;
  provider: ModelProvider;
  model_id: string;
  agent_class?: string;
  url?: string;
  enabled: boolean;
  capabilities: AgentCapabilities;
  temperature: number;
  max_tokens?: number;
  default_mode: CompetitionMode;
  risk_level: 'low' | 'medium' | 'high';
  system_prompt?: string;
  instructions: string[];
  metadata: Record<string, unknown>;
}

// API Request/Response Types
export interface CreateCompetitionRequest {
  competition_name?: string;
  models: Array<{
    name: string;
    provider: string;
    model_id: string;
    api_key?: string;
    initial_capital: number;
    trading_style?: string;
    metadata?: Record<string, unknown>;
  }>;
  symbols?: string[];
  initial_capital?: number;
  mode?: CompetitionMode;
  cycle_interval_seconds?: number;
  exchange_id?: string;
  max_cycles?: number;
}

export interface CreateCompetitionResponse {
  success: boolean;
  competition_id?: string;
  config?: CompetitionConfig;
  status?: string;
  error?: string;
}

export interface RunCycleResponse {
  success: boolean;
  competition_id?: string;
  cycle_number?: number;
  events?: CycleEvent[];
  leaderboard?: LeaderboardEntry[];
  error?: string;
  warnings?: string[];
}

export interface LeaderboardResponse {
  success: boolean;
  leaderboard?: LeaderboardEntry[];
  cycle_number?: number;
  error?: string;
}

export interface DecisionsResponse {
  success: boolean;
  decisions?: ModelDecision[];
  error?: string;
}

export interface SnapshotsResponse {
  success: boolean;
  snapshots?: PerformanceSnapshot[];
  error?: string;
}

export interface ListCompetitionsResponse {
  success: boolean;
  competitions?: Array<{
    competition_id: string;
    config: CompetitionConfig;
    status: CompetitionStatus;
    cycle_count: number;
    start_time?: string;
    end_time?: string;
    created_at: string;
  }>;
  count?: number;
  error?: string;
}

export interface ListAgentsResponse {
  success: boolean;
  agents?: Array<{
    name: string;
    provider: string;
    model_id: string;
    description: string;
    enabled: boolean;
  }>;
  count?: number;
  error?: string;
}

export interface SystemInfoResponse {
  success: boolean;
  version?: string;
  framework?: string;
  features?: string[];
  supported_providers?: string[];
  default_exchange?: string;
  error?: string;
}

// Evaluation Metrics
export interface ModelMetrics {
  model_name: string;
  total_decisions: number;
  correct_decisions: number;
  total_pnl: number;
  win_rate: number;
  avg_confidence: number;
  avg_return: number;
  confidence_accuracy_correlation: number;
  overconfidence_ratio: number;
  sharpe_ratio: number;
  max_drawdown: number;
  profit_factor: number;
  avg_trade_pnl: number;
  best_trade: number;
  worst_trade: number;
  consecutive_wins: number;
  consecutive_losses: number;
}

// Chart Data Types
export interface ChartDataPoint {
  cycle: number;
  timestamp: string;
  [modelName: string]: number | string;
}

export type ChartData = ChartDataPoint[];

// Color mapping for models
export const MODEL_COLORS: Record<string, string> = {
  'GPT-4o Mini': '#10B981',      // Emerald
  'GPT-4o': '#3B82F6',           // Blue
  'Claude 3.5 Sonnet': '#F97316', // Orange
  'Gemini 2.0 Flash': '#8B5CF6', // Purple
  'DeepSeek Chat': '#EC4899',    // Pink
  'Llama 3.3 70B': '#14B8A6',    // Teal
  'Qwen 2.5 72B': '#F59E0B',     // Amber
};

// Helper function to get color for model
export function getModelColor(modelName: string): string {
  return MODEL_COLORS[modelName] || '#6B7280'; // Default gray
}

// Format helpers
export function formatCurrency(value: number): string {
  return new Intl.NumberFormat('en-US', {
    style: 'currency',
    currency: 'USD',
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  }).format(value);
}

export function formatPercent(value: number): string {
  const sign = value >= 0 ? '+' : '';
  return `${sign}${value.toFixed(2)}%`;
}

export function formatNumber(value: number, decimals = 2): string {
  return value.toLocaleString('en-US', {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
}
