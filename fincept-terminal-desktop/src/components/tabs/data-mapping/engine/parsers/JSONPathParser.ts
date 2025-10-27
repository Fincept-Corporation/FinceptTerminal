// JSONPath Parser Implementation

import jp from 'jsonpath';
import { BaseParser } from './BaseParser';

export class JSONPathParser extends BaseParser {
  name = 'JSONPath';
  description = 'Query JSON using JSONPath syntax ($.data.field[*])';

  async execute(data: any, expression: string): Promise<any> {
    try {
      const result = jp.query(data, expression);

      // If result is array with single element, unwrap it (unless it's explicitly an array query)
      if (Array.isArray(result)) {
        // Keep as array if expression contains wildcards or is querying an array
        if (expression.includes('[*]') || expression.includes('..')) {
          return result;
        }
        // Unwrap single-element arrays for simple field access
        if (result.length === 1) {
          return result[0];
        }
        // Empty array returns undefined
        if (result.length === 0) {
          return undefined;
        }
      }

      return result;
    } catch (error) {
      throw new Error(`JSONPath execution failed: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  validate(expression: string): { valid: boolean; error?: string } {
    // Basic JSONPath validation
    if (!expression || expression.trim().length === 0) {
      return { valid: false, error: 'Expression cannot be empty' };
    }

    // Must start with $ or @
    if (!expression.startsWith('$') && !expression.startsWith('@')) {
      return { valid: false, error: 'JSONPath must start with $ or @' };
    }

    // Check for balanced brackets
    const openBrackets = (expression.match(/\[/g) || []).length;
    const closeBrackets = (expression.match(/\]/g) || []).length;
    if (openBrackets !== closeBrackets) {
      return { valid: false, error: 'Unbalanced brackets' };
    }

    // Try to parse (will throw on invalid syntax)
    try {
      jp.parse(expression);
      return { valid: true };
    } catch (error) {
      return {
        valid: false,
        error: `Invalid JSONPath syntax: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  getExample(): string {
    return '$.data.candles[*]';
  }

  getSyntaxHelp(): string {
    return `
JSONPath Syntax Examples:

Basic Selection:
  $.data                     - Select 'data' field
  $.data.candles            - Select nested field
  $.data.candles[0]         - Select first array element
  $.data.candles[*]         - Select all array elements
  $.data.candles[-1]        - Select last element

Recursive:
  $..price                  - Find all 'price' fields recursively
  $..candles[*]             - Find all candles arrays

Filtering:
  $.data.quotes[?(@.volume > 1000)]    - Filter by condition
  $.data.quotes[?(@.price)]            - Filter where field exists

Multiple Selection:
  $.data['open','close']    - Select multiple fields
  $.data.candles[0,2,4]     - Select specific indices
  $.data.candles[0:5]       - Slice (indices 0-4)
  $.data.candles[-5:]       - Last 5 elements
    `.trim();
  }

  /**
   * Generate JSONPath from user's visual selection
   */
  generatePath(keys: string[]): string {
    let path = '$';

    for (const key of keys) {
      // Handle array indices
      if (key.match(/^\[\d+\]$/)) {
        path += key;
      } else if (key.includes('.') || key.includes('[')) {
        // Quote if contains special chars
        path += `['${key}']`;
      } else {
        path += `.${key}`;
      }
    }

    return path;
  }

  /**
   * Convert JSONPath to array of keys
   */
  pathToKeys(expression: string): string[] {
    // Remove leading $
    let path = expression.replace(/^\$\.?/, '');

    // Split by . but respect brackets
    const keys: string[] = [];
    let current = '';
    let inBracket = false;

    for (let i = 0; i < path.length; i++) {
      const char = path[i];

      if (char === '[') {
        if (current) {
          keys.push(current);
          current = '';
        }
        inBracket = true;
        current += char;
      } else if (char === ']') {
        current += char;
        keys.push(current);
        current = '';
        inBracket = false;
      } else if (char === '.' && !inBracket) {
        if (current) {
          keys.push(current);
          current = '';
        }
      } else {
        current += char;
      }
    }

    if (current) {
      keys.push(current);
    }

    return keys;
  }
}
