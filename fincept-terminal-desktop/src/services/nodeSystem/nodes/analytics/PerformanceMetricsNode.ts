/**
 * Performance Metrics Node
 *
 * Calculate comprehensive performance metrics for portfolios and strategies:
 * - Returns (total, annualized, CAGR)
 * - Risk-adjusted returns (Sharpe, Sortino, Calmar)
 * - Benchmark comparison
 * - Attribution analysis
 * - Rolling performance metrics
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

export class PerformanceMetricsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Performance Stats',
    name: 'performanceMetrics',
    icon: 'fa:trophy',
    group: ['Analytics', 'Performance'],
    version: 1,
    description: 'Calculate comprehensive performance and attribution metrics',
    defaults: {
      name: 'Performance Stats',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Symbols',
        name: 'symbols',
        type: NodePropertyType.String,
        default: 'AAPL',
        required: true,
        description: 'Comma-separated symbols',
      },
      {
        displayName: 'Period',
        name: 'period',
        type: NodePropertyType.Options,
        options: [
          { name: '1 Month', value: '1mo' },
          { name: '3 Months', value: '3mo' },
          { name: '6 Months', value: '6mo' },
          { name: '1 Year', value: '1y' },
          { name: '2 Years', value: '2y' },
          { name: '3 Years', value: '3y' },
          { name: '5 Years', value: '5y' },
        ],
        default: '1y',
        description: 'Analysis period',
      },
      {
        displayName: 'Benchmark',
        name: 'benchmark',
        type: NodePropertyType.String,
        default: 'SPY',
        description: 'Benchmark symbol for comparison',
      },
      {
        displayName: 'Risk-Free Rate (%)',
        name: 'riskFreeRate',
        type: NodePropertyType.Number,
        default: 4.5,
        description: 'Annual risk-free rate',
      },
      {
        displayName: 'Include Rolling Metrics',
        name: 'includeRolling',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Calculate rolling performance metrics',
      },
      {
        displayName: 'Rolling Window (days)',
        name: 'rollingWindow',
        type: NodePropertyType.Number,
        default: 30,
        displayOptions: {
          show: {
            includeRolling: [true],
          },
        },
        description: 'Window size for rolling metrics',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData = [];

    const symbolsStr = this.getNodeParameter('symbols', 0) as string;
    const period = this.getNodeParameter('period', 0) as string;
    const benchmark = this.getNodeParameter('benchmark', 0) as string;
    const riskFreeRate = this.getNodeParameter('riskFreeRate', 0) as number;
    const includeRolling = this.getNodeParameter('includeRolling', 0) as boolean;
    const rollingWindow = this.getNodeParameter('rollingWindow', 0, 30) as number;

    try {
      const symbols = symbolsStr.split(',').map(s => s.trim()).filter(s => s);

      const params = {
        symbols,
        period,
        benchmark,
        risk_free_rate: riskFreeRate / 100,
        include_rolling: includeRolling,
        rolling_window: rollingWindow,
      };

      const result = await invoke('execute_python_command', {
        script: 'Analytics/performance/performance_metrics.py',
        args: [JSON.stringify(params)],
      });

      const parsedResult = typeof result === 'string' ? JSON.parse(result) : result;

      if (parsedResult.error) {
        throw new Error(parsedResult.error);
      }

      const outputData = {
        returns: parsedResult.returns,
        riskAdjusted: parsedResult.risk_adjusted,
        benchmarkComparison: parsedResult.benchmark_comparison,
        rollingMetrics: includeRolling ? parsedResult.rolling_metrics : undefined,
        metadata: {
          calculatedAt: new Date().toISOString(),
          period,
          benchmark,
          riskFreeRate: riskFreeRate / 100,
        },
      };

      returnData.push({
        json: outputData,
        pairedItem: 0,
      });
    } catch (error: any) {
      if (this.continueOnFail()) {
        returnData.push({
          json: { error: error.message },
          pairedItem: 0,
        });
      } else {
        throw error;
      }
    }

    return [returnData];
  }
}
