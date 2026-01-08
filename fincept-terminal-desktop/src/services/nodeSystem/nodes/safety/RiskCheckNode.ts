import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class RiskCheckNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Risk Check',
    name: 'riskCheck',
    icon: 'fa:shield-alt',
    group: ['Safety', 'Risk Management'],
    version: 1,
    description: 'Validate trades against risk limits',
    defaults: {
      name: 'Risk Check',
      color: '#ef4444',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [
      { type: NodeConnectionType.Main, displayName: 'Approved' },
      { type: NodeConnectionType.Main, displayName: 'Rejected' },
    ],
    properties: [
      {
        displayName: 'Max Position Size',
        name: 'maxPositionSize',
        type: NodePropertyType.Number,
        default: 10000,
        description: 'Maximum position size ($)',
      },
      {
        displayName: 'Max Portfolio %',
        name: 'maxPortfolioPercent',
        type: NodePropertyType.Number,
        default: 10,
        description: 'Maximum % of portfolio',
      },
      {
        displayName: 'Check Volatility',
        name: 'checkVolatility',
        type: NodePropertyType.Boolean,
        default: true,
      },
      {
        displayName: 'Max Volatility',
        name: 'maxVolatility',
        type: NodePropertyType.Number,
        default: 50,
        displayOptions: {
          show: {
            checkVolatility: [true],
          },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const approved: any[] = [];
    const rejected: any[] = [];
    const maxSize = this.getNodeParameter('maxPositionSize', 0) as number;
    const maxPct = this.getNodeParameter('maxPortfolioPercent', 0) as number;

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const size = Number(item.json.positionSize || item.json.size || 0);

      if (size > maxSize) {
        rejected.push({
          ...item,
          json: { ...item.json, rejectionReason: 'Position size exceeds limit' },
          pairedItem: i,
        });
      } else {
        approved.push({ ...item, pairedItem: i });
      }
    }

    return [approved, rejected];
  }
}
