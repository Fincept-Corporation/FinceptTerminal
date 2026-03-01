/**
 * Node Editor MCP Tools — Natural Language Helpers
 *
 * Tools for chat-bubble integration: describe workflows, suggest next nodes,
 * and add nodes using natural language descriptions.
 */

import { InternalTool } from '../../types';
import { AVAILABLE_NODE_TYPES, getNodeColor } from './constants';

export const nlHelperTools: InternalTool[] = [
  // ============================================================================
  // NATURAL LANGUAGE HELPERS (For Chat Bubble Integration)
  // ============================================================================
  {
    name: 'node_editor_describe_workflow',
    description: `Get a human-readable description of a workflow. Useful to understand what a workflow does before modifying it or to explain it to the user.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to describe',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      const workflow = await contexts.getNodeWorkflow(args.workflow_id);
      if (!workflow) {
        return { success: false, error: `Workflow '${args.workflow_id}' not found` };
      }

      const nodes = workflow.nodes || [];
      const edges = workflow.edges || [];

      // Build description
      const nodeDescriptions = nodes.map(n => {
        const type = n.parameters?.nodeType || n.type || 'unknown';
        const label = n.label || type;
        const desc = AVAILABLE_NODE_TYPES[type as keyof typeof AVAILABLE_NODE_TYPES] || '';
        return `  - ${label} (${type})${desc ? ': ' + desc : ''}`;
      });

      // Find trigger and output nodes
      const triggerNodes = nodes.filter(n =>
        (n.type || '').toLowerCase().includes('trigger') ||
        (n.parameters?.nodeType || '').toLowerCase().includes('trigger')
      );
      const outputNodes = nodes.filter(n =>
        !edges.some(e => e.source === n.id) // Nodes with no outgoing edges
      );

      const description = `
**Workflow: ${workflow.name}**
${workflow.description ? `Description: ${workflow.description}` : ''}

**Nodes (${nodes.length}):**
${nodeDescriptions.join('\n')}

**Flow:**
- Starts with: ${triggerNodes.length > 0 ? triggerNodes.map(n => n.label || n.type).join(', ') : 'No trigger (manual start)'}
- Ends with: ${outputNodes.map(n => n.label || n.type).join(', ')}
- Connections: ${edges.length}

**Status:** ${workflow.status}
      `.trim();

      return {
        success: true,
        data: {
          summary: description,
          nodeCount: nodes.length,
          edgeCount: edges.length,
          hasTrigger: triggerNodes.length > 0,
          status: workflow.status,
        },
        message: description,
      };
    },
  },

  {
    name: 'node_editor_suggest_next_node',
    description: `Suggest what node to add next based on the current workflow state. Helps users build workflows step by step.`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to analyze',
        },
        goal: {
          type: 'string',
          description: 'Optional: What the user wants to achieve (e.g., "send alert when RSI is low")',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      const workflow = await contexts.getNodeWorkflow(args.workflow_id);
      if (!workflow) {
        return { success: false, error: `Workflow '${args.workflow_id}' not found` };
      }

      const nodes = workflow.nodes || [];
      const edges = workflow.edges || [];

      // Find the last node (no outgoing edges)
      const lastNodes = nodes.filter(n => !edges.some(e => e.source === n.id));
      const lastNode = lastNodes[lastNodes.length - 1];
      const lastNodeType = lastNode?.parameters?.nodeType || lastNode?.type || '';

      // Analyze workflow state
      const hasTrigger = nodes.some(n =>
        (n.type || '').toLowerCase().includes('trigger') ||
        (n.parameters?.nodeType || '').toLowerCase().includes('trigger')
      );
      const hasDataSource = nodes.some(n =>
        (n.type || '').includes('data-source') ||
        ['yfinance', 'getQuote', 'getHistoricalData'].includes(n.parameters?.nodeType || '')
      );
      const hasIndicators = nodes.some(n =>
        (n.type || '').includes('indicator') ||
        n.parameters?.nodeType === 'technicalIndicators'
      );
      const hasCondition = nodes.some(n =>
        n.parameters?.nodeType === 'ifElse' || n.parameters?.nodeType === 'switch'
      );
      const hasNotification = nodes.some(n =>
        ['telegram', 'discord', 'slack', 'email', 'webhook'].includes(n.parameters?.nodeType || '')
      );
      const hasTrading = nodes.some(n =>
        ['placeOrder', 'riskCheck'].includes(n.parameters?.nodeType || '')
      );

      // Build suggestions based on state
      const suggestions: Array<{ node: string; reason: string; priority: number }> = [];

      if (nodes.length === 0) {
        suggestions.push({ node: 'manualTrigger', reason: 'Start with a trigger to define when the workflow runs', priority: 1 });
        suggestions.push({ node: 'scheduleTrigger', reason: 'Or use schedule trigger for automated runs', priority: 2 });
      } else if (!hasTrigger) {
        suggestions.push({ node: 'manualTrigger', reason: 'Add a trigger so the workflow can be started', priority: 1 });
      } else if (!hasDataSource) {
        suggestions.push({ node: 'yfinance', reason: 'Add a data source to fetch market data', priority: 1 });
      } else if (!hasIndicators && hasDataSource) {
        suggestions.push({ node: 'technicalIndicators', reason: 'Add technical indicators (RSI, MACD, SMA) for analysis', priority: 1 });
        suggestions.push({ node: 'riskAnalysis', reason: 'Or add risk metrics calculation', priority: 2 });
      } else if (!hasCondition && hasIndicators) {
        suggestions.push({ node: 'ifElse', reason: 'Add a condition to decide when to take action (e.g., RSI < 30)', priority: 1 });
      } else if (hasCondition && !hasNotification && !hasTrading) {
        suggestions.push({ node: 'telegram', reason: 'Add notification to alert you when condition is met', priority: 1 });
        suggestions.push({ node: 'placeOrder', reason: 'Or add trading node to execute trades (with risk check)', priority: 2 });
      } else if (hasCondition && !hasTrading) {
        suggestions.push({ node: 'riskCheck', reason: 'Add risk check before placing orders', priority: 1 });
        suggestions.push({ node: 'placeOrder', reason: 'Add order placement after risk check', priority: 2 });
      } else {
        suggestions.push({ node: 'resultsDisplay', reason: 'Add results display to visualize output', priority: 1 });
      }

      // Add goal-specific suggestions
      if (args.goal) {
        const goalLower = args.goal.toLowerCase();
        if (goalLower.includes('alert') || goalLower.includes('notify')) {
          suggestions.unshift({ node: 'telegram', reason: 'Goal mentions alerts - add Telegram notification', priority: 0 });
        }
        if (goalLower.includes('trade') || goalLower.includes('order') || goalLower.includes('buy') || goalLower.includes('sell')) {
          suggestions.unshift({ node: 'riskCheck', reason: 'Goal mentions trading - add risk check first', priority: 0 });
        }
        if (goalLower.includes('backtest')) {
          suggestions.unshift({ node: 'backtestEngine', reason: 'Goal mentions backtesting - add backtest engine', priority: 0 });
        }
      }

      // Sort by priority and take top 3
      suggestions.sort((a, b) => a.priority - b.priority);
      const topSuggestions = suggestions.slice(0, 3);

      return {
        success: true,
        data: {
          suggestions: topSuggestions,
          currentState: {
            nodeCount: nodes.length,
            hasTrigger,
            hasDataSource,
            hasIndicators,
            hasCondition,
            hasNotification,
            hasTrading,
            lastNode: lastNode?.label || lastNodeType,
          },
        },
        message: `Suggested next nodes:\n${topSuggestions.map((s, i) => `${i + 1}. ${s.node}: ${s.reason}`).join('\n')}`,
      };
    },
  },

  {
    name: 'node_editor_add_node_by_description',
    description: `Add a node to a workflow using natural language description. Automatically determines the best node type and position.

Examples:
- "add RSI indicator" -> adds technicalIndicators node with RSI
- "add price alert for AAPL at $200" -> adds ifElse with price condition
- "send telegram notification" -> adds telegram node
- "add risk check with max loss $500" -> adds riskCheck node`,
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to add the node to',
        },
        description: {
          type: 'string',
          description: 'Natural language description of what node to add (e.g., "add RSI indicator", "send telegram alert")',
        },
        connect_to_last: {
          type: 'string',
          description: 'Whether to connect the new node to the last node in the workflow',
          enum: ['yes', 'no', ''],
        },
      },
      required: ['workflow_id', 'description'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getNodeWorkflow || !contexts.addNodeToWorkflow || !contexts.connectNodes) {
        return { success: false, error: 'Node editor context not available' };
      }

      const workflow = await contexts.getNodeWorkflow(args.workflow_id);
      if (!workflow) {
        return { success: false, error: `Workflow '${args.workflow_id}' not found` };
      }

      const desc = args.description.toLowerCase();
      const nodes = workflow.nodes || [];
      const edges = workflow.edges || [];

      // Find last node for positioning and connection
      const lastNodes = nodes.filter(n => !edges.some(e => e.source === n.id));
      const lastNode = lastNodes[lastNodes.length - 1];
      const lastX = lastNode?.position?.x ?? 100;
      const lastY = lastNode?.position?.y ?? 100;

      // Determine node type from description
      let nodeType = 'custom';
      let uiType = 'custom';
      let label = '';
      let params: Record<string, any> = {};

      // RSI/MACD/SMA/EMA indicators
      if (desc.includes('rsi')) {
        nodeType = 'technicalIndicators';
        uiType = 'technical-indicator';
        label = 'RSI(14)';
        params = { indicator: 'RSI', period: 14 };
      } else if (desc.includes('macd')) {
        nodeType = 'technicalIndicators';
        uiType = 'technical-indicator';
        label = 'MACD';
        params = { indicator: 'MACD', period: 12 };
      } else if (desc.includes('sma') || desc.includes('moving average')) {
        nodeType = 'technicalIndicators';
        uiType = 'technical-indicator';
        label = 'SMA(20)';
        params = { indicator: 'SMA', period: 20 };
      } else if (desc.includes('ema')) {
        nodeType = 'technicalIndicators';
        uiType = 'technical-indicator';
        label = 'EMA(9)';
        params = { indicator: 'EMA', period: 9 };
      } else if (desc.includes('bollinger') || desc.includes('bb')) {
        nodeType = 'technicalIndicators';
        uiType = 'technical-indicator';
        label = 'Bollinger Bands';
        params = { indicator: 'BB', period: 20 };
      }
      // Data sources
      else if (desc.includes('stock data') || desc.includes('fetch') || desc.includes('yfinance') || desc.includes('historical')) {
        nodeType = 'yfinance';
        uiType = 'data-source';
        label = 'Stock Data';
        params = { operation: 'history', period: '1mo', interval: '1d' };
        // Extract symbol if mentioned
        const symbolMatch = desc.match(/\b([A-Z]{1,5})\b/);
        if (symbolMatch) {
          params.symbol = symbolMatch[1];
          label = `${symbolMatch[1]} Data`;
        }
      } else if (desc.includes('quote') || desc.includes('price')) {
        nodeType = 'yfinance';
        uiType = 'data-source';
        label = 'Get Quote';
        params = { operation: 'quote' };
      }
      // Conditions
      else if (desc.includes('if') || desc.includes('condition') || desc.includes('check') || desc.includes('when')) {
        nodeType = 'ifElse';
        uiType = 'custom';
        label = 'Condition';
        params = { condition: '{{$json.value}} > 0' };
      }
      // Notifications
      else if (desc.includes('telegram')) {
        nodeType = 'telegram';
        uiType = 'custom';
        label = 'Telegram Alert';
        params = { message: 'Alert triggered!' };
      } else if (desc.includes('discord')) {
        nodeType = 'discord';
        uiType = 'custom';
        label = 'Discord Alert';
        params = { message: 'Alert triggered!' };
      } else if (desc.includes('slack')) {
        nodeType = 'slack';
        uiType = 'custom';
        label = 'Slack Alert';
        params = { message: 'Alert triggered!' };
      } else if (desc.includes('email')) {
        nodeType = 'email';
        uiType = 'custom';
        label = 'Email Alert';
        params = { subject: 'Alert', message: 'Alert triggered!' };
      } else if (desc.includes('notification') || desc.includes('alert')) {
        nodeType = 'telegram';
        uiType = 'custom';
        label = 'Notification';
        params = { message: 'Alert triggered!' };
      }
      // Trading
      else if (desc.includes('order') || desc.includes('buy') || desc.includes('sell') || desc.includes('trade')) {
        nodeType = 'placeOrder';
        uiType = 'custom';
        label = desc.includes('buy') ? 'Buy Order' : desc.includes('sell') ? 'Sell Order' : 'Place Order';
        params = {
          side: desc.includes('buy') ? 'BUY' : 'SELL',
          orderType: 'MARKET',
          quantity: 1,
          paperTrading: true,
        };
      } else if (desc.includes('risk')) {
        nodeType = 'riskCheck';
        uiType = 'custom';
        label = 'Risk Check';
        params = { maxPositionSize: 1000, maxDailyLoss: 500 };
        // Extract numbers if mentioned
        const lossMatch = desc.match(/\$?(\d+)/);
        if (lossMatch) {
          params.maxDailyLoss = parseInt(lossMatch[1]);
        }
      }
      // Triggers
      else if (desc.includes('trigger') || desc.includes('start')) {
        if (desc.includes('schedule') || desc.includes('cron') || desc.includes('daily') || desc.includes('hourly')) {
          nodeType = 'scheduleTrigger';
          uiType = 'system';
          label = 'Schedule Trigger';
          params = { cronExpression: '0 9 * * 1-5' };
        } else {
          nodeType = 'manualTrigger';
          uiType = 'system';
          label = 'Manual Start';
          params = {};
        }
      }
      // Backtest
      else if (desc.includes('backtest')) {
        nodeType = 'backtestEngine';
        uiType = 'backtest';
        label = 'Backtest';
        params = {};
      }
      // Results
      else if (desc.includes('result') || desc.includes('output') || desc.includes('display')) {
        nodeType = 'resultsDisplay';
        uiType = 'results-display';
        label = 'Results';
        params = { displayFormat: 'table' };
      }
      // Default: custom node with the description as label
      else {
        label = args.description.slice(0, 30);
      }

      // Generate node ID
      const nodeId = `${nodeType}_${Date.now()}`;

      // Calculate position (to the right of last node)
      const position = {
        x: lastX + 280,
        y: lastY,
      };

      // Add the node
      const result = await contexts.addNodeToWorkflow({
        workflowId: args.workflow_id,
        nodeId,
        nodeType: uiType,
        position,
        parameters: {
          nodeType,
          color: getNodeColor(nodeType),
          ...params,
        },
        label,
      });

      // Connect to last node if requested
      if (args.connect_to_last !== 'no' && lastNode) {
        await contexts.connectNodes({
          workflowId: args.workflow_id,
          sourceNodeId: lastNode.id,
          targetNodeId: nodeId,
        });
      }

      return {
        success: true,
        data: {
          nodeId,
          nodeType,
          label,
          position,
          connectedTo: lastNode?.id,
        },
        message: `Added "${label}" (${nodeType}) to workflow${lastNode ? ` and connected to "${lastNode.label || lastNode.id}"` : ''}`,
      };
    },
  },
];
