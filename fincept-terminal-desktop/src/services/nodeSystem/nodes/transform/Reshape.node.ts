import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class ReshapeNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Reshape',
    name: 'reshape',
    icon: 'fa:vector-square',
    iconColor: '#f97316',
    group: ['transform'],
    version: 1,
    description: 'Reshape data',
    defaults: { name: 'Reshape', color: '#f97316' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
