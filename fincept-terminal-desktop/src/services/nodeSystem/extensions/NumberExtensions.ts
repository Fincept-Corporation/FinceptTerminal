/**
 * Number Extensions
 * Based on n8n number-extensions
 */

import type { ExtensionMap } from './types';

function round(value: number, args: any[]): number {
  const [decimalPlaces = 0] = args;
  return +value.toFixed(decimalPlaces);
}

function floor(value: number): number {
  return Math.floor(value);
}

function ceil(value: number): number {
  return Math.ceil(value);
}

function abs(value: number): number {
  return Math.abs(value);
}

function isEven(value: number): boolean {
  if (!Number.isInteger(value)) {
    throw new Error('isEven() only works on integers');
  }
  return value % 2 === 0;
}

function isOdd(value: number): boolean {
  if (!Number.isInteger(value)) {
    throw new Error('isOdd() only works on integers');
  }
  return Math.abs(value) % 2 === 1;
}

function format(value: number, args: any[]): string {
  const [locales = 'en-US', config = {}] = args;
  return new Intl.NumberFormat(locales, config).format(value);
}

// Finance-specific extensions
function toPercent(value: number, args: any[]): string {
  const [decimals = 2] = args;
  return (value * 100).toFixed(decimals) + '%';
}

function toCurrency(value: number, args: any[]): string {
  const [currency = 'USD', locale = 'en-US'] = args;
  return new Intl.NumberFormat(locale, { style: 'currency', currency }).format(value);
}

function toBps(value: number): number {
  return value * 10000;
}

function fromBps(value: number): number {
  return value / 10000;
}

export const numberExtensions: ExtensionMap = {
  typeName: 'number',
  functions: {
    round,
    floor,
    ceil,
    abs,
    isEven,
    isOdd,
    format,
    toPercent,
    toCurrency,
    toBps,
    fromBps,
  },
};
