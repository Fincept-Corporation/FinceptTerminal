/**
 * Node Editor - Shared Utilities
 */

/** Strip callback functions from node data before serialization */
export function stripCallbacks<T extends Record<string, any>>(nodeData: T): T {
  const cleaned = {} as any;
  for (const [key, value] of Object.entries(nodeData)) {
    if (typeof value !== 'function') {
      cleaned[key] = value;
    }
  }
  return cleaned as T;
}
