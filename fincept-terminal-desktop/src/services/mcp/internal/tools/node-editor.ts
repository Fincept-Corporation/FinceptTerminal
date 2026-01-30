/**
 * Node Editor MCP Tools
 *
 * Provides AI/Chat access to the node-based workflow editor.
 * Allows LLM to:
 * - List available nodes and their configurations
 * - Create complete workflows with nodes and edges
 * - Save workflows to the node editor for execution
 * - Execute and manage workflows
 */

import { InternalTool } from '../types';

/**
 * Node categories for easier discovery
 */
const NODE_CATEGORIES = [
  'trigger',      // Workflow entry points (ManualTrigger, ScheduleTrigger, PriceAlert, etc.)
  'marketData',   // Market data nodes (YFinance, GetQuote, GetHistoricalData, etc.)
  'trading',      // Trading nodes (PlaceOrder, CancelOrder, GetPositions, etc.)
  'analytics',    // Analytics nodes (TechnicalIndicators, PortfolioOptimization, Backtest, etc.)
  'controlFlow',  // Control flow (IfElse, Switch, Loop, Wait, Merge, Split, etc.)
  'transform',    // Data transformation (Filter, Map, Aggregate, Sort, GroupBy, Join, etc.)
  'safety',       // Risk management (RiskCheck, PositionSizeLimit, LossLimit, etc.)
  'notifications', // Notifications (Webhook, Email, Telegram, Discord, Slack, SMS)
  'agents',       // AI agents (AgentMediator, GeopoliticsAgent, HedgeFundAgent, etc.)
  'core',         // Core nodes (Filter, Merge, Set, Switch, Code)
];

/**
 * Available node types mapped to their internal names
 * This helps LLM know exactly which node types exist
 */
const AVAILABLE_NODE_TYPES = {
  // Triggers
  manualTrigger: 'Start workflow manually',
  scheduleTrigger: 'Run on schedule (cron)',
  priceAlertTrigger: 'Trigger when price hits threshold',
  newsEventTrigger: 'Trigger on news events',
  marketEventTrigger: 'Trigger on market open/close/volatility',
  webhookTrigger: 'Trigger from external webhook',

  // Market Data
  yfinance: 'Fetch stock data from Yahoo Finance',
  getQuote: 'Get real-time price quote',
  getHistoricalData: 'Fetch historical OHLCV data',
  getMarketDepth: 'Get order book depth',
  streamQuotes: 'Stream real-time quotes (WebSocket)',
  getFundamentals: 'Fetch fundamental data (P/E, dividends)',
  getTickerStats: 'Get ticker statistics',
  stockData: 'General stock data node',

  // Trading
  placeOrder: 'Place buy/sell order',
  modifyOrder: 'Modify existing order',
  cancelOrder: 'Cancel open order',
  getPositions: 'Get current positions',
  getHoldings: 'Get portfolio holdings',
  getOrders: 'Get order history',
  getBalance: 'Get account balance',
  closePosition: 'Close a position',

  // Analytics
  technicalIndicators: 'Calculate technical indicators (RSI, MACD, SMA, EMA, BB, etc.)',
  portfolioOptimization: 'Optimize portfolio allocation (Mean-Variance)',
  backtestEngine: 'Backtest trading strategy',
  riskAnalysis: 'Calculate risk metrics (VaR, Sharpe, etc.)',
  performanceMetrics: 'Analyze performance (returns, drawdown)',
  correlationMatrix: 'Calculate asset correlations',

  // Control Flow
  ifElse: 'Conditional branching (if/else)',
  switch: 'Multi-way routing based on value',
  loop: 'Iterate over array items',
  wait: 'Pause execution for duration',
  merge: 'Combine multiple data streams',
  split: 'Split stream into multiple',
  errorHandler: 'Catch and handle errors',

  // Transform
  filterTransform: 'Filter items by condition',
  map: 'Transform each item',
  aggregate: 'Aggregate data (sum, avg, count)',
  sort: 'Sort items by field',
  groupBy: 'Group items by field',
  join: 'Join two datasets',
  deduplicate: 'Remove duplicates',
  reshape: 'Restructure data shape',

  // Safety
  riskCheck: 'Validate against risk limits',
  positionSizeLimit: 'Enforce position size limits',
  lossLimit: 'Enforce max loss limits',
  tradingHoursCheck: 'Validate trading hours',

  // Notifications
  webhookNotification: 'Send HTTP webhook',
  email: 'Send email notification',
  telegram: 'Send Telegram message',
  discord: 'Send Discord message',
  slack: 'Send Slack message',
  sms: 'Send SMS notification',

  // AI Agents
  agentMediator: 'Mediate between AI agents',
  agent: 'Custom AI agent',
  multiAgent: 'Orchestrate multiple agents',
  geopoliticsAgent: 'Geopolitical analysis',
  hedgeFundAgent: 'Hedge fund strategy',
  investorAgent: 'Investor persona analysis',

  // Core
  filter: 'Basic filter node',
  set: 'Set/modify data fields',
  code: 'Execute custom JavaScript',
};

