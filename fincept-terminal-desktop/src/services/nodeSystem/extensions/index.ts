/**
 * Extension System
 * Type-based method extensions for expressions
 * Based on n8n's extend() pattern
 */

import type { ExtensionMap } from './types';
import { numberExtensions } from './NumberExtensions';
import { stringExtensions } from './StringExtensions';
import { arrayExtensions } from './ArrayExtensions';
import { objectExtensions } from './ObjectExtensions';

export const EXTENSIONS: ExtensionMap[] = [
  numberExtensions,
  stringExtensions,
  arrayExtensions,
  objectExtensions,
];

/**
 * Extend function - applies extension method to value
 * Usage: extend(value, 'methodName', [args])
 */
export function extend(value: any, methodName: string, args: any[] = []): any {
  const extension = findExtension(value, methodName);

  if (!extension) {
    throw new Error(`Method '${methodName}' not found for type '${typeof value}'`);
  }

  return extension(value, args);
}

/**
 * Find extension function for value type
 */
function findExtension(value: any, methodName: string): ((val: any, args: any[]) => any) | null {
  let extensionMap: ExtensionMap | undefined;

  if (Array.isArray(value)) {
    extensionMap = arrayExtensions;
  } else if (typeof value === 'string') {
    extensionMap = stringExtensions;
  } else if (typeof value === 'number') {
    extensionMap = numberExtensions;
  } else if (value !== null && typeof value === 'object') {
    extensionMap = objectExtensions;
  }

  if (extensionMap && methodName in extensionMap.functions) {
    return extensionMap.functions[methodName];
  }

  // Check if it's a native method
  if (value && typeof value[methodName] === 'function') {
    return (val: any, args: any[]) => val[methodName](...args);
  }

  return null;
}

/**
 * Check if method exists for value type
 */
export function hasExtension(value: any, methodName: string): boolean {
  return findExtension(value, methodName) !== null;
}

/**
 * Get all available methods for a value type
 */
export function getAvailableMethods(value: any): string[] {
  const methods: string[] = [];

  for (const ext of EXTENSIONS) {
    if (
      (ext.typeName === 'array' && Array.isArray(value)) ||
      (ext.typeName === 'string' && typeof value === 'string') ||
      (ext.typeName === 'number' && typeof value === 'number') ||
      (ext.typeName === 'object' && value !== null && typeof value === 'object' && !Array.isArray(value))
    ) {
      methods.push(...Object.keys(ext.functions));
    }
  }

  return methods;
}

export * from './types';
export { numberExtensions, stringExtensions, arrayExtensions, objectExtensions };
