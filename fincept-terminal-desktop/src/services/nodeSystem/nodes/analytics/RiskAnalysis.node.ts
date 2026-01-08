import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class RiskAnalysisNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Risk Analysis',
    name: 'riskAnalysis',
    icon: 'fa:shield-alt',
    iconColor: '#ef4444',
    group: ['analytics'],
    version: 1,
    description: 'Analyze portfolio risk',
    defaults: { name: 'Risk Analysis', color: '#ef4444' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      { displayName: 'Risk Metric', name: 'metric', type: NodePropertyType.Options, options: [
        { name: 'Value at Risk (VaR)', value: 'var' },
        { name: 'Sharpe Ratio', value: 'sharpe' },
        { name: 'Beta', value: 'beta' },
      ], default: 'var' },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
