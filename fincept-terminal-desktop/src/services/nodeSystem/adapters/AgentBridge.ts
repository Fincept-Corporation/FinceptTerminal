/**
 * Agent Bridge
 * Connects workflow nodes to 30+ Python AI agents
 * Provides unified interface for agent execution and multi-agent orchestration
 */

import { IDataObject } from '../types';

// ================================
// TYPES
// ================================

export type AgentCategory =
  | 'geopolitics'
  | 'hedgeFund'
  | 'investor'
  | 'economic'
  | 'trader'
  | 'all';

export type LLMProvider =
  | 'openai'
  | 'anthropic'
  | 'google'
  | 'ollama'
  | 'openrouter'
  | 'deepseek'
  | 'groq'
  | 'active'; // Use currently active provider

export interface AgentMetadata {
  id: string;
  name: string;
  category: AgentCategory;
  description: string;
  icon?: string;
  color?: string;
  parameters: AgentParameter[];
}

export interface AgentParameter {
  name: string;
  type: 'string' | 'number' | 'boolean' | 'options';
  description: string;
  required: boolean;
  default_value?: any;
  options?: Array<{ name: string; value: string }>;
}

export interface AgentExecutionRequest {
  agentId: string;
  category: AgentCategory;
  parameters: IDataObject;
  llmProvider?: LLMProvider;
  llmModel?: string;
  inputs?: IDataObject;
}

export interface AgentExecutionResult {
  success: boolean;
  agentId: string;
  agentName?: string;
  data?: any;
  error?: string;
  executionTime?: number;
  llmProvider?: string;
  llmModel?: string;
  tokensUsed?: number;
}

export interface MultiAgentRequest {
  agents: AgentExecutionRequest[];
  mediationPrompt?: string;
  parallel?: boolean;
  llmProvider?: LLMProvider;
}

export interface MultiAgentResult {
  success: boolean;
  agentResults: AgentExecutionResult[];
  mediatedAnalysis?: string;
  executionTime?: number;
  error?: string;
}

// ================================
// AGENT DEFINITIONS
// ================================

