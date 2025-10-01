// Direct Parser - Simple object property access

import { BaseParser } from './BaseParser';

export class DirectParser extends BaseParser {
  name = 'Direct';
  description = 'Simple object property access (data.field.nested)';

  async execute(data: any, expression: string): Promise<any> {
    try {
      return this.extractNestedValue(data, expression);
    } catch (error) {
      throw new Error(`Direct access failed: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  validate(expression: string): { valid: boolean; error?: string } {
    if (!expression || expression.trim().length === 0) {
      return { valid: false, error: 'Expression cannot be empty' };
    }

    // Simple validation - check for valid characters
    if (!/^[a-zA-Z0-9_.\[\]]+$/.test(expression)) {
      return {
        valid: false,
        error: 'Expression can only contain alphanumeric characters, dots, and brackets'
      };
    }

    // Check for balanced brackets
    const openBrackets = (expression.match(/\[/g) || []).length;
    const closeBrackets = (expression.match(/\]/g) || []).length;
    if (openBrackets !== closeBrackets) {
      return { valid: false, error: 'Unbalanced brackets' };
    }

    return { valid: true };
  }

  getExample(): string {
    return 'data.candles[0].open';
  }

  getSyntaxHelp(): string {
    return `
Direct Access Syntax:

Simple field access:
  data                        - Access 'data' property
  data.candles               - Access nested property
  data.candles.0             - Access array element by index
  data.candles[0]            - Alternative array access

Nested access:
  data.quotes.AAPL.price     - Deep nested access
  response.data[0].value     - Mixed object/array access

Note: This parser is for simple cases. For complex queries
or transformations, use JSONPath, JSONata, or JMESPath.
    `.trim();
  }

  /**
   * Override to handle both dot and bracket notation
   */
  protected extractNestedValue(obj: any, path: string): any {
    if (!path) return obj;

    // Normalize path: convert [0] to .0
    const normalizedPath = path.replace(/\[(\d+)\]/g, '.$1');

    const keys = normalizedPath.split('.');
    let value = obj;

    for (const key of keys) {
      if (value == null) return undefined;
      value = value[key];
    }

    return value;
  }

  /**
   * Check if value exists at path
   */
  pathExists(data: any, expression: string): boolean {
    try {
      const value = this.extractNestedValue(data, expression);
      return value !== undefined;
    } catch {
      return false;
    }
  }

  /**
   * Get type of value at path
   */
  getValueType(data: any, expression: string): string {
    try {
      const value = this.extractNestedValue(data, expression);
      if (value === null) return 'null';
      if (value === undefined) return 'undefined';
      if (Array.isArray(value)) return 'array';
      return typeof value;
    } catch {
      return 'undefined';
    }
  }
}
