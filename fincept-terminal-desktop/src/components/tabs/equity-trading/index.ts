/**
 * Equity Trading Tab - Main Export
 * Production-grade multi-broker trading system
 */

// Main Component
export { default as EquityTradingTab } from './EquityTradingTab';

// Core Types & Interfaces
export * from './core/types';
export type { PluginContext, PluginResult, Plugin } from './core/PluginSystem';

// Adapters
export { BaseBrokerAdapter } from './adapters/BaseBrokerAdapter';
export { FyersStandardAdapter } from './adapters/FyersStandardAdapter';
export { KiteStandardAdapter } from './adapters/KiteStandardAdapter';

// Core Services
export { brokerOrchestrator } from './core/BrokerOrchestrator';
export { orderRouter, RoutingStrategy } from './core/OrderRouter';
export { pluginManager } from './core/PluginSystem';
export { registerAllAdapters, areAdaptersRegistered } from './core/AdapterRegistry';

// Authentication & Token Management
export { authManager } from './services/AuthManager';
export { tokenRefreshService } from './services/TokenRefreshService';

// Real-time Data Services
export { webSocketManager } from './services/WebSocketManager';
export { dataAggregator } from './services/DataAggregator';
export { historicalDataService } from './services/HistoricalDataService';

// React Hooks
export { useBrokerState } from './hooks/useBrokerState';
export { useOrderExecution } from './hooks/useOrderExecution';

// UI Components
export { default as BrokerConfigTab } from './sub-tabs/BrokerConfigTab';
export { default as ComparisonTab } from './sub-tabs/ComparisonTab';
export { default as BacktestingTab } from './sub-tabs/BacktestingTab';
export { default as PerformanceTab } from './sub-tabs/PerformanceTab';
export { default as TradingInterface } from './ui/TradingInterface';
export { default as OrdersPanel } from './ui/OrdersPanel';
export { default as PositionsPanel } from './ui/PositionsPanel';
export { default as HoldingsPanel } from './ui/HoldingsPanel';
export { default as MarketDepthChart } from './ui/MarketDepthChart';
export { default as TradingChart } from './ui/TradingChart';

// Integrations
export { integrationManager } from './integrations/IntegrationManager';
export { paperTradingIntegration } from './integrations/PaperTradingIntegration';

// Utilities
export { ErrorBoundary } from './utils/ErrorBoundary';
export { loggingService, logger, LogLevel } from './utils/LoggingService';
export { notificationService } from './utils/NotificationService';
export * from './utils/TestHelpers';
