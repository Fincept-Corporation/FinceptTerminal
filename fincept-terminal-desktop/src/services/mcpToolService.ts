// MCP Tool Service - Unified interface for MCP tool access across all tabs
// Provides fault-tolerant, modular access to MCP tools for Chat, Node Editor, and Agents

import { mcpManager } from './mcpManager';

export interface MCPTool {
  serverId: string;
  serverName: string;
  name: string;
  description?: string;
  inputSchema?: {
    type: string;
    properties?: Record<string, any>;
    required?: string[];
  };
}

export interface MCPToolExecutionResult {
  success: boolean;
  data?: any;
  error?: string;
  toolName: string;
  serverId: string;
  executionTime?: number;
}

export interface MCPToolValidationResult {
  valid: boolean;
  errors: string[];
  missingRequired: string[];
}

export type MCPToolCategory = 'database' | 'web' | 'file' | 'api' | 'data' | 'productivity' | 'other';

class MCPToolService {
  private toolCache: MCPTool[] = [];
  private cacheTimestamp: number = 0;
  private cacheTTL: number = 5000; // 5 seconds cache

  /**
   * Get all available MCP tools from running servers
   * Uses caching to reduce frequent calls
   */
  async getAllTools(): Promise<MCPTool[]> {
    try {
      const now = Date.now();

      // Return cached tools if still valid
      if (this.toolCache.length > 0 && (now - this.cacheTimestamp) < this.cacheTTL) {
        return this.toolCache;
      }

      // Fetch fresh tools
      const tools = await mcpManager.getAllTools();
      this.toolCache = tools;
      this.cacheTimestamp = now;

      // Only log when tools are actually available or when count changes
      if (tools.length > 0 && tools.length !== this.toolCache.length) {
        console.log('[MCPToolService] Tools updated:', tools.length);
      }
      return tools;
    } catch (error) {
      console.error('[MCPToolService] Error fetching tools:', error);
      // Return cached tools as fallback
      return this.toolCache;
    }
  }

  /**
   * Execute an MCP tool directly without LLM assistance
   * Used by Node Editor for direct tool execution
   */
  async executeToolDirect(
    serverId: string,
    toolName: string,
    params: Record<string, any>
  ): Promise<MCPToolExecutionResult> {
    const startTime = Date.now();

    try {
      // Validate tool exists
      const tools = await this.getAllTools();
      const tool = tools.find(t => t.serverId === serverId && t.name === toolName);

      if (!tool) {
        return {
          success: false,
          error: `Tool not found: ${serverId}.${toolName}`,
          toolName,
          serverId
        };
      }

      // Validate parameters
      const validation = this.validateToolParams(tool, params);
      if (!validation.valid) {
        return {
          success: false,
          error: `Invalid parameters: ${validation.errors.join(', ')}`,
          toolName,
          serverId
        };
      }

      // Execute tool
      const result = await mcpManager.callTool(serverId, toolName, params);
      const executionTime = Date.now() - startTime;

      return {
        success: true,
        data: result,
        toolName,
        serverId,
        executionTime
      };
    } catch (error: any) {
      const executionTime = Date.now() - startTime;
      console.error(`[MCPToolService] Tool execution failed:`, error);

      return {
        success: false,
        error: error.message || 'Unknown error during tool execution',
        toolName,
        serverId,
        executionTime
      };
    }
  }

  /**
   * Get tools filtered by category
   * Used to show relevant tools in different contexts
   */
  async getToolsByCategory(category: MCPToolCategory): Promise<MCPTool[]> {
    try {
      const allTools = await this.getAllTools();
      return allTools.filter(tool => this.categorizeTool(tool) === category);
    } catch (error) {
      console.error('[MCPToolService] Error filtering tools by category:', error);
      return [];
    }
  }

  /**
   * Get tools from a specific server
   */
  async getToolsByServer(serverId: string): Promise<MCPTool[]> {
    try {
      const allTools = await this.getAllTools();
      return allTools.filter(tool => tool.serverId === serverId);
    } catch (error) {
      console.error('[MCPToolService] Error filtering tools by server:', error);
      return [];
    }
  }

