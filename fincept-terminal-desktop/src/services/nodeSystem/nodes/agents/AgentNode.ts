/**
 * Agent Node
 * Generic node that can execute any AI agent from the library
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

export class AgentNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'AI Agent',
    name: 'aiAgent',
    icon: 'file:agent.svg',
    group: ['agents'],
    version: 1,
    subtitle: '={{$parameter["agentId"]}}',
    description: 'Execute any AI agent from the Fincept agent library',
    defaults: {
      name: 'AI Agent',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Agent Category',
        name: 'category',
        type: NodePropertyType.Options,
        default: 'geopolitics',
        options: [
          { name: 'Geopolitics', value: 'geopolitics', description: '19 geopolitical analysis agents' },
          { name: 'Hedge Fund', value: 'hedgeFund', description: '8 hedge fund strategy agents' },
          { name: 'Investor', value: 'investor', description: 'Famous investor personas' },
          { name: 'Economic', value: 'economic', description: 'Economic analysis agents' },
          { name: 'Trader', value: 'trader', description: 'Trading strategy agents' },
        ],
        description: 'Category of agent to use',
      },
      {
        displayName: 'AI Assistant',
        name: 'agentId',
        type: NodePropertyType.Options,
        default: '',
        options: [], // Dynamically populated based on category
        description: 'Specific agent to execute',
        // This would be dynamically populated in the UI
      },
      {
        displayName: 'Query',
        name: 'query',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Analyze the geopolitical implications of...',
        description: 'Query or topic for the agent to analyze',
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
        displayName: 'Input Query Field',
        name: 'inputQueryField',
        type: NodePropertyType.String,
        default: 'query',
        description: 'Field name in input data containing the query',
        displayOptions: {
          show: { useInputQuery: [true] },
        },
      },
      {
        displayName: 'Context',
        name: 'context',
        type: NodePropertyType.Json,
        default: '{}',
        description: 'Additional context to provide to the agent',
      },
      {
        displayName: 'LLM Provider',
        name: 'llmProvider',
        type: NodePropertyType.Options,
        default: 'ollama',
        options: [
          { name: 'Ollama (Local)', value: 'ollama', description: 'Local LLM via Ollama' },
          { name: 'OpenAI', value: 'openai', description: 'OpenAI GPT models' },
          { name: 'Anthropic', value: 'anthropic', description: 'Claude models' },
          { name: 'Groq', value: 'groq', description: 'Groq cloud' },
        ],
        description: 'LLM provider to use',
      },
      {
        displayName: 'Model',
        name: 'model',
        type: NodePropertyType.String,
        default: 'llama3',
        placeholder: 'llama3, gpt-4, claude-3-opus',
        description: 'Specific model to use',
      },
      {
        displayName: 'Max Tokens',
        name: 'maxTokens',
        type: NodePropertyType.Number,
        default: 2048,
        description: 'Maximum tokens in response',
      },
      {
        displayName: 'Temperature',
        name: 'temperature',
        type: NodePropertyType.Number,
        default: 0.7,
        description: 'Response temperature (0-1)',
      },
      {
        displayName: 'Include Sources',
        name: 'includeSources',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include sources and references in output',
      },
      {
        displayName: 'Output Format',
        name: 'outputFormat',
        type: NodePropertyType.Options,
        default: 'full',
        options: [
          { name: 'Full Response', value: 'full' },
          { name: 'Summary Only', value: 'summary' },
          { name: 'Key Points', value: 'keyPoints' },
          { name: 'Structured JSON', value: 'json' },
        ],
        description: 'Format of agent output',
      },
      {
        displayName: 'Timeout (seconds)',
        name: 'timeout',
        type: NodePropertyType.Number,
        default: 120,
        description: 'Maximum execution time',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const category = this.getNodeParameter('category', 0) as string;
    const agentId = this.getNodeParameter('agentId', 0) as string;
    const useInputQuery = this.getNodeParameter('useInputQuery', 0) as boolean;
    const llmProvider = this.getNodeParameter('llmProvider', 0) as string;
    const model = this.getNodeParameter('model', 0) as string;
    const maxTokens = this.getNodeParameter('maxTokens', 0) as number;
    const temperature = this.getNodeParameter('temperature', 0) as number;
    const includeSources = this.getNodeParameter('includeSources', 0) as boolean;
    const outputFormat = this.getNodeParameter('outputFormat', 0) as string;
    const timeout = this.getNodeParameter('timeout', 0) as number;

    // Get query
    let query: string;
    if (useInputQuery) {
      const inputData = this.getInputData();
      const field = this.getNodeParameter('inputQueryField', 0) as string;
      query = inputData[0]?.json?.[field] as string || '';
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

    // Parse context
    let context: Record<string, any> = {};
    try {
      const contextStr = this.getNodeParameter('context', 0) as string;
      context = JSON.parse(contextStr);
    } catch {
      // Invalid JSON, use empty context
    }

    // Get agent definition
    const agentDef = AGENT_DEFINITIONS.find(a => a.id === agentId);
    const agentName = agentDef?.name || agentId;

    try {
      const result = await AgentBridge.executeAgent({
        agentId: agentId || category,
        category: category as any,
        parameters: {
          query,
          context,
          maxTokens,
          temperature,
          timeout: timeout * 1000,
        },
        llmProvider: llmProvider as any,
        llmModel: model,
      });

      // Format output based on preference
      const formattedOutput: Record<string, any> = {
        success: true,
        agentId: agentId || category,
        agentName,
        category,
        query,
      };

      const responseData = result.data || {};
      const responseText = responseData.response || responseData.analysis || JSON.stringify(responseData);

      switch (outputFormat) {
        case 'summary':
          formattedOutput.summary = responseData.summary || responseText?.substring(0, 500);
          break;
        case 'keyPoints':
          formattedOutput.keyPoints = responseData.keyPoints || AgentNode.extractKeyPointsStatic(responseText);
          break;
        case 'json':
          formattedOutput.structured = responseData.structured || AgentNode.structureResponseStatic(responseText);
          break;
        case 'full':
        default:
          formattedOutput.response = responseText;
          formattedOutput.analysis = responseData.analysis;
          if (includeSources && responseData.sources) {
            formattedOutput.sources = responseData.sources;
          }
      }

      formattedOutput.llmProvider = llmProvider;
      formattedOutput.model = model;
      formattedOutput.executionTime = result.executionTime || 0;
      formattedOutput.timestamp = new Date().toISOString();
      formattedOutput.executionId = this.getExecutionId();

      return [[{ json: formattedOutput }]];
    } catch (error) {
      return [[{
        json: {
          success: false,
          agentId: agentId || category,
          agentName,
          query,
          error: error instanceof Error ? error.message : 'Unknown error',
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }

  private static extractKeyPointsStatic(response: string): string[] {
    // Simple key point extraction
    if (!response) return [];

    const lines = response.split('\n').filter(line => line.trim());
    const keyPoints: string[] = [];

    for (const line of lines) {
      const trimmed = line.trim();
      // Look for bullet points or numbered items
      if (trimmed.match(/^[-•*]\s+/) || trimmed.match(/^\d+\.\s+/)) {
        keyPoints.push(trimmed.replace(/^[-•*\d.]+\s+/, ''));
      }
    }

    return keyPoints.length > 0 ? keyPoints : lines.slice(0, 5);
  }

  private static structureResponseStatic(response: string): Record<string, unknown> {
    // Try to extract structured data from response
    return {
      raw: response,
      wordCount: response?.split(/\s+/).length || 0,
      paragraphs: response?.split('\n\n').filter(p => p.trim()).length || 0,
    };
  }
}
