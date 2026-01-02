/**
 * News Event Trigger Node
 * Triggers workflow based on news events or sentiment changes
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class NewsEventTriggerNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'News Event Trigger',
    name: 'newsEventTrigger',
    icon: 'file:news.svg',
    group: ['trigger'],
    version: 1,
    subtitle: '={{$parameter["eventType"]}} for {{$parameter["symbols"]}}',
    description: 'Triggers workflow on news events or sentiment changes',
    defaults: {
      name: 'News Event',
      color: '#f59e0b',
    },
    inputs: [],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Event Type',
        name: 'eventType',
        type: NodePropertyType.Options,
        default: 'news',
        options: [
          { name: 'Any News', value: 'news', description: 'Any news article for symbols' },
          { name: 'Earnings Release', value: 'earnings', description: 'Earnings announcement' },
          { name: 'SEC Filing', value: 'secFiling', description: 'SEC filing (10-K, 10-Q, 8-K)' },
          { name: 'Analyst Rating', value: 'analystRating', description: 'Analyst upgrade/downgrade' },
          { name: 'Dividend Announcement', value: 'dividend', description: 'Dividend declaration' },
          { name: 'Insider Trade', value: 'insiderTrade', description: 'Insider buy/sell' },
          { name: 'Sentiment Shift', value: 'sentiment', description: 'Sentiment score change' },
          { name: 'Social Buzz', value: 'socialBuzz', description: 'Social media activity spike' },
        ],
        description: 'Type of news event to monitor',
      },
      {
        displayName: 'Symbols',
        name: 'symbols',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'AAPL, MSFT, GOOGL',
        description: 'Comma-separated list of symbols to monitor',
      },
      {
        displayName: 'SEC Filing Types',
        name: 'secFilingTypes',
        type: NodePropertyType.MultiOptions,
        default: ['10-K', '10-Q', '8-K'],
        options: [
          { name: '10-K (Annual Report)', value: '10-K' },
          { name: '10-Q (Quarterly Report)', value: '10-Q' },
          { name: '8-K (Current Report)', value: '8-K' },
          { name: '4 (Insider Trading)', value: '4' },
          { name: 'DEF 14A (Proxy)', value: 'DEF 14A' },
          { name: '13F (Institutional Holdings)', value: '13F' },
          { name: 'S-1 (IPO Registration)', value: 'S-1' },
        ],
        description: 'Types of SEC filings to monitor',
        displayOptions: {
          show: { eventType: ['secFiling'] },
        },
      },
      {
        displayName: 'Rating Change',
        name: 'ratingChange',
        type: NodePropertyType.Options,
        default: 'any',
        options: [
          { name: 'Any Change', value: 'any' },
          { name: 'Upgrade Only', value: 'upgrade' },
          { name: 'Downgrade Only', value: 'downgrade' },
          { name: 'New Coverage', value: 'initiate' },
        ],
        description: 'Type of rating change to trigger on',
        displayOptions: {
          show: { eventType: ['analystRating'] },
        },
      },
      {
        displayName: 'Insider Trade Type',
        name: 'insiderTradeType',
        type: NodePropertyType.Options,
        default: 'any',
        options: [
          { name: 'Any Trade', value: 'any' },
          { name: 'Buy Only', value: 'buy' },
          { name: 'Sell Only', value: 'sell' },
          { name: 'Large Trade (>$1M)', value: 'large' },
        ],
        description: 'Type of insider trade to trigger on',
        displayOptions: {
          show: { eventType: ['insiderTrade'] },
        },
      },
      {
        displayName: 'Sentiment Direction',
        name: 'sentimentDirection',
        type: NodePropertyType.Options,
        default: 'any',
        options: [
          { name: 'Any Change', value: 'any' },
          { name: 'Turning Positive', value: 'positive' },
          { name: 'Turning Negative', value: 'negative' },
          { name: 'Significant Move', value: 'significant' },
        ],
        description: 'Direction of sentiment change',
        displayOptions: {
          show: { eventType: ['sentiment'] },
        },
      },
      {
        displayName: 'Sentiment Threshold',
        name: 'sentimentThreshold',
        type: NodePropertyType.Number,
        default: 0.3,
        description: 'Minimum sentiment score change to trigger (0-1)',
        displayOptions: {
          show: { eventType: ['sentiment'] },
        },
      },
      {
        displayName: 'Buzz Multiplier',
        name: 'buzzMultiplier',
        type: NodePropertyType.Number,
        default: 2,
        description: 'Multiple of average social mentions to trigger (e.g., 2 for 2x average)',
        displayOptions: {
          show: { eventType: ['socialBuzz'] },
        },
      },
      {
        displayName: 'Data Provider',
        name: 'provider',
        type: NodePropertyType.Options,
        default: 'benzinga',
        options: [
          { name: 'Benzinga', value: 'benzinga' },
          { name: 'Alpha Vantage', value: 'alphavantage' },
          { name: 'Finnhub', value: 'finnhub' },
          { name: 'Polygon.io', value: 'polygon' },
          { name: 'SEC EDGAR', value: 'sec' },
          { name: 'Twitter/X', value: 'twitter' },
          { name: 'Reddit', value: 'reddit' },
          { name: 'StockTwits', value: 'stocktwits' },
        ],
        description: 'News/data provider to use',
      },
      {
        displayName: 'Check Interval',
        name: 'checkInterval',
        type: NodePropertyType.Options,
        default: '60',
        options: [
          { name: 'Every 30 seconds', value: '30' },
          { name: 'Every 1 minute', value: '60' },
          { name: 'Every 5 minutes', value: '300' },
          { name: 'Every 15 minutes', value: '900' },
          { name: 'Every 30 minutes', value: '1800' },
        ],
        description: 'How often to check for new events',
      },
      {
        displayName: 'Keywords Filter',
        name: 'keywords',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'acquisition, merger, layoff',
        description: 'Optional keywords to filter news (comma-separated)',
      },
      {
        displayName: 'Exclude Keywords',
        name: 'excludeKeywords',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'rumor, speculation',
        description: 'Keywords to exclude from results (comma-separated)',
      },
      {
        displayName: 'Minimum Importance',
        name: 'minImportance',
        type: NodePropertyType.Options,
        default: 'any',
        options: [
          { name: 'Any', value: 'any' },
          { name: 'Low+', value: 'low' },
          { name: 'Medium+', value: 'medium' },
          { name: 'High Only', value: 'high' },
        ],
        description: 'Minimum importance level of news',
      },
      {
        displayName: 'Market Hours Only',
        name: 'marketHoursOnly',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Only trigger during market hours',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const eventType = this.getNodeParameter('eventType', 0) as string;
    const symbolsStr = this.getNodeParameter('symbols', 0) as string;
    const provider = this.getNodeParameter('provider', 0) as string;
    const checkInterval = this.getNodeParameter('checkInterval', 0) as string;
    const keywords = this.getNodeParameter('keywords', 0) as string;
    const excludeKeywords = this.getNodeParameter('excludeKeywords', 0) as string;
    const minImportance = this.getNodeParameter('minImportance', 0) as string;
    const marketHoursOnly = this.getNodeParameter('marketHoursOnly', 0) as boolean;

    const symbols = symbolsStr.split(',').map(s => s.trim()).filter(Boolean);

    // Build trigger configuration
    const triggerConfig: Record<string, unknown> = {
      eventType,
      symbols,
      provider,
      checkIntervalSeconds: parseInt(checkInterval),
      keywords: keywords ? keywords.split(',').map(k => k.trim()) : [],
      excludeKeywords: excludeKeywords ? excludeKeywords.split(',').map(k => k.trim()) : [],
      minImportance,
      marketHoursOnly,
    };

    // Add event-specific parameters
    switch (eventType) {
      case 'secFiling':
        triggerConfig.secFilingTypes = this.getNodeParameter('secFilingTypes', 0);
        break;
      case 'analystRating':
        triggerConfig.ratingChange = this.getNodeParameter('ratingChange', 0);
        break;
      case 'insiderTrade':
        triggerConfig.insiderTradeType = this.getNodeParameter('insiderTradeType', 0);
        break;
      case 'sentiment':
        triggerConfig.sentimentDirection = this.getNodeParameter('sentimentDirection', 0);
        triggerConfig.sentimentThreshold = this.getNodeParameter('sentimentThreshold', 0);
        break;
      case 'socialBuzz':
        triggerConfig.buzzMultiplier = this.getNodeParameter('buzzMultiplier', 0);
        break;
    }

    return [[{
      json: {
        trigger: 'newsEvent',
        ...triggerConfig,
        timestamp: new Date().toISOString(),
        executionId: this.getExecutionId(),
        status: 'monitoring',
        message: `Monitoring ${symbols.join(', ')} for ${this.getEventTypeLabel(eventType)} events`,
      },
    }]];
  }

  private getEventTypeLabel(eventType: string): string {
    const labels: Record<string, string> = {
      news: 'news',
      earnings: 'earnings',
      secFiling: 'SEC filing',
      analystRating: 'analyst rating',
      dividend: 'dividend',
      insiderTrade: 'insider trade',
      sentiment: 'sentiment',
      socialBuzz: 'social buzz',
    };
    return labels[eventType] || eventType;
  }
}
