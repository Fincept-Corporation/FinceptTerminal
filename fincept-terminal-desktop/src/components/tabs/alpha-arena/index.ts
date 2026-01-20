/**
 * Alpha Arena Tab Module
 *
 * AI Trading Competition where multiple LLM models compete in real-time trading.
 */

// Default export for lazy loading
export { default } from './AlphaArenaTab';

// Main Tab Component
export { default as AlphaArenaTab } from './AlphaArenaTab';

// Types
export * from './types';

// Services - export only the default to avoid duplicate type exports
export { default as alphaArenaService } from './services/alphaArenaService';

// Components
export * from './components';
