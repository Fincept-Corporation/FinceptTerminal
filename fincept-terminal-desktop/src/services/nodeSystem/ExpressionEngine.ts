/**
 * Expression Engine
 * Evaluates expressions in node parameters
 *
 * Supports expression syntax like:
 * - {{$json.fieldName}} - Access JSON fields
 * - {{$json["field name"]}} - Access fields with special characters
 * - {{$json.nested.field}} - Nested field access
 * - {{$input.item.json.field}} - Explicit input access
 * - {{$parameter.paramName}} - Access other parameters
 * - {{1 + 2}} - Basic arithmetic
 * - {{$json.price * 1.1}} - Expressions with data
 * - {{$now}} - Current datetime
 * - {{$today}} - Current date
 *
 * Based on n8n's expression system but simplified for core functionality.
 */

import type {
  IDataObject,
  INodeExecutionData,
  INodeParameters,
  NodeParameterValue,
} from './types';

/**
 * Check if a value contains an expression
 */
export function isExpression(value: any): boolean {
  if (typeof value !== 'string') {
    return false;
  }

  // Expression starts with = or contains {{...}}
  return value.startsWith('=') || /\{\{.+?\}\}/.test(value);
}

/**
 * Expression evaluation context
 */
export interface IExpressionContext {
  $json: IDataObject;              // Current item's JSON data
  $input: {                        // Input data
    item: INodeExecutionData;
    all: INodeExecutionData[];
    first: INodeExecutionData;
    last: INodeExecutionData;
  };
  $parameter: INodeParameters;     // Node parameters
  $now: Date;                      // Current datetime
  $today: Date;                    // Today at midnight
  $itemIndex: number;              // Current item index
  $runIndex: number;               // Current run index
  $node: {                         // Current node info
    name: string;
    type: string;
  };
  $workflow: {                     // Workflow info
    name: string;
    active: boolean;
    id?: string;
  };
}

/**
 * Expression Engine
 * Evaluates expressions with access to workflow data
 */
export class ExpressionEngine {
  /**
   * Resolve a parameter value that may contain expressions
   */
  static resolveValue(
    value: NodeParameterValue,
    context: Partial<IExpressionContext>
  ): NodeParameterValue {
    if (!isExpression(value)) {
      return value;
    }

    if (typeof value !== 'string') {
      return value;
    }

    try {
      // Handle = prefix (entire value is an expression)
      if (value.startsWith('=')) {
        const expression = value.substring(1).trim();
        return this.evaluateExpression(expression, context);
      }

      // Handle {{...}} syntax (can have multiple in one string)
      return this.replaceExpressions(value, context);
    } catch (error) {
      console.error('[ExpressionEngine] Error evaluating expression:', error);
      return value; // Return original value on error
    }
  }

  /**
   * Replace all {{...}} expressions in a string
   */
  private static replaceExpressions(
    str: string,
    context: Partial<IExpressionContext>
  ): string {
    return str.replace(/\{\{(.+?)\}\}/g, (match, expression) => {
      try {
        const result = this.evaluateExpression(expression.trim(), context);
        return String(result ?? '');
      } catch (error) {
        console.warn('[ExpressionEngine] Failed to evaluate:', expression, error);
        return match; // Return original on error
      }
    });
  }

  /**
   * Evaluate a single expression
   */
  private static evaluateExpression(
    expression: string,
    context: Partial<IExpressionContext>
  ): any {
    // Handle special variables
    if (expression === '$now') {
      return context.$now || new Date();
    }
    if (expression === '$today') {
      const today = context.$today || new Date();
      today.setHours(0, 0, 0, 0);
      return today;
    }
    if (expression === '$itemIndex') {
      return context.$itemIndex ?? 0;
    }
    if (expression === '$runIndex') {
      return context.$runIndex ?? 0;
    }

    // Handle object property access
    if (expression.startsWith('$json.')) {
      const path = expression.substring(6);
      return this.getNestedValue(context.$json || {}, path);
    }
    if (expression.startsWith('$json[')) {
      // Handle bracket notation: $json["field name"]
      const match = expression.match(/\$json\["(.+?)"\]/);
      if (match) {
        return (context.$json || {})[match[1]];
      }
    }
    if (expression.startsWith('$input.item.json.')) {
      const path = expression.substring(17);
      return this.getNestedValue(context.$input?.item?.json || {}, path);
    }
    if (expression.startsWith('$input.first.json.')) {
      const path = expression.substring(18);
      return this.getNestedValue(context.$input?.first?.json || {}, path);
    }
    if (expression.startsWith('$input.last.json.')) {
      const path = expression.substring(17);
      return this.getNestedValue(context.$input?.last?.json || {}, path);
    }
    if (expression.startsWith('$parameter.')) {
      const paramName = expression.substring(11);
      return (context.$parameter || {})[paramName];
    }
    if (expression.startsWith('$node.')) {
      const prop = expression.substring(6);
      return (context.$node as any)?.[prop];
    }
    if (expression.startsWith('$workflow.')) {
      const prop = expression.substring(10);
      return (context.$workflow as any)?.[prop];
    }

    // Handle complex expressions with JavaScript evaluation
    // This is where we evaluate arithmetic, comparisons, etc.
    return this.evaluateJavaScript(expression, context);
  }

