/**
 * Expression Engine - Based on n8n's core expression system
 * Evaluates expressions with finance-specific extensions
 */

import type {
  IDataObject,
  INodeExecutionData,
  INodeParameters,
  NodeParameterValue,
} from './types';
import { extend, hasExtension } from './extensions';

/**
 * Check if a value is an expression (starts with =)
 */
export function isExpression(value: any): boolean {
  if (typeof value !== 'string') return false;
  return value.charAt(0) === '=';
}

/**
 * Check if value has template syntax {{...}}
 */
export function hasExpressionSyntax(value: any): boolean {
  if (typeof value !== 'string') return false;
  return /\{\{.+?\}\}/.test(value);
}

/**
 * Expression context - matches n8n's WorkflowDataProxy structure
 */
export interface IExpressionContext {
  $json: IDataObject;
  $input: {
    item: INodeExecutionData;
    all: INodeExecutionData[];
    first: INodeExecutionData;
    last: INodeExecutionData;
  };
  $parameter: INodeParameters;
  $now: Date;
  $today: Date;
  $itemIndex: number;
  $runIndex: number;
  $node: { name: string; type: string; };
  $workflow: { name: string; active: boolean; id?: string; };
  // Finance-specific helpers
  $helpers: ReturnType<typeof createFinanceHelpers>;
}

/**
 * Finance-specific helper functions
 */
function createFinanceHelpers() {
  return {
    formatCurrency: (value: number, currency: string = 'USD'): string => {
      return new Intl.NumberFormat('en-US', { style: 'currency', currency }).format(value);
    },
    calculateReturn: (oldValue: number, newValue: number): number => {
      return ((newValue - oldValue) / oldValue) * 100;
    },
    validateSymbol: (symbol: string): boolean => {
      return /^[A-Z]{1,5}$/.test(symbol);
    },
    parseTimestamp: (timestamp: number | string): Date => {
      return typeof timestamp === 'number' ? new Date(timestamp) : new Date(timestamp);
    },
  };
}

/**
 * Expression Engine
 * Evaluates expressions with access to workflow data
 */
export class ExpressionEngine {
  /**
   * Resolve parameter value - matches n8n's resolveSimpleParameterValue
   */
  static resolveValue(
    value: NodeParameterValue,
    context: Partial<IExpressionContext>
  ): NodeParameterValue {
    // Not an expression - return as-is
    if (!isExpression(value) && !hasExpressionSyntax(value)) {
      return value;
    }

    if (typeof value !== 'string') {
      return value;
    }

    try {
      // Handle = prefix (entire value is expression)
      if (value.startsWith('=')) {
        return this.evaluateExpression(value.substring(1).trim(), context);
      }

      // Handle {{...}} template syntax
      return this.replaceTemplates(value, context);
    } catch (error) {
      console.error('[ExpressionEngine] Evaluation error:', error);
      return value;
    }
  }

  /**
   * Replace {{...}} templates in string
   */
  private static replaceTemplates(
    str: string,
    context: Partial<IExpressionContext>
  ): string {
    return str.replace(/\{\{(.+?)\}\}/g, (match, expression) => {
      try {
        const result = this.evaluateExpression(expression.trim(), context);
        return String(result ?? '');
      } catch (error) {
        console.warn('[ExpressionEngine] Template eval failed:', expression, error);
        return match;
      }
    });
  }