export const nodeEditorTools: InternalTool[] = [
  // ============================================================================
  // SMART WORKFLOW BUILDER (PRIMARY TOOL FOR LLM)
  // ============================================================================
  {
    name: 'node_editor_build_workflow',
    description: `PREFERRED TOOL: Build a workflow automatically using available nodes. Describe what you want the workflow to do and this tool will create it with sensible defaults.

AVAILABLE NODE TYPES:
${Object.entries(AVAILABLE_NODE_TYPES).map(([type, desc]) => `- ${type}: ${desc}`).join('\n')}

PRIMARY DATA SOURCE: yfinance (Yahoo Finance)
The yfinance node is the main data source supporting:
- Historical OHLCV data (operation: "history")
- Real-time quotes (operation: "quote")
- Company info, financials, balance sheet, cash flow
- Earnings, dividends, splits, recommendations
- Options chains, institutional holders, ESG data

WORKFLOW PATTERNS:
1. Technical Analysis: manualTrigger -> yfinance(history) -> technicalIndicators -> results
2. Alert System: manualTrigger -> yfinance(quote) -> ifElse (condition) -> telegram/discord
3. Trading Bot: scheduleTrigger -> yfinance(quote) -> technicalIndicators -> ifElse -> riskCheck -> placeOrder
4. Fundamental Analysis: manualTrigger -> yfinance(financials/balanceSheet) -> transform -> results
5. Backtesting: manualTrigger -> yfinance(history, 1y+) -> technicalIndicators -> backtestEngine

DEFAULT PARAMETERS:
- yfinance: operation="history", period="1mo", interval="1d" (for technical analysis)
- yfinance: operation="quote" (for real-time alerts)
- technicalIndicators: indicator="RSI", period=14
- ifElse: uses expression like "{{$json.RSI}} < 30"
- placeOrder: paperTrading=true, includes riskCheck by default

Just describe what you want and provide any specific symbols, indicators, or conditions.`,
    inputSchema: {
      type: 'object',
      properties: {
        name: {
          type: 'string',
          description: 'Workflow name',
        },
        goal: {
          type: 'string',
          description: 'What should this workflow do? E.g., "Monitor AAPL RSI and alert when oversold", "Backtest SMA crossover strategy on TSLA"',
        },
        symbols: {
          type: 'string',
          description: 'Stock/crypto symbols to use (comma-separated). E.g., "AAPL", "AAPL,MSFT,GOOGL"',
        },
        data_type: {
          type: 'string',
          description: 'What data to fetch from yfinance: history (default, OHLCV), quote (real-time), info, financials, balanceSheet, cashFlow, earnings, dividends, recommendations, options',
          enum: ['history', 'quote', 'info', 'financials', 'balanceSheet', 'cashFlow', 'earnings', 'dividends', 'recommendations', 'options', ''],
        },
        period: {
          type: 'string',
          description: 'Data period for history: 1d, 5d, 1mo (default), 3mo, 6mo, 1y, 2y, 5y, ytd, max',
          enum: ['1d', '5d', '1mo', '3mo', '6mo', '1y', '2y', '5y', 'ytd', 'max', ''],
        },
        interval: {
          type: 'string',
          description: 'Data interval: 1m, 5m, 15m, 30m, 1h, 1d (default), 1wk, 1mo',
          enum: ['1m', '5m', '15m', '30m', '1h', '1d', '1wk', '1mo', ''],
        },
        indicators: {
          type: 'string',
          description: 'Technical indicators with optional periods. Format: "RSI" or "RSI:14,MACD:12,SMA:20,EMA:9,BB:20". Available: RSI, MACD, SMA, EMA, BB, ATR, ADX, STOCH, OBV, VWAP. Leave empty for data-only workflows.',
        },
        conditions: {
          type: 'string',
          description: 'Conditions for alerts/actions. Use {{$json.field}} syntax. E.g., "{{$json.RSI}} < 30", "{{$json.close}} > 150", "{{$json.MACD}} > {{$json.signal}}"',
        },
        notification_type: {
          type: 'string',
          description: 'How to notify: telegram, discord, slack, email, webhook, or none',
          enum: ['telegram', 'discord', 'slack', 'email', 'webhook', 'none', ''],
        },
        notification_message: {
          type: 'string',
          description: 'Custom notification message. Can use {{$json.field}} placeholders. Default: "{symbol} Alert triggered!"',
        },
        include_trading: {
          type: 'string',
          description: 'Include trading nodes? "yes" adds risk check + place order nodes',
          enum: ['yes', 'no', ''],
        },
        trade_side: {
          type: 'string',
          description: 'If include_trading=yes, the trade direction',
          enum: ['BUY', 'SELL', ''],
        },
        trade_quantity: {
          type: 'number',
          description: 'If include_trading=yes, quantity to trade. Default: 1',
        },
        trade_order_type: {
          type: 'string',
          description: 'If include_trading=yes, order type',
          enum: ['MARKET', 'LIMIT', ''],
        },
        max_position_size: {
          type: 'number',
          description: 'If include_trading=yes, max position size for risk check. Default: 1000',
        },
        max_daily_loss: {
          type: 'number',
          description: 'If include_trading=yes, max daily loss for risk check. Default: 500',
        },
        trigger_type: {
          type: 'string',
          description: 'How to start: manual (default), schedule, priceAlert',
          enum: ['manual', 'schedule', 'priceAlert', ''],
        },
        schedule: {
          type: 'string',
          description: 'If trigger_type=schedule, cron expression. E.g., "0 9 * * 1-5" for 9am weekdays',
        },
        price_alert_threshold: {
          type: 'number',
          description: 'If trigger_type=priceAlert, the price threshold',
        },
      },
      required: ['name', 'goal'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      // Parse inputs - LLM provides all parameters explicitly
      const symbols = args.symbols ? args.symbols.split(',').map((s: string) => s.trim().toUpperCase()) : ['AAPL'];
      const dataType = args.data_type || 'history';
      const period = args.period || '1mo';
      const interval = args.interval || '1d';
      const triggerType = args.trigger_type || 'manual';
      const notificationType = args.notification_type || 'none';
      const includeTrading = args.include_trading === 'yes';
      const primarySymbol = symbols[0];

      // Parse indicators with optional periods: "RSI:14,MACD:12,SMA:20" or "RSI,MACD,SMA"
      const indicatorConfigs: Array<{ name: string; period: number }> = [];
      if (args.indicators) {
        const parts = args.indicators.split(',').map((s: string) => s.trim().toUpperCase());
        for (const part of parts) {
          if (part.includes(':')) {
            const [name, periodStr] = part.split(':');
            indicatorConfigs.push({ name: name.trim(), period: parseInt(periodStr) || 14 });
          } else {
            // Default periods if not specified
            const defaultPeriods: Record<string, number> = {
              RSI: 14, MACD: 12, SMA: 20, EMA: 9, BB: 20, ATR: 14, ADX: 14, STOCH: 14, OBV: 0, VWAP: 0
            };
            indicatorConfigs.push({ name: part, period: defaultPeriods[part] || 14 });
          }
        }
      }

      // Trading parameters
      const tradeSide = args.trade_side || 'BUY';
      const tradeQuantity = args.trade_quantity || 1;
      const tradeOrderType = args.trade_order_type || 'MARKET';
      const maxPositionSize = args.max_position_size || 1000;
      const maxDailyLoss = args.max_daily_loss || 500;
      const priceAlertThreshold = args.price_alert_threshold || 0;

      // Build nodes array using ReactFlow UI node types
      // The UI expects specific node types: 'data-source', 'technical-indicator', 'custom', 'backtest', etc.
      const nodes: any[] = [];
      const edges: any[] = [];
      let xPos = 100;
      let yPos = 100;
      const xStep = 280;
      const yStep = 150;

      // Helper to add node with proper ReactFlow structure
      // The 'type' is the UI component type, 'nodeType' is the backend execution type
      const addNode = (id: string, uiType: string, backendType: string, data: any, label: string, color: string) => {
        nodes.push({
          id,
          type: uiType, // UI component type (data-source, technical-indicator, custom, etc.)
          position: { x: xPos, y: yPos },
          parameters: {
            nodeType: backendType, // Backend execution type (yfinance, technicalIndicators, etc.)
            ...data,
          },
          label,
        });
        xPos += xStep;
        return id;
      };

      // Helper to connect nodes
      const connect = (source: string, target: string, sourceHandle?: string) => {
        edges.push({ source, target, sourceHandle });
      };

      // 1. Add trigger node (system type for triggers)
      let lastNodeId: string;
      if (triggerType === 'schedule' && args.schedule) {
        lastNodeId = addNode('trigger', 'system', 'scheduleTrigger', {
          cronExpression: args.schedule,
          icon: 'Clock',
        }, 'Schedule Trigger', '#a855f7');
      } else if (triggerType === 'priceAlert') {
        lastNodeId = addNode('trigger', 'system', 'priceAlertTrigger', {
          symbol: primarySymbol,
          threshold: priceAlertThreshold,
          icon: 'Bell',
        }, `Price Alert ($${priceAlertThreshold})`, '#f97316');
      } else {
        lastNodeId = addNode('trigger', 'system', 'manualTrigger', {
          icon: 'Play',
        }, 'Manual Start', '#10b981');
      }

      // 2. Add data fetching node (data-source type)
      const yfinanceParams: any = {
        symbol: primarySymbol,
        operation: dataType,
        dataSource: 'yfinance',
      };

      // Add history-specific params
      if (dataType === 'history') {
        yfinanceParams.period = period;
        yfinanceParams.interval = interval;
        yfinanceParams.autoAdjust = true;
      }

      // Add financials-specific params
      if (['financials', 'balanceSheet', 'cashFlow'].includes(dataType)) {
        yfinanceParams.frequency = 'annual';
      }

      const operationLabel = dataType === 'history' ? `${primarySymbol} Historical (${period})` :
                             dataType === 'quote' ? `${primarySymbol} Quote` :
                             `${primarySymbol} ${dataType.charAt(0).toUpperCase() + dataType.slice(1)}`;

      const dataNodeId = addNode('fetch_data', 'data-source', 'yfinance', yfinanceParams, operationLabel, '#3b82f6');
      connect(lastNodeId, dataNodeId);
      lastNodeId = dataNodeId;

      // 3. Add technical indicators (technical-indicator type)
      if (indicatorConfigs.length > 0 && dataType === 'history') {
        // For multiple indicators, combine them into one node
        const indicatorList = indicatorConfigs.map(ic => `${ic.name}(${ic.period})`).join(', ');
        const indicatorId = addNode('indicators', 'technical-indicator', 'technicalIndicators', {
          indicators: indicatorConfigs,
          symbol: primarySymbol,
          // First indicator as primary
          indicator: indicatorConfigs[0].name,
          period: indicatorConfigs[0].period,
        }, `Indicators: ${indicatorList}`, '#10b981');
        connect(lastNodeId, indicatorId);
        lastNodeId = indicatorId;
      }

      // 4. Add condition check if conditions provided
      if (args.conditions) {
        yPos += yStep; // Move down for branching
        xPos = 100 + xStep * 3; // Position after indicators

        const conditionId = addNode('condition', 'custom', 'ifElse', {
          condition: args.conditions,
          icon: 'GitBranch',
          color: '#eab308',
        }, `If: ${args.conditions}`, '#eab308');
        connect(lastNodeId, conditionId);
        lastNodeId = conditionId;

        // Branch for true condition
        let trueNodeId = lastNodeId;

        // 5. Add safety check if trading enabled
        if (includeTrading) {
          yPos += yStep; // Move down
          xPos = 100 + xStep * 4;

          const riskCheckId = addNode('risk_check', 'custom', 'riskCheck', {
            maxPositionSize: maxPositionSize,
            maxDailyLoss: maxDailyLoss,
            icon: 'Shield',
            color: '#ef4444',
          }, `Risk Check (max: $${maxPositionSize})`, '#ef4444');
          connect(lastNodeId, riskCheckId, 'true');

          const orderNodeId = addNode('place_order', 'custom', 'placeOrder', {
            symbol: primarySymbol,
            orderType: tradeOrderType,
            side: tradeSide,
            quantity: tradeQuantity,
            paperTrading: true,
            icon: 'TrendingUp',
            color: '#14b8a6',
          }, `${tradeSide} ${tradeQuantity} ${primarySymbol}`, '#14b8a6');
          connect(riskCheckId, orderNodeId);
          trueNodeId = orderNodeId;
        }

        // 6. Add notification if requested
        if (notificationType && notificationType !== 'none') {
          const message = args.notification_message || `${primarySymbol} Alert: Condition triggered!`;
          xPos += xStep;

          const notifyId = addNode('notify', 'custom', notificationType, {
            message: message,
            notificationType: notificationType,
            icon: 'Bell',
            color: '#f97316',
          }, `${notificationType.charAt(0).toUpperCase() + notificationType.slice(1)} Alert`, '#f97316');
          connect(includeTrading ? trueNodeId : lastNodeId, notifyId, includeTrading ? undefined : 'true');
        }
      } else {
        // No condition - add results display or notification
        if (notificationType && notificationType !== 'none') {
          const message = args.notification_message || `${primarySymbol} analysis complete.`;
          const notifyId = addNode('notify', 'custom', notificationType, {
            message: message,
            notificationType: notificationType,
            icon: 'Bell',
            color: '#f97316',
          }, `${notificationType.charAt(0).toUpperCase() + notificationType.slice(1)} Notification`, '#f97316');
          connect(lastNodeId, notifyId);
        } else {
          // Add results display at the end
          const resultsId = addNode('results', 'results-display', 'resultsDisplay', {
            displayFormat: 'table',
          }, 'Results', '#f59e0b');
          connect(lastNodeId, resultsId);
        }
      }

      // Create the workflow
      const result = await contexts.createNodeWorkflow({
        name: args.name,
        description: args.goal,
        nodes,
        edges,
      });

      return {
        success: true,
        data: {
          workflowId: result.id,
          workflow: result,
          summary: {
            nodesCount: nodes.length,
            edgesCount: edges.length,
            symbols: symbols,
            dataSource: 'yfinance',
            dataType: dataType,
            period: dataType === 'history' ? period : undefined,
            interval: dataType === 'history' ? interval : undefined,
            indicators: indicatorConfigs.length > 0 ? indicatorConfigs : undefined,
            conditions: args.conditions || undefined,
            trading: includeTrading ? {
              side: tradeSide,
              quantity: tradeQuantity,
              orderType: tradeOrderType,
              riskLimits: { maxPositionSize, maxDailyLoss }
            } : undefined,
            notifications: notificationType !== 'none' && notificationType !== '' ? {
              type: notificationType,
              message: args.notification_message || 'default'
            } : undefined,
          },
        },
        message: `Workflow "${args.name}" created with ${nodes.length} nodes! View it in the Node Editor tab (tab: "nodes"). Workflow ID: ${result.id}`,
      };
    },
  },

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

  // ============================================================================
  // WORKFLOW MANAGEMENT TOOLS
  // ============================================================================
  {
    name: 'node_editor_list_workflows',
    description: 'List all saved workflows in the node editor. Returns workflow IDs, names, descriptions, and status.',
    inputSchema: {
      type: 'object',
      properties: {
        status: {
          type: 'string',
          description: 'Filter by status: idle, running, completed, error, draft',
          enum: ['idle', 'running', 'completed', 'error', 'draft', ''],
        },
      },
      required: [],
    },
    handler: async (args, contexts) => {
      if (!contexts.listNodeWorkflows) {
        return { success: false, error: 'Node editor context not available' };
      }
      const workflows = await contexts.listNodeWorkflows(args.status);
      return {
        success: true,
        data: workflows,
        message: `Found ${workflows.length} workflows`,
      };
    },
  },

  {
    name: 'node_editor_get_workflow',
    description: 'Get complete details of a workflow including all nodes, edges, and their configurations.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to retrieve',
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
      return {
        success: true,
        data: workflow,
        message: `Workflow "${workflow.name}" with ${(workflow.nodes || []).length} nodes`,
      };
    },
  },

  {
    name: 'node_editor_update_workflow',
    description: 'Update an existing workflow (name, description, or full node/edge replacement).',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to update',
        },
        name: {
          type: 'string',
          description: 'New workflow name',
        },
        description: {
          type: 'string',
          description: 'New workflow description',
        },
        nodes: {
          type: 'string',
          description: 'Optional: JSON array of nodes to replace existing nodes',
        },
        edges: {
          type: 'string',
          description: 'Optional: JSON array of edges to replace existing edges',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.updateNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      let nodes, edges;
      if (args.nodes) {
        try {
          nodes = typeof args.nodes === 'string' ? JSON.parse(args.nodes) : args.nodes;
        } catch (e) {
          return { success: false, error: `Invalid JSON in nodes: ${e}` };
        }
      }
      if (args.edges) {
        try {
          edges = typeof args.edges === 'string' ? JSON.parse(args.edges) : args.edges;
        } catch (e) {
          return { success: false, error: `Invalid JSON in edges: ${e}` };
        }
      }

      const result = await contexts.updateNodeWorkflow({
        workflowId: args.workflow_id,
        name: args.name,
        description: args.description,
        nodes,
        edges,
      });

      return {
        success: true,
        data: result,
        message: `Workflow '${args.workflow_id}' updated successfully`,
      };
    },
  },

  {
    name: 'node_editor_delete_workflow',
    description: 'Delete a workflow from the node editor.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to delete',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.deleteNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      await contexts.deleteNodeWorkflow(args.workflow_id);
      return {
        success: true,
        message: `Workflow '${args.workflow_id}' deleted`,
      };
    },
  },

  {
    name: 'node_editor_duplicate_workflow',
    description: 'Duplicate an existing workflow with a new name.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to duplicate',
        },
        new_name: {
          type: 'string',
          description: 'Name for the duplicated workflow',
        },
      },
      required: ['workflow_id', 'new_name'],
    },
    handler: async (args, contexts) => {
      if (!contexts.duplicateNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      const result = await contexts.duplicateNodeWorkflow(args.workflow_id, args.new_name);
      return {
        success: true,
        data: result,
        message: `Workflow duplicated as "${args.new_name}" with ID: ${result.id}`,
      };
    },
  },

  // ============================================================================
  // WORKFLOW EXECUTION TOOLS
  // ============================================================================
  {
    name: 'node_editor_execute_workflow',
    description: 'Execute a workflow. The workflow will run all nodes from triggers to outputs.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to execute',
        },
        input_data: {
          type: 'string',
          description: 'Optional JSON input data for the workflow (passed to trigger nodes)',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.executeNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      let inputData;
      if (args.input_data) {
        try {
          inputData = typeof args.input_data === 'string' ? JSON.parse(args.input_data) : args.input_data;
        } catch (e) {
          return { success: false, error: `Invalid JSON in input_data: ${e}` };
        }
      }

      const result = await contexts.executeNodeWorkflow(args.workflow_id, inputData);
      return {
        success: true,
        data: result,
        message: `Workflow execution ${result.success ? 'completed' : 'failed'}`,
      };
    },
  },

  {
    name: 'node_editor_stop_workflow',
    description: 'Stop a running workflow execution.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to stop',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.stopNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      await contexts.stopNodeWorkflow(args.workflow_id);
      return {
        success: true,
        message: `Workflow '${args.workflow_id}' stopped`,
      };
    },
  },

  {
    name: 'node_editor_get_execution_results',
    description: 'Get the results of a workflow execution including output from each node.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to get results for',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getWorkflowResults) {
        return { success: false, error: 'Node editor context not available' };
      }
      const results = await contexts.getWorkflowResults(args.workflow_id);
      return {
        success: true,
        data: results,
        message: results ? 'Execution results retrieved' : 'No results available',
      };
    },
  },

  // ============================================================================
  // HELPER TOOLS
  // ============================================================================
  {
    name: 'node_editor_validate_workflow',
    description: 'Validate a workflow for errors before execution. Checks for missing connections, invalid parameters, etc.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to validate',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.validateNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      const validation = await contexts.validateNodeWorkflow(args.workflow_id);
      return {
        success: true,
        data: validation,
        message: validation.valid ? 'Workflow is valid' : `Found ${validation.errors.length} issues`,
      };
    },
  },

  {
    name: 'node_editor_open_workflow',
    description: 'Open a workflow in the Node Editor tab (tab: "nodes") for visual editing. Navigates to the Node Editor and loads the workflow.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to open in the editor',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.openWorkflowInEditor) {
        return { success: false, error: 'Node editor context not available' };
      }
      await contexts.openWorkflowInEditor(args.workflow_id);
      return {
        success: true,
        message: `Workflow opened in Node Editor. You can now visually edit it.`,
      };
    },
  },

  {
    name: 'node_editor_export_workflow',
    description: 'Export a workflow as JSON for sharing or backup.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_id: {
          type: 'string',
          description: 'The workflow ID to export',
        },
      },
      required: ['workflow_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.exportNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }
      const exported = await contexts.exportNodeWorkflow(args.workflow_id);
      return {
        success: true,
        data: exported,
        message: 'Workflow exported as JSON',
      };
    },
  },

  {
    name: 'node_editor_import_workflow',
    description: 'Import a workflow from JSON.',
    inputSchema: {
      type: 'object',
      properties: {
        workflow_json: {
          type: 'string',
          description: 'JSON string of the workflow to import',
        },
        new_name: {
          type: 'string',
          description: 'Optional: Override the workflow name',
        },
      },
      required: ['workflow_json'],
    },
    handler: async (args, contexts) => {
      if (!contexts.importNodeWorkflow) {
        return { success: false, error: 'Node editor context not available' };
      }

      let workflowData;
      try {
        workflowData = typeof args.workflow_json === 'string'
          ? JSON.parse(args.workflow_json)
          : args.workflow_json;
      } catch (e) {
        return { success: false, error: `Invalid workflow JSON: ${e}` };
      }

      if (args.new_name) {
        workflowData.name = args.new_name;
      }

      const result = await contexts.importNodeWorkflow(workflowData);
      return {
        success: true,
        data: result,
        message: `Workflow "${result.name}" imported with ID: ${result.id}`,
      };
    },
  },
];
