/**
 * Correlation Matrix Node
 *
 * Calculate correlation matrices for asset analysis:
 * - Pearson correlation
 * - Spearman rank correlation
 * - Rolling correlation
 * - Cross-asset correlation
 * - Heatmap data generation
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

export class CorrelationMatrixNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Asset Correlation',
    name: 'correlationMatrix',
    icon: 'fa:th',
    group: ['Analytics', 'Statistical'],
    version: 1,
    description: 'Calculate correlation matrices for asset analysis',
    defaults: {
      name: 'Asset Correlation',
      color: '#06b6d4',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Symbols',
        name: 'symbols',
        type: NodePropertyType.String,
        default: 'AAPL,MSFT,GOOGL,AMZN,TSLA',
        required: true,
        description: 'Comma-separated symbols to analyze',
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
        ],
        default: '1y',
        description: 'Historical data period',
      },
      {
        displayName: 'Correlation Method',
        name: 'method',
        type: NodePropertyType.Options,
        options: [
          { name: 'Pearson', value: 'pearson' },
          { name: 'Spearman', value: 'spearman' },
          { name: 'Kendall', value: 'kendall' },
        ],
        default: 'pearson',
        description: 'Correlation calculation method',
      },
      {
        displayName: 'Calculate Rolling Correlation',
        name: 'calculateRolling',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Calculate time-varying correlation',
      },
      {
        displayName: 'Rolling Window (days)',
        name: 'rollingWindow',
        type: NodePropertyType.Number,
        default: 60,
        displayOptions: {
          show: {
            calculateRolling: [true],
          },
        },
        description: 'Window size for rolling correlation',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData = [];

    const symbolsStr = this.getNodeParameter('symbols', 0) as string;
    const period = this.getNodeParameter('period', 0) as string;
    const method = this.getNodeParameter('method', 0) as string;
    const calculateRolling = this.getNodeParameter('calculateRolling', 0) as boolean;
    const rollingWindow = this.getNodeParameter('rollingWindow', 0, 60) as number;

    try {
      const symbols = symbolsStr.split(',').map(s => s.trim()).filter(s => s);

      if (symbols.length < 2) {
        throw new Error('At least 2 symbols required for correlation analysis');
      }

      const params = {
        symbols,
        period,
        method,
        calculate_rolling: calculateRolling,
        rolling_window: rollingWindow,
      };

      const result = await invoke('execute_python_command', {
        script: 'Analytics/correlation/correlation_matrix.py',
        args: [JSON.stringify(params)],
      });

      const parsedResult = typeof result === 'string' ? JSON.parse(result) : result;

      if (parsedResult.error) {
        throw new Error(parsedResult.error);
      }

      const outputData = {
        correlationMatrix: parsedResult.correlation_matrix,
        symbols,
        rollingCorrelation: calculateRolling ? parsedResult.rolling_correlation : undefined,
        statistics: parsedResult.statistics,
        metadata: {
          calculatedAt: new Date().toISOString(),
          method,
          period,
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
