import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class TradingHoursCheckNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Trading Hours Check',
    name: 'tradingHoursCheck',
    icon: 'fa:clock',
    iconColor: '#10b981',
    group: ['safety'],
    version: 1,
    description: 'Verify trading hours before execution',
    defaults: { name: 'Trading Hours Check', color: '#10b981' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      { displayName: 'Market', name: 'market', type: NodePropertyType.Options, options: [
        { name: 'US Stock Market', value: 'us' },
        { name: 'Crypto (24/7)', value: 'crypto' },
        { name: 'Forex', value: 'forex' },
      ], default: 'us' },
      { displayName: 'Allow After Hours', name: 'allowAfterHours', type: NodePropertyType.Boolean, default: false },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
