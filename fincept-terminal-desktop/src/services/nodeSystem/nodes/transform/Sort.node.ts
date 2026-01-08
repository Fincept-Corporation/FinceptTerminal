import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class SortNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Sort',
    name: 'sort',
    icon: 'fa:sort-amount-down',
    iconColor: '#10b981',
    group: ['transform'],
    version: 1,
    description: 'Sort items',
    defaults: { name: 'Sort', color: '#10b981' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
