import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class PositionSizeLimitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Position Limit',
    name: 'positionSizeLimit',
    icon: 'fa:balance-scale',
    group: ['Safety'],
    version: 1,
    description: 'Enforce position size limits',
    defaults: {
      name: 'Position Limit',
      color: '#f59e0b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Limit Type',
        name: 'limitType',
        type: NodePropertyType.Options,
        options: [
          { name: 'Dollar Amount', value: 'dollar' },
          { name: 'Percentage', value: 'percentage' },
          { name: 'Shares', value: 'shares' },
        ],
        default: 'dollar',
      },
      {
        displayName: 'Max Size',
        name: 'maxSize',
        type: NodePropertyType.Number,
        default: 10000,
      },
      {
        displayName: 'Action on Exceed',
        name: 'action',
        type: NodePropertyType.Options,
        options: [
          { name: 'Reject', value: 'reject' },
          { name: 'Cap to Limit', value: 'cap' },
          { name: 'Warn and Continue', value: 'warn' },
        ],
        default: 'cap',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const maxSize = this.getNodeParameter('maxSize', 0) as number;
    const action = this.getNodeParameter('action', 0) as string;
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      let size = Number(item.json.size || item.json.quantity || 0);

      if (size > maxSize) {
        if (action === 'cap') size = maxSize;
        else if (action === 'reject') continue;
      }

      returnData.push({
        json: { ...item.json, size, limited: size !== item.json.size },
        pairedItem: i,
      });
    }

    return [returnData];
  }
}
