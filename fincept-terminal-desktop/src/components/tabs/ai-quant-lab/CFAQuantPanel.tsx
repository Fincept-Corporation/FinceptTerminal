/**
 * CFA Quant Panel
 *
 * This file re-exports the CFAQuantPanel from its new modular location.
 * The component has been refactored into smaller, maintainable pieces.
 *
 * See ./cfa-quant/ for:
 * - CFAQuantPanel.tsx - Main component
 * - components/ - UI components (Header, DataStep, AnalysisStep, ResultsStep, etc.)
 * - hooks/ - Custom hooks (useChartZoom, usePriceData)
 * - utils/ - Utility functions (formatters, calculations, metricsExtractor, chartDataBuilder)
 * - constants.ts - Color palette (BB) and analysis configurations
 * - types.ts - TypeScript interfaces
 */

export { CFAQuantPanel, default } from './cfa-quant';
