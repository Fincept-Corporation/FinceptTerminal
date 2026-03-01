/**
 * Node Editor MCP Tools — Smart Workflow Builder
 *
 * Primary LLM tool: builds a complete workflow automatically from a high-level
 * description + parameters.
 */

import { InternalTool } from '../../types';
import { AVAILABLE_NODE_TYPES } from './constants';

export const smartBuilderTools: InternalTool[] = [
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
];
