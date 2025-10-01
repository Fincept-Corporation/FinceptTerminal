// JavaScript Parser - Sandboxed custom JavaScript execution

import { BaseParser } from './BaseParser';

export class JavaScriptParser extends BaseParser {
  name = 'JavaScript';
  description = 'Custom JavaScript expressions (sandboxed)';

  async execute(data: any, expression: string): Promise<any> {
    try {
      // Create sandboxed execution context
      const sandbox = this.createSandbox(data);

      // Wrap expression in async function
      const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;

      // If expression doesn't have return, add it
      let code = expression.trim();
      if (!code.includes('return')) {
        code = `return ${code}`;
      }

      const func = new AsyncFunction('data', 'helpers', code);
      const result = await func(data, sandbox.helpers);

      return result;
    } catch (error) {
      throw new Error(`JavaScript execution failed: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  validate(expression: string): { valid: boolean; error?: string } {
    if (!expression || expression.trim().length === 0) {
      return { valid: false, error: 'Expression cannot be empty' };
    }

    // Basic security checks
    const dangerous = ['eval', 'Function', 'setTimeout', 'setInterval', 'XMLHttpRequest', 'fetch', 'import'];
    for (const keyword of dangerous) {
      if (expression.includes(keyword)) {
        return {
          valid: false,
          error: `Unsafe operation: ${keyword} is not allowed for security reasons`
        };
      }
    }

    // Try to create the function
    try {
      const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
      let code = expression.trim();
      if (!code.includes('return')) {
        code = `return ${code}`;
      }
      new AsyncFunction('data', 'helpers', code);
      return { valid: true };
    } catch (error: any) {
      return {
        valid: false,
        error: `Syntax error: ${error.message || String(error)}`
      };
    }
  }

  getExample(): string {
    return `data.candles.map(c => ({
  timestamp: helpers.parseDate(c[0]),
  open: c[1],
  high: c[2],
  low: c[3],
  close: c[4],
  volume: c[5]
}))`;
  }

  getSyntaxHelp(): string {
    return `
JavaScript Expression Syntax:

Available Variables:
  data       - The input data
  helpers    - Utility functions (see below)

Simple Expressions:
  data.price * 1.1                    - Calculate 10% markup
  data.quotes.filter(q => q.volume > 1000)  - Filter array
  data.candles.map(c => c.close)      - Extract field

Array of Arrays Transformation:
  data.candles.map(c => ({
    timestamp: c[0],
    open: c[1],
    high: c[2],
    low: c[3],
    close: c[4],
    volume: c[5]
  }))

Complex Object Restructuring:
  Object.entries(data.quotes).map(([symbol, quote]) => ({
    symbol: symbol.replace('NSE:', ''),
    price: quote.last_price,
    change: quote.change_percent
  }))

Conditional Logic:
  data.status === 'ok' ? data.quotes : []
  data.candles.filter(c => c.volume > helpers.avg(data.candles.map(x => x.volume)))

Available Helper Functions:

Date/Time:
  helpers.parseDate(str)              - Parse date string
  helpers.toISODate(timestamp)        - Convert to ISO format
  helpers.fromUnixTimestamp(ts)       - Unix timestamp to Date
  helpers.toUnixTimestamp(date)       - Date to Unix timestamp

Array Operations:
  helpers.flatten(array)              - Flatten nested arrays
  helpers.groupBy(array, key)         - Group by field
  helpers.sum(array)                  - Sum numbers
  helpers.avg(array)                  - Average
  helpers.min(array)                  - Minimum
  helpers.max(array)                  - Maximum
  helpers.unique(array)               - Remove duplicates

String Operations:
  helpers.uppercase(str)              - UPPERCASE
  helpers.lowercase(str)              - lowercase
  helpers.trim(str)                   - Remove whitespace
  helpers.replace(str, find, replace) - Replace text

Number Operations:
  helpers.round(num, decimals)        - Round number
  helpers.toFixed(num, decimals)      - Fixed decimals
  helpers.toPercent(num)              - Convert to percentage (0.15 -> 15)
  helpers.fromPercent(num)            - Convert from percentage (15 -> 0.15)

Security Note:
  For security, the following are disabled:
  - eval, Function constructor
  - setTimeout, setInterval
  - fetch, XMLHttpRequest
  - import, require
    `.trim();
  }

  /**
   * Create sandboxed execution context
   */
  private createSandbox(data: any) {
    return {
      data,
      helpers: {
        // Date/Time helpers
        parseDate: (value: any) => new Date(value),
        toISODate: (value: any) => new Date(value).toISOString(),
        fromUnixTimestamp: (ts: number) => new Date(ts * 1000).toISOString(),
        toUnixTimestamp: (date: any) => Math.floor(new Date(date).getTime() / 1000),

        // Array helpers
        flatten: (arr: any[]) => arr.flat(Infinity),
        groupBy: (arr: any[], key: string) => {
          return arr.reduce((acc, item) => {
            const groupKey = item[key];
            (acc[groupKey] = acc[groupKey] || []).push(item);
            return acc;
          }, {} as Record<string, any[]>);
        },
        sum: (arr: number[]) => arr.reduce((a, b) => a + b, 0),
        avg: (arr: number[]) => arr.reduce((a, b) => a + b, 0) / arr.length,
        min: (arr: number[]) => Math.min(...arr),
        max: (arr: number[]) => Math.max(...arr),
        unique: (arr: any[]) => [...new Set(arr)],

        // String helpers
        uppercase: (str: string) => str.toUpperCase(),
        lowercase: (str: string) => str.toLowerCase(),
        trim: (str: string) => str.trim(),
        replace: (str: string, find: string, replace: string) => str.replace(new RegExp(find, 'g'), replace),

        // Number helpers
        round: (num: number, decimals = 2) => Math.round(num * Math.pow(10, decimals)) / Math.pow(10, decimals),
        toFixed: (num: number, decimals = 2) => Number(num.toFixed(decimals)),
        toPercent: (num: number) => num * 100,
        fromPercent: (num: number) => num / 100,

        // Financial helpers
        bpsToPercent: (bps: number) => bps / 100,
        percentToBps: (percent: number) => percent * 100,
      },
    };
  }

  /**
   * Test expression safety
   */
  isSafe(expression: string): boolean {
    const dangerous = [
      'eval',
      'Function',
      'setTimeout',
      'setInterval',
      'XMLHttpRequest',
      'fetch',
      'import',
      'require',
      '__proto__',
      'constructor',
      'prototype',
    ];

    return !dangerous.some(keyword => expression.includes(keyword));
  }
}
