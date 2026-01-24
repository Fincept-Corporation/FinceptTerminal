// Internal Terminal MCP Provider - Core Registry & Executor

import { MCPTool, MCPToolExecutionResult } from '../mcpToolService';
import { InternalTool, InternalToolResult, TerminalContexts, INTERNAL_SERVER_ID, INTERNAL_SERVER_NAME } from './types';

class TerminalMCPProvider {
  private tools = new Map<string, InternalTool>();
  private contexts: TerminalContexts = {};

  registerTool(tool: InternalTool): void {
    this.tools.set(tool.name, tool);
  }

  registerTools(tools: InternalTool[]): void {
    for (const tool of tools) {
      this.tools.set(tool.name, tool);
    }
  }

  unregisterTool(name: string): void {
    this.tools.delete(name);
  }

  setContexts(contexts: Partial<TerminalContexts>): void {
    this.contexts = { ...this.contexts, ...contexts };
  }

  listTools(): MCPTool[] {
    return Array.from(this.tools.values()).map(tool => ({
      serverId: INTERNAL_SERVER_ID,
      serverName: INTERNAL_SERVER_NAME,
      name: tool.name,
      description: tool.description,
      inputSchema: tool.inputSchema,
    }));
  }

  async callTool(name: string, args: Record<string, any>): Promise<MCPToolExecutionResult> {
    const startTime = Date.now();
    const tool = this.tools.get(name);

    if (!tool) {
      return {
        success: false,
        error: `Tool not found: ${name}`,
        toolName: name,
        serverId: INTERNAL_SERVER_ID,
      };
    }

    try {
      const result: InternalToolResult = await tool.handler(args, this.contexts);
      return {
        success: result.success,
        data: result.data ?? result.message,
        error: result.error,
        toolName: name,
        serverId: INTERNAL_SERVER_ID,
        executionTime: Date.now() - startTime,
      };
    } catch (err: any) {
      return {
        success: false,
        error: err.message || 'Unknown error',
        toolName: name,
        serverId: INTERNAL_SERVER_ID,
        executionTime: Date.now() - startTime,
      };
    }
  }

  getToolCount(): number {
    return this.tools.size;
  }

  hasContexts(): boolean {
    return Object.keys(this.contexts).length > 0;
  }
}

export const terminalMCPProvider = new TerminalMCPProvider();
