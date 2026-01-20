/**
 * CFA Quant Panel - Barrel Export
 *
 * This module provides CFA-level quantitative analytics capabilities.
 *
 * The component has been refactored into smaller, maintainable pieces:
 * - CFAQuantPanel.tsx - Main component
 * - components/ - UI components
 * - hooks/ - Custom React hooks
 * - utils/ - Utility functions
 * - constants.ts - Configuration and color palette
 * - types.ts - TypeScript definitions
 */

export { CFAQuantPanel, default } from './CFAQuantPanel';
export { BB, analysisConfigs } from './constants';
export * from './types';
export * from './hooks';
export * from './utils';
export * from './components';
