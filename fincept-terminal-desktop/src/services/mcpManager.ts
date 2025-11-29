// MCP Manager - Server Lifecycle Management
// Handles server installation, startup, shutdown, and tool aggregation

import MCPClient, { MCPTool, MCPConnectionConfig } from './mcpClient';
import { sqliteService } from './sqliteService';

export interface MCPServer {
  id: string;
  name: string;
  description: string;
  command: string;
  args: string[];
  env?: Record<string, string>;
  category: string;
  icon: string;
  enabled: boolean;
  auto_start: boolean;
  status: 'running' | 'stopped' | 'error';
  created_at: string;
  updated_at: string;
}

export interface MCPServerWithStats extends MCPServer {
  toolCount: number;
  callsToday: number;
  lastUsed?: string;
}

class MCPManager {
  private clients = new Map<string, MCPClient>();
  private healthCheckInterval: NodeJS.Timeout | null = null;
  private serverErrors = new Map<string, { count: number; lastError: string; lastErrorTime: number }>();

  constructor() {
    // Start health check on initialization
    this.startHealthCheck();
  }

  // Install a new MCP server
  async installServer(config: {
    id: string;
    name: string;
    description: string;
    command: string;
    args: string[];
    env?: Record<string, string>;
    category: string;
    icon: string;
  }): Promise<void> {
    try {
      console.log(`[MCPManager] Installing server: ${config.name}`);

      // Check if server already exists
      const existing = await sqliteService.getMCPServer(config.id);

      if (existing) {
        console.log(`[MCPManager] Server ${config.id} already installed, updating config...`);

        // Update existing server config
        await sqliteService.updateMCPServerConfig(config.id, {
          env: config.env ? JSON.stringify(config.env) : null
        });

        // Restart if running
        if (this.clients.has(config.id)) {
          await this.stopServer(config.id);
        }

        // Try to start with new config
        try {
          await this.startServer(config.id);
        } catch (startError) {
          console.warn(`[MCPManager] Failed to start existing server: ${startError}`);
          // Don't throw - server is already installed
        }

        console.log(`[MCPManager] Server ${config.name} updated successfully`);
        return;
      }

      // Add new server to database
      await sqliteService.addMCPServer({
        id: config.id,
        name: config.name,
        description: config.description,
        command: config.command,
        args: JSON.stringify(config.args),
        env: config.env ? JSON.stringify(config.env) : null,
        category: config.category,
        icon: config.icon,
        enabled: true,
        auto_start: false,
        status: 'stopped'
      });

      // Try to start the server (don't fail install if start fails)
      try {
        await this.startServer(config.id);
      } catch (startError) {
        console.warn(`[MCPManager] Server installed but failed to start: ${startError}`);
        // Server is installed, just not running - this is OK
      }

      console.log(`[MCPManager] Server ${config.name} installed successfully`);
    } catch (error) {
      console.error('[MCPManager] Installation error:', error);
      throw error;
    }
  }

  // Start an MCP server
  async startServer(serverId: string): Promise<void> {
    try {
      // Get server config from database
      const server = await sqliteService.getMCPServer(serverId);
      if (!server) {
        throw new Error(`Server ${serverId} not found`);
      }

      if (!server.enabled) {
        throw new Error(`Server ${serverId} is disabled`);
      }

      console.log(`[MCPManager] Starting server: ${server.name}`);

      // Create client if doesn't exist
      if (!this.clients.has(serverId)) {
        this.clients.set(serverId, new MCPClient(serverId));
      }

      const client = this.clients.get(serverId)!;

      // Check if already connected
      if (client.isConnected()) {
        console.log(`[MCPManager] Server ${serverId} already running`);
        return;
      }

      // Parse config
      const args = JSON.parse(server.args);
      const env = server.env ? JSON.parse(server.env) : undefined;

      // Connect
      await client.connect({
        serverId,
        command: server.command,
        args,
        env
      });

      // Update status
      await sqliteService.updateMCPServerStatus(serverId, 'running');

      // Fetch and cache tools
      const tools = await client.listTools();
      await sqliteService.cacheMCPTools(serverId, tools);

      // Clear error tracking on successful start
      this.serverErrors.delete(serverId);

      console.log(`[MCPManager] Server ${server.name} started with ${tools.length} tools`);
    } catch (error) {
      console.error(`[MCPManager] Error starting server ${serverId}:`, error);

      // Track error
      const errorMsg = error instanceof Error ? error.message : 'Unknown error';
      const existing = this.serverErrors.get(serverId) || { count: 0, lastError: '', lastErrorTime: 0 };
      this.serverErrors.set(serverId, {
        count: existing.count + 1,
        lastError: errorMsg,
        lastErrorTime: Date.now()
      });

      await sqliteService.updateMCPServerStatus(serverId, 'error');
      throw error;
    }
  }

