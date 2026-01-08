/**
 * String Extensions
 * Based on n8n string-extensions
 */

import type { ExtensionMap } from './types';

function length(value: string): number {
  return value.length;
}

function isEmpty(value: string): boolean {
  return value.length === 0;
}

function isNotEmpty(value: string): boolean {
  return value.length > 0;
}

function toLowerCase(value: string): string {
  return value.toLowerCase();
}

function toUpperCase(value: string): string {
  return value.toUpperCase();
}

function trim(value: string): string {
  return value.trim();
}

function split(value: string, args: any[]): string[] {
  const [separator = ','] = args;
  return value.split(separator);
}

function substring(value: string, args: any[]): string {
  const [start, end] = args;
  return value.substring(start, end);
}

function replace(value: string, args: any[]): string {
  const [search, replacement] = args;
  if (typeof search === 'string') {
    return value.replace(search, replacement);
  }
  return value.replace(new RegExp(search), replacement);
}

function replaceAll(value: string, args: any[]): string {
  const [search, replacement] = args;
  return value.split(search).join(replacement);
}

function includes(value: string, args: any[]): boolean {
  const [search] = args;
  return value.includes(search);
}

function startsWith(value: string, args: any[]): boolean {
  const [search] = args;
  return value.startsWith(search);
}

function endsWith(value: string, args: any[]): boolean {
  const [search] = args;
  return value.endsWith(search);
}

// Finance-specific
function toSymbol(value: string): string {
  return value.toUpperCase().replace(/[^A-Z]/g, '');
}

function isTicker(value: string): boolean {
  return /^[A-Z]{1,5}$/.test(value);
}

export const stringExtensions: ExtensionMap = {
  typeName: 'string',
  functions: {
    length,
    isEmpty,
    isNotEmpty,
    toLowerCase,
    toUpperCase,
    trim,
    split,
    substring,
    replace,
    replaceAll,
    includes,
    startsWith,
    endsWith,
    toSymbol,
    isTicker,
  },
};
