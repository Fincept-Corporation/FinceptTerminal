// MCP Node Configurations
// Dynamically generates node configs from available MCP servers

import { mcpToolService, MCPTool } from '../../../services/mcpToolService';

export interface MCPNodeConfig {
  id: string;
  type: 'mcp-tool';
  label: string;
  category: 'MCP Tools';
  icon: string;
  serverId: string;
  toolName: string;
  description: string;
  inputs: string[];
  outputs: string[];
}

/**
 * Generate node configurations from available MCP tools
 * This function is called when Node Editor initializes
 */
export async function generateMCPNodeConfigs(): Promise<MCPNodeConfig[]> {
  try {
    const tools = await mcpToolService.getAllTools();

    return tools.map((tool: MCPTool) => {
      // Extract inputs from tool schema
      const inputs = tool.inputSchema?.properties
        ? Object.keys(tool.inputSchema.properties)
        : [];

      return {
        id: `mcp-${tool.serverId}-${tool.name}`,
        type: 'mcp-tool' as const,
        label: tool.name.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()),
        category: 'MCP Tools' as const,
        icon: getIconForTool(tool),
        serverId: tool.serverId,
        toolName: tool.name,
        description: tool.description || `${tool.name} from ${tool.serverId}`,
        inputs,
        outputs: ['output'] // All MCP tools have output
      };
    });
  } catch (error) {
    console.error('[MCPNodeConfigs] Failed to generate configs:', error);
    return [];
  }
}

/**
 * Get icon emoji for tool based on server and tool name
 */
function getIconForTool(tool: MCPTool): string {
  const serverId = tool.serverId.toLowerCase();
  const toolName = tool.name.toLowerCase();

  // Database tools
  if (serverId.includes('postgres')) return '🐘';
  if (serverId.includes('questdb')) return '⚡';
  if (serverId.includes('sqlite')) return '💾';
  if (serverId.includes('mysql')) return '🐬';

  // Web tools
  if (serverId.includes('wikipedia')) return '📚';
  if (serverId.includes('search') || serverId.includes('brave')) return '🔍';
  if (serverId.includes('fetch') || toolName.includes('fetch')) return '🌐';

  // File tools
  if (serverId.includes('filesystem') || serverId.includes('file')) return '📁';

  // API tools
  if (serverId.includes('api') || serverId.includes('http')) return '🔌';

  // Productivity tools
  if (serverId.includes('slack')) return '💬';
  if (serverId.includes('email')) return '📧';
  if (serverId.includes('calendar')) return '📅';

  // Default
  return '⚡';
}

/**
 * Get MCP tool categories for filtering in Node Editor
 */
export function getMCPToolCategories(tools: MCPNodeConfig[]): string[] {
  const categories = new Set<string>();

  tools.forEach(tool => {
    const category = categorizeMCPTool(tool);
    categories.add(category);
  });

  return Array.from(categories);
}

/**
 * Categorize MCP tool for grouping in UI
 */
function categorizeMCPTool(tool: MCPNodeConfig): string {
  const serverId = tool.serverId.toLowerCase();

  if (serverId.includes('postgres') || serverId.includes('questdb') ||
      serverId.includes('sqlite') || serverId.includes('mysql')) {
    return 'Database';
  }

  if (serverId.includes('wikipedia') || serverId.includes('search') ||
      serverId.includes('fetch')) {
    return 'Web';
  }

  if (serverId.includes('file')) {
    return 'File System';
  }

  if (serverId.includes('api') || serverId.includes('http')) {
    return 'API';
  }

  return 'Other';
}

/**
 * Refresh MCP node configurations
 * Call this when new MCP servers are installed
 */
export async function refreshMCPNodeConfigs(): Promise<MCPNodeConfig[]> {
  // Clear cache to get fresh tool list
  mcpToolService.clearCache();
  return generateMCPNodeConfigs();
}
