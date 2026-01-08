/**
 * Risk Analysis Node
 *
 * Comprehensive risk metrics for portfolios and trading strategies:
 * - Value at Risk (VaR)
 * - Conditional VaR (CVaR/Expected Shortfall)
 * - Beta and correlation to benchmark
 * - Volatility analysis
 * - Drawdown analysis
 * - Risk-adjusted returns
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

export class RiskAnalysisNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Risk Analysis',
    name: 'riskAnalysis',
    icon: 'fa:shield-alt',
    group: ['Analytics', 'Risk Management'],
    version: 1,
    description: 'Calculate comprehensive risk metrics for portfolios',
    defaults: {
      name: 'Risk Analysis',
      color: '#ef4444',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Analysis Type',
        name: 'analysisType',
        type: NodePropertyType.Options,
        options: [
          { name: 'Portfolio Risk', value: 'portfolio' },
          { name: 'Single Asset Risk', value: 'asset' },
          { name: 'Strategy Risk', value: 'strategy' },
        ],
        default: 'portfolio',
        description: 'Type of risk analysis to perform',
      },
      {
        displayName: 'Data Source',
        name: 'dataSource',
        type: NodePropertyType.Options,
        options: [
          { name: 'Symbols (fetch data)', value: 'symbols' },
          { name: 'Input Data', value: 'input' },
        ],
        default: 'symbols',
        description: 'Source of price data',
      },
      {
        displayName: 'Symbols',
        name: 'symbols',
        type: NodePropertyType.String,
        default: 'AAPL,MSFT,GOOGL',
        required: true,
        displayOptions: {
          show: {
            dataSource: ['symbols'],
          },
        },
        description: 'Comma-separated list of symbols',
      },
      {
        displayName: 'Weights',
        name: 'weights',
        type: NodePropertyType.String,
        default: '',
        displayOptions: {
          show: {
            dataSource: ['symbols'],
            analysisType: ['portfolio'],
          },
        },
        description: 'Comma-separated weights (leave empty for equal weight)',
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
        ],
        default: '1y',
        displayOptions: {
          show: {
            dataSource: ['symbols'],
          },
        },
        description: 'Historical data period',
      },
      {
        displayName: 'Confidence Level (%)',
        name: 'confidenceLevel',
        type: NodePropertyType.Number,
        default: 95,
        description: 'Confidence level for VaR calculation (typically 95 or 99)',
      },
      {
        displayName: 'VaR Method',
        name: 'varMethod',
        type: NodePropertyType.Options,
        options: [
          { name: 'Historical', value: 'historical' },
          { name: 'Parametric (Variance-Covariance)', value: 'parametric' },
          { name: 'Monte Carlo', value: 'monte_carlo' },
        ],
        default: 'historical',
        description: 'Method for calculating Value at Risk',
      },
      {
        displayName: 'Time Horizon (days)',
        name: 'timeHorizon',
        type: NodePropertyType.Number,
        default: 1,
        description: 'Risk time horizon in days',
      },
      {
        displayName: 'Benchmark',
        name: 'benchmark',
        type: NodePropertyType.String,
        default: 'SPY',
        description: 'Benchmark symbol for beta calculation (e.g., SPY for S&P 500)',
      },
      {
        displayName: 'Portfolio Value',
        name: 'portfolioValue',
        type: NodePropertyType.Number,
        default: 100000,
        description: 'Total portfolio value for VaR calculation ($)',
      },
      {
        displayName: 'Include Stress Tests',
        name: 'includeStressTests',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Include historical stress test scenarios',
      },
      {
        displayName: 'Include Correlation Matrix',
        name: 'includeCorrelation',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include asset correlation matrix',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData = [];

    // Get parameters
    const analysisType = this.getNodeParameter('analysisType', 0) as string;
    const dataSource = this.getNodeParameter('dataSource', 0) as string;
    const confidenceLevel = this.getNodeParameter('confidenceLevel', 0) as number;
    const varMethod = this.getNodeParameter('varMethod', 0) as string;
    const timeHorizon = this.getNodeParameter('timeHorizon', 0) as number;
    const benchmark = this.getNodeParameter('benchmark', 0) as string;
    const portfolioValue = this.getNodeParameter('portfolioValue', 0) as number;
    const includeStressTests = this.getNodeParameter('includeStressTests', 0) as boolean;
    const includeCorrelation = this.getNodeParameter('includeCorrelation', 0) as boolean;

    try {
      // Prepare parameters
      const params: Record<string, any> = {
        analysis_type: analysisType,
        confidence_level: confidenceLevel / 100,
        var_method: varMethod,
        time_horizon: timeHorizon,
        benchmark,
        portfolio_value: portfolioValue,
        include_stress_tests: includeStressTests,
        include_correlation: includeCorrelation,
      };

      // Handle data source
      if (dataSource === 'symbols') {
        const symbolsStr = this.getNodeParameter('symbols', 0) as string;
        const period = this.getNodeParameter('period', 0) as string;
        const weightsStr = this.getNodeParameter('weights', 0, '') as string;

        params.symbols = symbolsStr.split(',').map(s => s.trim()).filter(s => s);
        params.period = period;

        // Parse weights if provided
        if (weightsStr && analysisType === 'portfolio') {
          const weights = weightsStr.split(',').map(w => parseFloat(w.trim()));
          if (weights.length === params.symbols.length) {
            params.weights = weights;
          }
        }
      } else {
        // Use input data
        if (items.length === 0) {
          throw new Error('No input data available');
        }
        params.input_data = items[0].json;
      }

      // Execute risk analysis
      const result = await invoke('execute_python_command', {
        script: 'Analytics/risk/risk_analysis.py',
        args: [JSON.stringify(params)],
      });

      // Parse result
      const parsedResult = typeof result === 'string' ? JSON.parse(result) : result;

      if (parsedResult.error) {
        throw new Error(parsedResult.error);
      }

      // Structure output
      const outputData: Record<string, any> = {
        analysisType,
        riskMetrics: {
          // Value at Risk
          valueAtRisk: {
            amount: parsedResult.var_amount,
            percentage: parsedResult.var_percentage,
            method: varMethod,
            confidenceLevel: confidenceLevel,
            timeHorizon: timeHorizon,
          },
          // Conditional VaR
          conditionalVaR: {
            amount: parsedResult.cvar_amount,
            percentage: parsedResult.cvar_percentage,
          },
          // Volatility
          volatility: {
            daily: parsedResult.daily_volatility,
            annual: parsedResult.annual_volatility,
          },
          // Drawdown
          drawdown: {
            max: parsedResult.max_drawdown,
            maxPct: parsedResult.max_drawdown_pct,
            current: parsedResult.current_drawdown,
            avgDrawdown: parsedResult.avg_drawdown,
          },
          // Market Risk
          beta: parsedResult.beta,
          correlation: parsedResult.correlation_to_benchmark,
          tracking_error: parsedResult.tracking_error,
        },
      };

      // Add correlation matrix if included
      if (includeCorrelation && parsedResult.correlation_matrix) {
        outputData.correlationMatrix = parsedResult.correlation_matrix;
      }

      // Add stress tests if included
      if (includeStressTests && parsedResult.stress_tests) {
        outputData.stressTests = parsedResult.stress_tests;
      }

      // Add metadata
      outputData.metadata = {
        analyzedAt: new Date().toISOString(),
        portfolioValue,
        dataSource,
        symbols: params.symbols,
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
            analysisType,
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