  // Stop an MCP server
  async stopServer(serverId: string): Promise<void> {
    try {
      const client = this.clients.get(serverId);
      if (client) {
        await client.disconnect();
        this.clients.delete(serverId);
      }

      await sqliteService.updateMCPServerStatus(serverId, 'stopped');
      console.log(`[MCPManager] Server ${serverId} stopped`);
    } catch (error) {
      console.error(`[MCPManager] Error stopping server ${serverId}:`, error);
      throw error;
    }
  }

  // Remove an MCP server
  async removeServer(serverId: string): Promise<void> {
    try {
      // Stop if running
      await this.stopServer(serverId);

      // Remove from database
      await sqliteService.deleteMCPServer(serverId);

      console.log(`[MCPManager] Server ${serverId} removed`);
    } catch (error) {
      console.error(`[MCPManager] Error removing server ${serverId}:`, error);
      throw error;
    }
  }

  // Get all servers with stats
  async getServersWithStats(): Promise<MCPServerWithStats[]> {
    const servers = await sqliteService.getMCPServers();

    const serversWithStats = await Promise.all(
      servers.map(async (server) => {
        const stats = await sqliteService.getMCPServerStats(server.id);
        // Check actual status by verifying client exists and is connected
        const client = this.clients.get(server.id);
        const actualStatus = client && client.isConnected() ? 'running' : 'stopped';

        return {
          ...server,
          args: JSON.parse(server.args),
          env: server.env ? JSON.parse(server.env) : undefined,
          status: actualStatus as 'running' | 'stopped' | 'error',
          toolCount: stats.toolCount,
          callsToday: stats.callsToday,
          lastUsed: stats.lastUsed || undefined
        };
      })
    );

    return serversWithStats as MCPServerWithStats[];
  }

  // Get all available tools from all running servers
  async getAllTools(): Promise<Array<MCPTool & { serverId: string; serverName: string }>> {
    const allTools: Array<MCPTool & { serverId: string; serverName: string }> = [];

    for (const [serverId, client] of this.clients.entries()) {
      if (client.isConnected()) {
        try {
          const tools = await client.listTools();
          const server = await sqliteService.getMCPServer(serverId);

          const enrichedTools = tools.map(tool => ({
            ...tool,
            serverId,
            serverName: server?.name || serverId
          }));

          allTools.push(...enrichedTools);
        } catch (error) {
          console.error(`[MCPManager] Error getting tools from ${serverId}:`, error);
        }
      }
    }

    return allTools;
  }

