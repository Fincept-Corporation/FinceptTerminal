/**
 * Backtest Engine Node
 *
 * Comprehensive backtesting framework for trading strategies.
 * Supports multiple strategy types, performance metrics, and risk analysis.
 *
 * Features:
 * - Strategy simulation
 * - Trade tracking
 * - Performance metrics (Sharpe, Sortino, Max Drawdown, etc.)
 * - Equity curve generation
 * - Trade analytics
 */

import { invoke } from '@tauri-apps/api/core';
import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class BacktestEngineNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Strategy Backtest',
    name: 'backtestEngine',
    icon: 'fa:chart-area',
    group: ['Analytics', 'Trading'],
    version: 1,
    description: 'Test trading strategies on historical data and get performance metrics',
    defaults: {
      name: 'Strategy Backtest',
      color: '#f59e0b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Strategy Type',
        name: 'strategyType',
        type: NodePropertyType.Options,
        options: [
          { name: 'Moving Average Crossover', value: 'ma_crossover' },
          { name: 'RSI Mean Reversion', value: 'rsi_reversion' },
          { name: 'Breakout Strategy', value: 'breakout' },
          { name: 'Trend Following', value: 'trend_following' },
          { name: 'Custom Strategy (from input)', value: 'custom' },
        ],
        default: 'ma_crossover',
        description: 'Type of trading strategy to backtest',
      },
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: 'AAPL',
        required: true,
        description: 'Stock symbol to backtest',
      },
      {
        displayName: 'Start Date',
        name: 'startDate',
        type: NodePropertyType.DateTime,
        default: '',
        description: 'Backtest start date (leave empty for 1 year ago)',
      },
      {
        displayName: 'End Date',
        name: 'endDate',
        type: NodePropertyType.DateTime,
        default: '',
        description: 'Backtest end date (leave empty for today)',
      },
      {
        displayName: 'Initial Capital',
        name: 'initialCapital',
        type: NodePropertyType.Number,
        default: 100000,
        description: 'Starting capital for backtest ($)',
      },
      {
        displayName: 'Position Sizing',
        name: 'positionSizing',
        type: NodePropertyType.Options,
        options: [
          { name: 'Fixed Dollar Amount', value: 'fixed_dollar' },
          { name: 'Fixed Percentage', value: 'fixed_percent' },
          { name: 'Kelly Criterion', value: 'kelly' },
          { name: 'Risk-Based', value: 'risk_based' },
        ],
        default: 'fixed_percent',
        description: 'How to size positions',
      },
      {
        displayName: 'Position Size',
        name: 'positionSize',
        type: NodePropertyType.Number,
        default: 10,
        description: 'Position size ($ or % depending on sizing method)',
      },
      {
        displayName: 'Commission per Trade ($)',
        name: 'commission',
        type: NodePropertyType.Number,
        default: 1.0,
        description: 'Commission cost per trade',
      },
      {
        displayName: 'Slippage (%)',
        name: 'slippage',
        type: NodePropertyType.Number,
        default: 0.05,
        description: 'Slippage as percentage',
      },
      // MA Crossover specific
      {
        displayName: 'Fast MA Period',
        name: 'fastPeriod',
        type: NodePropertyType.Number,
        default: 50,
        displayOptions: {
          show: {
            strategyType: ['ma_crossover'],
          },
        },
        description: 'Fast moving average period',
      },
      {
        displayName: 'Slow MA Period',
        name: 'slowPeriod',
        type: NodePropertyType.Number,
        default: 200,
        displayOptions: {
          show: {
            strategyType: ['ma_crossover'],
          },
        },
        description: 'Slow moving average period',
      },
      // RSI specific
      {
        displayName: 'RSI Period',
        name: 'rsiPeriod',
        type: NodePropertyType.Number,
        default: 14,
        displayOptions: {
          show: {
            strategyType: ['rsi_reversion'],
          },
        },
        description: 'RSI calculation period',
      },
      {
        displayName: 'RSI Oversold',
        name: 'rsiOversold',
        type: NodePropertyType.Number,
        default: 30,
        displayOptions: {
          show: {
            strategyType: ['rsi_reversion'],
          },
        },
        description: 'RSI oversold threshold',
      },
      {
        displayName: 'RSI Overbought',
        name: 'rsiOverbought',
        type: NodePropertyType.Number,
        default: 70,
        displayOptions: {
          show: {
            strategyType: ['rsi_reversion'],
          },
        },
        description: 'RSI overbought threshold',
      },
      {
        displayName: 'Include Detailed Trades',
        name: 'includeDetailedTrades',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include individual trade details in output',
      },
      {
        displayName: 'Include Equity Curve',
        name: 'includeEquityCurve',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include equity curve data in output',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData = [];

    // Get parameters
    const strategyType = this.getNodeParameter('strategyType', 0) as string;
    const symbol = this.getNodeParameter('symbol', 0) as string;
    const startDate = this.getNodeParameter('startDate', 0) as string;
    const endDate = this.getNodeParameter('endDate', 0) as string;
    const initialCapital = this.getNodeParameter('initialCapital', 0) as number;
    const positionSizing = this.getNodeParameter('positionSizing', 0) as string;
    const positionSize = this.getNodeParameter('positionSize', 0) as number;
    const commission = this.getNodeParameter('commission', 0) as number;
    const slippage = this.getNodeParameter('slippage', 0) as number;
    const includeDetailedTrades = this.getNodeParameter('includeDetailedTrades', 0) as boolean;
    const includeEquityCurve = this.getNodeParameter('includeEquityCurve', 0) as boolean;

    try {
      // Prepare strategy parameters
      const params: Record<string, any> = {
        symbol,
        strategy_type: strategyType,
        initial_capital: initialCapital,
        position_sizing: positionSizing,
        position_size: positionSize,
        commission,
        slippage: slippage / 100,
      };

      // Add dates if provided
      if (startDate) {
        params.start_date = startDate;
      }
      if (endDate) {
        params.end_date = endDate;
      }

      // Add strategy-specific parameters
      if (strategyType === 'ma_crossover') {
        params.fast_period = this.getNodeParameter('fastPeriod', 0) as number;
        params.slow_period = this.getNodeParameter('slowPeriod', 0) as number;
      } else if (strategyType === 'rsi_reversion') {
        params.rsi_period = this.getNodeParameter('rsiPeriod', 0) as number;
        params.rsi_oversold = this.getNodeParameter('rsiOversold', 0) as number;
        params.rsi_overbought = this.getNodeParameter('rsiOverbought', 0) as number;
      }

      // Add custom strategy from input if applicable
      if (strategyType === 'custom' && items.length > 0) {
        params.custom_signals = items[0].json;
      }

      // Execute backtest
      const result = await invoke('execute_python_command', {
        script: 'Analytics/backtest/backtest_engine.py',
        args: [JSON.stringify(params)],
      });

      // Parse result
      const parsedResult = typeof result === 'string' ? JSON.parse(result) : result;

      if (parsedResult.error) {
        throw new Error(parsedResult.error);
      }

      // Structure output
      const outputData: Record<string, any> = {
        strategy: {
          type: strategyType,
          symbol,
          parameters: params,
        },
        performance: parsedResult.performance || {},
        summary: {
          totalReturn: parsedResult.total_return,
          totalReturnPct: parsedResult.total_return_pct,
          annualizedReturn: parsedResult.annualized_return,
          sharpeRatio: parsedResult.sharpe_ratio,
          sortinoRatio: parsedResult.sortino_ratio,
          maxDrawdown: parsedResult.max_drawdown,
          maxDrawdownPct: parsedResult.max_drawdown_pct,
          winRate: parsedResult.win_rate,
          profitFactor: parsedResult.profit_factor,
          totalTrades: parsedResult.total_trades,
          winningTrades: parsedResult.winning_trades,
          losingTrades: parsedResult.losing_trades,
        },
      };

      // Add detailed trades if requested
      if (includeDetailedTrades && parsedResult.trades) {
        outputData.trades = parsedResult.trades;
      }

      // Add equity curve if requested
      if (includeEquityCurve && parsedResult.equity_curve) {
        outputData.equityCurve = parsedResult.equity_curve;
      }

      // Add metadata
      outputData.metadata = {
        backtestCompletedAt: new Date().toISOString(),
        initialCapital,
        finalCapital: parsedResult.final_capital,
        commission,
        slippage,
      };

      returnData.push({
        json: outputData,
        pairedItem: 0,
      });
    } catch (error: any) {
      if (this.continueOnFail()) {
        returnData.push({
          json: {
            error: error.message,
            strategyType,
            symbol,
          },
          pairedItem: 0,
        });
      } else {
        throw error;
      }
    }

    return [returnData];
  }
}
