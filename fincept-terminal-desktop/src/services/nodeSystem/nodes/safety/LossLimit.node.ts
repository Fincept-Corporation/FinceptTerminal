import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class LossLimitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Loss Limit',
    name: 'lossLimit',
    icon: 'fa:stop-circle',
    iconColor: '#dc2626',
    group: ['safety'],
    version: 1,
    description: 'Stop trading on loss limits',
    defaults: { name: 'Loss Limit', color: '#dc2626' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      { displayName: 'Daily Loss Limit $', name: 'dailyLimit', type: NodePropertyType.Number, default: 1000 },
      { displayName: 'Total Loss Limit $', name: 'totalLimit', type: NodePropertyType.Number, default: 5000 },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
