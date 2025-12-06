/**
 * WebSocket Shim for Browser
 *
 * Replaces Node.js 'ws' module with native browser WebSocket
 * This is needed because many Node.js libraries (ccxt, openai, langchain)
 * import 'ws' which doesn't work in browsers.
 */

// Get native browser WebSocket
const NativeWebSocket = typeof globalThis.WebSocket !== 'undefined'
  ? globalThis.WebSocket
  : class MockWebSocket {};

// Export native browser WebSocket as default
export default NativeWebSocket;

// Also export as named export for compatibility
export const WebSocket = NativeWebSocket;

// Export WebSocketServer as a no-op (not used in browser)
export class WebSocketServer {
  constructor() {
    console.warn('WebSocketServer is not available in browser environment');
  }
}
