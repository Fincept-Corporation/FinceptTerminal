/**
 * Schedule Trigger Node
 * Triggers workflow based on cron schedule
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { MarketEventTriggerNode } from './MarketEventTriggerNode';

export class ScheduleTriggerNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Scheduled Start',
    name: 'scheduleTrigger',
    icon: 'file:schedule.svg',
    group: ['trigger'],
    version: 1,
    subtitle: '={{$parameter["scheduleType"]}}',
    description: 'Triggers workflow on a schedule (cron)',
    defaults: {
      name: 'Scheduled Start',
      color: '#f59e0b',
    },
    inputs: [],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Schedule Type',
        name: 'scheduleType',
        type: NodePropertyType.Options,
        default: 'interval',
        options: [
          { name: 'Every Minute', value: 'everyMinute', description: 'Run every minute' },
          { name: 'Every Hour', value: 'everyHour', description: 'Run every hour' },
          { name: 'Every Day', value: 'everyDay', description: 'Run every day at specified time' },
          { name: 'Every Week', value: 'everyWeek', description: 'Run every week on specified days' },
          { name: 'Market Open', value: 'marketOpen', description: 'Run at market open (9:30 AM ET)' },
          { name: 'Market Close', value: 'marketClose', description: 'Run at market close (4:00 PM ET)' },
          { name: 'Custom Cron', value: 'cron', description: 'Custom cron expression' },
        ],
        description: 'When to run the workflow',
      },
      {
        displayName: 'Time',
        name: 'time',
        type: NodePropertyType.String,
        default: '09:00',
        placeholder: 'HH:MM',
        description: 'Time to run (24-hour format)',
        displayOptions: {
          show: { scheduleType: ['everyDay'] },
        },
      },
      {
        displayName: 'Days of Week',
        name: 'daysOfWeek',
        type: NodePropertyType.MultiOptions,
        default: ['1', '2', '3', '4', '5'],
        options: [
          { name: 'Monday', value: '1' },
          { name: 'Tuesday', value: '2' },
          { name: 'Wednesday', value: '3' },
          { name: 'Thursday', value: '4' },
          { name: 'Friday', value: '5' },
          { name: 'Saturday', value: '6' },
          { name: 'Sunday', value: '0' },
        ],
        description: 'Days to run the workflow',
        displayOptions: {
          show: { scheduleType: ['everyWeek'] },
        },
      },
      {
        displayName: 'Cron Expression',
        name: 'cronExpression',
        type: NodePropertyType.String,
        default: '0 9 * * 1-5',
        placeholder: '0 9 * * 1-5',
        description: 'Cron expression (minute hour day month dayOfWeek)',
        displayOptions: {
          show: { scheduleType: ['cron'] },
        },
      },
      {
        displayName: 'Exchange',
        name: 'exchange',
        type: NodePropertyType.Options,
        default: 'NYSE',
        options: [
          { name: 'NYSE/NASDAQ (US)', value: 'NYSE', description: '9:30 AM - 4:00 PM ET' },
          { name: 'NSE (India)', value: 'NSE', description: '9:15 AM - 3:30 PM IST' },
          { name: 'LSE (UK)', value: 'LSE', description: '8:00 AM - 4:30 PM GMT' },
          { name: 'Crypto 24/7', value: 'CRYPTO', description: 'Always open' },
        ],
        description: 'Exchange for market hours',
        displayOptions: {
          show: { scheduleType: ['marketOpen', 'marketClose'] },
        },
      },
      {
        displayName: 'Timezone',
        name: 'timezone',
        type: NodePropertyType.Options,
        default: 'America/New_York',
        options: [
          { name: 'New York (ET)', value: 'America/New_York' },
          { name: 'Mumbai (IST)', value: 'Asia/Kolkata' },
          { name: 'London (GMT)', value: 'Europe/London' },
          { name: 'Tokyo (JST)', value: 'Asia/Tokyo' },
          { name: 'Singapore (SGT)', value: 'Asia/Singapore' },
          { name: 'UTC', value: 'UTC' },
        ],
        description: 'Timezone for schedule',
      },
      {
        displayName: 'Skip Weekends',
        name: 'skipWeekends',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Skip execution on weekends (Sat/Sun)',
        displayOptions: {
          show: { scheduleType: ['everyMinute', 'everyHour', 'everyDay', 'cron'] },
        },
      },
      {
        displayName: 'Skip Holidays',
        name: 'skipHolidays',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Skip execution on market holidays',
        displayOptions: {
          show: { scheduleType: ['everyMinute', 'everyHour', 'everyDay', 'cron', 'marketOpen', 'marketClose'] },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const scheduleType = this.getNodeParameter('scheduleType', 0) as string;
    const timezone = this.getNodeParameter('timezone', 0) as string;
    const now = new Date();

    // Calculate next run time based on schedule type
    let nextRun: Date;
    let cronExpression: string;

    switch (scheduleType) {
      case 'everyMinute':
        cronExpression = '* * * * *';
        nextRun = new Date(now.getTime() + 60000);
        break;
      case 'everyHour':
        cronExpression = '0 * * * *';
        nextRun = new Date(now.getTime() + 3600000);
        break;
      case 'everyDay': {
        const time = this.getNodeParameter('time', 0) as string;
        const [hour, minute] = time.split(':').map(Number);
        cronExpression = `${minute} ${hour} * * *`;
        nextRun = ScheduleTriggerNode.getNextDailyRun(hour, minute, timezone);
        break;
      }
      case 'everyWeek': {
        const daysOfWeek = this.getNodeParameter('daysOfWeek', 0) as string[];
        cronExpression = `0 9 * * ${daysOfWeek.join(',')}`;
        nextRun = ScheduleTriggerNode.getNextWeeklyRun(daysOfWeek, timezone);
        break;
      }
      case 'marketOpen': {
        const openExchange = this.getNodeParameter('exchange', 0) as string;
        cronExpression = ScheduleTriggerNode.getMarketOpenCron(openExchange);
        nextRun = ScheduleTriggerNode.getNextMarketOpen(openExchange);
        break;
      }
      case 'marketClose': {
        const closeExchange = this.getNodeParameter('exchange', 0) as string;
        cronExpression = ScheduleTriggerNode.getMarketCloseCron(closeExchange);
        nextRun = ScheduleTriggerNode.getNextMarketClose(closeExchange);
        break;
      }
      case 'cron':
        cronExpression = this.getNodeParameter('cronExpression', 0) as string;
        nextRun = new Date(now.getTime() + 60000); // Approximate
        break;
      default:
        cronExpression = '0 * * * *';
        nextRun = new Date(now.getTime() + 3600000);
    }

    return [[{
      json: {
        trigger: 'schedule',
        scheduleType,
        cronExpression,
        timezone,
        currentTime: now.toISOString(),
        nextRun: nextRun.toISOString(),
        executionId: this.getExecutionId(),
        isWeekend: now.getDay() === 0 || now.getDay() === 6,
      },
    }]];
  }

  private static getNextDailyRun(hour: number, minute: number, timezone: string): Date {
    const now = new Date();
    const next = new Date(now);
    next.setHours(hour, minute, 0, 0);
    if (next <= now) {
      next.setDate(next.getDate() + 1);
    }
    return next;
  }

  private static getNextWeeklyRun(days: string[], timezone: string): Date {
    const now = new Date();
    const currentDay = now.getDay();
    const daysNum = days.map(Number).sort((a, b) => a - b);

    for (const day of daysNum) {
      if (day > currentDay) {
        const next = new Date(now);
        next.setDate(now.getDate() + (day - currentDay));
        next.setHours(9, 0, 0, 0);
        return next;
      }
    }

    // Next week
    const next = new Date(now);
    next.setDate(now.getDate() + (7 - currentDay + daysNum[0]));
    next.setHours(9, 0, 0, 0);
    return next;
  }

  private static getMarketOpenCron(exchange: string): string {
    switch (exchange) {
      case 'NYSE': return '30 9 * * 1-5'; // 9:30 AM ET
      case 'NSE': return '15 9 * * 1-5'; // 9:15 AM IST
      case 'LSE': return '0 8 * * 1-5'; // 8:00 AM GMT
      case 'CRYPTO': return '0 0 * * *'; // Midnight (always open)
      default: return '30 9 * * 1-5';
    }
  }

  private static getMarketCloseCron(exchange: string): string {
    switch (exchange) {
      case 'NYSE': return '0 16 * * 1-5'; // 4:00 PM ET
      case 'NSE': return '30 15 * * 1-5'; // 3:30 PM IST
      case 'LSE': return '30 16 * * 1-5'; // 4:30 PM GMT
      case 'CRYPTO': return '59 23 * * *'; // 11:59 PM (never closes)
      default: return '0 16 * * 1-5';
    }
  }

  private static getNextMarketOpen(exchange: string): Date {
    const now = new Date();
    const next = new Date(now);

    switch (exchange) {
      case 'NYSE':
        next.setUTCHours(14, 30, 0, 0); // 9:30 AM ET = 14:30 UTC
        break;
      case 'NSE':
        next.setUTCHours(3, 45, 0, 0); // 9:15 AM IST = 3:45 UTC
        break;
      case 'LSE':
        next.setUTCHours(8, 0, 0, 0); // 8:00 AM GMT
        break;
      default:
        next.setHours(9, 30, 0, 0);
    }

    if (next <= now) {
      next.setDate(next.getDate() + 1);
    }

    // Skip weekends
    while (next.getDay() === 0 || next.getDay() === 6) {
      next.setDate(next.getDate() + 1);
    }

    return next;
  }

  private static getNextMarketClose(exchange: string): Date {
    const now = new Date();
    const next = new Date(now);

    switch (exchange) {
      case 'NYSE':
        next.setUTCHours(21, 0, 0, 0); // 4:00 PM ET = 21:00 UTC
        break;
      case 'NSE':
        next.setUTCHours(10, 0, 0, 0); // 3:30 PM IST = 10:00 UTC
        break;
      case 'LSE':
        next.setUTCHours(16, 30, 0, 0); // 4:30 PM GMT
        break;
      default:
        next.setHours(16, 0, 0, 0);
    }

    if (next <= now) {
      next.setDate(next.getDate() + 1);
    }

    // Skip weekends
    while (next.getDay() === 0 || next.getDay() === 6) {
      next.setDate(next.getDate() + 1);
    }

    return next;
  }
}
