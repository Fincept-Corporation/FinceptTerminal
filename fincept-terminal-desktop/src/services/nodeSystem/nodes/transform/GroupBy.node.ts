import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class GroupByNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Group By',
    name: 'groupBy',
    icon: 'fa:layer-group',
    iconColor: '#06b6d4',
    group: ['transform'],
    version: 1,
    description: 'Group items',
    defaults: { name: 'Group By', color: '#06b6d4' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
