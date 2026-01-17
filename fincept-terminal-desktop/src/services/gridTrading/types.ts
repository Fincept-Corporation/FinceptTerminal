/**
 * Grid Trading Types
 *
 * Unified types for grid trading that work with both:
 * - Crypto exchanges (via IExchangeAdapter)
 * - Stock brokers (via IStockBrokerAdapter)
 */

// ============================================================================
// GRID CONFIGURATION TYPES
// ============================================================================

export type GridType =
  | 'arithmetic'    // Equal price gaps between levels
  | 'geometric';    // Equal percentage gaps between levels

export type GridDirection = 'long' | 'short' | 'neutral';

export type GridStatus =
  | 'draft'         // Not started, being configured
  | 'running'       // Active and placing orders
  | 'paused'        // Temporarily stopped
  | 'stopped'       // Manually stopped
  | 'completed'     // All grid levels filled/closed
  | 'error';        // Error state

export type BrokerType = 'crypto' | 'stock';

// ============================================================================
// GRID CONFIGURATION
// ============================================================================

export interface GridConfig {
  // Core grid parameters
  id: string;                           // Unique grid ID
  symbol: string;                       // Trading pair/symbol
  exchange?: string;                    // Exchange for stocks (NSE, BSE, etc.)

  // Price range
  upperPrice: number;                   // Upper bound of grid
  lowerPrice: number;                   // Lower bound of grid
  gridLevels: number;                   // Number of grid lines
  gridType: GridType;                   // Arithmetic or geometric

  // Investment
  totalInvestment: number;              // Total amount to invest
  quantityPerGrid: number;              // Quantity per grid level

  // Direction and limits
  direction: GridDirection;             // Long/Short/Neutral bias
  stopLoss?: number;                    // Stop loss price
  takeProfit?: number;                  // Take profit price

  // Broker configuration
  brokerType: BrokerType;               // crypto or stock
  brokerId: string;                     // Specific broker ID
  productType?: string;                 // For stocks: CASH, INTRADAY, MARGIN

  // Advanced options
  triggerPrice?: number;                // Price to start the grid
  trailingUp?: boolean;                 // Trail upper bound up
  trailingDown?: boolean;               // Trail lower bound down
  rebalanceOnTrend?: boolean;           // Rebalance when price breaks range
}

// ============================================================================
// GRID LEVEL & STATE
// ============================================================================

export interface GridLevel {
  levelIndex: number;                   // 0-based level index
  price: number;                        // Price at this level
  side: 'buy' | 'sell';                 // Order side
  quantity: number;                     // Order quantity
  status: GridLevelStatus;              // Current status
  orderId?: string;                     // Exchange order ID if placed
  filledQuantity?: number;              // Filled quantity
  filledPrice?: number;                 // Average fill price
  pnl?: number;                         // P&L for this level
  createdAt?: number;                   // Timestamp when order was placed
  filledAt?: number;                    // Timestamp when order was filled
}

export type GridLevelStatus =
  | 'pending'       // Waiting to be placed
  | 'open'          // Order placed, waiting to fill
  | 'filled'        // Order filled
  | 'cancelled'     // Order cancelled
  | 'error';        // Error placing/monitoring order

// ============================================================================
// GRID STATE
// ============================================================================

export interface GridState {
  config: GridConfig;                   // Grid configuration
  levels: GridLevel[];                  // All grid levels
  status: GridStatus;                   // Current grid status
  currentPrice: number;                 // Current market price

  // Statistics
  totalBuys: number;                    // Total buy orders filled
  totalSells: number;                   // Total sell orders filled
  realizedPnl: number;                  // Realized profit/loss
  unrealizedPnl: number;                // Unrealized profit/loss
  totalPnl: number;                     // Total P&L
  gridProfit: number;                   // Profit from grid trades

  // Position
  currentPosition: number;              // Current net position
  averageEntryPrice: number;            // Average entry price

  // Timestamps
  createdAt: number;                    // Grid creation time
  startedAt?: number;                   // Grid start time
  lastUpdatedAt: number;                // Last update time

  // Error tracking
  lastError?: string;                   // Last error message
  errorCount: number;                   // Total error count
}

