// Agent MCP Executor - Enables agents to autonomously use MCP tools
// Integrates LLM decision-making with MCP tool execution

import { mcpToolService, MCPTool } from '../../../services/mcpToolService';
import { llmApiService, ChatMessage } from '../../../services/llmApi';

export interface AgentMCPConfig {
  enabled: boolean;
  allowedServers: string[]; // Which MCP servers this agent can use
  autonomousMode: boolean; // Agent decides when to use tools vs user explicitly triggers
  maxToolCalls: number; // Maximum tool calls per execution to prevent infinite loops
}

export interface AgentExecutionContext {
  objective: string; // What the agent is trying to accomplish
  inputs: Record<string, any>; // Inputs from connected nodes
  conversationHistory: ChatMessage[]; // Previous messages in the workflow
}

export interface AgentExecutionResult {
  success: boolean;
  output: string;
  toolCallsMade: ToolCallRecord[];
  error?: string;
  tokensUsed?: number;
}

export interface ToolCallRecord {
  toolName: string;
  serverId: string;
  arguments: Record<string, any>;
  result: any;
  executionTime: number;
  success: boolean;
  error?: string;
}

/**
 * Execute an MCP-enabled agent workflow
 * The agent uses LLM to decide which tools to call and when
 */
export async function executeAgentWithMCP(
  config: AgentMCPConfig,
  context: AgentExecutionContext,
  onProgress?: (message: string) => void
): Promise<AgentExecutionResult> {
  const toolCallRecords: ToolCallRecord[] = [];

  try {
    onProgress?.('Initializing agent with MCP tools...');

    // Get available MCP tools filtered by allowed servers
    let availableTools = await mcpToolService.getAllTools();

    if (config.allowedServers.length > 0) {
      availableTools = availableTools.filter(tool =>
        config.allowedServers.includes(tool.serverId)
      );
    }

    if (availableTools.length === 0) {
      return {
        success: false,
        output: '',
        toolCallsMade: [],
        error: 'No MCP tools available for this agent'
      };
    }

    onProgress?.(`Agent has access to ${availableTools.length} MCP tools`);

    // Format context into initial prompt
    const systemPrompt = `You are an AI agent with access to external tools. Your objective is: ${context.objective}

Available inputs: ${JSON.stringify(context.inputs, null, 2)}

You can use the following MCP tools to accomplish your objective. Use them wisely and only when necessary.
When you're done, provide a final summary of your work.`;

    // Build conversation history
    const conversationHistory: ChatMessage[] = [
      ...context.conversationHistory,
      {
        role: 'user',
        content: systemPrompt
      }
    ];

    // Call LLM with tools (tool execution happens inside chatWithTools)
    // The onToolCall callback tracks all tool executions
    let toolExecutionCount = 0;

    onProgress?.('Calling LLM with available tools...');

    const response = await llmApiService.chatWithTools(
      context.objective, // User's objective
      conversationHistory,
      availableTools,
      undefined, // No streaming for agent execution
      (toolName: string, args: any, result?: any) => {
        if (!result) {
          onProgress?.(`Agent is calling tool: ${toolName}`);
          toolExecutionCount++;
        } else {
          onProgress?.(`Tool ${toolName} completed`);

          // Parse tool name to get serverId and toolName
          const parsed = mcpToolService.parseOpenAIFunctionName(toolName);
          if (parsed) {
            toolCallRecords.push({
              toolName: parsed.toolName,
              serverId: parsed.serverId,
              arguments: args,
              result: result,
              executionTime: 0, // Not tracked in callback
              success: true,
              error: undefined
            });
          }
        }
      }
    );

    if (response.error) {
      return {
        success: false,
        output: '',
        toolCallsMade: toolCallRecords,
        error: response.error
      };
    }

    onProgress?.('Agent execution completed');

    return {
      success: true,
      output: response.content,
      toolCallsMade: toolCallRecords,
      tokensUsed: response.usage?.totalTokens
    };
  } catch (error: any) {
    return {
      success: false,
      output: '',
      toolCallsMade: toolCallRecords,
      error: error.message || 'Unknown error during agent execution'
    };
  }
}

/**
 * Execute a single MCP tool directly without LLM
 * Used when agent workflow has explicit tool nodes
 */
export async function executeToolDirectly(
  serverId: string,
  toolName: string,
  params: Record<string, any>,
  onProgress?: (message: string) => void
): Promise<AgentExecutionResult> {
  try {
    onProgress?.(`Executing ${serverId}.${toolName}...`);

    const result = await mcpToolService.executeToolDirect(serverId, toolName, params);

    const toolCallRecord: ToolCallRecord = {
      toolName,
      serverId,
      arguments: params,
      result: result.data,
      executionTime: result.executionTime || 0,
      success: result.success,
      error: result.error
    };

    if (result.success) {
      onProgress?.('Tool executed successfully');
      return {
        success: true,
        output: JSON.stringify(result.data, null, 2),
        toolCallsMade: [toolCallRecord]
      };
    } else {
      return {
        success: false,
        output: '',
        toolCallsMade: [toolCallRecord],
        error: result.error
      };
    }
  } catch (error: any) {
    return {
      success: false,
      output: '',
      toolCallsMade: [],
      error: error.message || 'Unknown error during tool execution'
    };
  }
}

/**
 * Get available MCP tools for an agent based on its configuration
 */
export async function getAgentAvailableTools(config: AgentMCPConfig): Promise<MCPTool[]> {
  try {
    let tools = await mcpToolService.getAllTools();

    if (config.allowedServers.length > 0) {
      tools = tools.filter(tool => config.allowedServers.includes(tool.serverId));
    }

    return tools;
  } catch (error) {
    console.error('[AgentMCPExecutor] Failed to get available tools:', error);
    return [];
  }
}

/**
 * Validate agent MCP configuration
 */
export function validateAgentMCPConfig(config: AgentMCPConfig): { valid: boolean; errors: string[] } {
  const errors: string[] = [];

  if (config.maxToolCalls <= 0) {
    errors.push('maxToolCalls must be greater than 0');
  }

  if (config.maxToolCalls > 20) {
    errors.push('maxToolCalls cannot exceed 20 (to prevent infinite loops)');
  }

  if (config.enabled && config.allowedServers.length === 0) {
    errors.push('At least one MCP server must be allowed when MCP is enabled');
  }

  return {
    valid: errors.length === 0,
    errors
  };
}
