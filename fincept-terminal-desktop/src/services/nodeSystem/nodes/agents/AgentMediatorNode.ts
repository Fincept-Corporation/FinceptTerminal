/**
 * Agent Mediator Node
 * Synthesizes and mediates between multiple agent responses
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { AgentBridge } from '../../adapters/AgentBridge';

export class AgentMediatorNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'AI Coordinator',
    name: 'agentMediator',
    icon: 'file:mediator.svg',
    group: ['agents'],
    version: 1,
    subtitle: 'Synthesize agent responses',
    description: 'Synthesizes and mediates between multiple agent responses',
    defaults: {
      name: 'AI Coordinator',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mediation Mode',
        name: 'mediationMode',
        type: NodePropertyType.Options,
        default: 'synthesize',
        options: [
          { name: 'Synthesize', value: 'synthesize', description: 'Create a unified synthesis of all responses' },
          { name: 'Debate', value: 'debate', description: 'Identify and present different viewpoints' },
          { name: 'Consensus', value: 'consensus', description: 'Find points of agreement' },
          { name: 'Compare', value: 'compare', description: 'Compare and contrast responses' },
          { name: 'Rank', value: 'rank', description: 'Rank responses by relevance/quality' },
          { name: 'Extract Actions', value: 'actions', description: 'Extract actionable insights' },
        ],
        description: 'How to mediate between responses',
      },
      {
        displayName: 'Input Mode',
        name: 'inputMode',
        type: NodePropertyType.Options,
        default: 'auto',
        options: [
          { name: 'Auto-detect from Input', value: 'auto' },
          { name: 'Responses Array Field', value: 'array' },
          { name: 'Multiple Input Items', value: 'items' },
        ],
        description: 'How to read agent responses from input',
      },
      {
        displayName: 'Responses Field',
        name: 'responsesField',
        type: NodePropertyType.String,
        default: 'responses',
        description: 'Field containing array of responses',
        displayOptions: {
          show: { inputMode: ['array'] },
        },
      },
      {
        displayName: 'Original Query',
        name: 'originalQuery',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'The original question that was asked',
        description: 'Original query for context (optional, will try to get from input)',
      },
      {
        displayName: 'Focus Areas',
        name: 'focusAreas',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'Risk analysis, opportunities, timeline',
        description: 'Specific areas to focus the synthesis on (comma-separated)',
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
        description: 'LLM provider for mediation',
      },
      {
        displayName: 'Model',
        name: 'model',
        type: NodePropertyType.String,
        default: 'llama3',
        description: 'Specific model to use for mediation',
      },
      {
        displayName: 'Max Output Length',
        name: 'maxLength',
        type: NodePropertyType.Options,
        default: 'medium',
        options: [
          { name: 'Brief (500 words)', value: 'brief' },
          { name: 'Medium (1000 words)', value: 'medium' },
          { name: 'Detailed (2000 words)', value: 'detailed' },
          { name: 'Comprehensive (No limit)', value: 'full' },
        ],
        description: 'Length of mediated output',
      },
      {
        displayName: 'Include Agent Attribution',
        name: 'includeAttribution',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Attribute insights to specific agents',
      },
      {
        displayName: 'Generate Recommendations',
        name: 'generateRecommendations',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Generate actionable recommendations from synthesis',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const mediationMode = this.getNodeParameter('mediationMode', 0) as string;
    const inputMode = this.getNodeParameter('inputMode', 0) as string;
    const llmProvider = this.getNodeParameter('llmProvider', 0) as string;
    const model = this.getNodeParameter('model', 0) as string;
    const maxLength = this.getNodeParameter('maxLength', 0) as string;
    const includeAttribution = this.getNodeParameter('includeAttribution', 0) as boolean;
    const generateRecommendations = this.getNodeParameter('generateRecommendations', 0) as boolean;
    const focusAreasStr = this.getNodeParameter('focusAreas', 0) as string;

    const inputData = this.getInputData();

    // Extract responses based on input mode
    let responses: Array<{ agentId: string; agentName?: string; response: string }> = [];
    let originalQuery = this.getNodeParameter('originalQuery', 0) as string;

    switch (inputMode) {
      case 'array': {
        const field = this.getNodeParameter('responsesField', 0) as string;
        const firstItem = inputData[0]?.json;
        if (firstItem && Array.isArray(firstItem[field])) {
          responses = firstItem[field] as Array<{ agentId: string; agentName?: string; response: string }>;
        }
        originalQuery = originalQuery || (firstItem?.query as string) || '';
        break;
      }

      case 'items': {
        responses = inputData.map(item => ({
          agentId: item.json?.agentId as string || 'unknown',
          agentName: item.json?.agentName as string,
          response: item.json?.response as string || '',
        }));
        originalQuery = originalQuery || (inputData[0]?.json?.query as string) || '';
        break;
      }

      case 'auto':
      default: {
        const firstItem = inputData[0]?.json;

        // Try to find responses in common formats
        if (firstItem?.responses && Array.isArray(firstItem.responses)) {
          responses = firstItem.responses as Array<{ agentId: string; agentName?: string; response: string }>;
        } else if (firstItem?.synthesis) {
          // Already synthesized, just pass through
          return [[{ json: firstItem }]];
        } else if (inputData.length > 1) {
          // Multiple input items
          responses = inputData.map(item => ({
            agentId: item.json?.agentId as string || 'unknown',
            agentName: item.json?.agentName as string,
            response: item.json?.response as string || '',
          }));
        }

        originalQuery = originalQuery || (firstItem?.query as string) || '';
        break;
      }
    }

    if (responses.length === 0) {
      return [[{
        json: {
          success: false,
          error: 'No agent responses found in input',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    const focusAreas = focusAreasStr ? focusAreasStr.split(',').map(s => s.trim()) : [];

    try {
      // Convert responses to AgentExecutionResult format
      const agentResults: any[] = responses.filter(r => r.response).map(r => ({
        success: true,
        agentId: r.agentId,
        agentName: r.agentName,
        data: { response: r.response },
      }));

      // Build mediation prompt
      const mediationPrompt = `Mode: ${mediationMode}. Original query: ${originalQuery}. ${
        focusAreas.length > 0 ? `Focus on: ${focusAreas.join(', ')}.` : ''
      }`;

      const result = await AgentBridge.mediateResults(agentResults, mediationPrompt);

      const output: Record<string, any> = {
        success: true,
        mediationMode,
        agentCount: responses.length,
        originalQuery,
        synthesis: result,
        timestamp: new Date().toISOString(),
      };

      // Simple mediation result - just include the synthesis
      output.analysis = result;

      return [[{ json: output }]];
    } catch (error) {
      return [[{
        json: {
          success: false,
          mediationMode,
          agentCount: responses.length,
          error: error instanceof Error ? error.message : 'Unknown error',
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }
}
