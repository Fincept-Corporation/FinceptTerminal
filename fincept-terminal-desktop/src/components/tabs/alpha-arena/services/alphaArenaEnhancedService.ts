/**
 * Alpha Arena Enhanced Service
 *
 * Additional service methods for new Alpha Arena features:
 * - HITL (Human-in-the-Loop) approval
 * - Sentiment analysis
 * - Portfolio metrics
 * - Grid trading
 * - Multi-broker support
 * - Research/SEC filings
 */

import { invoke } from '@tauri-apps/api/core';

// =============================================================================
// Types
// =============================================================================

export interface ApprovalRequest {
  id: string;
  decision: {
    model_name: string;
    action: string;
    symbol: string;
    quantity: number;
    confidence: number;
    reasoning: string;
    price_at_decision: number;
  };
  risk_level: 'low' | 'medium' | 'high' | 'critical';
  reason: string;
  created_at: string;
  expires_at: string | null;
  status: 'pending' | 'approved' | 'rejected' | 'timeout' | 'auto_approved';
}

export interface HITLStatus {
  rules: Array<{
    name: string;
    description: string;
    risk_level: string;
    auto_approve_timeout: number | null;
    enabled: boolean;
  }>;
  pending_count: number;
  pending: ApprovalRequest[];
  history_count: number;
}

export interface SentimentResult {
  symbol: string;
  timestamp: string;
  articles_analyzed: number;
  overall_sentiment: 'very_bearish' | 'bearish' | 'neutral' | 'bullish' | 'very_bullish';
  sentiment_score: number;
  confidence: number;
  bullish_count: number;
  bearish_count: number;
  neutral_count: number;
  key_headlines: string[];
  sources: string[];
}

export interface MarketMood {
  symbols_analyzed: number;
  overall_mood: 'risk_on' | 'risk_off' | 'mixed';
  average_sentiment: number;
  individual_sentiments: Record<string, SentimentResult>;
}

export interface PortfolioMetrics {
  total_return: number;
  sharpe_ratio: number;
  sortino_ratio: number;
  max_drawdown_pct: number;
  max_drawdown_value: number;
  win_rate: number;
  profit_factor: number;
  expectancy: number;
  total_trades: number;
  winning_trades: number;
  losing_trades: number;
  avg_win: number;
  avg_loss: number;
  volatility: number;
}

export interface GridConfig {
  symbol: string;
  lower_price: number;
  upper_price: number;
  num_grids: number;
  quantity_per_grid: number;
  take_profit_pct?: number;
  stop_loss_pct?: number;
}

export interface GridStatus {
  agent_name: string;
  grid_status: {
    grid_levels: number[];
    triggered_levels: number[];
    active_orders: number;
    total_profit: number;
    trades_executed: number;
  };
}

export interface BrokerInfo {
  id: string;
  name: string;
  display_name: string;
  broker_type: 'crypto' | 'stocks' | 'forex';
  region: 'global' | 'us' | 'india' | 'europe' | 'asia';
  supports_spot: boolean;
  supports_margin: boolean;
  supports_futures: boolean;
  supports_options: boolean;
  max_leverage: number;
  default_symbols: string[];
  maker_fee: number;
  taker_fee: number;
  websocket_enabled: boolean;
}

export interface TickerData {
  symbol: string;
  price: number;
  bid: number;
  ask: number;
  high_24h: number;
  low_24h: number;
  volume_24h: number;
  timestamp: string;
}

export interface ResearchReport {
  symbol: string;
  generated_at: string;
  company_info: {
    cik: string;
    name: string;
    ticker: string;
    sic: string;
    industry: string;
    fiscal_year_end: string;
    state: string;
  } | null;
  recent_filings: Array<{
    accession_number: string;
    form_type: string;
    filing_date: string;
    description: string;
    primary_document: string;
  }>;
  financials: Array<{
    period: string;
    revenue: number | null;
    net_income: number | null;
    total_assets: number | null;
    eps: number | null;
  }>;
  insider_activity: unknown[];
  summary: string;
  risk_factors: string[];
}