  /**
   * Validate tool parameters against schema
   */
  validateToolParams(tool: MCPTool, params: Record<string, any>): MCPToolValidationResult {
    const errors: string[] = [];
    const missingRequired: string[] = [];

    // If no schema, assume valid
    if (!tool.inputSchema || !tool.inputSchema.properties) {
      return { valid: true, errors: [], missingRequired: [] };
    }

    const schema = tool.inputSchema;
    const required = schema.required || [];

    // Check required parameters
    for (const reqParam of required) {
      if (!(reqParam in params) || params[reqParam] === undefined || params[reqParam] === '') {
        missingRequired.push(reqParam);
        errors.push(`Missing required parameter: ${reqParam}`);
      }
    }

    // Basic type validation
    if (schema.properties) {
      for (const [key, value] of Object.entries(params)) {
        if (schema.properties[key]) {
          const expectedType = schema.properties[key].type;
          const actualType = typeof value;

          // Simple type checking
          if (expectedType === 'string' && actualType !== 'string') {
            errors.push(`Parameter '${key}' should be string, got ${actualType}`);
          } else if (expectedType === 'number' && actualType !== 'number') {
            errors.push(`Parameter '${key}' should be number, got ${actualType}`);
          } else if (expectedType === 'boolean' && actualType !== 'boolean') {
            errors.push(`Parameter '${key}' should be boolean, got ${actualType}`);
          }
        }
      }
    }

    return {
      valid: errors.length === 0,
      errors,
      missingRequired
    };
  }

  /**
   * Categorize a tool based on server ID and tool name
   */
  private categorizeTool(tool: MCPTool): MCPToolCategory {
    const serverId = tool.serverId.toLowerCase();
    const toolName = tool.name.toLowerCase();

    // Database tools
    if (serverId.includes('postgres') || serverId.includes('questdb') ||
        serverId.includes('sqlite') || serverId.includes('mysql') ||
        toolName.includes('query') || toolName.includes('database')) {
      return 'database';
    }

    // Web tools
    if (serverId.includes('wikipedia') || serverId.includes('search') ||
        serverId.includes('fetch') || serverId.includes('brave') ||
        toolName.includes('search') || toolName.includes('fetch')) {
      return 'web';
    }

    // File tools
    if (serverId.includes('filesystem') || serverId.includes('file') ||
        toolName.includes('read') || toolName.includes('write') || toolName.includes('file')) {
      return 'file';
    }

    // API tools
    if (serverId.includes('api') || serverId.includes('http') ||
        toolName.includes('api') || toolName.includes('request')) {
      return 'api';
    }

    // Data tools
    if (toolName.includes('data') || toolName.includes('analyze') ||
        toolName.includes('transform')) {
      return 'data';
    }

    // Productivity tools
    if (serverId.includes('slack') || serverId.includes('email') ||
        serverId.includes('calendar') || serverId.includes('notion')) {
      return 'productivity';
    }

    return 'other';
  }

  /**
   * Get a specific tool by server and name
   */
  async getTool(serverId: string, toolName: string): Promise<MCPTool | null> {
    try {
      const tools = await this.getAllTools();
      return tools.find(t => t.serverId === serverId && t.name === toolName) || null;
    } catch (error) {
      console.error('[MCPToolService] Error getting tool:', error);
      return null;
    }
  }

  /**
   * Check if a specific tool is available
   */
  async isToolAvailable(serverId: string, toolName: string): Promise<boolean> {
    const tool = await this.getTool(serverId, toolName);
    return tool !== null;
  }

  /**
   * Get all available server IDs
   */
  async getAvailableServers(): Promise<string[]> {
    try {
      const tools = await this.getAllTools();
      const serverIds = new Set(tools.map(t => t.serverId));
      return Array.from(serverIds);
    } catch (error) {
      console.error('[MCPToolService] Error getting available servers:', error);
      return [];
    }
  }

  /**
   * Clear tool cache (useful when servers are added/removed)
   */
  clearCache(): void {
    this.toolCache = [];
    this.cacheTimestamp = 0;
  }

  /**
   * Format tools for OpenAI function calling
   * Used by Chat tab and Agents tab
   */
  formatToolsForOpenAI(tools: MCPTool[]): any[] {
    return tools.map(tool => ({
      type: 'function' as const,
      function: {
        name: `${tool.serverId}__${tool.name}`,
        description: tool.description || `Tool: ${tool.name} from ${tool.serverId}`,
        parameters: tool.inputSchema || {
          type: 'object',
          properties: {},
          required: []
        }
      }
    }));
  }

  /**
   * Parse OpenAI function name back to serverId and toolName
   */
  parseOpenAIFunctionName(functionName: string): { serverId: string; toolName: string } | null {
    const parts = functionName.split('__');
    if (parts.length !== 2) {
      console.error('[MCPToolService] Invalid function name format:', functionName);
      return null;
    }
    return {
      serverId: parts[0],
      toolName: parts[1]
    };
  }
}

// Export singleton instance
export const mcpToolService = new MCPToolService();
