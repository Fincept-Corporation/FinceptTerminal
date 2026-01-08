/**
 * Portfolio Optimization Node
 */

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
    iconColor: '#8b5cf6',
    group: ['analytics'],
    version: 1,
    description: 'Optimize portfolio weights',
    defaults: { name: 'Portfolio Optimization', color: '#8b5cf6' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Optimization Method',
        name: 'method',
        type: NodePropertyType.Options,
        options: [
          { name: 'Maximum Sharpe Ratio', value: 'sharpe' },
          { name: 'Minimum Variance', value: 'minVariance' },
          { name: 'Equal Weight', value: 'equal' },
        ],
        default: 'sharpe',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    return [items];
  }
}