export interface TechnicalFeatures {
  sma_20: number | null;
  sma_50: number | null;
  ema_12: number | null;
  ema_26: number | null;
  rsi_14: number | null;
  rsi_interpretation: string;
  macd: number | null;
  macd_signal: number | null;
  macd_interpretation: string;
  bollinger_upper: number | null;
  bollinger_middle: number | null;
  bollinger_lower: number | null;
  bollinger_interpretation: string;
  support_level: number | null;
  resistance_level: number | null;
}

// =============================================================================
// HITL (Human-in-the-Loop) Functions
// =============================================================================

export async function getHITLStatus(): Promise<{
  success: boolean;
  status?: HITLStatus;
  error?: string;
}> {
  try {
    const result = await invoke<{ success: boolean; status: HITLStatus }>('get_alpha_hitl_status');
    return result;
  } catch (error) {
    console.error('Error getting HITL status:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function getPendingApprovals(): Promise<{
  success: boolean;
  pending_approvals?: ApprovalRequest[];
  count?: number;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      pending_approvals: ApprovalRequest[];
      count: number;
    }>('get_alpha_pending_approvals');
    return result;
  } catch (error) {
    console.error('Error getting pending approvals:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function approveDecision(
  requestId: string,
  notes?: string
): Promise<{ success: boolean; error?: string }> {
  try {
    const result = await invoke<{ success: boolean; action: string }>('approve_alpha_decision', {
      requestId,
      approvedBy: 'user',
      notes: notes || '',
    });
    return { success: result.success };
  } catch (error) {
    console.error('Error approving decision:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function rejectDecision(
  requestId: string,
  notes?: string
): Promise<{ success: boolean; error?: string }> {
  try {
    const result = await invoke<{ success: boolean; action: string }>('reject_alpha_decision', {
      requestId,
      rejectedBy: 'user',
      notes: notes || '',
    });
    return { success: result.success };
  } catch (error) {
    console.error('Error rejecting decision:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function updateHITLRule(
  ruleName: string,
  enabled: boolean
): Promise<{ success: boolean; error?: string }> {
  try {
    const result = await invoke<{ success: boolean }>('update_alpha_hitl_rule', {
      ruleName,
      enabled,
    });
    return { success: result.success };
  } catch (error) {
    console.error('Error updating HITL rule:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

// =============================================================================
// Sentiment Analysis Functions
// =============================================================================

export async function getSentiment(
  symbol: string,
  maxArticles?: number
): Promise<{
  success: boolean;
  sentiment?: SentimentResult;
  prompt_context?: string;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      sentiment: SentimentResult;
      prompt_context: string;
    }>('get_alpha_sentiment', {
      symbol,
      maxArticles: maxArticles || 15,
    });
    return result;
  } catch (error) {
    console.error('Error getting sentiment:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function getMarketMood(
  symbols?: string[]
): Promise<{
  success: boolean;
  market_mood?: MarketMood;
  error?: string;
}> {
  try {
    const result = await invoke<{ success: boolean; market_mood: MarketMood }>(
      'get_alpha_market_mood',
      { symbols }
    );
    return result;
  } catch (error) {
    console.error('Error getting market mood:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

// =============================================================================
// Portfolio Metrics Functions
// =============================================================================

export async function getPortfolioMetrics(
  competitionId: string,
  modelName?: string
): Promise<{
  success: boolean;
  metrics?: PortfolioMetrics | Record<string, PortfolioMetrics>;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      metrics: PortfolioMetrics | Record<string, PortfolioMetrics>;
    }>('get_alpha_portfolio_metrics', {
      competitionId,
      modelName: modelName || null,
    });
    return result;
  } catch (error) {
    console.error('Error getting portfolio metrics:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function getEquityCurve(
  competitionId: string,
  modelName?: string
): Promise<{
  success: boolean;
  equity_curve?: Array<{
    timestamp: string;
    cycle: number;
    value: number;
    model: string;
  }>;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      equity_curve: Array<{
        timestamp: string;
        cycle: number;
        value: number;
        model: string;
      }>;
    }>('get_alpha_equity_curve', {
      competitionId,
      modelName: modelName || null,
    });
    return result;
  } catch (error) {
    console.error('Error getting equity curve:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

// =============================================================================
// Grid Trading Functions
// =============================================================================

export async function createGridAgent(
  name: string,
  gridConfig: GridConfig
): Promise<{
  success: boolean;
  agent_name?: string;
  grid_config?: GridConfig & { grid_spacing: number };
  grid_levels?: number[];
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      agent_name: string;
      grid_config: GridConfig & { grid_spacing: number };
      grid_levels: number[];
    }>('create_alpha_grid_agent', {
      name,
      upperPrice: gridConfig.upper_price,
      lowerPrice: gridConfig.lower_price,
      gridLevels: gridConfig.num_grids,
      gridType: 'arithmetic',
      totalInvestment: gridConfig.quantity_per_grid * gridConfig.num_grids * gridConfig.lower_price,
    });
    return result;
  } catch (error) {
    console.error('Error creating grid agent:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function getGridStatus(
  agentName: string
): Promise<{
  success: boolean;
  grid_status?: GridStatus['grid_status'];
  error?: string;
}> {
  try {
    const result = await invoke<GridStatus>('get_alpha_grid_status', { agentName });
    return {
      success: true,
      grid_status: result.grid_status,
    };
  } catch (error) {
    console.error('Error getting grid status:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

// =============================================================================
// Broker Functions
// =============================================================================

export async function listBrokers(
  brokerType?: 'crypto' | 'stocks' | 'forex',
  region?: 'global' | 'us' | 'india' | 'europe' | 'asia'
): Promise<{
  success: boolean;
  brokers?: BrokerInfo[];
  count?: number;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      brokers: BrokerInfo[];
      count: number;
    }>('list_alpha_brokers', {
      brokerType: brokerType || null,
      region: region || null,
    });
    return result;
  } catch (error) {
    console.error('Error listing brokers:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function getBrokerTicker(
  symbol: string,
  brokerId?: string
): Promise<{
  success: boolean;
  ticker?: TickerData;
  broker_id?: string;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      ticker: TickerData;
      broker_id: string;
    }>('get_alpha_broker_ticker', {
      symbol,
      brokerId: brokerId || null,
    });
    return result;
  } catch (error) {
    console.error('Error getting broker ticker:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

// =============================================================================
// Research Functions
// =============================================================================

export async function getResearch(
  ticker: string
): Promise<{
  success: boolean;
  report?: ResearchReport;
  prompt_context?: string;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      report: ResearchReport;
      prompt_context: string;
    }>('get_alpha_research', { ticker });
    return result;
  } catch (error) {
    console.error('Error getting research:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

export async function getSecFilings(
  ticker: string,
  formTypes?: string[],
  limit?: number
): Promise<{
  success: boolean;
  filings?: ResearchReport['recent_filings'];
  count?: number;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      ticker: string;
      filings: ResearchReport['recent_filings'];
      count: number;
    }>('get_alpha_sec_filings', {
      ticker,
      formTypes: formTypes || null,
      limit: limit || 10,
    });
    return result;
  } catch (error) {
    console.error('Error getting SEC filings:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

// =============================================================================
// Technical Features Functions
// =============================================================================

export async function getTechnicalFeatures(
  symbol: string,
  price: number
): Promise<{
  success: boolean;
  features?: {
    technical: TechnicalFeatures | null;
    sentiment: SentimentResult | null;
  };
  prompt_context?: string;
  error?: string;
}> {
  try {
    const result = await invoke<{
      success: boolean;
      features: {
        technical: TechnicalFeatures | null;
        sentiment: SentimentResult | null;
      };
      prompt_context: string;
    }>('get_alpha_features', { symbol, price });
    return result;
  } catch (error) {
    console.error('Error getting technical features:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
    };
  }
}

// =============================================================================
// Export Service Object
// =============================================================================

export const alphaArenaEnhancedService = {
  // HITL
  getHITLStatus,
  getPendingApprovals,
  approveDecision,
  rejectDecision,
  updateHITLRule,

  // Sentiment
  getSentiment,
  getMarketMood,

  // Portfolio Metrics
  getPortfolioMetrics,
  getEquityCurve,

  // Grid Trading
  createGridAgent,
  getGridStatus,

  // Brokers
  listBrokers,
  getBrokerTicker,

  // Research
  getResearch,
  getSecFilings,

  // Technical Features
  getTechnicalFeatures,
};

export default alphaArenaEnhancedService;
