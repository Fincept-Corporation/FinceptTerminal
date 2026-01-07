import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class PositionSizeLimitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Position Size Limit',
    name: 'positionSizeLimit',
    icon: 'fa:balance-scale',
    iconColor: '#f59e0b',
    group: ['safety'],
    version: 1,
    description: 'Limit position sizes',
    defaults: { name: 'Position Size Limit', color: '#f59e0b' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      { displayName: 'Max Position %', name: 'maxPosition', type: NodePropertyType.Number, default: 5, description: 'Max % of portfolio per position' },
      { displayName: 'Max Shares', name: 'maxShares', type: NodePropertyType.Number, default: 1000 },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
