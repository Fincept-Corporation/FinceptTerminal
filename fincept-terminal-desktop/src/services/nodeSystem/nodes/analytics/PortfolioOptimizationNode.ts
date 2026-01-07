/**
 * Portfolio Optimization Node
 *
 * Modern Portfolio Theory optimization using various strategies:
 * - Maximum Sharpe Ratio
 * - Minimum Volatility
 * - Risk Parity
 * - Maximum Return
 * - Efficient Frontier generation
 *
 * Uses Python analytics scripts for sophisticated optimization algorithms.
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

export class PortfolioOptimizationNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Portfolio Optimization',
    name: 'portfolioOptimization',
    icon: 'fa:chart-pie',
    group: ['Analytics', 'Portfolio Management'],
    version: 1,
    description: 'Optimize portfolio allocation using Modern Portfolio Theory',
    defaults: {
      name: 'Portfolio Optimization',
      color: '#6366f1',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Optimization Method',
        name: 'method',
        type: NodePropertyType.Options,
        options: [
          { name: 'Maximum Sharpe Ratio', value: 'max_sharpe' },
          { name: 'Minimum Volatility', value: 'min_volatility' },
          { name: 'Risk Parity', value: 'risk_parity' },
          { name: 'Maximum Return', value: 'max_return' },
          { name: 'Efficient Frontier', value: 'efficient_frontier' },
        ],
        default: 'max_sharpe',
        description: 'Portfolio optimization strategy',
      },
      {
        displayName: 'Symbols',
        name: 'symbols',
        type: NodePropertyType.String,
        default: 'AAPL,MSFT,GOOGL,AMZN',
        required: true,
        description: 'Comma-separated list of stock symbols',
      },
      {
        displayName: 'Time Period',
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
        description: 'Historical data period for optimization',
      },
      {
        displayName: 'Risk-Free Rate (%)',
        name: 'riskFreeRate',
        type: NodePropertyType.Number,
        default: 4.5,
        description: 'Annual risk-free rate (e.g., Treasury yield)',
      },
      {
        displayName: 'Target Return (%)',
        name: 'targetReturn',
        type: NodePropertyType.Number,
        default: 12,
        displayOptions: {
          show: {
            method: ['efficient_frontier'],
          },
        },
        description: 'Target annual return for efficient frontier',
      },
      {
        displayName: 'Number of Portfolios',
        name: 'numPortfolios',
        type: NodePropertyType.Number,
        default: 10000,
        displayOptions: {
          show: {
            method: ['efficient_frontier'],
          },
        },
        description: 'Number of random portfolios to generate',
      },
      {
        displayName: 'Constraints',
        name: 'constraints',
        type: NodePropertyType.Collection,
        default: {},
        options: [
          {
            displayName: 'Min Weight (%)',
            name: 'minWeight',
            type: NodePropertyType.Number,
            default: 0,
            description: 'Minimum weight for any asset',
          },
          {
            displayName: 'Max Weight (%)',
            name: 'maxWeight',
            type: NodePropertyType.Number,
            default: 100,
            description: 'Maximum weight for any asset',
          },
          {
            displayName: 'Long Only',
            name: 'longOnly',
            type: NodePropertyType.Boolean,
            default: true,
            description: 'Restrict to long positions only',
          },
        ],
        description: 'Portfolio constraints',
      },
      {
        displayName: 'Include Metrics',
        name: 'includeMetrics',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include performance metrics in output',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData = [];

    // Get parameters
    const method = this.getNodeParameter('method', 0) as string;
    const symbolsStr = this.getNodeParameter('symbols', 0) as string;
    const period = this.getNodeParameter('period', 0) as string;
    const riskFreeRate = this.getNodeParameter('riskFreeRate', 0) as number;
    const constraints = this.getNodeParameter('constraints', 0, {}) as Record<string, any>;
    const includeMetrics = this.getNodeParameter('includeMetrics', 0) as boolean;

    // Parse symbols
    const symbols = symbolsStr.split(',').map(s => s.trim()).filter(s => s);

    if (symbols.length < 2) {
      throw new Error('At least 2 symbols are required for portfolio optimization');
    }

    try {
      // Prepare parameters for Python script
      const params: Record<string, any> = {
        symbols,
        period,
        method,
        risk_free_rate: riskFreeRate / 100, // Convert to decimal
        min_weight: (constraints.minWeight || 0) / 100,
        max_weight: (constraints.maxWeight || 100) / 100,
        long_only: constraints.longOnly !== false,
      };

      // Add method-specific parameters
      if (method === 'efficient_frontier') {
        params.target_return = this.getNodeParameter('targetReturn', 0) as number / 100;
        params.num_portfolios = this.getNodeParameter('numPortfolios', 0) as number;
      }

      // Execute Python portfolio optimization script
      const result = await invoke('execute_python_command', {
        script: 'Analytics/equityInvestment/portfolio_optimization.py',
        args: [JSON.stringify(params)],
      });

      // Parse result
      const parsedResult = typeof result === 'string' ? JSON.parse(result) : result;

      if (parsedResult.error) {
        throw new Error(parsedResult.error);
      }

      // Structure output data
      const outputData: Record<string, any> = {
        method,
        symbols,
        weights: parsedResult.weights || {},
        allocation: calculateAllocation(parsedResult.weights, symbols),
      };

      // Add metrics if requested
      if (includeMetrics && parsedResult.metrics) {
        outputData.metrics = parsedResult.metrics;
      }

      // Add efficient frontier data if applicable
      if (method === 'efficient_frontier' && parsedResult.efficient_frontier) {
        outputData.efficientFrontier = parsedResult.efficient_frontier;
      }

      // Add metadata
      outputData.metadata = {
        optimizedAt: new Date().toISOString(),
        period,
        riskFreeRate: riskFreeRate / 100,
        constraints,
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
            method,
            symbols,
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

/**
 * Calculate allocation details from weights
 */
function calculateAllocation(weights: Record<string, number>, symbols: string[]): any[] {
  const allocation = [];

  for (const symbol of symbols) {
    const weight = weights[symbol] || 0;
    allocation.push({
      symbol,
      weight: weight * 100, // Convert to percentage
      weightDecimal: weight,
    });
  }

  // Sort by weight descending
  allocation.sort((a, b) => b.weight - a.weight);

  return allocation;
}
