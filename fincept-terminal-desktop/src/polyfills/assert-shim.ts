// Browser polyfill for node assert module
export default function assert(value: any, message?: string): asserts value {
  if (!value) {
    throw new Error(message || 'Assertion failed');
  }
}

export const ok = assert;

export function strictEqual(actual: any, expected: any, message?: string): void {
  if (actual !== expected) {
    throw new Error(message || `Expected ${actual} to strictly equal ${expected}`);
  }
}

export function deepStrictEqual(actual: any, expected: any, message?: string): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(message || `Expected ${JSON.stringify(actual)} to deep equal ${JSON.stringify(expected)}`);
  }
}
