/**
 * Array Extensions
 * Based on n8n array-extensions
 */

import type { ExtensionMap } from './types';

function first(value: any[]): any {
  return value[0];
}

function last(value: any[]): any {
  return value[value.length - 1];
}

function isEmpty(value: any[]): boolean {
  return value.length === 0;
}

function isNotEmpty(value: any[]): boolean {
  return value.length > 0;
}

function length(value: any[]): number {
  return value.length;
}

function sum(value: any[]): number {
  return value.reduce((acc, curr) => {
    const num = typeof curr === 'string' ? parseFloat(curr) : curr;
    return acc + (typeof num === 'number' ? num : 0);
  }, 0);
}

function min(value: any[]): number {
  const numbers = value.map(v => typeof v === 'string' ? parseFloat(v) : v);
  return Math.min(...numbers);
}

function max(value: any[]): number {
  const numbers = value.map(v => typeof v === 'string' ? parseFloat(v) : v);
  return Math.max(...numbers);
}

function average(value: any[]): number {
  if (value.length === 0) return 0;
  return sum(value) / value.length;
}

function unique(value: any[]): any[] {
  return [...new Set(value)];
}

function pluck(value: any[], args: any[]): any[] {
  const [field] = args;
  if (!field) return value;
  return value.map(item =>
    item && typeof item === 'object' ? item[field] : undefined
  ).filter(v => v !== undefined);
}

function filter(value: any[], args: any[]): any[] {
  const [predicate] = args;
  if (typeof predicate === 'function') {
    return value.filter(predicate);
  }
  return value;
}

function map(value: any[], args: any[]): any[] {
  const [mapper] = args;
  if (typeof mapper === 'function') {
    return value.map(mapper);
  }
  return value;
}

// Finance-specific
function returns(value: number[]): number[] {
  if (value.length < 2) return [];
  const result: number[] = [];
  for (let i = 1; i < value.length; i++) {
    result.push((value[i] - value[i - 1]) / value[i - 1]);
  }
  return result;
}

function volatility(value: number[]): number {
  if (value.length < 2) return 0;
  const avg = average(value);
  const squaredDiffs = value.map(v => Math.pow(v - avg, 2));
  const variance = sum(squaredDiffs) / value.length;
  return Math.sqrt(variance);
}

export const arrayExtensions: ExtensionMap = {
  typeName: 'array',
  functions: {
    first,
    last,
    isEmpty,
    isNotEmpty,
    length,
    sum,
    min,
    max,
    average,
    unique,
    pluck,
    filter,
    map,
    returns,
    volatility,
  },
};
