/**
 * Critical Global Polyfills Injection
 *
 * This file is injected by esbuild during dependency optimization
 * to ensure globals are available BEFORE any library code runs
 */

import { Buffer as BufferPolyfill } from 'buffer';

// === CRITICAL: Set up globals immediately ===

// 1. Buffer (required by CCXT, crypto libraries)
if (typeof globalThis.Buffer === 'undefined') {
  (globalThis as any).Buffer = BufferPolyfill;
}
if (typeof window !== 'undefined' && typeof (window as any).Buffer === 'undefined') {
  (window as any).Buffer = BufferPolyfill;
}

// 2. Process (required by many Node.js libraries)
if (typeof globalThis.process === 'undefined') {
  (globalThis as any).process = {
    env: { NODE_ENV: import.meta.env.MODE || 'production' },
    version: 'v18.0.0',
    browser: true,
    nextTick: (fn: Function, ...args: any[]) => setTimeout(() => fn(...args), 0),
    cwd: () => '/',
    platform: 'browser',
    argv: [],
    pid: 1,
  };
}
if (typeof window !== 'undefined' && typeof (window as any).process === 'undefined') {
  (window as any).process = (globalThis as any).process;
}

// 3. Global (points to globalThis)
if (typeof globalThis.global === 'undefined') {
  (globalThis as any).global = globalThis;
}
if (typeof window !== 'undefined' && typeof (window as any).global === 'undefined') {
  (window as any).global = globalThis;
}

// 4. Module (required for CommonJS compatibility)
if (typeof globalThis.module === 'undefined') {
  (globalThis as any).module = {
    exports: {},
    require: (id: string) => {
      console.warn(`[Polyfill] module.require('${id}') called - returning empty object`);
      return {};
    },
    id: '.',
    filename: 'index.js',
    loaded: false,
    parent: null,
    children: [],
  };
}
if (typeof window !== 'undefined' && typeof (window as any).module === 'undefined') {
  (window as any).module = (globalThis as any).module;
}

// 5. Exports (for CommonJS modules)
if (typeof globalThis.exports === 'undefined') {
  (globalThis as any).exports = (globalThis as any).module.exports;
}

console.log('[Polyfills] Global injection complete:', {
  Buffer: typeof BufferPolyfill !== 'undefined',
  process: typeof (globalThis as any).process !== 'undefined',
  global: typeof (globalThis as any).global !== 'undefined',
  module: typeof (globalThis as any).module !== 'undefined',
});

// Export to ensure this module executes
export { BufferPolyfill as Buffer };
export const process = (globalThis as any).process;
export const global = (globalThis as any).global;
export const module = (globalThis as any).module;
