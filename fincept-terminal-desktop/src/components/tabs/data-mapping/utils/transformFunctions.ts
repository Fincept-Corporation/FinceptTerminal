// Transformation Functions Library

import { TransformDefinition, TransformFunction } from '../types';

// ========== TYPE CONVERSIONS ==========

const toNumber: TransformDefinition = {
  name: 'toNumber',
  description: 'Convert value to number',
  category: 'type',
  examples: [
    { input: '123.45', output: 123.45 },
    { input: '1,234.56', output: 1234.56 },
  ],
  implementation: (value: any) => {
    if (typeof value === 'number') return value;
    if (typeof value === 'string') {
      // Remove commas and parse
      const cleaned = value.replace(/,/g, '');
      const parsed = parseFloat(cleaned);
      return isNaN(parsed) ? null : parsed;
    }
    return null;
  },
};

const toString: TransformDefinition = {
  name: 'toString',
  description: 'Convert value to string',
  category: 'type',
  examples: [
    { input: 123, output: '123' },
    { input: true, output: 'true' },
  ],
  implementation: (value: any) => String(value),
};

const toBoolean: TransformDefinition = {
  name: 'toBoolean',
  description: 'Convert value to boolean',
  category: 'type',
  examples: [
    { input: 'true', output: true },
    { input: 1, output: true },
    { input: 0, output: false },
  ],
  implementation: (value: any) => Boolean(value),
};

// ========== DATE/TIME CONVERSIONS ==========

const toISODate: TransformDefinition = {
  name: 'toISODate',
  description: 'Convert to ISO 8601 date string',
  category: 'date',
  examples: [
    { input: 1696176000000, output: '2023-10-01T12:00:00.000Z' },
    { input: '2023-10-01', output: '2023-10-01T00:00:00.000Z' },
  ],
  implementation: (value: any) => {
    try {
      return new Date(value).toISOString();
    } catch {
      return null;
    }
  },
};

const fromUnixTimestamp: TransformDefinition = {
  name: 'fromUnixTimestamp',
  description: 'Convert Unix timestamp (seconds) to ISO date',
  category: 'date',
  examples: [
    { input: 1696176000, output: '2023-10-01T12:00:00.000Z' },
  ],
  implementation: (value: number) => {
    try {
      return new Date(value * 1000).toISOString();
    } catch {
      return null;
    }
  },
};

const fromUnixTimestampMs: TransformDefinition = {
  name: 'fromUnixTimestampMs',
  description: 'Convert Unix timestamp (milliseconds) to ISO date',
  category: 'date',
  examples: [
    { input: 1696176000000, output: '2023-10-01T12:00:00.000Z' },
  ],
  implementation: (value: number) => {
    try {
      return new Date(value).toISOString();
    } catch {
      return null;
    }
  },
};

const toUnixTimestamp: TransformDefinition = {
  name: 'toUnixTimestamp',
  description: 'Convert date to Unix timestamp (seconds)',
  category: 'date',
  examples: [
    { input: '2023-10-01T12:00:00.000Z', output: 1696176000 },
  ],
  implementation: (value: any) => {
    try {
      return Math.floor(new Date(value).getTime() / 1000);
    } catch {
      return null;
    }
  },
};

// ========== MATH OPERATIONS ==========

const round: TransformDefinition = {
  name: 'round',
  description: 'Round number to specified decimal places',
  category: 'math',
  parameters: [
    { name: 'decimals', type: 'number', required: false, defaultValue: 2 },
  ],
  examples: [
    { input: 123.456, output: 123.46, params: { decimals: 2 } },
    { input: 123.456, output: 123, params: { decimals: 0 } },
  ],
  implementation: (value: number, context?: any) => {
    const decimals = context?.decimals ?? 2;
    const multiplier = Math.pow(10, decimals);
    return Math.round(value * multiplier) / multiplier;
  },
};

const multiply: TransformDefinition = {
  name: 'multiply',
  description: 'Multiply by a constant',
  category: 'math',
  parameters: [
    { name: 'multiplier', type: 'number', required: true },
  ],
  examples: [
    { input: 10, output: 100, params: { multiplier: 10 } },
  ],
  implementation: (value: number, context?: any) => {
    return value * (context?.multiplier ?? 1);
  },
};

const divide: TransformDefinition = {
  name: 'divide',
  description: 'Divide by a constant',
  category: 'math',
  parameters: [
    { name: 'divisor', type: 'number', required: true },
  ],
  examples: [
    { input: 100, output: 10, params: { divisor: 10 } },
  ],
  implementation: (value: number, context?: any) => {
    const divisor = context?.divisor ?? 1;
    return divisor !== 0 ? value / divisor : null;
  },
};

