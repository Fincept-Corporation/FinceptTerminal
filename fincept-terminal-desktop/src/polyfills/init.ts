// This file MUST be imported first before any other modules
import { Buffer } from 'buffer';

// Create a minimal process shim with all required properties
const processShim: any = {
  version: 'v18.0.0',
  browser: true,
  env: { NODE_ENV: 'development' },
  nextTick: (fn: Function, ...args: any[]) => {
    setTimeout(() => fn(...args), 0);
  },
  cwd: () => '/',
  platform: 'browser',
  argv: [],
  pid: 1
};

// Inject globals immediately BEFORE any imports that might need them
(globalThis as any).Buffer = Buffer;
(globalThis as any).global = globalThis;
(globalThis as any).process = processShim;
(window as any).Buffer = Buffer;
(window as any).process = processShim;

// Export to ensure this module runs
export {};
