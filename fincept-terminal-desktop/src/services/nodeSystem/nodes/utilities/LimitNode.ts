import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class LimitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Limit Items',
    name: 'limit',
    icon: 'fa:filter',
    group: ['utilities'],
    version: 1,
    description: 'Limit the number of items passed through',
    defaults: { name: 'Limit Items' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Max Items',
        name: 'maxItems',
        type: NodePropertyType.Number,
        default: 10,
        description: 'Maximum number of items to output',
      },
      {
        displayName: 'Skip',
        name: 'skip',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Number of items to skip before starting',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const maxItems = this.getNodeParameter('maxItems', 0) as number;
    const skip = this.getNodeParameter('skip', 0) as number;

    const returnData = items.slice(skip, skip + maxItems).map((item, idx) => ({
      ...item,
      pairedItem: { item: skip + idx },
    }));

    return [returnData];
  }
}
