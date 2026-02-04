/**
 * DeepAgent Service - Frontend API for DeepAgents integration
 * Hierarchical agentic automation for autonomous financial workflows
 */

import { invoke } from '@tauri-apps/api/core';

export interface DeepAgentStatus {
  success: boolean;
  available: boolean;
  error?: string;
  agent_types?: string[];
  subagent_templates?: string[];
}

export interface DeepAgentCapabilities {
  success: boolean;
  agent_types?: Record<string, string>;
  subagent_templates?: string[];
  features?: string[];
  error?: string;
}

export interface ExecuteTaskRequest {
  agent_type: string;
  task: string;
  thread_id?: string;
  context?: Record<string, any>;
  config?: AgentConfig;
}

export interface AgentConfig {
  model?: string;
  api_key?: string;
  llm_provider?: string;
  llm_api_key?: string;
  llm_base_url?: string;
  llm_model?: string;
  enable_checkpointing?: boolean;
  enable_longterm_memory?: boolean;
  enable_summarization?: boolean;
  recursion_limit?: number;
}

export interface ExecuteTaskResponse {
  success: boolean;
  result?: string;
  todos?: Todo[];
  files?: Record<string, string>;
  thread_id?: string;
  error?: string;
}

export interface Todo {
  id: string;
  task: string;
  status: 'pending' | 'in_progress' | 'completed';
  subtasks?: Todo[];
}

export interface StreamChunk {
  success: boolean;
  chunk?: any;
  thread_id?: string;
  error?: string;
}

export interface PlanResponse {
  success: boolean;
  todos?: PlanTodo[];
  plan?: PlanStep[];
  error?: string;
}

export interface PlanTodo {
  id: string;
  task: string;
  status: 'pending' | 'in_progress' | 'completed';
  specialist: string;
  prompt: string;
  subtasks: any[];
}

export interface PlanStep {
  step: string;
  specialist: string;
  prompt: string;
}

export interface StepResult {
  success: boolean;
  result?: string;
  error?: string;
}

export interface SynthesizeRequest {
  task: string;
  step_results: { step: string; specialist: string; result: string }[];
  config?: AgentConfig;
}

class DeepAgentService {
  /**
   * Check DeepAgent availability and status
   */
  async checkStatus(): Promise<DeepAgentStatus> {
    console.log('[DeepAgentService] checkStatus() called');
    try {
      const result = await invoke<string>('deepagent_check_status');
      console.log('[DeepAgentService] Raw result:', result);
      const parsed = JSON.parse(result);
      console.log('[DeepAgentService] Parsed result:', parsed);
      return parsed;
    } catch (error) {
      console.error('[DeepAgentService] checkStatus error:', error);
      return {
        success: false,
        available: false,
        error: String(error)
      };
    }
  }

  /**
   * Get DeepAgent capabilities and available agent types
   */
  async getCapabilities(): Promise<DeepAgentCapabilities> {
    try {
      const result = await invoke<string>('deepagent_get_capabilities');
      return JSON.parse(result);
    } catch (error) {
      console.error('[DeepAgentService] getCapabilities error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Execute a task with DeepAgent
   */
  async executeTask(request: ExecuteTaskRequest): Promise<ExecuteTaskResponse> {
    console.log('[DeepAgentService] executeTask() called:', request);
    try {
      const result = await invoke<string>('deepagent_execute_task', {
        agentType: request.agent_type,
        task: request.task,
        threadId: request.thread_id,
        context: request.context ? JSON.stringify(request.context) : null,
        config: request.config ? JSON.stringify(request.config) : null
      });
      console.log('[DeepAgentService] Execute result:', result);
      const parsed = JSON.parse(result);
      return parsed;
    } catch (error) {
      console.error('[DeepAgentService] executeTask error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Create an execution plan without executing steps
   */
  async createPlan(request: { agent_type: string; task: string; config?: AgentConfig }): Promise<PlanResponse> {
    try {
      const result = await invoke<string>('deepagent_create_plan', {
        agentType: request.agent_type,
        task: request.task,
        config: request.config ? JSON.stringify(request.config) : null
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[DeepAgentService] createPlan error:', error);
      return { success: false, error: String(error) };
    }
  }

  /**
   * Execute a single step from the plan
   */
  async executeStep(request: {
    task: string;
    step_prompt: string;
    specialist: string;
    config?: AgentConfig;
    previous_results?: { step: string; specialist: string; result: string }[];
  }): Promise<StepResult> {
    try {
      const result = await invoke<string>('deepagent_execute_step', {
        task: request.task,
        stepPrompt: request.step_prompt,
        specialist: request.specialist,
        config: request.config ? JSON.stringify(request.config) : null,
        previousResults: request.previous_results ? JSON.stringify(request.previous_results) : null
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[DeepAgentService] executeStep error:', error);
      return { success: false, error: String(error) };
    }
  }

  /**
   * Synthesize all step results into a final report
   */
  async synthesizeResults(request: SynthesizeRequest): Promise<StepResult> {
    try {
      const result = await invoke<string>('deepagent_synthesize_results', {
        task: request.task,
        stepResults: JSON.stringify(request.step_results),
        config: request.config ? JSON.stringify(request.config) : null
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[DeepAgentService] synthesizeResults error:', error);
      return { success: false, error: String(error) };
    }
  }

  /**
   * Stream task execution (returns async generator)
   */
  async *streamTask(request: ExecuteTaskRequest): AsyncGenerator<StreamChunk> {
    try {
      // Note: This is a simplified implementation
      // For true streaming, you'd need WebSocket or SSE support
      const result = await this.executeTask(request);

      // Simulate streaming by yielding the final result
      yield {
        success: result.success,
        chunk: result,
        thread_id: result.thread_id,
        error: result.error
      };
    } catch (error) {
      yield {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Resume a task from checkpoint
   */
  async resumeTask(threadId: string, newInput?: string): Promise<ExecuteTaskResponse> {
    try {
      const result = await invoke<string>('deepagent_resume_task', {
        threadId,
        newInput: newInput || null
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[DeepAgentService] resumeTask error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }

  /**
   * Get agent state for a thread
   */
  async getState(threadId: string): Promise<any> {
    try {
      const result = await invoke<string>('deepagent_get_state', {
        threadId
      });
      return JSON.parse(result);
    } catch (error) {
      console.error('[DeepAgentService] getState error:', error);
      return {
        success: false,
        error: String(error)
      };
    }
  }
}

// Export singleton instance
export const deepAgentService = new DeepAgentService();
