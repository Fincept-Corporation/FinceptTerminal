/**
 * Agent Node Generator
 *
 * Utility to automatically generate node type definitions from Python agent metadata.
 * This allows us to create 30+ agent nodes without manual duplication.
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
  INodeProperties,
} from '../types';
import { pythonAgentService, AgentMetadata } from '@/services/chat/pythonAgentService';

/**
 * Convert Python agent metadata to node properties
 */
function agentMetadataToNodeProperties(agent: AgentMetadata): INodeProperties[] {
  const properties: INodeProperties[] = [];

  // Add agent description as notice
  properties.push({
    displayName: 'About',
    name: 'about',
    type: NodePropertyType.Notice,
    default: '',
    description: agent.description,
  });

  // Convert agent parameters to node properties
  for (const param of agent.parameters) {
    let propertyType: NodePropertyType;

    // Map Python types to node property types
    switch (param.type.toLowerCase()) {
      case 'string':
      case 'str':
        propertyType = NodePropertyType.String;
        break;
      case 'number':
      case 'int':
      case 'float':
        propertyType = NodePropertyType.Number;
        break;
      case 'boolean':
      case 'bool':
        propertyType = NodePropertyType.Boolean;
        break;
      case 'array':
      case 'list':
        propertyType = NodePropertyType.Json;
        break;
      case 'object':
      case 'dict':
        propertyType = NodePropertyType.Json;
        break;
      default:
        propertyType = NodePropertyType.String;
    }

    properties.push({
      displayName: param.label || param.name,
      name: param.name,
      type: propertyType,
      default: param.default_value ?? '',
      required: param.required,
      description: param.description || `Parameter: ${param.name}`,
    });
  }

  // Add LLM provider selection
  properties.push({
    displayName: 'LLM Provider',
    name: 'llmProvider',
    type: NodePropertyType.Options,
    options: [
      { name: 'Active Provider', value: 'active' },
      { name: 'OpenAI', value: 'openai' },
      { name: 'Anthropic', value: 'anthropic' },
      { name: 'Ollama (Local)', value: 'ollama' },
      { name: 'Google Gemini', value: 'gemini' },
    ],
    default: 'active',
    description: 'Which LLM provider to use for this agent',
  });

  // Add model selection
  properties.push({
    displayName: 'Model',
    name: 'model',
    type: NodePropertyType.String,
    default: '',
    description: 'Specific model to use (leave empty for default)',
    displayOptions: {
      show: {
        llmProvider: ['openai', 'anthropic', 'ollama', 'gemini'],
      },
    },
  });

  return properties;
}

/**
 * Get category group for agent
 */
function getAgentGroup(category: string): string[] {
  switch (category.toLowerCase()) {
    case 'trader':
      return ['AI Agents', 'Investor Personas'];
    case 'hedge-fund':
      return ['AI Agents', 'Hedge Funds'];
    case 'economic':
      return ['AI Agents', 'Economic Analysis'];
    case 'geopolitics':
      return ['AI Agents', 'Geopolitics'];
    default:
      return ['AI Agents', 'Other'];
  }
}

/**
 * Generate a node type from agent metadata
 */
export function generateAgentNode(agent: AgentMetadata): INodeType {
  const description: INodeTypeDescription = {
    displayName: agent.name,
    name: agent.id,
    icon: 'fa:brain',
    group: getAgentGroup(agent.category),
    version: 1,
    description: agent.description,
    defaults: {
      name: agent.name,
      color: agent.color || '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: agentMetadataToNodeProperties(agent),
    usableAsTool: true,
  };

  // Create the node type class
  const nodeType: INodeType = {
    description,
    async execute(this: IExecuteFunctions): Promise<NodeOutput> {
      const items = this.getInputData();
      const returnData = [];

      // Get parameters
      const parameters: Record<string, any> = {};
      for (const param of agent.parameters) {
        const value = this.getNodeParameter(param.name, 0);
        if (value !== undefined && value !== null && value !== '') {
          parameters[param.name] = value;
        }
      }

      // Get LLM configuration
      const llmProvider = this.getNodeParameter('llmProvider', 0) as string;
      const model = this.getNodeParameter('model', 0) as string;

      // Prepare LLM config
      const llmConfig: Record<string, any> = {
        provider: llmProvider,
      };
      if (model) {
        llmConfig.model = model;
      }

      // Execute agent for each input item
      for (let i = 0; i < items.length; i++) {
        const item = items[i];

        try {
          // Merge input data with parameters
          const inputData = {
            ...item.json,
            ...parameters,
            llm_config: llmConfig,
          };


          // Execute Python agent
          const result = await pythonAgentService.executeAgent(
            agent.id,
            parameters,
            inputData
          );

          if (!result.success) {
            throw new Error(result.error || 'Agent execution failed');
          }

          // Return result
          returnData.push({
            json: {
              agent: agent.name,
              agentType: agent.id,
              agentCategory: agent.category,
              input: inputData,
              output: result.data,
              executionTime: result.execution_time_ms,
              timestamp: new Date().toISOString(),
            },
            pairedItem: i,
          });
        } catch (error: any) {
          // Handle errors
          if (this.continueOnFail()) {
            returnData.push({
              json: {
                agent: agent.name,
                agentType: agent.id,
                error: error.message,
                timestamp: new Date().toISOString(),
              },
              pairedItem: i,
            });
          } else {
            throw error;
          }
        }
      }

      return [returnData];
    },
  };

  return nodeType;
}

/**
 * Generate all agent nodes from available Python agents
 */
export async function generateAllAgentNodes(): Promise<Map<string, INodeType>> {
  const agentNodes = new Map<string, INodeType>();

  try {
    // Get all available agents
    const agents = await pythonAgentService.getAvailableAgents();


    // Generate a node for each agent
    for (const agent of agents) {
      const nodeType = generateAgentNode(agent);
      agentNodes.set(agent.id, nodeType);
    }

  } catch (error) {
    console.error('[AgentNodeGenerator] Failed to generate agent nodes:', error);
  }

  return agentNodes;
}

/**
 * Generate nodes for a specific agent category
 */
export async function generateAgentNodesByCategory(category: string): Promise<Map<string, INodeType>> {
  const agentNodes = new Map<string, INodeType>();

  try {
    const agents = await pythonAgentService.getAgentsByCategory(category);


    for (const agent of agents) {
      const nodeType = generateAgentNode(agent);
      agentNodes.set(agent.id, nodeType);
    }
  } catch (error) {
    console.error(`[AgentNodeGenerator] Failed to generate nodes for category ${category}:`, error);
  }

  return agentNodes;
}

/**
 * Get agent node statistics
 */
export async function getAgentNodeStatistics() {
  try {
    const agents = await pythonAgentService.getAvailableAgents();
    const categories = await pythonAgentService.getCategories();

    const stats = {
      totalAgents: agents.length,
      categories: categories.length,
      byCategory: {} as Record<string, number>,
    };

    for (const category of categories) {
      const categoryAgents = agents.filter(a => a.category === category);
      stats.byCategory[category] = categoryAgents.length;
    }

    return stats;
  } catch (error) {
    console.error('[AgentNodeGenerator] Failed to get statistics:', error);
    return {
      totalAgents: 0,
      categories: 0,
      byCategory: {},
    };
  }
}
