/**
 * Node Editor MCP Tools — Discovery Tools
 *
 * Tools for discovering available nodes and categories.
 */

import { InternalTool } from '../../types';
import { NODE_CATEGORIES } from './constants';

export const discoveryTools: InternalTool[] = [
  // ============================================================================
  // NODE DISCOVERY TOOLS
  // ============================================================================
  {
    name: 'node_editor_list_available_nodes',
    description: `List all available nodes in the workflow editor. Returns node types with their configurations, parameters, and connection types. Use this to understand what nodes can be used to build a workflow.

Categories available: ${NODE_CATEGORIES.join(', ')}

Common use cases:
- "What nodes are available for technical analysis?" -> filter by 'analytics' category
- "Show me trading nodes" -> filter by 'trading' category
- "What trigger nodes can start a workflow?" -> filter by 'trigger' category`,
    inputSchema: {
      type: 'object',
      properties: {
        category: {
          type: 'string',
          description: `Filter by category: ${NODE_CATEGORIES.join(', ')}. Leave empty to get all nodes.`,
          enum: [...NODE_CATEGORIES, ''],
        },
        search: {
          type: 'string',
          description: 'Search query to filter nodes by name or description',
        },
      },
      required: [],
    },
    handler: async (args, contexts) => {
      if (!contexts.listAvailableNodes) {
        return { success: false, error: 'Node editor context not available' };
      }
      const nodes = await contexts.listAvailableNodes(args.category, args.search);
      return {
        success: true,
        data: nodes,
        message: `Found ${nodes.length} nodes${args.category ? ` in category '${args.category}'` : ''}`,
      };
    },
  },

  {
    name: 'node_editor_get_node_details',
    description: `Get detailed information about a specific node type including all its parameters, default values, and connection types. Use this before configuring a node in a workflow.`,
    inputSchema: {
      type: 'object',
      properties: {
        node_type: {
          type: 'string',
          description: 'The node type name (e.g., "technicalIndicators", "placeOrder", "yfinance")',
        },
      },
      required: ['node_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getNodeDetails) {
        return { success: false, error: 'Node editor context not available' };
      }
      const details = await contexts.getNodeDetails(args.node_type);
      if (!details) {
        return { success: false, error: `Node type '${args.node_type}' not found` };
      }
      return {
        success: true,
        data: details,
        message: `Node details for '${details.displayName}'`,
      };
    },
  },

  {
    name: 'node_editor_get_node_categories',
    description: 'Get all available node categories with their descriptions and node counts.',
    inputSchema: {
      type: 'object',
      properties: {},
      required: [],
    },
    handler: async (_args, contexts) => {
      if (!contexts.getNodeCategories) {
        return { success: false, error: 'Node editor context not available' };
      }
      const categories = await contexts.getNodeCategories();
      return {
        success: true,
        data: categories,
        message: `Found ${categories.length} node categories`,
      };
    },
  },
];
