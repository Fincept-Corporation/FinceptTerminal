// Base Parser Interface

import { ParserConfig } from '../../types';

export interface ParseResult {
  success: boolean;
  data?: any;
  error?: string;
}

export abstract class BaseParser {
  abstract name: string;
  abstract description: string;

  /**
   * Execute the parser on given data
   */
  abstract execute(data: any, expression: string): Promise<any>;

  /**
   * Validate expression syntax
   */
  abstract validate(expression: string): { valid: boolean; error?: string };

  /**
   * Generate example expression
   */
  abstract getExample(): string;

  /**
   * Get syntax help
   */
  abstract getSyntaxHelp(): string;

  /**
   * Parse with error handling
   */
  async parse(data: any, config: ParserConfig): Promise<ParseResult> {
    try {
      // Validate expression first
      const validation = this.validate(config.expression);
      if (!validation.valid) {
        return {
          success: false,
          error: validation.error || 'Invalid expression',
        };
      }

      // Execute parser
      const result = await this.execute(data, config.expression);

      return {
        success: true,
        data: result,
      };
    } catch (error) {
      // Try fallback if provided
      if (config.fallback) {
        try {
          const fallbackResult = await this.execute(data, config.fallback);
          return {
            success: true,
            data: fallbackResult,
          };
        } catch (fallbackError) {
          return {
            success: false,
            error: `Primary and fallback parsing failed: ${error instanceof Error ? error.message : String(error)}`,
          };
        }
      }

      return {
        success: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Extract value from nested path
   */
  protected extractNestedValue(obj: any, path: string): any {
    const keys = path.split('.');
    let value = obj;

    for (const key of keys) {
      if (value == null) return undefined;

      // Handle array indices like [0]
      const arrayMatch = key.match(/^(.+)\[(\d+)\]$/);
      if (arrayMatch) {
        const [, arrayKey, index] = arrayMatch;
        value = value[arrayKey];
        if (Array.isArray(value)) {
          value = value[parseInt(index, 10)];
        }
      } else {
        value = value[key];
      }
    }

    return value;
  }
}
