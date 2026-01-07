import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class DeduplicateNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Deduplicate',
    name: 'deduplicate',
    icon: 'fa:copy',
    iconColor: '#ec4899',
    group: ['transform'],
    version: 1,
    description: 'Remove duplicates',
    defaults: { name: 'Deduplicate', color: '#ec4899' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