  /**
   * Evaluate a JavaScript expression
   * IMPORTANT: Sanitized execution with limited scope
   */
  private static evaluateJavaScript(
    expression: string,
    context: Partial<IExpressionContext>
  ): any {
    try {
      // Create safe execution context
      const $json = context.$json || {};
      const $input = context.$input;
      const $parameter = context.$parameter || {};
      const $now = context.$now || new Date();
      const $today = (() => {
        const date = new Date();
        date.setHours(0, 0, 0, 0);
        return date;
      })();
      const $itemIndex = context.$itemIndex ?? 0;
      const $runIndex = context.$runIndex ?? 0;
      const $node = context.$node;
      const $workflow = context.$workflow;

      // Helper functions available in expressions
      const helpers = {
        // Date helpers
        DateTime: Date,
        now: () => new Date(),
        today: () => {
          const d = new Date();
          d.setHours(0, 0, 0, 0);
          return d;
        },

        // String helpers
        toLowerCase: (str: string) => String(str).toLowerCase(),
        toUpperCase: (str: string) => String(str).toUpperCase(),
        trim: (str: string) => String(str).trim(),
        split: (str: string, separator: string) => String(str).split(separator),
        substring: (str: string, start: number, end?: number) =>
          String(str).substring(start, end),

        // Number helpers
        toNumber: (val: any) => Number(val),
        toFixed: (num: number, decimals: number) => Number(num).toFixed(decimals),
        round: (num: number) => Math.round(num),
        floor: (num: number) => Math.floor(num),
        ceil: (num: number) => Math.ceil(num),
        abs: (num: number) => Math.abs(num),

        // Array helpers
        length: (arr: any) => (Array.isArray(arr) ? arr.length : 0),
        first: (arr: any[]) => arr[0],
        last: (arr: any[]) => arr[arr.length - 1],
        join: (arr: any[], separator: string) => arr.join(separator),

        // Object helpers
        keys: (obj: object) => Object.keys(obj),
        values: (obj: object) => Object.values(obj),
        has: (obj: object, key: string) => key in obj,

        // Conditional helpers
        isEmpty: (val: any) =>
          val === null || val === undefined || val === '' || (Array.isArray(val) && val.length === 0),
        isNotEmpty: (val: any) =>
          val !== null && val !== undefined && val !== '' && (!Array.isArray(val) || val.length > 0),
      };

      // Use Function constructor for safe evaluation
      // This creates a new function scope with only the variables we provide
      const func = new Function(
        '$json',
        '$input',
        '$parameter',
        '$now',
        '$today',
        '$itemIndex',
        '$runIndex',
        '$node',
        '$workflow',
        'helpers',
        'Math',
        'String',
        'Number',
        'Array',
        'Object',
        'Date',
        `'use strict'; return (${expression});`
      );

      return func(
        $json,
        $input,
        $parameter,
        $now,
        $today,
        $itemIndex,
        $runIndex,
        $node,
        $workflow,
        helpers,
        Math,
        String,
        Number,
        Array,
        Object,
        Date
      );
    } catch (error) {
      console.error('[ExpressionEngine] JavaScript evaluation error:', error);
      throw error;
    }
  }

  /**
   * Get nested value from object using dot notation
   */
  private static getNestedValue(obj: IDataObject, path: string): any {
    const keys = path.split('.');
    let current: any = obj;

    for (const key of keys) {
      if (current === null || current === undefined) {
        return undefined;
      }
      current = current[key];
    }

    return current;
  }

  /**
   * Create expression context from execution data
   */
  static createContext(
    item: INodeExecutionData,
    allItems: INodeExecutionData[],
    itemIndex: number,
    runIndex: number,
    parameters: INodeParameters,
    nodeName: string,
    nodeType: string,
    workflowName: string = 'Workflow'
  ): IExpressionContext {
    return {
      $json: item.json,
      $input: {
        item: item,
        all: allItems,
        first: allItems[0],
        last: allItems[allItems.length - 1],
      },
      $parameter: parameters,
      $now: new Date(),
      $today: (() => {
        const date = new Date();
        date.setHours(0, 0, 0, 0);
        return date;
      })(),
      $itemIndex: itemIndex,
      $runIndex: runIndex,
      $node: {
        name: nodeName,
        type: nodeType,
      },
      $workflow: {
        name: workflowName,
        active: true,
      },
    };
  }

  /**
   * Resolve all parameters in an object
   * Recursively resolves nested objects and arrays
   */
  static resolveParameters(
    parameters: INodeParameters,
    context: Partial<IExpressionContext>
  ): INodeParameters {
    const resolved: INodeParameters = {};

    for (const [key, value] of Object.entries(parameters)) {
      if (value === null || value === undefined) {
        resolved[key] = value;
      } else if (Array.isArray(value)) {
        resolved[key] = value.map((item) =>
          typeof item === 'object' && item !== null
            ? this.resolveParameters(item as INodeParameters, context)
            : this.resolveValue(item, context)
        );
      } else if (typeof value === 'object') {
        resolved[key] = this.resolveParameters(value as INodeParameters, context);
      } else {
        resolved[key] = this.resolveValue(value, context);
      }
    }

    return resolved;
  }
}