const AGENT_DEFINITIONS: AgentMetadata[] = [
  // ==================== GEOPOLITICS AGENTS ====================
  // Grand Chessboard Framework
  {
    id: 'american_primacy',
    name: 'American Primacy Analyst',
    category: 'geopolitics',
    description: 'Analyzes US hegemonic strategy based on Brzezinski\'s Grand Chessboard',
    icon: '🇺🇸',
    color: '#3b82f6',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
      { name: 'region', type: 'string', description: 'Focus region', required: false, default_value: 'Global' },
    ],
  },
  {
    id: 'eurasian_balkans',
    name: 'Eurasian Balkans Analyst',
    category: 'geopolitics',
    description: 'Analyzes Central Asian geopolitical dynamics',
    icon: '🌍',
    color: '#8b5cf6',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'heartland_theory',
    name: 'Heartland Theory Analyst',
    category: 'geopolitics',
    description: 'Analyzes Mackinder\'s Heartland theory implications',
    icon: '🗺️',
    color: '#6366f1',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'pivots_analyst',
    name: 'Geopolitical Pivots Analyst',
    category: 'geopolitics',
    description: 'Analyzes key pivot states in global politics',
    icon: '🔄',
    color: '#a855f7',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'players_analyst',
    name: 'Global Players Analyst',
    category: 'geopolitics',
    description: 'Analyzes major power strategies and interactions',
    icon: '♟️',
    color: '#ec4899',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },

  // Prisoners of Geography Framework
  {
    id: 'russia_geography',
    name: 'Russia Geographic Analyst',
    category: 'geopolitics',
    description: 'Analyzes Russia through geographic determinism lens',
    icon: '🇷🇺',
    color: '#ef4444',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'china_geography',
    name: 'China Geographic Analyst',
    category: 'geopolitics',
    description: 'Analyzes China through geographic determinism lens',
    icon: '🇨🇳',
    color: '#dc2626',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'usa_geography',
    name: 'USA Geographic Analyst',
    category: 'geopolitics',
    description: 'Analyzes USA through geographic determinism lens',
    icon: '🇺🇸',
    color: '#2563eb',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'europe_geography',
    name: 'Europe Geographic Analyst',
    category: 'geopolitics',
    description: 'Analyzes Europe through geographic determinism lens',
    icon: '🇪🇺',
    color: '#1d4ed8',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'middle_east_geography',
    name: 'Middle East Geographic Analyst',
    category: 'geopolitics',
    description: 'Analyzes Middle East through geographic determinism lens',
    icon: '🏜️',
    color: '#ca8a04',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },

  // World Order Framework
  {
    id: 'american_order',
    name: 'American Order Analyst',
    category: 'geopolitics',
    description: 'Analyzes the American-led world order',
    icon: '🦅',
    color: '#3b82f6',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'chinese_order',
    name: 'Chinese Order Analyst',
    category: 'geopolitics',
    description: 'Analyzes China\'s vision of world order',
    icon: '🐉',
    color: '#dc2626',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'multipolar_order',
    name: 'Multipolar Order Analyst',
    category: 'geopolitics',
    description: 'Analyzes emerging multipolar world order',
    icon: '🌐',
    color: '#10b981',
    parameters: [
      { name: 'topic', type: 'string', description: 'Topic to analyze', required: true, default_value: '' },
    ],
  },

  // ==================== HEDGE FUND AGENTS ====================
  {
    id: 'bridgewater',
    name: 'Bridgewater Analyst',
    category: 'hedgeFund',
    description: 'Analyzes markets using Ray Dalio\'s Bridgewater principles',
    icon: '🌊',
    color: '#0ea5e9',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
      { name: 'analysis_type', type: 'options', description: 'Type of analysis', required: false, default_value: 'comprehensive', options: [
        { name: 'Comprehensive', value: 'comprehensive' },
        { name: 'Macro', value: 'macro' },
        { name: 'Debt Cycle', value: 'debt_cycle' },
      ]},
    ],
  },
  {
    id: 'citadel',
    name: 'Citadel Analyst',
    category: 'hedgeFund',
    description: 'Analyzes markets using Citadel\'s quantitative approach',
    icon: '🏰',
    color: '#6366f1',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'renaissance',
    name: 'Renaissance Technologies Analyst',
    category: 'hedgeFund',
    description: 'Analyzes markets using Renaissance\'s quantitative methods',
    icon: '📊',
    color: '#8b5cf6',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'two_sigma',
    name: 'Two Sigma Analyst',
    category: 'hedgeFund',
    description: 'Analyzes markets using Two Sigma\'s data science approach',
    icon: '📈',
    color: '#a855f7',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'de_shaw',
    name: 'D.E. Shaw Analyst',
    category: 'hedgeFund',
    description: 'Analyzes markets using D.E. Shaw\'s computational strategies',
    icon: '🔬',
    color: '#ec4899',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'elliott',
    name: 'Elliott Management Analyst',
    category: 'hedgeFund',
    description: 'Analyzes markets using Elliott\'s activist investing approach',
    icon: '🦅',
    color: '#f97316',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'pershing_square',
    name: 'Pershing Square Analyst',
    category: 'hedgeFund',
    description: 'Analyzes markets using Bill Ackman\'s activist approach',
    icon: '🎯',
    color: '#14b8a6',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'aqr',
    name: 'AQR Capital Analyst',
    category: 'hedgeFund',
    description: 'Analyzes markets using AQR\'s factor-based approach',
    icon: '⚖️',
    color: '#22c55e',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
    ],
  },

  // ==================== INVESTOR AGENTS ====================
  {
    id: 'warren_buffett',
    name: 'Warren Buffett Analyst',
    category: 'investor',
    description: 'Analyzes stocks using Warren Buffett\'s value investing principles',
    icon: '🎩',
    color: '#f59e0b',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
      { name: 'include_moat', type: 'boolean', description: 'Include competitive moat analysis', required: false, default_value: true },
    ],
  },
  {
    id: 'benjamin_graham',
    name: 'Benjamin Graham Analyst',
    category: 'investor',
    description: 'Analyzes stocks using Ben Graham\'s deep value approach',
    icon: '📚',
    color: '#84cc16',
    parameters: [
      { name: 'symbol', type: 'string', description: 'Stock symbol to analyze', required: true, default_value: '' },
      { name: 'margin_of_safety', type: 'number', description: 'Required margin of safety (%)', required: false, default_value: 30 },
    ],
  },

  // ==================== ECONOMIC AGENTS ====================
  {
    id: 'capitalism_analyst',
    name: 'Capitalism Analyst',
    category: 'economic',
    description: 'Analyzes from free market capitalism perspective',
    icon: '💰',
    color: '#22c55e',
    parameters: [
      { name: 'topic', type: 'string', description: 'Economic topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'keynesian_analyst',
    name: 'Keynesian Analyst',
    category: 'economic',
    description: 'Analyzes from Keynesian economics perspective',
    icon: '🏛️',
    color: '#3b82f6',
    parameters: [
      { name: 'topic', type: 'string', description: 'Economic topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'neoliberal_analyst',
    name: 'Neoliberal Analyst',
    category: 'economic',
    description: 'Analyzes from neoliberal economics perspective',
    icon: '🌐',
    color: '#8b5cf6',
    parameters: [
      { name: 'topic', type: 'string', description: 'Economic topic to analyze', required: true, default_value: '' },
    ],
  },
  {
    id: 'mixed_economy_analyst',
    name: 'Mixed Economy Analyst',
    category: 'economic',
    description: 'Analyzes from mixed economy perspective',
    icon: '⚖️',
    color: '#f59e0b',
    parameters: [
      { name: 'topic', type: 'string', description: 'Economic topic to analyze', required: true, default_value: '' },
    ],
  },
];

// ================================
// AGENT BRIDGE
// ================================

class AgentBridgeClass {
  private pythonAgentService: any = null;
  private agentLLMService: any = null;

  /**
   * Get all available agents
   */
  async getAvailableAgents(category?: AgentCategory): Promise<AgentMetadata[]> {
    // First try to get from Python agent service
    try {
      if (!this.pythonAgentService) {
        const module = await import('@/services/chat/pythonAgentService');
        this.pythonAgentService = module.pythonAgentService;
      }

      const agents = await this.pythonAgentService.getAvailableAgents();
      if (agents && agents.length > 0) {
        // Merge with local definitions
        const merged = this.mergeAgentDefinitions(agents);
        return category && category !== 'all'
          ? merged.filter((a) => a.category === category)
          : merged;
      }
    } catch (error) {
      console.warn('[AgentBridge] Could not load agents from Python service:', error);
    }

    // Fall back to local definitions
    return category && category !== 'all'
      ? AGENT_DEFINITIONS.filter((a) => a.category === category)
      : AGENT_DEFINITIONS;
  }

  /**
   * Get agent by ID
   */
  async getAgent(agentId: string): Promise<AgentMetadata | null> {
    const agents = await this.getAvailableAgents();
    return agents.find((a) => a.id === agentId) || null;
  }

  /**
   * Get agent categories
   */
  getCategories(): Array<{ id: AgentCategory; name: string; icon: string }> {
    return [
      { id: 'geopolitics', name: 'Geopolitics', icon: '🌍' },
      { id: 'hedgeFund', name: 'Hedge Funds', icon: '🏦' },
      { id: 'investor', name: 'Investors', icon: '📈' },
      { id: 'economic', name: 'Economic', icon: '💹' },
      { id: 'trader', name: 'Traders', icon: '📊' },
    ];
  }

  /**
   * Execute a single agent
   */
  async executeAgent(request: AgentExecutionRequest): Promise<AgentExecutionResult> {
    const startTime = Date.now();

    try {
      // Try Python agent service first
      if (!this.pythonAgentService) {
        try {
          const module = await import('@/services/chat/pythonAgentService');
          this.pythonAgentService = module.pythonAgentService;
        } catch (error) {
          console.warn('[AgentBridge] Python agent service not available');
        }
      }

      if (this.pythonAgentService) {
        const result = await this.pythonAgentService.executeAgent(
          request.agentId,
          request.parameters,
          request.inputs || {}
        );

        return {
          success: result.success,
          agentId: request.agentId,
          agentName: (await this.getAgent(request.agentId))?.name,
          data: result.data,
          error: result.error,
          executionTime: Date.now() - startTime,
          llmProvider: request.llmProvider,
          llmModel: request.llmModel,
        };
      }

      // Fall back to mock response
      return this.getMockAgentResponse(request, startTime);
    } catch (error: any) {
      return {
        success: false,
        agentId: request.agentId,
        error: error.message || 'Agent execution failed',
        executionTime: Date.now() - startTime,
      };
    }
  }

  /**
   * Execute multiple agents
   */
  async executeMultipleAgents(request: MultiAgentRequest): Promise<MultiAgentResult> {
    const startTime = Date.now();

    try {
      let agentResults: AgentExecutionResult[];

      if (request.parallel) {
        // Execute in parallel
        agentResults = await Promise.all(
          request.agents.map((agent) => this.executeAgent(agent))
        );
      } else {
        // Execute sequentially
        agentResults = [];
        for (const agent of request.agents) {
          const result = await this.executeAgent(agent);
          agentResults.push(result);
        }
      }

      // Mediate results if requested
      let mediatedAnalysis: string | undefined;
      if (request.mediationPrompt) {
        mediatedAnalysis = await this.mediateResults(agentResults, request.mediationPrompt);
      }

      return {
        success: agentResults.every((r) => r.success),
        agentResults,
        mediatedAnalysis,
        executionTime: Date.now() - startTime,
      };
    } catch (error: any) {
      return {
        success: false,
        agentResults: [],
        error: error.message || 'Multi-agent execution failed',
        executionTime: Date.now() - startTime,
      };
    }
  }

  /**
   * Mediate agent outputs
   */
  async mediateResults(
    results: AgentExecutionResult[],
    mediationPrompt?: string
  ): Promise<string> {
    try {
      // Try to use agentLLMService
      if (!this.agentLLMService) {
        try {
          const module = await import('@/services/chat/agentLLMService');
          this.agentLLMService = module.agentLLMService;
        } catch (error) {
          console.warn('[AgentBridge] Agent LLM service not available');
        }
      }

      if (this.agentLLMService) {
        const agentOutputs = results.map((r) => ({
          agentName: r.agentName || r.agentId,
          output: r.data,
        }));

        const result = await this.agentLLMService.synthesizeMultipleAgents(
          agentOutputs,
          mediationPrompt
        );

        if (result.success) {
          return result.mediatedContext;
        }
      }

      // Fall back to simple concatenation
      return results
        .map((r) => `**${r.agentName || r.agentId}:**\n${JSON.stringify(r.data, null, 2)}`)
        .join('\n\n---\n\n');
    } catch (error) {
      console.error('[AgentBridge] Mediation failed:', error);
      return 'Mediation failed. Raw results available in agent outputs.';
    }
  }

  /**
   * Get available LLM providers
   */
  async getLLMProviders(): Promise<Array<{ id: LLMProvider; name: string; available: boolean }>> {
    return [
      { id: 'active', name: 'Active Provider', available: true },
      { id: 'openai', name: 'OpenAI (GPT-4)', available: true },
      { id: 'anthropic', name: 'Anthropic (Claude)', available: true },
      { id: 'google', name: 'Google (Gemini)', available: true },
      { id: 'ollama', name: 'Ollama (Local)', available: true },
      { id: 'groq', name: 'Groq (Fast)', available: true },
      { id: 'deepseek', name: 'DeepSeek', available: true },
      { id: 'openrouter', name: 'OpenRouter', available: true },
    ];
  }

  // ================================
  // PRIVATE METHODS
  // ================================

  /**
   * Merge agent definitions from Python service with local
   */
  private mergeAgentDefinitions(pythonAgents: any[]): AgentMetadata[] {
    const merged: AgentMetadata[] = [];
    const seenIds = new Set<string>();

    // Add Python agents first
    for (const agent of pythonAgents) {
      const normalized: AgentMetadata = {
        id: agent.id || agent.agent_type,
        name: agent.name,
        category: this.normalizeCategory(agent.category),
        description: agent.description,
        icon: agent.icon,
        color: agent.color,
        parameters: agent.parameters || [],
      };
      merged.push(normalized);
      seenIds.add(normalized.id);
    }

    // Add local definitions not in Python
    for (const local of AGENT_DEFINITIONS) {
      if (!seenIds.has(local.id)) {
        merged.push(local);
      }
    }

    return merged;
  }

  /**
   * Normalize category from various formats
   */
  private normalizeCategory(category: string): AgentCategory {
    const categoryMap: Record<string, AgentCategory> = {
      geopolitics: 'geopolitics',
      'hedge-fund': 'hedgeFund',
      hedgefund: 'hedgeFund',
      hedge_fund: 'hedgeFund',
      investor: 'investor',
      economic: 'economic',
      trader: 'trader',
    };
    return categoryMap[category?.toLowerCase()] || 'economic';
  }

  /**
   * Get mock agent response for development
   */
  private getMockAgentResponse(
    request: AgentExecutionRequest,
    startTime: number
  ): AgentExecutionResult {
    const agent = AGENT_DEFINITIONS.find((a) => a.id === request.agentId);

    return {
      success: true,
      agentId: request.agentId,
      agentName: agent?.name,
      data: {
        analysis: `Mock analysis from ${agent?.name || request.agentId}`,
        parameters: request.parameters,
        recommendations: [
          'This is a mock response for development',
          'Connect to Python backend for real analysis',
        ],
        confidence: 0.85,
        timestamp: new Date().toISOString(),
      },
      executionTime: Date.now() - startTime,
      llmProvider: request.llmProvider,
    };
  }
}

// Export singleton
export const AgentBridge = new AgentBridgeClass();
export { AgentBridgeClass, AGENT_DEFINITIONS };
