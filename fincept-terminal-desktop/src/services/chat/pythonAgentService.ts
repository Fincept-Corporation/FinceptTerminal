// Python Agent Service - Interface to execute Python agents via finagent_core
import { invoke } from '@tauri-apps/api/core';
import { pythonAgentLogger } from '../core/loggerService';
import { buildApiKeysMap, getLLMConfigs } from '../core/sqliteService';
export interface AgentParameter {
  name: string;
  label: string;
  type: string;
  required: boolean;
  default_value?: any;
  description?: string;
}

export interface AgentMetadata {
  id: string;
  name: string;
  type: string;
  category: string;
  description: string;
  script_path: string;
  parameters: AgentParameter[];
  required_inputs: string[];
  outputs: string[];
  icon: string;
  color: string;
}

export interface AgentExecutionResult {
  success: boolean;
  data?: any;
  error?: string;
  execution_time_ms: number;
  agent_type: string;
}

const AGENT_INSTRUCTIONS_FALLBACK: Record<string, string> = {
  american_primacy: 'You are a geopolitical analyst specializing in American primacy. Analyze events through US global dominance and the liberal international order.',
  eurasian_balkans: 'You are a geopolitical analyst focusing on the Eurasian Balkans. Analyze competition for influence across Central Asia and the Caucasus.',
  heartland_theory: 'You are a geopolitical analyst applying the Heartland Theory. Analyze global power through control of the Eurasian heartland.',
  pivots_analyst: 'You are a geopolitical analyst identifying pivot states whose orientation affects regional and global power balances.',
  players_analyst: 'You are a geopolitical analyst assessing key geostrategic players, capabilities, interests, and strategies.',
  american_order: 'You are a geopolitical analyst examining the American-led world order, its institutions, alliances, and challenges.',
  chinese_order: 'You are a geopolitical analyst examining the Chinese vision for an alternative world order, including Belt and Road.',
  multipolar_order: 'You are a geopolitical analyst focused on the emerging multipolar world order with multiple competing power centers.',
  bridgewater: 'You are a financial analyst embodying Bridgewater Associates approach. Apply all-weather principles, debt cycle analysis, and macroeconomic frameworks.',
  citadel: 'You are a quantitative financial analyst embodying Citadel approach. Apply systematic data-driven analysis combining macro and micro strategies.',
  renaissance: 'You are a quantitative analyst embodying Renaissance Technologies approach. Focus on mathematical patterns and statistical arbitrage.',
  two_sigma: 'You are a data-driven financial analyst embodying Two Sigma approach. Apply machine learning and alternative data methods.',
  de_shaw: 'You are a computational financial analyst embodying D.E. Shaw approach. Apply complex mathematical models and quantitative analysis.',
  elliott: 'You are an activist investor analyst embodying Elliott Management approach. Focus on distressed situations and event-driven opportunities.',
  pershing_square: 'You are a concentrated value investor embodying Pershing Square approach. Apply deep fundamental analysis with activist catalysts.',
  aqr: 'You are a factor-based quantitative analyst embodying AQR approach. Apply value, momentum, quality, and carry factors systematically.',
};

async function buildApiKeys(): Promise<Record<string, string>> {
  return buildApiKeysMap();
}

function extractQuery(parameters: Record<string, any>, inputs: Record<string, any>): string {
  return parameters.query || parameters.topic || parameters.symbol
    || inputs.query || inputs.text
    || JSON.stringify({ ...parameters, ...inputs });
}

class PythonAgentService {
  private agentMetadataCache: AgentMetadata[] | null = null;

  async getAvailableAgents(): Promise<AgentMetadata[]> {
    try {
      const agents = await invoke<AgentMetadata[]>('list_available_agents');
      pythonAgentLogger.info('Loaded agents:', agents.length);
      this.agentMetadataCache = agents;
      return agents;
    } catch (error) {
      pythonAgentLogger.error('Failed to load agents:', error);
      return [];
    }
  }

  async executeAgent(
    agentType: string,
    parameters: Record<string, any>,
    inputs: Record<string, any>,
    llmProvider?: string,
    llmModel?: string,
  ): Promise<AgentExecutionResult> {
    const startTime = Date.now();
    try {
      pythonAgentLogger.info('Executing agent via finagent_core: ' + agentType);

      const apiKeys = await buildApiKeys();

      let provider = llmProvider || 'openai';
      let modelId = llmModel || 'gpt-4-turbo';
      if (!llmProvider) {
        try {
          const configs = await getLLMConfigs();
          if (configs.length > 0) { provider = configs[0].provider; modelId = configs[0].model; }
        } catch { /* keep defaults */ }
      }

      let instructions = AGENT_INSTRUCTIONS_FALLBACK[agentType]
        || 'You are a specialized AI agent: ' + agentType + '. Provide detailed, professional analysis.';
      if (this.agentMetadataCache) {
        const cached = this.agentMetadataCache.find(a => a.id === agentType) as any;
        if (cached?.instructions) instructions = cached.instructions;
      }

      const query = extractQuery(parameters, inputs);
      const payload = {
        action: 'run',
        api_keys: apiKeys,
        config: { model: { provider, model_id: modelId }, instructions },
        params: { query },
      };

      const resultStr = await invoke<string>('execute_core_agent', { payload });
      const parsed = JSON.parse(resultStr);
      pythonAgentLogger.info('Agent ' + agentType + ' completed via finagent_core');
      return {
        success: parsed.success,
        data: { analysis: parsed.response, raw: parsed.response },
        error: parsed.error,
        execution_time_ms: Date.now() - startTime,
        agent_type: agentType,
      };
    } catch (error: any) {
      pythonAgentLogger.error('Agent ' + agentType + ' failed:', error);
      return { success: false, error: error.message || 'Unknown error', execution_time_ms: Date.now() - startTime, agent_type: agentType };
    }
  }

  async getAgentMetadata(agentType: string): Promise<AgentMetadata | null> {
    try {
      const metadata = await invoke<AgentMetadata>('get_agent_metadata', { agentType });
      return metadata;
    } catch (error) {
      pythonAgentLogger.error('Failed to get metadata for ' + agentType + ':', error);
      return null;
    }
  }

  async getAgentsByCategory(category: string): Promise<AgentMetadata[]> {
    const allAgents = await this.getAvailableAgents();
    return allAgents.filter(agent => agent.category === category);
  }

  async getCategories(): Promise<string[]> {
    const allAgents = await this.getAvailableAgents();
    const categories = new Set(allAgents.map(agent => agent.category));
    return Array.from(categories);
  }
}

export const pythonAgentService = new PythonAgentService();
