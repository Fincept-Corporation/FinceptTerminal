/**
 * Multi-Agent Node
 * Executes multiple AI agents in parallel and aggregates results
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { AgentBridge, AGENT_DEFINITIONS } from '../../adapters/AgentBridge';

export class MultiAgentNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Multi-Agent Analysis',
    name: 'multiAgent',
    icon: 'file:multi-agent.svg',
    group: ['agents'],
    version: 1,
    subtitle: '={{$parameter["agents"].length}} agents',
    description: 'Execute multiple AI agents in parallel and aggregate results',
    defaults: {
      name: 'Multi-Agent',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Agent Selection',
        name: 'agentSelection',
        type: NodePropertyType.Options,
        default: 'preset',
        options: [
          { name: 'Preset Group', value: 'preset' },
          { name: 'Custom Selection', value: 'custom' },
          { name: 'By Category', value: 'category' },
        ],
        description: 'How to select agents to run',
      },
      {
        displayName: 'Preset Group',
        name: 'presetGroup',
        type: NodePropertyType.Options,
        default: 'geopolitics_full',
        options: [
          { name: 'All Geopolitics (19 agents)', value: 'geopolitics_full' },
          { name: 'Core Geopolitics (5 agents)', value: 'geopolitics_core' },
          { name: 'All Hedge Funds (8 agents)', value: 'hedgefund_full' },
          { name: 'Value Investors', value: 'investors_value' },
          { name: 'Growth Investors', value: 'investors_growth' },
          { name: 'Macro Analysis', value: 'macro_full' },
          { name: 'Comprehensive (All agents)', value: 'all' },
        ],
        description: 'Preset group of agents to run',
        displayOptions: {
          show: { agentSelection: ['preset'] },
        },
      },
      {
        displayName: 'Categories',
        name: 'categories',
        type: NodePropertyType.MultiOptions,
        default: ['geopolitics'],
        options: [
          { name: 'Geopolitics', value: 'geopolitics' },
          { name: 'Hedge Fund', value: 'hedgeFund' },
          { name: 'Investor', value: 'investor' },
          { name: 'Economic', value: 'economic' },
          { name: 'Trader', value: 'trader' },
        ],
        description: 'Categories of agents to include',
        displayOptions: {
          show: { agentSelection: ['category'] },
        },
      },
      {
        displayName: 'Selected Agents',
        name: 'selectedAgents',
        type: NodePropertyType.MultiOptions,
        default: [],
        options: AGENT_DEFINITIONS.map(a => ({ name: a.name, value: a.id })),
        description: 'Specific agents to run',
        displayOptions: {
          show: { agentSelection: ['custom'] },
        },
      },
      {
        displayName: 'Query',
        name: 'query',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Analyze the implications of...',
        description: 'Query for all agents to analyze',
        typeOptions: {
          rows: 4,
        },
      },
      {
        displayName: 'Use Input as Query',
        name: 'useInputQuery',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use data from input as the query',
      },
      {
        displayName: 'Execution Mode',
        name: 'executionMode',
        type: NodePropertyType.Options,
        default: 'parallel',
        options: [
          { name: 'Parallel (Fast)', value: 'parallel', description: 'Run all agents simultaneously' },
          { name: 'Sequential', value: 'sequential', description: 'Run agents one after another' },
          { name: 'Batched', value: 'batched', description: 'Run in batches of N agents' },
        ],
        description: 'How to execute the agents',
      },
      {
        displayName: 'Batch Size',
        name: 'batchSize',
        type: NodePropertyType.Number,
        default: 5,
        description: 'Number of agents per batch',
        displayOptions: {
          show: { executionMode: ['batched'] },
        },
      },
      {
        displayName: 'Aggregation Mode',
        name: 'aggregationMode',
        type: NodePropertyType.Options,
        default: 'separate',
        options: [
          { name: 'Separate Responses', value: 'separate', description: 'Return each agent response separately' },
          { name: 'Synthesized Summary', value: 'synthesize', description: 'Use mediator to synthesize responses' },
          { name: 'Consensus View', value: 'consensus', description: 'Find areas of agreement/disagreement' },
          { name: 'Both', value: 'both', description: 'Return both individual and synthesized' },
        ],
        description: 'How to aggregate agent responses',
      },
      {
        displayName: 'LLM Provider',
        name: 'llmProvider',
        type: NodePropertyType.Options,
        default: 'ollama',
        options: [
          { name: 'Ollama (Local)', value: 'ollama' },
          { name: 'OpenAI', value: 'openai' },
          { name: 'Anthropic', value: 'anthropic' },
          { name: 'Groq', value: 'groq' },
        ],
        description: 'LLM provider to use',
      },
      {
        displayName: 'Model',
        name: 'model',
        type: NodePropertyType.String,
        default: 'llama3',
        description: 'Specific model to use',
      },
      {
        displayName: 'Max Concurrency',
        name: 'maxConcurrency',
        type: NodePropertyType.Number,
        default: 5,
        description: 'Maximum concurrent agent executions',
        displayOptions: {
          show: { executionMode: ['parallel'] },
        },
      },
      {
        displayName: 'Continue on Error',
        name: 'continueOnError',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Continue executing other agents if one fails',
      },
      {
        displayName: 'Timeout per Agent (seconds)',
        name: 'timeout',
        type: NodePropertyType.Number,
        default: 120,
        description: 'Maximum execution time per agent',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const agentSelection = this.getNodeParameter('agentSelection', 0) as string;
    const useInputQuery = this.getNodeParameter('useInputQuery', 0) as boolean;
    const executionMode = this.getNodeParameter('executionMode', 0) as string;
    const aggregationMode = this.getNodeParameter('aggregationMode', 0) as string;
    const llmProvider = this.getNodeParameter('llmProvider', 0) as string;
    const model = this.getNodeParameter('model', 0) as string;
    const continueOnError = this.getNodeParameter('continueOnError', 0) as boolean;
    const timeout = this.getNodeParameter('timeout', 0) as number;

    // Get query
    let query: string;
    if (useInputQuery) {
      const inputData = this.getInputData();
      query = inputData[0]?.json?.query as string || '';
    } else {
      query = this.getNodeParameter('query', 0) as string;
    }

    if (!query) {
      return [[{
        json: {
          success: false,
          error: 'No query provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    // Determine which agents to run
    let agentIds: string[] = [];

    switch (agentSelection) {
      case 'preset': {
        const preset = this.getNodeParameter('presetGroup', 0) as string;
        agentIds = MultiAgentNode.getPresetAgentsStatic(preset);
        break;
      }
      case 'category': {
        const categories = this.getNodeParameter('categories', 0) as string[];
        agentIds = AGENT_DEFINITIONS
          .filter(a => categories.includes(a.category))
          .map(a => a.id);
        break;
      }
      case 'custom': {
        agentIds = this.getNodeParameter('selectedAgents', 0) as string[];
        break;
      }
    }

    if (agentIds.length === 0) {
      return [[{
        json: {
          success: false,
          error: 'No agents selected',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    const startTime = Date.now();

    try {
      // Build requests for each agent
      const agentRequests = agentIds.map(agentId => {
        const agentDef = AGENT_DEFINITIONS.find(a => a.id === agentId);
        return {
          agentId,
          category: (agentDef?.category as any) || 'geopolitics',
          parameters: {
            query,
          },
          llmProvider: llmProvider as any,
          llmModel: model,
        };
      });

      // Execute agents using MultiAgentRequest interface
      const multiAgentResult = await AgentBridge.executeMultipleAgents({
        agents: agentRequests,
        parallel: executionMode === 'parallel',
        llmProvider: llmProvider as any,
      });

      const executionTime = Date.now() - startTime;
      const results = multiAgentResult.agentResults;

      // Process results based on aggregation mode
      const output: Record<string, any> = {
        success: true,
        query,
        agentCount: agentIds.length,
        successCount: results.filter(r => r.success).length,
        failureCount: results.filter(r => !r.success).length,
        executionMode,
        executionTime,
        timestamp: new Date().toISOString(),
      };

      switch (aggregationMode) {
        case 'separate':
          output.responses = results.map(r => ({
            agentId: r.agentId,
            agentName: AGENT_DEFINITIONS.find(a => a.id === r.agentId)?.name || r.agentId,
            success: r.success,
            response: r.data?.response || r.data?.analysis || JSON.stringify(r.data),
            error: r.error,
          }));
          break;

        case 'synthesize': {
          const synthesis = await AgentBridge.mediateResults(
            results.filter(r => r.success),
          );
          output.synthesis = synthesis;
          break;
        }

        case 'consensus': {
          output.consensus = MultiAgentNode.findConsensusStatic(results);
          break;
        }

        case 'both': {
          output.responses = results.map(r => ({
            agentId: r.agentId,
            agentName: AGENT_DEFINITIONS.find(a => a.id === r.agentId)?.name || r.agentId,
            success: r.success,
            response: r.data?.response || r.data?.analysis || JSON.stringify(r.data),
            error: r.error,
          }));

          const synthesis = await AgentBridge.mediateResults(
            results.filter(r => r.success),
          );
          output.synthesis = synthesis;
          output.consensus = MultiAgentNode.findConsensusStatic(results);
          break;
        }
      }

      return [[{ json: output }]];
    } catch (error) {
      return [[{
        json: {
          success: false,
          query,
          agentCount: agentIds.length,
          error: error instanceof Error ? error.message : 'Unknown error',
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }

  private static getPresetAgentsStatic(preset: string): string[] {
    const presets: Record<string, string[]> = {
      geopolitics_full: AGENT_DEFINITIONS.filter(a => a.category === 'geopolitics').map(a => a.id),
      geopolitics_core: ['grand_chessboard', 'world_order', 'prisoners_geography', 'rise_fall_powers', 'clash_civilizations'],
      hedgefund_full: AGENT_DEFINITIONS.filter(a => a.category === 'hedgeFund').map(a => a.id),
      investors_value: ['warren_buffett', 'benjamin_graham', 'charlie_munger'],
      investors_growth: ['peter_lynch', 'philip_fisher'],
      macro_full: AGENT_DEFINITIONS.filter(a => a.category === 'economic').map(a => a.id),
      all: AGENT_DEFINITIONS.map(a => a.id),
    };

    return presets[preset] || [];
  }

  private static findConsensusStatic(results: any[]): Record<string, any> {
    const successfulResults = results.filter(r => r.success);

    if (successfulResults.length < 2) {
      return {
        hasConsensus: false,
        reason: 'Not enough successful responses for consensus',
      };
    }

    // Simple word frequency analysis for finding common themes
    const allWords: Record<string, number> = {};
    for (const result of successfulResults) {
      const responseText = result.data?.response || result.data?.analysis || JSON.stringify(result.data) || '';
      const words = (responseText as string).toLowerCase().split(/\s+/);
      const uniqueWords = new Set(words);
      for (const word of uniqueWords) {
        if (word.length > 5) { // Only meaningful words
          allWords[word] = (allWords[word] || 0) + 1;
        }
      }
    }

    // Find words mentioned by most agents
    const threshold = Math.ceil(successfulResults.length * 0.6);
    const commonThemes = Object.entries(allWords)
      .filter(([, count]) => count >= threshold)
      .sort((a, b) => b[1] - a[1])
      .slice(0, 10)
      .map(([word]) => word);

    return {
      hasConsensus: commonThemes.length > 0,
      agentCount: successfulResults.length,
      commonThemes,
      agreementLevel: commonThemes.length > 5 ? 'high' : commonThemes.length > 2 ? 'medium' : 'low',
    };
  }
}
