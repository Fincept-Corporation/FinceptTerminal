import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class TradingHoursCheckNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Trading Hours',
    name: 'tradingHoursCheck',
    icon: 'fa:clock',
    group: ['Safety'],
    version: 1,
    description: 'Validate market trading hours',
    defaults: {
      name: 'Trading Hours',
      color: '#3b82f6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [
      { type: NodeConnectionType.Main, displayName: 'Market Open' },
      { type: NodeConnectionType.Main, displayName: 'Market Closed' },
    ],
    properties: [
      {
        displayName: 'Market',
        name: 'market',
        type: NodePropertyType.Options,
        options: [
          { name: 'US Stocks (NYSE)', value: 'nyse' },
          { name: 'Crypto (24/7)', value: 'crypto' },
          { name: 'Forex', value: 'forex' },
        ],
        default: 'nyse',
      },
      {
        displayName: 'Allow Extended Hours',
        name: 'allowExtended',
        type: NodePropertyType.Boolean,
        default: false,
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const market = this.getNodeParameter('market', 0) as string;
    const open: any[] = [];
    const closed: any[] = [];

    const now = new Date();
    const hour = now.getHours();
    const day = now.getDay();

    let isOpen = false;
    if (market === 'crypto') {
      isOpen = true;
    } else if (market === 'nyse') {
      isOpen = day >= 1 && day <= 5 && hour >= 9 && hour < 16;
    } else if (market === 'forex') {
      isOpen = day >= 1 && day <= 5;
    }

    for (let i = 0; i < items.length; i++) {
      if (isOpen) {
        open.push({ ...items[i], pairedItem: i });
      } else {
        closed.push({ ...items[i], pairedItem: i });
      }
    }

    return [open, closed];
  }
}
