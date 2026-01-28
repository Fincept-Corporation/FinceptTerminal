/**
 * Price Alert Trigger Node
 * Triggers workflow when price crosses a threshold
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class PriceAlertTriggerNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Price Alert Trigger',
    name: 'priceAlertTrigger',
    icon: 'file:price-alert.svg',
    group: ['trigger'],
    version: 1,
    subtitle: '={{$parameter["symbol"]}} {{$parameter["condition"]}} {{$parameter["price"]}}',
    description: 'Triggers workflow when price meets condition',
    defaults: {
      name: 'Price Alert',
      color: '#f59e0b',
    },
    inputs: [],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: 'AAPL',
        required: true,
        description: 'Stock or crypto symbol to monitor',
        placeholder: 'AAPL, BTC-USD, etc.',
      },
      {
        displayName: 'Condition',
        name: 'condition',
        type: NodePropertyType.Options,
        default: 'above',
        options: [
          { name: 'Price Above', value: 'above', description: 'Trigger when price goes above target' },
          { name: 'Price Below', value: 'below', description: 'Trigger when price goes below target' },
          { name: 'Price Crosses', value: 'crosses', description: 'Trigger when price crosses target (either direction)' },
          { name: 'Percent Change Up', value: 'percentUp', description: 'Trigger on percentage increase' },
          { name: 'Percent Change Down', value: 'percentDown', description: 'Trigger on percentage decrease' },
          { name: 'Volume Spike', value: 'volumeSpike', description: 'Trigger on unusual volume' },
        ],
        description: 'Condition to trigger the alert',
      },
      {
        displayName: 'Target Price',
        name: 'price',
        type: NodePropertyType.Number,
        default: 0,
        required: true,
        description: 'Target price for the alert',
        displayOptions: {
          show: { condition: ['above', 'below', 'crosses'] },
        },
      },
      {
        displayName: 'Percent Change',
        name: 'percentChange',
        type: NodePropertyType.Number,
        default: 5,
        description: 'Percentage change to trigger (e.g., 5 for 5%)',
        displayOptions: {
          show: { condition: ['percentUp', 'percentDown'] },
        },
      },
      {
        displayName: 'Time Period',
        name: 'timePeriod',
        type: NodePropertyType.Options,
        default: '1h',
        options: [
          { name: '5 Minutes', value: '5m' },
          { name: '15 Minutes', value: '15m' },
          { name: '1 Hour', value: '1h' },
          { name: '4 Hours', value: '4h' },
          { name: '1 Day', value: '1d' },
        ],
        description: 'Time period for percentage change',
        displayOptions: {
          show: { condition: ['percentUp', 'percentDown'] },
        },
      },
      {
        displayName: 'Volume Multiplier',
        name: 'volumeMultiplier',
        type: NodePropertyType.Number,
        default: 2,
        description: 'Multiple of average volume to trigger (e.g., 2 for 2x average)',
        displayOptions: {
          show: { condition: ['volumeSpike'] },
        },
      },
      {
        displayName: 'Data Provider',
        name: 'provider',
        type: NodePropertyType.Options,
        default: 'yahoo',
        options: [
          { name: 'Yahoo Finance', value: 'yahoo' },
          { name: 'Binance', value: 'binance' },
          { name: 'CoinGecko', value: 'coingecko' },
        ],
        description: 'Market data provider for price checks',
      },
      {
        displayName: 'Check Interval',
        name: 'checkInterval',
        type: NodePropertyType.Options,
        default: '60',
        options: [
          { name: 'Every 15 seconds', value: '15' },
          { name: 'Every 30 seconds', value: '30' },
          { name: 'Every 1 minute', value: '60' },
          { name: 'Every 5 minutes', value: '300' },
          { name: 'Every 15 minutes', value: '900' },
        ],
        description: 'How often to check the price',
      },
      {
        displayName: 'Market Hours Only',
        name: 'marketHoursOnly',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Only check during market hours',
      },
      {
        displayName: 'Exchange',
        name: 'exchange',
        type: NodePropertyType.Options,
        default: 'NYSE',
        options: [
          { name: 'NYSE/NASDAQ', value: 'NYSE' },
          { name: 'NSE', value: 'NSE' },
          { name: 'Crypto 24/7', value: 'CRYPTO' },
        ],
        description: 'Exchange for market hours check',
        displayOptions: {
          show: { marketHoursOnly: [true] },
        },
      },
      {
        displayName: 'One-Time Trigger',
        name: 'oneTime',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Only trigger once, then disable',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const symbol = this.getNodeParameter('symbol', 0) as string;
    const condition = this.getNodeParameter('condition', 0) as string;
    const provider = this.getNodeParameter('provider', 0) as string;
    const checkInterval = this.getNodeParameter('checkInterval', 0) as string;
    const oneTime = this.getNodeParameter('oneTime', 0) as boolean;

    // Build trigger configuration
    let triggerConfig: any = {
      symbol,
      condition,
      provider,
      checkIntervalSeconds: parseInt(checkInterval),
      oneTime,
    };

    // Add condition-specific parameters
    switch (condition) {
      case 'above':
      case 'below':
      case 'crosses':
        triggerConfig.targetPrice = this.getNodeParameter('price', 0);
        break;
      case 'percentUp':
      case 'percentDown':
        triggerConfig.percentChange = this.getNodeParameter('percentChange', 0);
        triggerConfig.timePeriod = this.getNodeParameter('timePeriod', 0);
        break;
      case 'volumeSpike':
        triggerConfig.volumeMultiplier = this.getNodeParameter('volumeMultiplier', 0);
        break;
    }

    // For now, return trigger configuration
    // In production, this would register with the event scheduler
    return [[{
      json: {
        trigger: 'priceAlert',
        ...triggerConfig,
        timestamp: new Date().toISOString(),
        executionId: this.getExecutionId(),
        status: 'monitoring',
        message: `Monitoring ${symbol} for ${PriceAlertTriggerNode.formatCondition(condition, triggerConfig)}`,
      },
    }]];
  }

  private static formatCondition(condition: string, config: any): string {
    switch (condition) {
      case 'above':
        return `price > $${config.targetPrice}`;
      case 'below':
        return `price < $${config.targetPrice}`;
      case 'crosses':
        return `price crosses $${config.targetPrice}`;
      case 'percentUp':
        return `+${config.percentChange}% in ${config.timePeriod}`;
      case 'percentDown':
        return `-${config.percentChange}% in ${config.timePeriod}`;
      case 'volumeSpike':
        return `volume > ${config.volumeMultiplier}x average`;
      default:
        return condition;
    }
  }
}
