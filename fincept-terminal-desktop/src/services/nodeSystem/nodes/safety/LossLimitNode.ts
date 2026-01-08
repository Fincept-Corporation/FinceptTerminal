import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class LossLimitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Loss Limit',
    name: 'lossLimit',
    icon: 'fa:hand-paper',
    group: ['Safety'],
    version: 1,
    description: 'Circuit breaker for max losses',
    defaults: {
      name: 'Loss Limit',
      color: '#dc2626',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [
      { type: NodeConnectionType.Main, displayName: 'Allowed' },
      { type: NodeConnectionType.Main, displayName: 'Blocked' },
    ],
    properties: [
      {
        displayName: 'Max Loss ($)',
        name: 'maxLossDollar',
        type: NodePropertyType.Number,
        default: 1000,
      },
      {
        displayName: 'Max Loss %',
        name: 'maxLossPercent',
        type: NodePropertyType.Number,
        default: 5,
      },
      {
        displayName: 'Time Period',
        name: 'period',
        type: NodePropertyType.Options,
        options: [
          { name: 'Per Trade', value: 'trade' },
          { name: 'Daily', value: 'daily' },
          { name: 'Weekly', value: 'weekly' },
        ],
        default: 'daily',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const maxLoss = this.getNodeParameter('maxLossDollar', 0) as number;
    const allowed: any[] = [];
    const blocked: any[] = [];

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const loss = Math.abs(Number(item.json.pnl || item.json.loss || 0));

      if (loss > maxLoss) {
        blocked.push({
          ...item,
          json: { ...item.json, blockReason: 'Loss limit exceeded' },
          pairedItem: i,
        });
      } else {
        allowed.push({ ...item, pairedItem: i });
      }
    }

    return [allowed, blocked];
  }
}
