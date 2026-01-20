// Main exports for Equity Research module

// Default export
export { default } from './EquityResearchTab';
export { default as EquityResearchTab } from './EquityResearchTab';

// Types
export * from './types';

// Hooks
export { useStockData } from './hooks/useStockData';
export { useSymbolSearch } from './hooks/useSymbolSearch';
export { useNews } from './hooks/useNews';

// Components
export { IndicatorBox } from './components/IndicatorBox';
export { FinancialAnalysisPanel } from './components/FinancialAnalysisPanel';
export { PeerComparisonPanel } from './components/PeerComparisonPanel';

// Tab Components
export { OverviewTab } from './tabs/OverviewTab';
export { FinancialsTab } from './tabs/FinancialsTab';
export { AnalysisTab } from './tabs/AnalysisTab';
export { TechnicalsTab } from './tabs/TechnicalsTab';
export { NewsTab } from './tabs/NewsTab';

// Utilities
export * from './utils/formatters';
