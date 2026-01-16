/**
 * Agno Trading Service
 *
 * TypeScript wrapper for Agno AI Trading Agent system.
 * Provides type-safe interface to Rust/Python Agno commands.
 * Reads API keys from Settings tab LLM configuration.
 */

import { invoke } from '@tauri-apps/api/core';
import { sqliteService } from '../core/sqliteService';

// ============================================================================
// Types
// ============================================================================

export interface AgnoResponse<T = any> {
  success: boolean;
  data?: T;
  error?: string;
}

export interface ModelInfo {
  provider: string;
  model: string;
  max_tokens?: number;
  supports_streaming?: boolean;
  supports_reasoning?: boolean;
  cost_per_1k_input?: number;
  cost_per_1k_output?: number;
}

export interface ModelRecommendation {
  provider: string;
  model: string;
  model_string: string;
}

export interface AgentConfig {
  name: string;
  role: string;
  description?: string;
  instructions?: string[];
  agent_model?: string;
  temperature?: number;
  tools?: string[];
  symbols?: string[];
  enable_memory?: boolean;
  risk_tolerance?: string;
}

export interface AgentInfo {
  agent_id: string;
  status: string;
  config: {
    name: string;
    role: string;
    model: string;
  };
}

export interface AgentRunResult {
  agent_id: string;
  session_id?: string;
  content: string;
  response_time?: number;
  timestamp: string;
  metrics?: {
    input_tokens: number;
    output_tokens: number;
    total_tokens: number;
  };
}

export interface MarketAnalysis {
  symbol: string;
  analysis_type: string;
  analysis: string;
  agent: string;
}

export interface TradeSignal {
  symbol: string;
  strategy: string;
  timeframe?: string;
  signal: string;
  agent: string;
}

export interface RiskAnalysis {
  portfolio_value: number;
  position_count: number;
  risk_analysis: string;
  agent: string;
}

export interface AgnoConfig {
  llm?: {
    provider: string;
    model: string;
    temperature?: number;
    max_tokens?: number;
    stream?: boolean;
    reasoning_effort?: string;
  };
  trading?: {
    mode: string;
    max_position_size?: number;
    max_leverage?: number;
    stop_loss_pct?: number;
    take_profit_pct?: number;
    initial_capital?: number;
    symbols?: string[];
    exchange?: string;
  };
  knowledge?: any;
  memory?: any;
  agent?: any;
  team?: any;
  workflow?: any;
}

export interface CompetitionInfo {
  team_id: string;
  models: string[];
  agent_count: number;
}

export interface ModelDecision {
  model: string;
  timestamp: string;
  content: string;
  action?: 'long' | 'short' | 'hold' | 'close';
  symbol?: string;
  confidence?: number;
  entry?: number;
  target?: number;
  stopLoss?: number;
}

export interface ModelPerformance {
  model: string;
  total_trades: number;
  winning_trades: number;
  losing_trades: number;
  win_rate: number;
  total_pnl: number;
  daily_pnl: number;
  positions: number;
  last_decision?: ModelDecision;
}

export interface CompetitionResult {
  team_id: string;
  results: Array<{
    model: string;
    decision: any;
    response: string;
    timestamp: string;
  }>;
  consensus: {
    action: string;
    votes: Record<string, number>;
    confidence: number;
    agreement: number;
    total_models: number;
  };
  leaderboard: ModelPerformance[];
  timestamp: string;
}

// ============================================================================
// NEW TYPES: Auto-Trading, Debate, Evolution
// ============================================================================

export interface AgentTrade {
  id: number;
  agent_id: string;
  model: string;
  symbol: string;
  side: 'buy' | 'sell';
  order_type: string;
  quantity: number;
  entry_price: number;
  exit_price?: number;
  stop_loss?: number;
  take_profit?: number;
  pnl: number;
  pnl_percent: number;
  status: 'open' | 'closed' | 'cancelled';
  entry_timestamp: number;
  exit_timestamp?: number;
  reasoning?: string;
  metadata?: string;
  execution_time_ms?: number;
  is_paper_trade: boolean;
}

