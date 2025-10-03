// MCP Client - Model Context Protocol Implementation
// Handles stdio/SSE transport for MCP servers

import { invoke } from '@tauri-apps/api/core';

export interface MCPTool {
  name: string;
  description?: string;
  inputSchema: {
    type: string;
    properties?: Record<string, any>;
    required?: string[];
  };
}

export interface MCPResource {
  uri: string;
  name: string;
  description?: string;
  mimeType?: string;
}

export interface MCPServerCapabilities {
  tools?: { listChanged?: boolean };
  resources?: { subscribe?: boolean; listChanged?: boolean };
  prompts?: { listChanged?: boolean };
}

export interface MCPToolResult {
  content: Array<{
    type: string;
    text?: string;
    data?: any;
  }>;
  isError?: boolean;
}

export interface MCPConnectionConfig {
  serverId: string;
  command: string;
  args: string[];
  env?: Record<string, string>;
}

class MCPClient {
  private serverId: string;
  private processId: number | null = null;
  private messageId = 0;
  private pendingRequests = new Map<number, {
    resolve: (value: any) => void;
    reject: (error: any) => void;
  }>();
  private isInitialized = false;
  private capabilities: MCPServerCapabilities = {};

  constructor(serverId: string) {
    this.serverId = serverId;
  }

  // Connect to MCP server via stdio
  async connect(config: MCPConnectionConfig): Promise<void> {
    try {
      console.log(`[MCP] Connecting to server: ${config.serverId}`);

      // Spawn MCP server process via Tauri
      const result = await invoke<{ pid: number; success: boolean; error?: string }>(
        'spawn_mcp_server',
        {
          serverId: config.serverId,
          command: config.command,
          args: config.args,
          env: config.env || {}
        }
      );

      if (!result.success || result.error) {
        throw new Error(result.error || 'Failed to spawn MCP server');
      }

      this.processId = result.pid;
      console.log(`[MCP] Server spawned with PID: ${this.processId}`);

      // Initialize MCP protocol
      await this.initialize();
      console.log(`[MCP] Server initialized successfully`);
    } catch (error) {
      console.error('[MCP] Connection error:', error);
      throw error;
    }
  }

  // Initialize MCP protocol handshake
  private async initialize(): Promise<void> {
    const response = await this.sendRequest('initialize', {
      protocolVersion: '2024-11-05',
      capabilities: {
        roots: {
          listChanged: false
        },
        sampling: {}
      },
      clientInfo: {
        name: 'fincept-terminal',
        version: '1.0.0'
      }
    });

    this.capabilities = response.capabilities || {};
    this.isInitialized = true;

    // Send initialized notification
    await this.sendNotification('notifications/initialized');
  }

  // List all available tools
  async listTools(): Promise<MCPTool[]> {
    if (!this.isInitialized) {
      throw new Error('MCP client not initialized');
    }

    try {
      const response = await this.sendRequest('tools/list', {});
      return response.tools || [];
    } catch (error) {
      console.error('[MCP] Error listing tools:', error);
      return [];
    }
  }

  // Call a tool
  async callTool(name: string, args: Record<string, any> = {}): Promise<MCPToolResult> {
    if (!this.isInitialized) {
      throw new Error('MCP client not initialized');
    }

    try {
      const response = await this.sendRequest('tools/call', {
        name,
        arguments: args
      });

      return response;
    } catch (error) {
      console.error(`[MCP] Error calling tool ${name}:`, error);
      return {
        content: [{
          type: 'text',
          text: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`
        }],
        isError: true
      };
    }
  }

  // List resources
  async listResources(): Promise<MCPResource[]> {
    if (!this.isInitialized) {
      throw new Error('MCP client not initialized');
    }

    try {
      const response = await this.sendRequest('resources/list', {});
      return response.resources || [];
    } catch (error) {
      console.error('[MCP] Error listing resources:', error);
      return [];
    }
  }

  // Read a resource
  async readResource(uri: string): Promise<any> {
    if (!this.isInitialized) {
      throw new Error('MCP client not initialized');
    }

    try {
      const response = await this.sendRequest('resources/read', { uri });
      return response.contents || [];
    } catch (error) {
      console.error(`[MCP] Error reading resource ${uri}:`, error);
      throw error;
    }
  }

  // Ping server to check if alive
  async ping(): Promise<boolean> {
    if (!this.processId) return false;

    try {
      const result = await invoke<boolean>('ping_mcp_server', {
        serverId: this.serverId
      });
      return result;
    } catch (error) {
      console.error('[MCP] Ping error:', error);
      return false;
    }
  }

  // Disconnect from server
  async disconnect(): Promise<void> {
    if (this.processId) {
      try {
        await invoke('kill_mcp_server', {
          serverId: this.serverId
        });
        console.log(`[MCP] Server ${this.serverId} disconnected`);
      } catch (error) {
        console.error('[MCP] Disconnect error:', error);
      }

      this.processId = null;
      this.isInitialized = false;
      this.pendingRequests.clear();
    }
  }

  // Send JSON-RPC request
  private async sendRequest(method: string, params: any): Promise<any> {
    const id = this.messageId++;

    const request = {
      jsonrpc: '2.0',
      id,
      method,
      params
    };

    return new Promise(async (resolve, reject) => {
      this.pendingRequests.set(id, { resolve, reject });

      try {
        const response = await invoke<string>('send_mcp_request', {
          serverId: this.serverId,
          request: JSON.stringify(request)
        });

        const parsed = JSON.parse(response);

        if (parsed.error) {
          reject(new Error(parsed.error.message || 'MCP request failed'));
          this.pendingRequests.delete(id);
        } else {
          resolve(parsed.result);
          this.pendingRequests.delete(id);
        }
      } catch (error) {
        reject(error);
        this.pendingRequests.delete(id);
      }
    });
  }

  // Send notification (no response expected)
  private async sendNotification(method: string, params?: any): Promise<void> {
    const notification = {
      jsonrpc: '2.0',
      method,
      params
    };

    try {
      await invoke('send_mcp_notification', {
        serverId: this.serverId,
        notification: JSON.stringify(notification)
      });
    } catch (error) {
      console.error('[MCP] Notification error:', error);
    }
  }

  // Get server capabilities
  getCapabilities(): MCPServerCapabilities {
    return this.capabilities;
  }

  // Check if connected
  isConnected(): boolean {
    return this.isInitialized && this.processId !== null;
  }
}

export default MCPClient;
