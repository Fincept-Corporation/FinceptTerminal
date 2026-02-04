/**
 * Per-tab state types for workspace save/restore.
 * Only simple, non-sensitive configuration values are captured.
 */

export interface ScreenerTabState {
  seriesIds: string;
  startDate: string;
  endDate: string;
  chartType: 'line' | 'area';
  normalizeData: boolean;
}

export interface WatchlistTabState {
  activeWatchlistName: string | null;
  sortBy: string;
}

export interface MarketsTabState {
  autoUpdate: boolean;
  updateInterval: number;
}

export interface NodeEditorTabState {
  workflowName: string;
  activeView: 'editor' | 'workflows';
}

export interface AIQuantLabTabState {
  activeView: string;
  isLeftPanelMinimized: boolean;
  isRightPanelMinimized: boolean;
}

export interface EquityTradingTabState {
  selectedSymbol: string;
  selectedExchange: string;
  chartInterval: string;
  leftSidebarView: string;
  centerView: string;
  rightPanelView: string;
  activeBottomTab: string;
  isBottomPanelMinimized: boolean;
}

export interface ChatTabState {
  activeProvider: string | null;
  activeModel: string | null;
  temperature: number;
  maxTokens: number;
}

export interface DataSourcesTabState {
  view: 'gallery' | 'connections';
  selectedCategory: string;
}

export interface PortfolioTabState {
  activeSubTab: string;
}

export interface DashboardTabState {
  widgetLayoutKey: string;
}

export interface EconomicsTabState {
  dataSource: string;
  view: string;
  selectedIndicator: string;
  selectedCountry: string;
  selectedTransform: string;
  bisDataType: string;
  profileCountry: string;
}

export interface NewsTabState {
  activeFilter: string;
  feedCount: number;
  refreshInterval: number;
  searchQuery: string;
}

export interface GeopoliticsTabState {
  countryFilter: string;
  categoryFilter: string;
  isPanelCollapsed: boolean;
}

export interface EquityResearchTabState {
  currentSymbol: string;
  chartPeriod: string;
  activeTab: string;
}

export interface CodeEditorTabState {
  activeFileId: string;
  showExplorer: boolean;
  showOutput: boolean;
}

export interface BacktestingTabState {
  selectedProvider: string;
  activeCommand: string;
  selectedCategory: string;
  selectedStrategy: string;
  symbols: string;
  startDate: string;
  endDate: string;
  initialCapital: number;
  commission: number;
}

export interface CryptoTradingTabState {
  selectedSymbol: string;
  selectedView: string;
  rightPanelView: string;
  leftSidebarView: string;
  activeBottomTab: string;
  isBottomPanelMinimized: boolean;
}

export interface NotesTabState {
  isRightPanelMinimized: boolean;
}

export interface ReportBuilderTabState {
  pageTheme: string;
  fontFamily: string;
  defaultFontSize: number;
  rightPanelView: string;
}

export interface DerivativesTabState {
  activeInstrument: string;
}

export interface MAAnalyticsTabState {
  activeTab: string;
}

export interface AlternativeInvestmentsTabState {
  activeCategory: string;
  isLeftPanelMinimized: boolean;
}

export interface AsiaMarketsTabState {
  activeRegion: string;
  viewMode: string;
  isBottomPanelMinimized: boolean;
  activeBottomTab: string;
}

export interface SurfaceAnalyticsTabState {
  activeChart: string;
}

/** Union of all tab state types */
export type TabState =
  | ScreenerTabState
  | WatchlistTabState
  | MarketsTabState
  | NodeEditorTabState
  | AIQuantLabTabState
  | EquityTradingTabState
  | ChatTabState
  | DataSourcesTabState
  | PortfolioTabState
  | DashboardTabState
  | EconomicsTabState
  | NewsTabState
  | GeopoliticsTabState
  | EquityResearchTabState
  | CodeEditorTabState
  | BacktestingTabState
  | CryptoTradingTabState
  | NotesTabState
  | ReportBuilderTabState
  | DerivativesTabState
  | MAAnalyticsTabState
  | AlternativeInvestmentsTabState
  | AsiaMarketsTabState
  | SurfaceAnalyticsTabState;