export interface AgentPerformance {
  agent_id: string;
  model: string;
  total_trades: number;
  winning_trades: number;
  losing_trades: number;
  total_pnl: number;
  daily_pnl: number;
  weekly_pnl: number;
  monthly_pnl: number;
  win_rate: number;
  sharpe_ratio: number;
  sortino_ratio: number;
  max_drawdown: number;
  current_drawdown: number;
  avg_win: number;
  avg_loss: number;
  profit_factor: number;
  largest_win: number;
  largest_loss: number;
  consecutive_wins: number;
  consecutive_losses: number;
  max_consecutive_wins: number;
  max_consecutive_losses: number;
  total_volume: number;
  positions: number;
  last_trade_timestamp?: number;
  last_updated: number;
}

export interface DebateResult {
  success: boolean;
  debate_id: string;
  symbol: string;
  bull_argument: string;
  bear_argument: string;
  analyst_decision: string;
  final_action: 'BUY' | 'SELL' | 'HOLD';
  confidence: number;
  entry_price?: number;
  stop_loss?: number;
  take_profit?: number;
  position_size?: number;
  reasoning?: string;
  execution_time_ms: number;
}

export interface Decision {
  id: number;
  agent_id: string;
  model: string;
  decision_type: 'analysis' | 'signal' | 'trade' | 'risk';
  symbol?: string;
  decision: string;
  reasoning?: string;
  confidence?: number;
  timestamp: number;
  execution_time_ms?: number;
}

export interface EvolutionCheck {
  should_evolve: boolean;
  trigger?: string;
  performance: AgentPerformance;
}

export interface EvolutionResult {
  success: boolean;
  agent_id: string;
  trigger: string;
  old_instructions: string[];
  new_instructions: string[];
  patterns: any;
  metrics_before: {
    total_pnl: number;
    win_rate: number;
    total_trades: number;
  };
}

export interface TradeExecutionResult {
  success: boolean;
  trade_id?: number;
  trade_data?: any;
  warnings?: string[];
  execution_time_ms?: number;
  error?: string;
}

// ============================================================================
// Service Class
// ============================================================================

class AgnoTradingService {

  /**
   * Get API keys from Settings and build env var map
   * Returns a map of API keys in the format: { OPENAI_API_KEY: "sk-...", ... }
   */
  private async getApiKeysMap(): Promise<Record<string, string>> {
    try {
      const configs = await sqliteService.getLLMConfigs();

      // Build API keys map
      const apiKeys: Record<string, string> = {};
      configs.forEach(config => {
        if (config.api_key) {
          const provider = config.provider.toLowerCase();

          // Map provider name to standard env var format
          // Handle aliases: gemini -> google
          let standardProvider = provider;
          if (provider === 'gemini') {
            standardProvider = 'google';
            // Also add GEMINI_API_KEY for backward compatibility
            apiKeys['GEMINI_API_KEY'] = config.api_key;
          }

          // Format: OPENAI_API_KEY, ANTHROPIC_API_KEY, GOOGLE_API_KEY, etc.
          const keyName = `${standardProvider.toUpperCase()}_API_KEY`;
          apiKeys[keyName] = config.api_key;
        }
      });

      return apiKeys;
    } catch (error) {
      console.warn('Failed to load API keys:', error);
      return {};
    }
  }

