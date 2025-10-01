// JMESPath Parser Implementation

import jmespath from 'jmespath';
import { BaseParser } from './BaseParser';

export class JMESPathParser extends BaseParser {
  name = 'JMESPath';
  description = 'AWS-style JSON query language';

  async execute(data: any, expression: string): Promise<any> {
    try {
      const result = jmespath.search(data, expression);
      return result;
    } catch (error) {
      throw new Error(`JMESPath execution failed: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  validate(expression: string): { valid: boolean; error?: string } {
    if (!expression || expression.trim().length === 0) {
      return { valid: false, error: 'Expression cannot be empty' };
    }

    try {
      // Try to search with a dummy object to validate the expression
      jmespath.search({}, expression);
      return { valid: true };
    } catch (error: any) {
      return {
        valid: false,
        error: `Invalid JMESPath syntax: ${error.message || String(error)}`
      };
    }
  }

  getExample(): string {
    return 'data.candles[*].[timestamp, open, high, low, close, volume]';
  }

  getSyntaxHelp(): string {
    return `
JMESPath Syntax Examples:

Basic Selection:
  data.candles                    - Select field
  data.candles[0]                 - First element
  data.candles[-1]                - Last element
  data.candles[*]                 - All elements

Projection:
  data.candles[*].price           - Extract price from all
  data.candles[*].open            - Extract open from all

Multi-Select List (Array output):
  data.candles[*].[open, close]   - Arrays of [open, close]
  data[*].[timestamp, price]      - Multiple fields as arrays

Multi-Select Hash (Object output):
  data.candles[*].{
    open: open,
    close: close,
    volume: volume
  }

Filtering:
  data.quotes[?volume > \`1000\`]            - Filter by condition
  data.quotes[?price > \`100\` && volume > \`1000\`]  - Multiple conditions
  data.quotes[?contains(symbol, 'AA')]      - String contains

Pipe (Chaining):
  data.candles[*].open | [0]      - First open price
  data.quotes | sort_by(@, &volume) | reverse(@)  - Sort desc by volume

Functions:
  length(data.candles)            - Array length
  max(data.candles[*].high)       - Maximum value
  min(data.candles[*].low)        - Minimum value
  avg(data.candles[*].close)      - Average
  sum(data.candles[*].volume)     - Sum

Flattening:
  data[*].values[]                - Flatten nested arrays
  data.*.quotes[].price           - Multi-level flattening

Slicing:
  data.candles[0:5]               - First 5 elements
  data.candles[-5:]               - Last 5 elements
  data.candles[::2]               - Every 2nd element

Sorting:
  sort_by(data.quotes, &price)    - Sort ascending
  reverse(sort_by(data.quotes, &price))  - Sort descending
    `.trim();
  }

  /**
   * Build multi-select hash (object) expression
   */
  buildObjectProjection(path: string, mappings: Array<{ target: string; source: string }>): string {
    const fields = mappings.map(m => `${m.target}: ${m.source}`).join(', ');
    return `${path}.{${fields}}`;
  }

  /**
   * Build multi-select list (array) expression
   */
  buildArrayProjection(path: string, fields: string[]): string {
    return `${path}.[${fields.join(', ')}]`;
  }

  /**
   * Add filter
   */
  addFilter(expression: string, condition: string): string {
    return `${expression}[?${condition}]`;
  }

  /**
   * Add sorting
   */
  addSort(expression: string, field: string, order: 'asc' | 'desc' = 'asc'): string {
    const sorted = `sort_by(${expression}, &${field})`;
    return order === 'desc' ? `reverse(${sorted})` : sorted;
  }
}
