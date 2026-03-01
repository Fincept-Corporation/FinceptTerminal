/**
 * Node Editor MCP Tools — Creation Tools
 *
 * Tools for creating workflows and adding/connecting nodes.
 */

import { InternalTool } from '../../types';

export const creationTools: InternalTool[] = [
  // ============================================================================
  // WORKFLOW CREATION TOOLS
  // ============================================================================
  {
    name: 'node_editor_create_workflow',
    description: `Create a complete workflow with nodes and edges. The workflow will be saved and available in the Node Editor tab.

IMPORTANT:
- Each node needs a unique ID (use descriptive names like 'trigger_1', 'fetch_data_1', 'indicators_1')
- Position nodes logically (start from left, flow to right). Use x: 100, 280, 560... and y: 100
- Use UI node types: 'system' (triggers), 'data-source' (data fetching), 'technical-indicator', 'custom', 'results-display'
- Include nodeType in parameters for backend execution type
- Include color and label for proper rendering

Example workflow structure:
{
  "name": "RSI Alert Workflow",
  "description": "Fetches stock data and calculates RSI, sends alert if oversold",
  "nodes": [
    { "id": "trigger_1", "type": "system", "position": { "x": 100, "y": 100 }, "parameters": { "nodeType": "manualTrigger", "color": "#10b981" }, "label": "Manual Start" },
    { "id": "data_1", "type": "data-source", "position": { "x": 380, "y": 100 }, "parameters": { "nodeType": "yfinance", "symbol": "AAPL", "period": "1mo", "operation": "history", "color": "#3b82f6" }, "label": "AAPL Historical" },
    { "id": "rsi_1", "type": "technical-indicator", "position": { "x": 660, "y": 100 }, "parameters": { "nodeType": "technicalIndicators", "indicator": "RSI", "period": 14, "color": "#10b981" }, "label": "RSI(14)" },
    { "id": "check_1", "type": "custom", "position": { "x": 940, "y": 100 }, "parameters": { "nodeType": "ifElse", "condition": "{{$json.RSI}} < 30", "color": "#eab308" }, "label": "If RSI < 30" },
    { "id": "alert_1", "type": "custom", "position": { "x": 1220, "y": 50 }, "parameters": { "nodeType": "telegram", "message": "RSI Oversold Alert!", "color": "#f97316" }, "label": "Telegram Alert" }
  ],
  "edges": [
    { "source": "trigger_1", "target": "data_1" },
    { "source": "data_1", "target": "rsi_1" },
    { "source": "rsi_1", "target": "check_1" },
    { "source": "check_1", "target": "alert_1", "sourceHandle": "true" }
  ]
}`,
    inputSchema: {
      type: 'object',
      properties: {
        name: {
          type: 'string',
          description: 'Workflow name (e.g., "RSI Alert Strategy", "Portfolio Rebalancer")',
        },
        description: {
          type: 'string',
          description: 'Description of what the workflow does',
        },
        nodes: {
          type: 'string',
          description: 'JSON array of nodes. Each node has: id (string), type (node type name), position ({x, y}), parameters (object with node-specific config)',
        },
        edges: {
          type: 'string',
          description: 'JSON array of edges connecting nodes. Each edge has: source (node id), target (node id), optional sourceHandle/targetHandle for specific outputs',
        },
      },
      required: ['name', 'nodes', 'edges'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      // Parse nodes and edges from JSON strings
      let nodes, edges;
      try {
        nodes = typeof args.nodes === 'string' ? JSON.parse(args.nodes) : args.nodes;
        edges = typeof args.edges === 'string' ? JSON.parse(args.edges) : args.edges;
      } catch (e) {
        return { success: false, error: `Invalid JSON in nodes or edges: ${e}` };
      }

      const result = await contexts.createNodeWorkflow({
        name: args.name,
        description: args.description || '',
        nodes,
        edges,
      });

      return {
        success: true,
        data: result,
        message: `Workflow "${args.name}" created with ${nodes.length} nodes and ${edges.length} edges. ID: ${result.id}`,
      };
    },
  },

  {
    name: 'node_editor_add_node_to_workflow',
    description: `Add a single node to an existing workflow. Useful for building workflows incrementally.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to add the node to',
        },
        node_id: {
          type: 'string',
          description: 'Unique ID for the new node (e.g., "indicator_2", "alert_1")',
        },
        node_type: {
          type: 'string',
          description: 'Type of node to add (e.g., "technicalIndicators", "placeOrder")',
        },
        position_x: {
          type: 'number',
          description: 'X position in the editor (default: auto-calculated)',
          default: 100,
        },
        position_y: {
          type: 'number',
          description: 'Y position in the editor (default: auto-calculated)',
          default: 100,
        },
        parameters: {
          type: 'string',
          description: 'JSON object with node parameters',
        },
        label: {
          type: 'string',
          description: 'Display label for the node',
        },
      },
      required: ['workflow_id', 'node_id', 'node_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addNodeToWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      let parameters = {};
      if (args.parameters) {
        try {
          parameters = typeof args.parameters === 'string' ? JSON.parse(args.parameters) : args.parameters;
        } catch (e) {
          return { success: false, error: `Invalid JSON in parameters: ${e}` };
        }
      }

      const result = await contexts.addNodeToWorkflow({
        workflowId: args.workflow_id,
        nodeId: args.node_id,
        nodeType: args.node_type,
        position: { x: args.position_x || 100, y: args.position_y || 100 },
        parameters,
        label: args.label,
      });

      return {
        success: true,
        data: result,
        message: `Node '${args.node_id}' of type '${args.node_type}' added to workflow`,
      };
    },
  },

  {
    name: 'node_editor_connect_nodes',
    description: `Connect two nodes in a workflow with an edge. Creates the data flow between nodes.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID',
        },
        source_node_id: {
          type: 'string',
          description: 'ID of the source node (output)',
        },
        target_node_id: {
          type: 'string',
          description: 'ID of the target node (input)',
        },
        source_handle: {
          type: 'string',
          description: 'Output handle name (e.g., "true", "false" for IfElse, or "main" for standard)',
        },
        target_handle: {
          type: 'string',
          description: 'Input handle name (usually "main")',
        },
      },
      required: ['workflow_id', 'source_node_id', 'target_node_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.connectNodes) {
        return { success: false, error: 'Node editor context not available' };
      }

      const result = await contexts.connectNodes({
        workflowId: args.workflow_id,
        sourceNodeId: args.source_node_id,
        targetNodeId: args.target_node_id,
        sourceHandle: args.source_handle || 'main',
        targetHandle: args.target_handle || 'main',
      });

      return {
        success: true,
        data: result,
        message: `Connected '${args.source_node_id}' -> '${args.target_node_id}'`,
      };
    },
  },

  {
    name: 'node_editor_configure_node',
    description: `Update the parameters/configuration of a node in a workflow.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID',
        },
        node_id: {
          type: 'string',
          description: 'ID of the node to configure',
        },
        parameters: {
          type: 'string',
          description: 'JSON object with parameter updates (will be merged with existing)',
        },
      },
      required: ['workflow_id', 'node_id', 'parameters'],
    },
    handler: async (args, contexts) => {
      if (!contexts.configureNode) {
        return { success: false, error: 'Node editor context not available' };
      }

      let parameters;
      try {
        parameters = typeof args.parameters === 'string' ? JSON.parse(args.parameters) : args.parameters;
      } catch (e) {
        return { success: false, error: `Invalid JSON in parameters: ${e}` };
      }

      const result = await contexts.configureNode({
        workflowId: args.workflow_id,
        nodeId: args.node_id,
        parameters,
      });

      return {
        success: true,
        data: result,
        message: `Node '${args.node_id}' configured successfully`,
      };
    },
  },
];