  /**
   * Evaluate expression - simplified special vars + JS eval
   */
  private static evaluateExpression(
    expression: string,
    context: Partial<IExpressionContext>
  ): any {
    // Special variables
    const specialVars: Record<string, any> = {
      '$now': context.$now || new Date(),
      '$today': (() => { const d = new Date(); d.setHours(0, 0, 0, 0); return d; })(),
      '$itemIndex': context.$itemIndex ?? 0,
      '$runIndex': context.$runIndex ?? 0,
    };

    if (expression in specialVars) {
      return specialVars[expression];
    }

    // Property access
    const propertyAccessors: Array<[string, () => any]> = [
      ['$json.', () => this.getNestedValue(context.$json || {}, expression.substring(6))],
      ['$input.item.json.', () => this.getNestedValue(context.$input?.item?.json || {}, expression.substring(17))],
      ['$input.first.json.', () => this.getNestedValue(context.$input?.first?.json || {}, expression.substring(18))],
      ['$input.last.json.', () => this.getNestedValue(context.$input?.last?.json || {}, expression.substring(17))],
      ['$parameter.', () => (context.$parameter || {})[expression.substring(11)]],
      ['$node.', () => (context.$node as any)?.[expression.substring(6)]],
      ['$workflow.', () => (context.$workflow as any)?.[expression.substring(10)]],
      ['$helpers.', () => (context.$helpers as any)?.[expression.substring(9)]],
    ];

    for (const [prefix, accessor] of propertyAccessors) {
      if (expression.startsWith(prefix)) {
        return accessor();
      }
    }

    // Bracket notation: $json["field"]
    const bracketMatch = expression.match(/\$json\["(.+?)"\]/);
    if (bracketMatch) {
      return (context.$json || {})[bracketMatch[1]];
    }

    // Complex JavaScript evaluation
    return this.evaluateJavaScript(expression, context);
  }

  /**
   * Evaluate JavaScript - n8n-style sandbox with allowlist/denylist
   */
  private static evaluateJavaScript(
    expression: string,
    context: Partial<IExpressionContext>
  ): any {
    try {
      // Build execution data (allowlist pattern from n8n)
      const data: any = {
        // Workflow data
        $json: context.$json || {},
        $input: context.$input,
        $parameter: context.$parameter || {},
        $now: context.$now || new Date(),
        $today: (() => { const d = new Date(); d.setHours(0, 0, 0, 0); return d; })(),
        $itemIndex: context.$itemIndex ?? 0,
        $runIndex: context.$runIndex ?? 0,
        $node: context.$node,
        $workflow: context.$workflow,
        $helpers: context.$helpers || createFinanceHelpers(),

        // Extension system
        extend: extend,

        // Denylist (security)
        eval: {},
        Function: {},
        setTimeout: {},
        setInterval: {},
        fetch: {},
        XMLHttpRequest: {},
        Promise: {},
        alert: {},
        confirm: {},
        prompt: {},

        // Allowlist (safe globals)
        Date: Date,
        Math: Math,
        String: String,
        Number: Number,
        Array: Array,
        Object: Object,
        Boolean: Boolean,
        JSON: JSON,
        parseInt: parseInt,
        parseFloat: parseFloat,
        isNaN: isNaN,
        isFinite: isFinite,
      };

      // Constructor validation (n8n security check)
      if (/\.\s*constructor/gm.test(expression)) {
        throw new Error('Constructor access not allowed');
      }

      // Evaluate with Function constructor (sandboxed scope)
      const paramNames = Object.keys(data);
      const paramValues = Object.values(data);
      const func = new Function(...paramNames, `'use strict'; return (${expression});`);

      return func(...paramValues);
    } catch (error) {
      console.error('[ExpressionEngine] JS eval error:', error);
      throw error;
    }
  }

  /**
   * Get nested value via dot notation
   */
  private static getNestedValue(obj: IDataObject, path: string): any {
    const keys = path.split('.');
    let current: any = obj;

    for (const key of keys) {
      if (current === null || current === undefined) return undefined;
      current = current[key];
    }

    return current;
  }

  /**
   * Create expression context - matches n8n WorkflowDataProxy
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
    const today = new Date();
    today.setHours(0, 0, 0, 0);

    return {
      $json: item.json,
      $input: {
        item,
        all: allItems,
        first: allItems[0],
        last: allItems[allItems.length - 1],
      },
      $parameter: parameters,
      $now: new Date(),
      $today: today,
      $itemIndex: itemIndex,
      $runIndex: runIndex,
      $node: { name: nodeName, type: nodeType },
      $workflow: { name: workflowName, active: true },
      $helpers: createFinanceHelpers(),
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
