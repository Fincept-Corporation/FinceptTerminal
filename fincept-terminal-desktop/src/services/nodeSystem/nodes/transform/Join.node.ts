import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class JoinNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Join',
    name: 'join',
    icon: 'fa:link',
    iconColor: '#a855f7',
    group: ['transform'],
    version: 1,
    description: 'Join datasets',
    defaults: { name: 'Join', color: '#a855f7' },
    inputs: [NodeConnectionType.Main, NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
