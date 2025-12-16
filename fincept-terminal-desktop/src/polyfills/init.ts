// This file MUST be imported first before any other modules
import { Buffer } from 'buffer';

// Create a minimal process shim with all required properties
const processShim: any = {
  version: 'v18.0.0',
  browser: true,
  env: {
    NODE_ENV: typeof process !== 'undefined' && process.env ? process.env.NODE_ENV : 'production'
  },
  nextTick: (fn: Function, ...args: any[]) => {
    setTimeout(() => fn(...args), 0);
  },
  cwd: () => '/',
  platform: 'browser',
  argv: [],
  pid: 1
};

// Create minimal module shim for CommonJS compatibility
const moduleShim: any = {
  exports: {},
  require: (id: string) => {
    console.warn(`module.require('${id}') called in browser - returning empty object`);
    return {};
  },
  id: '.',
  filename: 'index.js',
  loaded: false,
  parent: null,
  children: []
};

// Inject globals immediately BEFORE any imports that might need them
// Use Object.defineProperty to make them non-configurable and prevent Vite from removing them
if (typeof (globalThis as any).Buffer === 'undefined') {
  Object.defineProperty(globalThis, 'Buffer', {
    value: Buffer,
    writable: false,
    configurable: false
  });
}

if (typeof (globalThis as any).global === 'undefined') {
  Object.defineProperty(globalThis, 'global', {
    value: globalThis,
    writable: false,
    configurable: false
  });
}

if (typeof (globalThis as any).process === 'undefined') {
  Object.defineProperty(globalThis, 'process', {
    value: processShim,
    writable: false,
    configurable: false
  });
}

if (typeof (globalThis as any).module === 'undefined') {
  Object.defineProperty(globalThis, 'module', {
    value: moduleShim,
    writable: false,
    configurable: false
  });
}

// Also set on window for compatibility
if (typeof window !== 'undefined') {
  if (typeof (window as any).Buffer === 'undefined') {
    (window as any).Buffer = Buffer;
  }
  if (typeof (window as any).process === 'undefined') {
    (window as any).process = processShim;
  }
  if (typeof (window as any).module === 'undefined') {
    (window as any).module = moduleShim;
  }
  if (typeof (window as any).global === 'undefined') {
    (window as any).global = globalThis;
  }
}

console.log('[Polyfills] Initialized: Buffer, global, process, module');

// Export to ensure this module runs
export {};
