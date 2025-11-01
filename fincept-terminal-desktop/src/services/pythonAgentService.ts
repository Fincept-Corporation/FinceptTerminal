// Python Agent Service - Interface to execute Python agents via Tauri
import { invoke } from '@tauri-apps/api/core';

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

class PythonAgentService {
  /**
   * Get list of all available Python agents
   */
  async getAvailableAgents(): Promise<AgentMetadata[]> {
    try {
      const agents = await invoke<AgentMetadata[]>('list_available_agents');
      console.log('[PythonAgentService] Loaded agents:', agents.length);
      return agents;
    } catch (error) {
      console.error('[PythonAgentService] Failed to load agents:', error);
      return [];
    }
  }

  /**
   * Execute a Python agent
   */
  async executeAgent(
    agentType: string,
    parameters: Record<string, any>,
    inputs: Record<string, any>
  ): Promise<AgentExecutionResult> {
    try {
      console.log(`[PythonAgentService] Executing agent: ${agentType}`, {parameters, inputs});

      const result = await invoke<AgentExecutionResult>('execute_python_agent', {
        agentType,
        parameters,
        inputs
      });

      console.log(`[PythonAgentService] Agent ${agentType} completed:`, result);
      return result;
    } catch (error: any) {
      console.error(`[PythonAgentService] Agent ${agentType} failed:`, error);
      return {
        success: false,
        error: error.message || 'Unknown error',
        execution_time_ms: 0,
        agent_type: agentType
      };
    }
  }

  /**
   * Get metadata for a specific agent
   */
  async getAgentMetadata(agentType: string): Promise<AgentMetadata | null> {
    try {
      const metadata = await invoke<AgentMetadata>('get_agent_metadata', { agentType });
      return metadata;
    } catch (error) {
      console.error(`[PythonAgentService] Failed to get metadata for ${agentType}:`, error);
      return null;
    }
  }

  /**
   * Get agents by category
   */
  async getAgentsByCategory(category: string): Promise<AgentMetadata[]> {
    const allAgents = await this.getAvailableAgents();
    return allAgents.filter(agent => agent.category === category);
  }

  /**
   * Get unique categories
   */
  async getCategories(): Promise<string[]> {
    const allAgents = await this.getAvailableAgents();
    const categories = new Set(allAgents.map(agent => agent.category));
    return Array.from(categories);
  }
}

export const pythonAgentService = new PythonAgentService();
