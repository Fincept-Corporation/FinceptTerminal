/**
 * Object Extensions
 * Based on n8n object-extensions
 */

import type { ExtensionMap } from './types';

function keys(value: object): string[] {
  return Object.keys(value);
}

function values(value: object): any[] {
  return Object.values(value);
}

function entries(value: object): Array<[string, any]> {
  return Object.entries(value);
}

function isEmpty(value: object): boolean {
  return Object.keys(value).length === 0;
}

function isNotEmpty(value: object): boolean {
  return Object.keys(value).length > 0;
}

function hasField(value: object, args: any[]): boolean {
  const [field] = args;
  return field in value;
}

function pick(value: object, args: any[]): object {
  const fields = args;
  const result: any = {};
  for (const field of fields) {
    if (field in value) {
      result[field] = (value as any)[field];
    }
  }
  return result;
}

function omit(value: object, args: any[]): object {
  const fields = args;
  const result: any = { ...value };
  for (const field of fields) {
    delete result[field];
  }
  return result;
}

export const objectExtensions: ExtensionMap = {
  typeName: 'object',
  functions: {
    keys,
    values,
    entries,
    isEmpty,
    isNotEmpty,
    hasField,
    pick,
    omit,
  },
};
