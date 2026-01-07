import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class RiskCheckNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Risk Check',
    name: 'riskCheck',
    icon: 'fa:shield',
    iconColor: '#ef4444',
    group: ['safety'],
    version: 1,
    description: 'Validate risk parameters before trading',
    defaults: { name: 'Risk Check', color: '#ef4444' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      { displayName: 'Max Risk %', name: 'maxRisk', type: NodePropertyType.Number, default: 2, description: 'Maximum risk per trade as % of portfolio' },
      { displayName: 'Max Drawdown %', name: 'maxDrawdown', type: NodePropertyType.Number, default: 10 },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const maxRisk = this.getNodeParameter('maxRisk', 0) as number;
    
    const validItems = items.filter(item => {
      const riskPct = Number(item.json.riskPercent || 0);
      return riskPct <= maxRisk;
    });
    
    return [this.helpers.returnJsonArray(validItems.map(i => i.json))];
  }
}