const add: TransformDefinition = {
  name: 'add',
  description: 'Add a constant',
  category: 'math',
  parameters: [
    { name: 'addend', type: 'number', required: true },
  ],
  examples: [
    { input: 10, output: 15, params: { addend: 5 } },
  ],
  implementation: (value: number, context?: any) => {
    return value + (context?.addend ?? 0);
  },
};

const subtract: TransformDefinition = {
  name: 'subtract',
  description: 'Subtract a constant',
  category: 'math',
  parameters: [
    { name: 'subtrahend', type: 'number', required: true },
  ],
  examples: [
    { input: 10, output: 5, params: { subtrahend: 5 } },
  ],
  implementation: (value: number, context?: any) => {
    return value - (context?.subtrahend ?? 0);
  },
};

// ========== STRING OPERATIONS ==========

const uppercase: TransformDefinition = {
  name: 'uppercase',
  description: 'Convert string to uppercase',
  category: 'string',
  examples: [
    { input: 'hello', output: 'HELLO' },
  ],
  implementation: (value: string) => value.toUpperCase(),
};

const lowercase: TransformDefinition = {
  name: 'lowercase',
  description: 'Convert string to lowercase',
  category: 'string',
  examples: [
    { input: 'HELLO', output: 'hello' },
  ],
  implementation: (value: string) => value.toLowerCase(),
};

const trim: TransformDefinition = {
  name: 'trim',
  description: 'Remove leading and trailing whitespace',
  category: 'string',
  examples: [
    { input: '  hello  ', output: 'hello' },
  ],
  implementation: (value: string) => value.trim(),
};

const replace: TransformDefinition = {
  name: 'replace',
  description: 'Replace text in string',
  category: 'string',
  parameters: [
    { name: 'find', type: 'string', required: true },
    { name: 'replace', type: 'string', required: true },
  ],
  examples: [
    { input: 'NSE:SBIN', output: 'SBIN', params: { find: 'NSE:', replace: '' } },
  ],
  implementation: (value: string, context?: any) => {
    const find = context?.find ?? '';
    const replaceWith = context?.replace ?? '';
    return value.replace(new RegExp(find, 'g'), replaceWith);
  },
};

const substring: TransformDefinition = {
  name: 'substring',
  description: 'Extract substring',
  category: 'string',
  parameters: [
    { name: 'start', type: 'number', required: true },
    { name: 'end', type: 'number', required: false },
  ],
  examples: [
    { input: 'HELLO', output: 'ELL', params: { start: 1, end: 4 } },
  ],
  implementation: (value: string, context?: any) => {
    const start = context?.start ?? 0;
    const end = context?.end;
    return value.substring(start, end);
  },
};

// ========== FINANCIAL OPERATIONS ==========

const bpsToPercent: TransformDefinition = {
  name: 'bpsToPercent',
  description: 'Convert basis points to percentage',
  category: 'math',
  examples: [
    { input: 250, output: 2.5 },
  ],
  implementation: (value: number) => value / 100,
};

const percentToBps: TransformDefinition = {
  name: 'percentToBps',
  description: 'Convert percentage to basis points',
  category: 'math',
  examples: [
    { input: 2.5, output: 250 },
  ],
  implementation: (value: number) => value * 100,
};

const roundPrice: TransformDefinition = {
  name: 'roundPrice',
  description: 'Round price to 2 decimal places',
  category: 'format',
  examples: [
    { input: 123.456789, output: 123.46 },
  ],
  implementation: (value: number) => Math.round(value * 100) / 100,
};

// ========== REGISTRY ==========

export const TRANSFORM_FUNCTIONS: Record<string, TransformDefinition> = {
  // Type conversions
  toNumber,
  toString,
  toBoolean,

  // Date/Time
  toISODate,
  fromUnixTimestamp,
  fromUnixTimestampMs,
  toUnixTimestamp,

  // Math
  round,
  multiply,
  divide,
  add,
  subtract,

  // String
  uppercase,
  lowercase,
  trim,
  replace,
  substring,

  // Financial
  bpsToPercent,
  percentToBps,
  roundPrice,
};

/**
 * Get transform function by name
 */
export function getTransform(name: string): TransformFunction | null {
  const definition = TRANSFORM_FUNCTIONS[name];
  return definition ? definition.implementation : null;
}

/**
 * Apply transform to value
 */
export function applyTransform(value: any, transformName: string, params?: any): any {
  const transform = getTransform(transformName);
  if (!transform) {
    throw new Error(`Transform function not found: ${transformName}`);
  }
  return transform(value, params);
}

/**
 * Get all transforms by category
 */
export function getTransformsByCategory(category: TransformDefinition['category']): TransformDefinition[] {
  return Object.values(TRANSFORM_FUNCTIONS).filter(t => t.category === category);
}

/**
 * Get transform names
 */
export function getTransformNames(): string[] {
  return Object.keys(TRANSFORM_FUNCTIONS);
}