// ============================================================================
// GRID TRADING SERVICE INTERFACE
// ============================================================================

export interface IGridTradingService {
  // Grid lifecycle
  createGrid(config: GridConfig): Promise<GridState>;
  startGrid(gridId: string): Promise<void>;
  pauseGrid(gridId: string): Promise<void>;
  resumeGrid(gridId: string): Promise<void>;
  stopGrid(gridId: string): Promise<void>;
  deleteGrid(gridId: string): Promise<void>;

  // Grid management
  getGrid(gridId: string): GridState | undefined;
  getAllGrids(): GridState[];
  getActiveGrids(): GridState[];

  // Real-time updates
  updateGridPrice(gridId: string, price: number): Promise<void>;
  processFilledOrder(gridId: string, orderId: string, filledQty: number, filledPrice: number): Promise<void>;

  // Events
  onGridUpdate(callback: (grid: GridState) => void): () => void;
  onOrderFilled(callback: (gridId: string, level: GridLevel) => void): () => void;
  onGridError(callback: (gridId: string, error: string) => void): () => void;
}

// ============================================================================
// BROKER ADAPTER INTERFACE (abstraction for both crypto and stock)
// ============================================================================

export interface IGridBrokerAdapter {
  // Identification
  brokerId: string;
  brokerType: BrokerType;
  isConnected: boolean;

  // Order operations
  placeGridOrder(params: GridOrderParams): Promise<GridOrderResult>;
  cancelGridOrder(orderId: string, symbol: string): Promise<boolean>;
  getOrderStatus(orderId: string, symbol: string): Promise<GridOrderStatus>;

  // Market data
  getCurrentPrice(symbol: string, exchange?: string): Promise<number>;
  subscribeToPrice(symbol: string, callback: (price: number) => void, exchange?: string): () => void;

  // Account
  getAvailableBalance(currency?: string): Promise<number>;
}

export interface GridOrderParams {
  symbol: string;
  exchange?: string;                    // For stocks
  side: 'buy' | 'sell';
  quantity: number;
  price: number;
  type: 'limit';
  productType?: string;                 // For stocks: CASH, INTRADAY, etc.
  tag?: string;                         // Order tag for identification
}

export interface GridOrderResult {
  success: boolean;
  orderId?: string;
  message?: string;
  errorCode?: string;
}

export interface GridOrderStatus {
  orderId: string;
  status: 'open' | 'filled' | 'partial' | 'cancelled' | 'rejected' | 'unknown';
  filledQuantity: number;
  remainingQuantity: number;
  averagePrice: number;
}

// ============================================================================
// GRID CALCULATION HELPERS
// ============================================================================

export interface GridCalculation {
  levels: number[];                     // Price levels
  gridSpacing: number;                  // Spacing between levels (absolute or %)
  quantityPerLevel: number;             // Quantity per grid level
  totalQuantity: number;                // Total quantity across all levels
  totalInvestment: number;              // Total investment required
  breakEvenPrice: number;               // Break-even price
  potentialProfit: number;              // Potential profit if all grids execute
}

// ============================================================================
// EVENTS
// ============================================================================

export type GridEvent =
  | { type: 'grid_created'; grid: GridState }
  | { type: 'grid_started'; gridId: string }
  | { type: 'grid_paused'; gridId: string }
  | { type: 'grid_stopped'; gridId: string }
  | { type: 'grid_updated'; grid: GridState }
  | { type: 'level_placed'; gridId: string; level: GridLevel }
  | { type: 'level_filled'; gridId: string; level: GridLevel }
  | { type: 'level_cancelled'; gridId: string; level: GridLevel }
  | { type: 'grid_error'; gridId: string; error: string }
  | { type: 'price_update'; gridId: string; price: number };

// ============================================================================
// PERSISTENCE
// ============================================================================

export interface GridPersistence {
  saveGrid(grid: GridState): Promise<void>;
  loadGrid(gridId: string): Promise<GridState | null>;
  loadAllGrids(): Promise<GridState[]>;
  deleteGrid(gridId: string): Promise<void>;
}
