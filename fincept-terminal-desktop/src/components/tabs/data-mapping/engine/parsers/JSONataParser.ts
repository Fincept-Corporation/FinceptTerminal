// JSONata Parser Implementation

import jsonata from 'jsonata';
import { BaseParser } from './BaseParser';

export class JSONataParser extends BaseParser {
  name = 'JSONata';
  description = 'Powerful JSON transformation and query language';

  async execute(data: any, expression: string): Promise<any> {
    try {
      const compiled = jsonata(expression);
      const result = await compiled.evaluate(data);
      return result;
    } catch (error) {
      throw new Error(`JSONata execution failed: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  validate(expression: string): { valid: boolean; error?: string } {
    if (!expression || expression.trim().length === 0) {
      return { valid: false, error: 'Expression cannot be empty' };
    }

    try {
      // Try to compile the expression
      jsonata(expression);
      return { valid: true };
    } catch (error: any) {
      return {
        valid: false,
        error: `Invalid JSONata syntax: ${error.message || String(error)}`
      };
    }
  }

  getExample(): string {
    return 'data.candles.{ "timestamp": $[0], "open": $[1], "close": $[4] }';
  }

  getSyntaxHelp(): string {
    return `
JSONata Syntax Examples:

Basic Selection:
  data.candles              - Select field
  data.candles[0]           - First array element
  data.candles[-1]          - Last element

Array Transformation:
  data.candles[].price      - Extract price from all items
  data.candles.{ "open": $[1], "close": $[4] }  - Map array to object

Object Construction:
  {
    "symbol": data.symbol,
    "price": data.last_price,
    "volume": data.volume
  }

Filtering:
  data.quotes[volume > 1000]           - Filter by condition
  data.quotes[symbol = "AAPL"]         - Exact match
  data.quotes[$contains(symbol, "AA")] - String contains

Aggregation:
  $sum(data.candles.volume)            - Sum array
  $average(data.candles.close)         - Average
  $max(data.candles.high)              - Maximum
  $min(data.candles.low)               - Minimum
  $count(data.quotes)                  - Count items

String Functions:
  $uppercase(symbol)                    - UPPERCASE
  $lowercase(symbol)                    - lowercase
  $substring(symbol, 0, 3)             - Extract substring
  $replace(symbol, "NSE:", "")         - Replace text

Date/Time:
  $now()                               - Current timestamp
  $fromMillis(timestamp)               - Convert from unix ms
  $toMillis(datetime)                  - Convert to unix ms

Conditionals:
  volume > 1000 ? "high" : "low"       - Ternary operator
  status = "ok" ? data : null          - Conditional selection

Array of Arrays (Common in Financial APIs):
  data.candles.{
    "timestamp": $[0],
    "open": $[1],
    "high": $[2],
    "low": $[3],
    "close": $[4],
    "volume": $[5]
  }

Nested Object Access:
  data.quotes.*.{
    "symbol": symbol,
    "bid": spread.bid,
    "ask": spread.ask
  }
    `.trim();
  }

  /**
   * Build JSONata object constructor from field mappings
   */
  buildObjectConstructor(mappings: Array<{ target: string; source: string }>): string {
    const fields = mappings.map(m => `"${m.target}": ${m.source}`).join(',\n  ');
    return `{\n  ${fields}\n}`;
  }

  /**
   * Build array mapping expression
   */
  buildArrayMapper(arrayPath: string, mappings: Array<{ target: string; source: string }>): string {
    const objectConstructor = this.buildObjectConstructor(mappings);
    return `${arrayPath}.${objectConstructor}`;
  }

  /**
   * Build expression for array of arrays
   */
  buildArrayOfArraysMapper(arrayPath: string, indexMappings: Array<{ target: string; index: number }>): string {
    const fields = indexMappings.map(m => `"${m.target}": $[${m.index}]`).join(',\n  ');
    return `${arrayPath}.{\n  ${fields}\n}`;
  }

  /**
   * Add filter condition
   */
  addFilter(expression: string, condition: string): string {
    // Remove trailing object constructor if present
    const match = expression.match(/^(.+?)(\.\{.+\})$/s);

    if (match) {
      const [, path, constructor] = match;
      return `${path}[${condition}]${constructor}`;
    } else {
      return `${expression}[${condition}]`;
    }
  }

  /**
   * Add sorting
   */
  addSort(expression: string, field: string, order: 'asc' | 'desc' = 'asc'): string {
    const operator = order === 'asc' ? '<' : '>';
    return `${expression}^(${operator}${field})`;
  }
}
