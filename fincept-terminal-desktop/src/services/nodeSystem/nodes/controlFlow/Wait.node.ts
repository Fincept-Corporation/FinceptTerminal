import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class WaitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Wait',
    name: 'wait',
    icon: 'fa:clock',
    iconColor: '#8b5cf6',
    group: ['controlFlow'],
    version: 1,
    description: 'Pause execution',
    defaults: { name: 'Wait', color: '#8b5cf6' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      { displayName: 'Duration (seconds)', name: 'duration', type: NodePropertyType.Number, default: 1 },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const duration = this.getNodeParameter('duration', 0) as number;
    await new Promise(resolve => setTimeout(resolve, duration * 1000));
    return [this.getInputData()];
  }
}
