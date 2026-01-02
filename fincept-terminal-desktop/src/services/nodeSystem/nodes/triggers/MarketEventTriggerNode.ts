/**
 * Market Event Trigger Node
 * Triggers workflow based on market events (open, close, pre-market, after-hours)
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class MarketEventTriggerNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Market Event Trigger',
    name: 'marketEventTrigger',
    icon: 'file:market-event.svg',
    group: ['trigger'],
    version: 1,
    subtitle: '={{$parameter["eventType"]}} - {{$parameter["exchange"]}}',
    description: 'Triggers workflow on market events like open, close, or economic releases',
    defaults: {
      name: 'Market Event',
      color: '#f59e0b',
    },
    inputs: [],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Event Type',
        name: 'eventType',
        type: NodePropertyType.Options,
        default: 'marketOpen',
        options: [
          { name: 'Market Open', value: 'marketOpen', description: 'When market opens' },
          { name: 'Market Close', value: 'marketClose', description: 'When market closes' },
          { name: 'Pre-Market Start', value: 'preMarketStart', description: 'Pre-market trading begins' },
          { name: 'After-Hours Start', value: 'afterHoursStart', description: 'After-hours trading begins' },
          { name: 'Economic Release', value: 'economicRelease', description: 'Economic data release' },
          { name: 'FOMC Announcement', value: 'fomc', description: 'Federal Reserve announcement' },
          { name: 'Options Expiry', value: 'optionsExpiry', description: 'Options expiration day' },
          { name: 'Futures Rollover', value: 'futuresRollover', description: 'Futures contract rollover' },
          { name: 'Quarter End', value: 'quarterEnd', description: 'End of fiscal quarter' },
          { name: 'Month End', value: 'monthEnd', description: 'End of month' },
          { name: 'Crypto Session', value: 'cryptoSession', description: 'Crypto market sessions' },
        ],
        description: 'Type of market event to trigger on',
      },
      {
        displayName: 'Exchange',
        name: 'exchange',
        type: NodePropertyType.Options,
        default: 'NYSE',
        options: [
          { name: 'NYSE/NASDAQ (US)', value: 'NYSE', description: '9:30 AM - 4:00 PM ET' },
          { name: 'NSE (India)', value: 'NSE', description: '9:15 AM - 3:30 PM IST' },
          { name: 'BSE (India)', value: 'BSE', description: '9:15 AM - 3:30 PM IST' },
          { name: 'LSE (UK)', value: 'LSE', description: '8:00 AM - 4:30 PM GMT' },
          { name: 'TSE (Tokyo)', value: 'TSE', description: '9:00 AM - 3:00 PM JST' },
          { name: 'HKEX (Hong Kong)', value: 'HKEX', description: '9:30 AM - 4:00 PM HKT' },
          { name: 'SSE (Shanghai)', value: 'SSE', description: '9:30 AM - 3:00 PM CST' },
          { name: 'Euronext', value: 'EURONEXT', description: '9:00 AM - 5:30 PM CET' },
          { name: 'XETRA (Frankfurt)', value: 'XETRA', description: '9:00 AM - 5:30 PM CET' },
          { name: 'Crypto 24/7', value: 'CRYPTO', description: 'Always open' },
        ],
        description: 'Exchange for market hours',
        displayOptions: {
          show: { eventType: ['marketOpen', 'marketClose', 'preMarketStart', 'afterHoursStart'] },
        },
      },
      {
        displayName: 'Crypto Session',
        name: 'cryptoSession',
        type: NodePropertyType.Options,
        default: 'asia',
        options: [
          { name: 'Asia Session Start (00:00 UTC)', value: 'asia' },
          { name: 'Europe Session Start (08:00 UTC)', value: 'europe' },
          { name: 'US Session Start (13:00 UTC)', value: 'us' },
          { name: 'Daily Reset (00:00 UTC)', value: 'dailyReset' },
          { name: 'Weekly Reset (Sunday 00:00 UTC)', value: 'weeklyReset' },
        ],
        description: 'Crypto trading session to trigger on',
        displayOptions: {
          show: { eventType: ['cryptoSession'] },
        },
      },
      {
        displayName: 'Economic Indicator',
        name: 'economicIndicator',
        type: NodePropertyType.Options,
        default: 'any',
        options: [
          { name: 'Any Release', value: 'any' },
          { name: 'GDP', value: 'gdp' },
          { name: 'CPI (Inflation)', value: 'cpi' },
          { name: 'Employment/NFP', value: 'nfp' },
          { name: 'Interest Rate Decision', value: 'interest' },
          { name: 'PMI', value: 'pmi' },
          { name: 'Retail Sales', value: 'retail' },
          { name: 'Housing Data', value: 'housing' },
          { name: 'Trade Balance', value: 'trade' },
          { name: 'Consumer Confidence', value: 'consumer' },
        ],
        description: 'Type of economic indicator',
        displayOptions: {
          show: { eventType: ['economicRelease'] },
        },
      },
      {
        displayName: 'Country',
        name: 'country',
        type: NodePropertyType.Options,
        default: 'US',
        options: [
          { name: 'United States', value: 'US' },
          { name: 'Eurozone', value: 'EU' },
          { name: 'United Kingdom', value: 'UK' },
          { name: 'Japan', value: 'JP' },
          { name: 'China', value: 'CN' },
          { name: 'India', value: 'IN' },
          { name: 'Germany', value: 'DE' },
          { name: 'France', value: 'FR' },
          { name: 'Canada', value: 'CA' },
          { name: 'Australia', value: 'AU' },
        ],
        description: 'Country for economic data',
        displayOptions: {
          show: { eventType: ['economicRelease'] },
        },
      },
      {
        displayName: 'Importance',
        name: 'importance',
        type: NodePropertyType.Options,
        default: 'high',
        options: [
          { name: 'Any', value: 'any' },
          { name: 'Low+', value: 'low' },
          { name: 'Medium+', value: 'medium' },
          { name: 'High Only', value: 'high' },
        ],
        description: 'Minimum importance of economic release',
        displayOptions: {
          show: { eventType: ['economicRelease'] },
        },
      },
      {
        displayName: 'Offset (Minutes)',
        name: 'offsetMinutes',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Trigger X minutes before (-) or after (+) the event',
      },
      {
        displayName: 'Options Expiry Type',
        name: 'optionsExpiryType',
        type: NodePropertyType.Options,
        default: 'monthly',
        options: [
          { name: 'Daily (0DTE)', value: 'daily' },
          { name: 'Weekly', value: 'weekly' },
          { name: 'Monthly', value: 'monthly' },
          { name: 'Quarterly', value: 'quarterly' },
          { name: 'LEAPS', value: 'leaps' },
        ],
        description: 'Type of options expiration',
        displayOptions: {
          show: { eventType: ['optionsExpiry'] },
        },
      },
      {
        displayName: 'Skip Holidays',
        name: 'skipHolidays',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Skip execution on market holidays',
      },
      {
        displayName: 'Run on Half Days',
        name: 'runOnHalfDays',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Run on shortened trading days',
      },
    ],
  };

  // Market hours configuration
  private readonly marketHours: Record<string, { open: string; close: string; timezone: string; preMarket?: string; afterHours?: string }> = {
    NYSE: { open: '09:30', close: '16:00', timezone: 'America/New_York', preMarket: '04:00', afterHours: '20:00' },
    NSE: { open: '09:15', close: '15:30', timezone: 'Asia/Kolkata', preMarket: '09:00' },
    BSE: { open: '09:15', close: '15:30', timezone: 'Asia/Kolkata', preMarket: '09:00' },
    LSE: { open: '08:00', close: '16:30', timezone: 'Europe/London' },
    TSE: { open: '09:00', close: '15:00', timezone: 'Asia/Tokyo' },
    HKEX: { open: '09:30', close: '16:00', timezone: 'Asia/Hong_Kong' },
    SSE: { open: '09:30', close: '15:00', timezone: 'Asia/Shanghai' },
    EURONEXT: { open: '09:00', close: '17:30', timezone: 'Europe/Paris' },
    XETRA: { open: '09:00', close: '17:30', timezone: 'Europe/Berlin' },
    CRYPTO: { open: '00:00', close: '23:59', timezone: 'UTC' },
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const eventType = this.getNodeParameter('eventType', 0) as string;
    const offsetMinutes = this.getNodeParameter('offsetMinutes', 0) as number;
    const skipHolidays = this.getNodeParameter('skipHolidays', 0) as boolean;
    const runOnHalfDays = this.getNodeParameter('runOnHalfDays', 0) as boolean;

    const now = new Date();
    const triggerConfig: Record<string, unknown> = {
      eventType,
      offsetMinutes,
      skipHolidays,
      runOnHalfDays,
    };

    let nextTrigger: Date | null = null;
    let description = '';

    switch (eventType) {
      case 'marketOpen':
      case 'marketClose':
      case 'preMarketStart':
      case 'afterHoursStart': {
        const exchange = this.getNodeParameter('exchange', 0) as string;
        const hours = this.marketHours[exchange];
        triggerConfig.exchange = exchange;
        triggerConfig.timezone = hours.timezone;

        if (eventType === 'marketOpen') {
          triggerConfig.triggerTime = hours.open;
          description = `${exchange} market open at ${hours.open} ${hours.timezone}`;
        } else if (eventType === 'marketClose') {
          triggerConfig.triggerTime = hours.close;
          description = `${exchange} market close at ${hours.close} ${hours.timezone}`;
        } else if (eventType === 'preMarketStart' && hours.preMarket) {
          triggerConfig.triggerTime = hours.preMarket;
          description = `${exchange} pre-market at ${hours.preMarket} ${hours.timezone}`;
        } else if (eventType === 'afterHoursStart' && hours.afterHours) {
          triggerConfig.triggerTime = hours.afterHours;
          description = `${exchange} after-hours at ${hours.afterHours} ${hours.timezone}`;
        }

        nextTrigger = this.getNextMarketEvent(exchange, eventType, offsetMinutes);
        break;
      }

      case 'economicRelease': {
        const indicator = this.getNodeParameter('economicIndicator', 0) as string;
        const country = this.getNodeParameter('country', 0) as string;
        const importance = this.getNodeParameter('importance', 0) as string;
        triggerConfig.indicator = indicator;
        triggerConfig.country = country;
        triggerConfig.importance = importance;
        description = `${country} ${indicator === 'any' ? 'economic release' : indicator.toUpperCase()}`;
        break;
      }

      case 'fomc': {
        description = 'FOMC announcement';
        break;
      }

      case 'optionsExpiry': {
        const expiryType = this.getNodeParameter('optionsExpiryType', 0) as string;
        triggerConfig.expiryType = expiryType;
        description = `${expiryType} options expiration`;
        break;
      }

      case 'futuresRollover': {
        description = 'Futures contract rollover';
        break;
      }

      case 'quarterEnd': {
        description = 'Quarter end';
        nextTrigger = this.getNextQuarterEnd();
        break;
      }

      case 'monthEnd': {
        description = 'Month end';
        nextTrigger = this.getNextMonthEnd();
        break;
      }

      case 'cryptoSession': {
        const session = this.getNodeParameter('cryptoSession', 0) as string;
        triggerConfig.session = session;
        description = `Crypto ${session} session start`;
        break;
      }
    }

    if (offsetMinutes !== 0) {
      description += ` (${offsetMinutes > 0 ? '+' : ''}${offsetMinutes} min)`;
    }

    return [[{
      json: {
        trigger: 'marketEvent',
        ...triggerConfig,
        description,
        currentTime: now.toISOString(),
        nextTrigger: nextTrigger?.toISOString() || null,
        executionId: this.getExecutionId(),
        status: 'scheduled',
        message: `Waiting for ${description}`,
      },
    }]];
  }

  private getNextMarketEvent(exchange: string, eventType: string, offsetMinutes: number): Date {
    const hours = this.marketHours[exchange];
    const now = new Date();
    const next = new Date(now);

    let timeStr = hours.open;
    if (eventType === 'marketClose') timeStr = hours.close;
    else if (eventType === 'preMarketStart' && hours.preMarket) timeStr = hours.preMarket;
    else if (eventType === 'afterHoursStart' && hours.afterHours) timeStr = hours.afterHours;

    const [hour, minute] = timeStr.split(':').map(Number);
    next.setHours(hour, minute + offsetMinutes, 0, 0);

    if (next <= now) {
      next.setDate(next.getDate() + 1);
    }

    // Skip weekends for non-crypto
    if (exchange !== 'CRYPTO') {
      while (next.getDay() === 0 || next.getDay() === 6) {
        next.setDate(next.getDate() + 1);
      }
    }

    return next;
  }

  private getNextQuarterEnd(): Date {
    const now = new Date();
    const month = now.getMonth();
    const year = now.getFullYear();

    // Quarter end months: 2 (March), 5 (June), 8 (September), 11 (December)
    const quarterEndMonths = [2, 5, 8, 11];

    for (const qMonth of quarterEndMonths) {
      if (month <= qMonth) {
        const lastDay = new Date(year, qMonth + 1, 0);
        if (lastDay > now) return lastDay;
      }
    }

    // Next year Q1
    return new Date(year + 1, 2 + 1, 0);
  }

  private getNextMonthEnd(): Date {
    const now = new Date();
    const currentMonthEnd = new Date(now.getFullYear(), now.getMonth() + 1, 0);

    if (currentMonthEnd > now) {
      return currentMonthEnd;
    }

    return new Date(now.getFullYear(), now.getMonth() + 2, 0);
  }
}