  // Call a tool on a specific server
  async callTool(
    serverId: string,
    toolName: string,
    args: Record<string, any> = {}
  ): Promise<any> {
    const client = this.clients.get(serverId);

    if (!client || !client.isConnected()) {
      throw new Error(`Server ${serverId} is not connected`);
    }

    const startTime = Date.now();

    try {
      const result = await client.callTool(toolName, args);
      const executionTime = Date.now() - startTime;

      // Log usage
      await sqliteService.logMCPToolUsage({
        serverId,
        toolName,
        args: JSON.stringify(args),
        result: JSON.stringify(result),
        success: !result.isError,
        executionTime
      });

      return result;
    } catch (error) {
      const executionTime = Date.now() - startTime;

      // Log error
      await sqliteService.logMCPToolUsage({
        serverId,
        toolName,
        args: JSON.stringify(args),
        result: null,
        success: false,
        executionTime,
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      throw error;
    }
  }

  // Update server configuration
  async updateServerConfig(
    serverId: string,
    updates: {
      env?: Record<string, string>;
      enabled?: boolean;
      auto_start?: boolean;
    }
  ): Promise<void> {
    // Convert env to string if provided
    const dbUpdates: Partial<{ env: string | null; enabled: boolean; auto_start: boolean }> = {
      ...updates,
      env: updates.env ? JSON.stringify(updates.env) : undefined
    };

    await sqliteService.updateMCPServerConfig(serverId, dbUpdates as any);

    // If server is running and env changed, restart it
    if (updates.env && this.clients.has(serverId)) {
      await this.stopServer(serverId);
      if (updates.enabled !== false) {
        await this.startServer(serverId);
      }
    }
  }

  // Start all enabled servers
  async startAllEnabledServers(): Promise<void> {
    const servers = await sqliteService.getMCPServers();
    const enabledServers = servers.filter(s => s.enabled);

    console.log(`[MCPManager] Starting ${enabledServers.length} enabled servers`);

    for (const server of enabledServers) {
      try {
        await this.startServer(server.id);
      } catch (error) {
        console.error(`[MCPManager] Failed to start ${server.name}:`, error);
      }
    }
  }

  // Start auto-start enabled servers on launch
  async startAutoStartServers(): Promise<void> {
    try {
      const autoStartServers = await sqliteService.getAutoStartServers();

      console.log(`[MCPManager] Auto-starting ${autoStartServers.length} servers`);

      for (const server of autoStartServers) {
        try {
          await this.startServer(server.id);
          console.log(`[MCPManager] Auto-started: ${server.name}`);
        } catch (error) {
          console.error(`[MCPManager] Failed to auto-start ${server.name}:`, error);
        }
      }
    } catch (error) {
      console.error('[MCPManager] Error during auto-start:', error);
    }
  }

  // Stop all servers
  async stopAllServers(): Promise<void> {
    const serverIds = Array.from(this.clients.keys());

    for (const serverId of serverIds) {
      try {
        await this.stopServer(serverId);
      } catch (error) {
        console.error(`[MCPManager] Error stopping ${serverId}:`, error);
      }
    }
  }

  // Health check - ping all servers periodically with automatic restart
  private startHealthCheck(): void {
    if (this.healthCheckInterval) {
      clearInterval(this.healthCheckInterval);
    }

    // Only start health check after servers have had time to initialize
    setTimeout(() => {
      this.healthCheckInterval = setInterval(async () => {
        for (const [serverId, client] of this.clients.entries()) {
          try {
            const isAlive = await client.ping();

            if (!isAlive) {
              console.warn(`[MCPManager] Server ${serverId} is not responding`);

              // Get server config to check if we should auto-restart
              const server = await sqliteService.getMCPServer(serverId);

              if (server && server.enabled) {
                const errorInfo = this.serverErrors.get(serverId) || { count: 0, lastError: '', lastErrorTime: 0 };

                // Only restart if last error was more than 5 minutes ago (avoid rapid restart loops)
                const timeSinceLastError = Date.now() - errorInfo.lastErrorTime;
                const shouldRetry = errorInfo.count < 3 && timeSinceLastError > 300000; // 5 minutes

                if (shouldRetry) {
                  console.log(`[MCPManager] Attempting automatic restart of ${serverId} (attempt ${errorInfo.count + 1}/3)`);

                  try {
                    // Stop and restart
                    await this.stopServer(serverId);
                    await this.startServer(serverId);
                    console.log(`[MCPManager] Successfully restarted ${serverId}`);
                  } catch (restartError) {
                    console.error(`[MCPManager] Failed to restart ${serverId}:`, restartError);
                  }
                } else if (errorInfo.count >= 3) {
                  console.error(`[MCPManager] Server ${serverId} exceeded max restart attempts (3)`);
                  await sqliteService.updateMCPServerStatus(serverId, 'error');
                }
              } else {
                await sqliteService.updateMCPServerStatus(serverId, 'error');
              }
            } else {
              // Server is healthy, clear any error tracking
              if (this.serverErrors.has(serverId)) {
                this.serverErrors.delete(serverId);
              }
            }
          } catch (error) {
            // Don't track errors during health check - too noisy
            console.debug(`[MCPManager] Health check failed for ${serverId}:`, error);
          }
        }
      }, 60000); // Check every 60 seconds (less aggressive)
    }, 10000); // Wait 10 seconds before starting health checks
  }

  // Get server health info
  getServerHealthInfo(serverId: string): { errorCount: number; lastError?: string; lastErrorTime?: number } | null {
    const errorInfo = this.serverErrors.get(serverId);
    if (!errorInfo) return null;

    return {
      errorCount: errorInfo.count,
      lastError: errorInfo.lastError,
      lastErrorTime: errorInfo.lastErrorTime
    };
  }

  // Clear error tracking for a server
  clearServerErrors(serverId: string): void {
    this.serverErrors.delete(serverId);
  }

  // Cleanup
  async cleanup(): Promise<void> {
    if (this.healthCheckInterval) {
      clearInterval(this.healthCheckInterval);
      this.healthCheckInterval = null;
    }

    await this.stopAllServers();
  }
}

// Singleton instance
export const mcpManager = new MCPManager();
