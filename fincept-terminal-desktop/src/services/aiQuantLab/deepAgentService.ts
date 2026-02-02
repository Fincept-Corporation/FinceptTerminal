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
