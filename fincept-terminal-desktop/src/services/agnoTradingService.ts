/**
 * Agno Trading Service
 *
 * TypeScript wrapper for Agno AI Trading Agent system.
 * Provides type-safe interface to Rust/Python Agno commands.
 * Reads API keys from Settings tab LLM configuration.
 */

import { invoke } from '@tauri-apps/api/core';
import { sqliteService } from './sqliteService';

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
// Service Class
// ============================================================================

class AgnoTradingService {

  /**
   * Sync API keys from Settings to environment
   * Called before each API request
   */
  private async syncApiKeys(): Promise<void> {
    try {
      const configs = await sqliteService.getLLMConfigs();

      // Map provider names to environment variable format
      const keyMap: Record<string, string> = {
        'openai': 'OPENAI_API_KEY',
        'anthropic': 'ANTHROPIC_API_KEY',
        'google': 'GOOGLE_API_KEY',
        'groq': 'GROQ_API_KEY',
        'deepseek': 'DEEPSEEK_API_KEY',
        'xai': 'XAI_API_KEY',
        'cohere': 'COHERE_API_KEY',
        'mistral': 'MISTRAL_API_KEY',
        'together': 'TOGETHER_API_KEY',
        'fireworks': 'FIREWORKS_API_KEY'
      };

      // Send API keys to Rust backend
      for (const config of configs) {
        if (config.api_key && keyMap[config.provider]) {
          await invoke('set_env_var', {
            key: keyMap[config.provider],
            value: config.api_key
          }).catch(() => {
            // Ignore errors - backend may not support this command
            console.log(`Could not sync ${config.provider} API key to backend`);
          });
        }
      }
    } catch (error) {
      console.warn('Failed to sync API keys:', error);
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
      const response = await invoke<string>('agno_analyze_market', {
        symbol,
        agentModel,
        analysisType
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
      const response = await invoke<string>('agno_generate_trade_signal', {
        symbol,
        strategy,
        agentModel,
        marketData: marketData ? JSON.stringify(marketData) : undefined
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
      const response = await invoke<string>('agno_manage_risk', {
        portfolioData: JSON.stringify(portfolioData),
        agentModel,
        riskTolerance
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
    await this.syncApiKeys();
    try {
      const response = await invoke<string>('agno_create_competition', {
        name,
        modelsJson: JSON.stringify(models),
        taskType
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
    await this.syncApiKeys();
    try {
      const response = await invoke<string>('agno_run_competition', {
        teamId,
        symbol,
        task
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
}

// ============================================================================
// Export singleton instance
// ============================================================================

export const agnoTradingService = new AgnoTradingService();
export default agnoTradingService;
