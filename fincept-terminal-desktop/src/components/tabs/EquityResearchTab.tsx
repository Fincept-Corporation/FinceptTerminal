/**
 * EquityResearchTab - Re-export from modularized folder
 *
 * This file has been refactored into smaller components.
 * The implementation is now in the equity-research folder:
 *
 * - equity-research/
 *   - EquityResearchTab.tsx   - Main tab component
 *   - types/index.ts          - Type definitions
 *   - hooks/                   - Custom hooks (useStockData, useSymbolSearch, useNews)
 *   - components/              - Reusable components (IndicatorBox, etc.)
 *   - tabs/                    - Tab content components (Overview, Financials, etc.)
 *   - utils/                   - Utility functions (formatters)
 *   - index.ts                 - Main exports
 */

export { default } from './equity-research/EquityResearchTab';