  /**
   * List available LLM models
   */
  async listModels(provider?: string): Promise<AgnoResponse<{ models: ModelInfo[] }>> {
    await this.syncApiKeys();
    try {
      const response = await invoke<string>('agno_list_models', { provider });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Recommend best model for a task
   */
  async recommendModel(
    task: string,
    budget: 'low' | 'medium' | 'high' = 'medium',
    reasoning: boolean = false
  ): Promise<AgnoResponse<ModelRecommendation>> {
    try {
      const response = await invoke<string>('agno_recommend_model', {
        task,
        budget,
        reasoning
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Validate Agno configuration
   */
  async validateConfig(config: AgnoConfig): Promise<AgnoResponse<{ valid: boolean }>> {
    try {
      const response = await invoke<string>('agno_validate_config', {
        configJson: JSON.stringify(config)
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Create a new trading agent
   */
  async createAgent(config: AgentConfig): Promise<AgnoResponse<AgentInfo>> {
    try {
      const response = await invoke<string>('agno_create_agent', {
        agentConfigJson: JSON.stringify(config)
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Run an agent with a prompt
   */
  async runAgent(
    agentId: string,
    prompt: string,
    sessionId?: string
  ): Promise<AgnoResponse<AgentRunResult>> {
    try {
      const response = await invoke<string>('agno_run_agent', {
        agentId,
        prompt,
        sessionId
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Analyze market for a symbol
   */
  async analyzeMarket(
    symbol: string,
    agentModel: string = 'openai:gpt-4o-mini',
    analysisType: 'quick' | 'comprehensive' | 'deep' = 'comprehensive'
  ): Promise<AgnoResponse<MarketAnalysis>> {
    try {
      // Get API keys from Settings
      const apiKeys = await this.getApiKeysMap();

      const response = await invoke<string>('agno_analyze_market', {
        symbol,
        agentModel,
        analysisType,
        apiKeysJson: JSON.stringify(apiKeys)
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Generate trading signal
   */
  async generateTradeSignal(
    symbol: string,
    strategy: 'momentum' | 'breakout' | 'reversal' | 'trend_following' = 'momentum',
    agentModel: string = 'anthropic:claude-sonnet-4',
    marketData?: any
  ): Promise<AgnoResponse<TradeSignal>> {
    try {
      // Get API keys from Settings
      const apiKeys = await this.getApiKeysMap();

      const response = await invoke<string>('agno_generate_trade_signal', {
        symbol,
        strategy,
        agentModel,
        marketData: marketData ? JSON.stringify(marketData) : undefined,
        apiKeysJson: JSON.stringify(apiKeys)
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Run risk management analysis
   */
  async manageRisk(
    portfolioData: {
      positions: any[];
      total_value: number;
    },
    agentModel: string = 'openai:gpt-4',
    riskTolerance: 'conservative' | 'moderate' | 'aggressive' = 'moderate'
  ): Promise<AgnoResponse<RiskAnalysis>> {
    try {
      // Get API keys from Settings
      const apiKeys = await this.getApiKeysMap();

      const response = await invoke<string>('agno_manage_risk', {
        portfolioData: JSON.stringify(portfolioData),
        agentModel,
        riskTolerance,
        apiKeysJson: JSON.stringify(apiKeys)
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get configuration template
   */
  async getConfigTemplate(
    configType: 'default' | 'conservative' | 'aggressive' = 'default'
  ): Promise<AgnoResponse<{ config: AgnoConfig }>> {
    try {
      const response = await invoke<string>('agno_get_config_template', {
        configType
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  // ============================================================================
  // Multi-Model Orchestration (Alpha Arena Style)
  // ============================================================================

  /**
   * Create multi-model competition
   */
  async createCompetition(
    name: string,
    models: string[],
    taskType: 'trading' | 'analysis' | 'research' = 'trading'
  ): Promise<AgnoResponse<CompetitionInfo>> {
    try {
      // Get API keys from Settings
      const apiKeys = await this.getApiKeysMap();

      const response = await invoke<string>('agno_create_competition', {
        name,
        modelsJson: JSON.stringify(models),
        taskType,
        apiKeysJson: JSON.stringify(apiKeys)
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Run multi-model competition
   */
  async runCompetition(
    teamId: string,
    symbol: string,
    task: 'analyze' | 'signal' | 'evaluate' = 'analyze'
  ): Promise<AgnoResponse<CompetitionResult>> {
    try {
      // Get API keys from Settings
      const apiKeys = await this.getApiKeysMap();

      const response = await invoke<string>('agno_run_competition', {
        teamId,
        symbol,
        task,
        apiKeysJson: JSON.stringify(apiKeys)
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get model performance leaderboard
   */
  async getLeaderboard(): Promise<AgnoResponse<{ leaderboard: ModelPerformance[] }>> {
    try {
      const response = await invoke<string>('agno_get_leaderboard');
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get recent model decisions
   */
  async getRecentDecisions(limit: number = 50): Promise<AgnoResponse<{ decisions: ModelDecision[] }>> {
    try {
      const response = await invoke<string>('agno_get_recent_decisions', {
        limit
      });
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  // ============================================================================
  // Helper Methods
  // ============================================================================

  /**
   * Get available LLM providers
   */
  getAvailableProviders(): string[] {
    return [
      'openai',
      'anthropic',
      'google',
      'groq',
      'deepseek',
      'xai',
      'ollama',
      'cohere',
      'mistral',
      'together',
      'fireworks'
    ];
  }

  /**
   * Get available agent types
   */
  getAgentTypes(): Array<{ value: string; label: string; description: string }> {
    return [
      {
        value: 'market_analyst',
        label: 'Market Analyst',
        description: 'Technical analysis and market insights'
      },
      {
        value: 'trading_strategy',
        label: 'Trading Strategy',
        description: 'Signal generation and trade recommendations'
      },
      {
        value: 'risk_manager',
        label: 'Risk Manager',
        description: 'Portfolio risk assessment and position sizing'
      },
      {
        value: 'portfolio_manager',
        label: 'Portfolio Manager',
        description: 'Asset allocation and portfolio optimization'
      },
      {
        value: 'sentiment_analyst',
        label: 'Sentiment Analyst',
        description: 'Market sentiment and news analysis'
      },
      {
        value: 'custom',
        label: 'Custom Agent',
        description: 'Create your own custom agent'
      }
    ];
  }

  /**
   * Get available trading strategies
   */
  getTradingStrategies(): Array<{ value: string; label: string; description: string }> {
    return [
      {
        value: 'momentum',
        label: 'Momentum',
        description: 'Follow strong price trends with momentum indicators'
      },
      {
        value: 'breakout',
        label: 'Breakout',
        description: 'Trade breakouts from consolidation zones'
      },
      {
        value: 'reversal',
        label: 'Reversal',
        description: 'Identify trend reversals at key levels'
      },
      {
        value: 'trend_following',
        label: 'Trend Following',
        description: 'Enter on pullbacks in established trends'
      }
    ];
  }

  /**
   * Format model string
   */
  formatModelString(provider: string, model: string): string {
    return `${provider}:${model}`;
  }

  /**
   * Parse model string
   */
  parseModelString(modelString: string): { provider: string; model: string } {
    const [provider, model] = modelString.split(':');
    return { provider, model };
  }

  // ============================================================================
  // NEW METHODS: Auto-Trading, Debate, Evolution
  // ============================================================================

  /**
   * Execute a trade based on signal
   */
  async executeTrade(
    signal: TradeSignal & { side?: string; quantity?: number; entry_price?: number; stop_loss?: number; take_profit?: number },
    portfolio: { total_value: number; positions: any[]; daily_pnl: number },
    agentId: string,
    model: string
  ): Promise<AgnoResponse<TradeExecutionResult>> {
    try {
      const apiKeysMap = await this.getApiKeysMap();

      const response = await invoke<string>('agno_execute_trade', {
        signalJson: JSON.stringify(signal),
        portfolioJson: JSON.stringify(portfolio),
        agentId,
        model,
        apiKeysJson: JSON.stringify(apiKeysMap)
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Execute trade error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Close an open position
   */
  async closePosition(
    tradeId: number,
    exitPrice: number,
    reason: string = 'manual'
  ): Promise<AgnoResponse<any>> {
    try {
      const response = await invoke<string>('agno_close_position', {
        tradeId,
        exitPrice,
        reason
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Close position error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get trade history for an agent
   */
  async getAgentTrades(
    agentId: string,
    limit: number = 100,
    status?: string
  ): Promise<AgnoResponse<{ trades: AgentTrade[] }>> {
    try {
      const response = await invoke<string>('agno_get_agent_trades', {
        agentId,
        limit,
        status
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Get agent trades error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get performance metrics for an agent
   */
  async getAgentPerformance(
    agentId: string
  ): Promise<AgnoResponse<{ performance: AgentPerformance }>> {
    try {
      const response = await invoke<string>('agno_get_agent_performance', {
        agentId
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Get agent performance error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get leaderboard from database
   */
  async getDbLeaderboard(
    limit: number = 20
  ): Promise<AgnoResponse<{ leaderboard: AgentPerformance[] }>> {
    try {
      const response = await invoke<string>('agno_get_db_leaderboard', {
        limit
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Get leaderboard error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get recent decisions from database
   */
  async getDbDecisions(
    limit: number = 50,
    agentId?: string
  ): Promise<AgnoResponse<{ decisions: Decision[] }>> {
    try {
      const response = await invoke<string>('agno_get_db_decisions', {
        limit,
        agentId
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Get decisions error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Run a Bull/Bear/Analyst debate
   */
  async runDebate(
    symbol: string,
    marketData: any,
    bullModel: string,
    bearModel: string,
    analystModel: string
  ): Promise<AgnoResponse<DebateResult>> {
    try {
      const apiKeysMap = await this.getApiKeysMap();

      const response = await invoke<string>('agno_run_debate', {
        symbol,
        marketDataJson: JSON.stringify(marketData),
        bullModel,
        bearModel,
        analystModel,
        apiKeysJson: JSON.stringify(apiKeysMap)
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Run debate error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get recent debate sessions
   */
  async getRecentDebates(
    limit: number = 10
  ): Promise<AgnoResponse<{ debates: any[] }>> {
    try {
      const response = await invoke<string>('agno_get_recent_debates', {
        limit
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Get debates error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Check if agent should evolve
   */
  async checkEvolution(
    agentId: string
  ): Promise<AgnoResponse<EvolutionCheck>> {
    try {
      const response = await invoke<string>('agno_check_evolution', {
        agentId
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Check evolution error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Evolve an agent based on performance
   */
  async evolveAgent(
    agentId: string,
    model: string,
    currentInstructions: string[],
    trigger: string,
    notes?: string
  ): Promise<AgnoResponse<EvolutionResult>> {
    try {
      const response = await invoke<string>('agno_evolve_agent', {
        agentId,
        model,
        currentInstructionsJson: JSON.stringify(currentInstructions),
        trigger,
        notes
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Evolve agent error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Get evolution summary for an agent
   */
  async getEvolutionSummary(
    agentId: string
  ): Promise<AgnoResponse<any>> {
    try {
      const response = await invoke<string>('agno_get_evolution_summary', {
        agentId
      });

      return this.parseResponse(response);
    } catch (error) {
      console.error('[AgnoService] Get evolution summary error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }

  /**
   * Sync API keys from Settings to ensure they're available
   * This is a no-op since getApiKeysMap() already loads from Settings
   */
  private async syncApiKeys(): Promise<void> {
    // API keys are loaded dynamically from Settings in getApiKeysMap()
    // This method exists for compatibility but doesn't need to do anything
    return Promise.resolve();
  }

  /**
   * Parse JSON response from Rust commands
   */
  private parseResponse<T = any>(response: string): AgnoResponse<T> {
    try {
      return JSON.parse(response);
    } catch (error) {
      return {
        success: false,
        error: `Failed to parse response: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }
}

// ============================================================================
// Export singleton instance
// ============================================================================

export const agnoTradingService = new AgnoTradingService();
export default agnoTradingService;
