/**
 * MCP Server Module
 * Handles MCP server configurations, tools, and usage logs
 */

import type { MCPServer } from '../types';
import type { SQLiteBase } from '../SQLiteBase';

export interface MCPToolUsageLog {
    serverId: string;
    toolName: string;
    args: string;
    result: string | null;
    success: boolean;
    executionTime: number;
    error?: string;
}

export interface MCPServerStats {
    toolCount: number;
    callsToday: number;
    lastUsed: string | null;
}

export class MCPServerModule {
    constructor(private base: SQLiteBase) { }

    async addMCPServer(server: Omit<MCPServer, 'created_at' | 'updated_at'>): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `INSERT OR REPLACE INTO mcp_servers
       (id, name, description, command, args, env, category, icon, enabled, auto_start, status, updated_at)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, CURRENT_TIMESTAMP)`,
            [
                server.id,
                server.name,
                server.description,
                server.command,
                server.args,
                server.env || null,
                server.category,
                server.icon,
                server.enabled ? 1 : 0,
                server.auto_start ? 1 : 0,
                server.status,
            ]
        );
    }

    async getMCPServers(): Promise<MCPServer[]> {
        await this.base.ensureInitialized();
        return await this.base.select<MCPServer[]>('SELECT * FROM mcp_servers ORDER BY name');
    }

    async getMCPServer(id: string): Promise<MCPServer | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<MCPServer[]>(
            'SELECT * FROM mcp_servers WHERE id = $1',
            [id]
        );
        return results[0] || null;
    }

    async updateMCPServerStatus(id: string, status: string): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute(
            'UPDATE mcp_servers SET status = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2',
            [status, id]
        );
    }

    async updateMCPServerConfig(id: string, updates: Partial<MCPServer>): Promise<void> {
        await this.base.ensureInitialized();

        const fields: string[] = [];
        const values: any[] = [];
        let paramIndex = 1;

        if (updates.name !== undefined) {
            fields.push(`name = $${paramIndex++}`);
            values.push(updates.name);
        }
        if (updates.description !== undefined) {
            fields.push(`description = $${paramIndex++}`);
            values.push(updates.description);
        }
        if (updates.command !== undefined) {
            fields.push(`command = $${paramIndex++}`);
            values.push(updates.command);
        }
        if (updates.args !== undefined) {
            fields.push(`args = $${paramIndex++}`);
            values.push(updates.args);
        }
        if (updates.env !== undefined) {
            fields.push(`env = $${paramIndex++}`);
            values.push(updates.env);
        }
        if (updates.category !== undefined) {
            fields.push(`category = $${paramIndex++}`);
            values.push(updates.category);
        }
        if (updates.icon !== undefined) {
            fields.push(`icon = $${paramIndex++}`);
            values.push(updates.icon);
        }
        if (updates.enabled !== undefined) {
            fields.push(`enabled = $${paramIndex++}`);
            values.push(updates.enabled ? 1 : 0);
        }
        if (updates.auto_start !== undefined) {
            fields.push(`auto_start = $${paramIndex++}`);
            values.push(updates.auto_start ? 1 : 0);
        }

        if (fields.length === 0) return;

        fields.push(`updated_at = CURRENT_TIMESTAMP`);
        values.push(id);

        const sql = `UPDATE mcp_servers SET ${fields.join(', ')} WHERE id = $${paramIndex}`;
        await this.base.execute(sql, values);
    }

    async deleteMCPServer(id: string): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute('DELETE FROM mcp_servers WHERE id = $1', [id]);
    }

    async getAutoStartServers(): Promise<MCPServer[]> {
        await this.base.ensureInitialized();
        return await this.base.select<MCPServer[]>(
            'SELECT * FROM mcp_servers WHERE auto_start = 1 AND enabled = 1'
        );
    }

    async cacheMCPTools(serverId: string, tools: any[]): Promise<void> {
        await this.base.ensureInitialized();

        // Delete existing tools for this server
        await this.base.execute('DELETE FROM mcp_tools WHERE server_id = $1', [serverId]);

        // Insert new tools
        for (const tool of tools) {
            await this.base.execute(
                `INSERT INTO mcp_tools (id, server_id, name, description, input_schema)
         VALUES ($1, $2, $3, $4, $5)`,
                [
                    `${serverId}_${tool.name}`,
                    serverId,
                    tool.name,
                    tool.description || null,
                    tool.inputSchema ? JSON.stringify(tool.inputSchema) : null
                ]
            );
        }
    }

    async getMCPServerStats(serverId: string): Promise<MCPServerStats> {
        await this.base.ensureInitialized();

        const toolCountResult = await this.base.select<{ count: number }[]>(
            'SELECT COUNT(*) as count FROM mcp_tools WHERE server_id = $1',
            [serverId]
        );
        const toolCount = toolCountResult[0]?.count || 0;

        const today = new Date().toISOString().split('T')[0];
        const callsTodayResult = await this.base.select<{ count: number }[]>(
            `SELECT COUNT(*) as count FROM mcp_tool_usage_logs
       WHERE server_id = $1 AND DATE(timestamp) = $2`,
            [serverId, today]
        );
        const callsToday = callsTodayResult[0]?.count || 0;

        const lastUsedResult = await this.base.select<{ timestamp: string }[]>(
            `SELECT timestamp FROM mcp_tool_usage_logs
       WHERE server_id = $1 ORDER BY timestamp DESC LIMIT 1`,
            [serverId]
        );
        const lastUsed = lastUsedResult[0]?.timestamp || null;

        return { toolCount, callsToday, lastUsed };
    }

    async logMCPToolUsage(log: MCPToolUsageLog): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `INSERT INTO mcp_tool_usage_logs (server_id, tool_name, args, result, success, execution_time, error)
       VALUES ($1, $2, $3, $4, $5, $6, $7)`,
            [
                log.serverId,
                log.toolName,
                log.args,
                log.result,
                log.success ? 1 : 0,
                log.executionTime,
                log.error || null
            ]
        );
    }

    async getMCPTools(): Promise<any[]> {
        await this.base.ensureInitialized();
        return await this.base.select<any[]>('SELECT * FROM mcp_tools ORDER BY name');
    }

    async getMCPToolsByServer(serverId: string): Promise<any[]> {
        await this.base.ensureInitialized();
        return await this.base.select<any[]>(
            'SELECT * FROM mcp_tools WHERE server_id = $1 ORDER BY name',
            [serverId]
        );
    }
}
