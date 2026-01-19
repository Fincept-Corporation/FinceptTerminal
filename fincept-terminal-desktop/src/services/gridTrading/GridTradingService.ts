/**
 * Grid Trading Service
 *
 * Main service for managing grid trading bots.
 * Works with both crypto and stock brokers through unified adapters.
 */

import type {
  GridConfig,
  GridState,
  GridLevel,
  IGridTradingService,
  IGridBrokerAdapter,
  GridEvent,
} from './types';
import {
  createInitialGridState,
  updateGridOnPriceChange,
  updateGridLevelStatus,
  updateGridOnFill,
  getPendingLevels,
  getOpenLevels,
  checkGridStopConditions,
  validateGridConfig,
  generateGridId,
  calculateGridDetails,
} from './GridEngine';
import { storageService } from '../core/storageService';

type GridEventCallback = (event: GridEvent) => void;

export class GridTradingService implements IGridTradingService {
  private grids: Map<string, GridState> = new Map();
  private adapters: Map<string, IGridBrokerAdapter> = new Map();
  private priceUnsubscribers: Map<string, () => void> = new Map();
  private orderPollers: Map<string, NodeJS.Timeout> = new Map();
  private eventCallbacks: Set<GridEventCallback> = new Set();

  // Individual event callbacks
  private gridUpdateCallbacks: Set<(grid: GridState) => void> = new Set();
  private orderFilledCallbacks: Set<(gridId: string, level: GridLevel) => void> = new Set();
  private gridErrorCallbacks: Set<(gridId: string, error: string) => void> = new Set();

  // Configuration
  private orderPollInterval = 10000; // 10 seconds

  constructor() {
    // Load persisted grids on startup
    this.loadPersistedGrids();
  }

  // ============================================================================
  // ADAPTER MANAGEMENT
  // ============================================================================

  /**
   * Register a broker adapter for use with grids
   */
  registerAdapter(adapter: IGridBrokerAdapter): void {
    this.adapters.set(adapter.brokerId, adapter);
    console.log(`[GridTradingService] Registered adapter: ${adapter.brokerId}`);
  }

  /**
   * Unregister a broker adapter
   */
  unregisterAdapter(brokerId: string): void {
    this.adapters.delete(brokerId);
    console.log(`[GridTradingService] Unregistered adapter: ${brokerId}`);
  }

  /**
   * Get adapter for a broker
   */
  private getAdapter(brokerId: string): IGridBrokerAdapter {
    const adapter = this.adapters.get(brokerId);
    if (!adapter) {
      throw new Error(`Adapter not found for broker: ${brokerId}`);
    }
    return adapter;
  }

  // ============================================================================
  // GRID LIFECYCLE
  // ============================================================================

  /**
   * Create a new grid trading bot
   */
  async createGrid(config: GridConfig): Promise<GridState> {
    // Validate configuration
    const errors = validateGridConfig(config);
    if (errors.length > 0) {
      throw new Error(`Invalid grid configuration: ${errors.join(', ')}`);
    }

    // Assign ID if not provided
    if (!config.id) {
      config.id = generateGridId();
    }

    // Get adapter and current price
    const adapter = this.getAdapter(config.brokerId);
    if (!adapter.isConnected) {
      throw new Error(`Broker ${config.brokerId} is not connected`);
    }

    const currentPrice = await adapter.getCurrentPrice(config.symbol, config.exchange);

    // Create initial state
    const grid = createInitialGridState(config, currentPrice);

    // Store grid
    this.grids.set(grid.config.id, grid);

    // Emit event
    this.emitEvent({ type: 'grid_created', grid });

    // Persist
    await this.persistGrid(grid);

    console.log(`[GridTradingService] Created grid: ${grid.config.id}`);
    return grid;
  }

  /**
   * Start a grid trading bot
   */
  async startGrid(gridId: string): Promise<void> {
    const grid = this.getGrid(gridId);
    if (!grid) {
      throw new Error(`Grid not found: ${gridId}`);
    }

    if (grid.status === 'running') {
      console.log(`[GridTradingService] Grid already running: ${gridId}`);
      return;
    }

    // Check trigger price
    if (grid.config.triggerPrice && grid.currentPrice !== grid.config.triggerPrice) {
      // Wait for trigger price
      console.log(`[GridTradingService] Grid waiting for trigger price: ${grid.config.triggerPrice}`);
    }

    // Update status
    const updatedGrid: GridState = {
      ...grid,
      status: 'running',
      startedAt: Date.now(),
      lastUpdatedAt: Date.now(),
    };
    this.grids.set(gridId, updatedGrid);

    // Subscribe to price updates
    await this.subscribeToPriceUpdates(updatedGrid);

    // Place initial orders
    await this.placeInitialOrders(updatedGrid);

    // Start order monitoring
    this.startOrderMonitoring(gridId);

    // Emit event
    this.emitEvent({ type: 'grid_started', gridId });

    // Persist
    await this.persistGrid(updatedGrid);

    console.log(`[GridTradingService] Started grid: ${gridId}`);
  }

  /**
   * Pause a grid trading bot
   */
  async pauseGrid(gridId: string): Promise<void> {
    const grid = this.getGrid(gridId);
    if (!grid) {
      throw new Error(`Grid not found: ${gridId}`);
    }

    if (grid.status !== 'running') {
      throw new Error(`Grid is not running: ${gridId}`);
    }

    // Update status
    const updatedGrid: GridState = {
      ...grid,
      status: 'paused',
      lastUpdatedAt: Date.now(),
    };
    this.grids.set(gridId, updatedGrid);

    // Stop price subscription and order monitoring
    this.stopPriceSubscription(gridId);
    this.stopOrderMonitoring(gridId);

    // Emit event
    this.emitEvent({ type: 'grid_paused', gridId });

    // Persist
    await this.persistGrid(updatedGrid);

    console.log(`[GridTradingService] Paused grid: ${gridId}`);
  }

  /**
   * Resume a paused grid
   */
  async resumeGrid(gridId: string): Promise<void> {
    const grid = this.getGrid(gridId);
    if (!grid) {
      throw new Error(`Grid not found: ${gridId}`);
    }

    if (grid.status !== 'paused') {
      throw new Error(`Grid is not paused: ${gridId}`);
    }

    await this.startGrid(gridId);
  }

  /**
   * Stop a grid trading bot
   */
  async stopGrid(gridId: string): Promise<void> {
    const grid = this.getGrid(gridId);
    if (!grid) {
      throw new Error(`Grid not found: ${gridId}`);
    }

    // Cancel all open orders
    await this.cancelAllGridOrders(grid);

    // Stop price subscription and order monitoring
    this.stopPriceSubscription(gridId);
    this.stopOrderMonitoring(gridId);

    // Update status
    const updatedGrid: GridState = {
      ...grid,
      status: 'stopped',
      lastUpdatedAt: Date.now(),
    };
    this.grids.set(gridId, updatedGrid);

    // Emit event
    this.emitEvent({ type: 'grid_stopped', gridId });

    // Persist
    await this.persistGrid(updatedGrid);

    console.log(`[GridTradingService] Stopped grid: ${gridId}`);
  }

  /**
   * Delete a grid trading bot
   */
  async deleteGrid(gridId: string): Promise<void> {
    const grid = this.getGrid(gridId);
    if (!grid) {
      throw new Error(`Grid not found: ${gridId}`);
    }

    // Stop if running
    if (grid.status === 'running' || grid.status === 'paused') {
      await this.stopGrid(gridId);
    }

    // Remove from storage
    this.grids.delete(gridId);

    // Remove persisted data
    await this.deletePersistedGrid(gridId);

    console.log(`[GridTradingService] Deleted grid: ${gridId}`);
  }

  // ============================================================================
  // GRID MANAGEMENT
  // ============================================================================

  /**
   * Get a grid by ID
   */
  getGrid(gridId: string): GridState | undefined {
    return this.grids.get(gridId);
  }

  /**
   * Get all grids
   */
  getAllGrids(): GridState[] {
    return Array.from(this.grids.values());
  }

  /**
   * Get active (running) grids
   */
  getActiveGrids(): GridState[] {
    return this.getAllGrids().filter(g => g.status === 'running');
  }

  /**
   * Get grids for a specific broker
   */
  getGridsForBroker(brokerId: string): GridState[] {
    return this.getAllGrids().filter(g => g.config.brokerId === brokerId);
  }

  // ============================================================================
  // REAL-TIME UPDATES
  // ============================================================================

  /**
   * Update grid when price changes
   */
  async updateGridPrice(gridId: string, price: number): Promise<void> {
    const grid = this.getGrid(gridId);
    if (!grid || grid.status !== 'running') {
      return;
    }

    // Update grid state
    const updatedGrid = updateGridOnPriceChange(grid, price);
    this.grids.set(gridId, updatedGrid);

    // Check stop conditions
    const stopCheck = checkGridStopConditions(updatedGrid);
    if (stopCheck.shouldStop) {
      console.log(`[GridTradingService] Stop condition triggered: ${stopCheck.reason}`);
      await this.stopGrid(gridId);
      return;
    }

    // Emit price update event
    this.emitEvent({ type: 'price_update', gridId, price });

    // Check if we need to place any new orders
    await this.checkAndPlaceOrders(updatedGrid);
  }

  /**
   * Process a filled order
   */
  async processFilledOrder(
    gridId: string,
    orderId: string,
    filledQty: number,
    filledPrice: number
  ): Promise<void> {
    const grid = this.getGrid(gridId);
    if (!grid) {
      return;
    }

    // Find the level with this order
    const levelIndex = grid.levels.findIndex(l => l.orderId === orderId);
    if (levelIndex === -1) {
      console.warn(`[GridTradingService] Order not found in grid: ${orderId}`);
      return;
    }

    // Update grid state
    const updatedGrid = updateGridOnFill(grid, levelIndex, filledQty, filledPrice);
    this.grids.set(gridId, updatedGrid);

    // Emit events
    this.emitEvent({ type: 'level_filled', gridId, level: updatedGrid.levels[levelIndex] });
    this.emitEvent({ type: 'grid_updated', grid: updatedGrid });

    // Notify callbacks
    this.orderFilledCallbacks.forEach(cb => {
      try {
        cb(gridId, updatedGrid.levels[levelIndex]);
      } catch (error) {
        console.error('[GridTradingService] Order filled callback error:', error);
      }
    });

    // Place the opposite order
    if (updatedGrid.status === 'running') {
      await this.placeOrderForLevel(updatedGrid, levelIndex);
    }

    // Persist
    await this.persistGrid(updatedGrid);
  }

  // ============================================================================
  // ORDER MANAGEMENT
  // ============================================================================

  /**
   * Place initial orders for a grid
   */
  private async placeInitialOrders(grid: GridState): Promise<void> {
    const pendingLevels = getPendingLevels(grid);

    for (const level of pendingLevels) {
      await this.placeOrderForLevel(grid, level.levelIndex);
    }
  }

  /**
   * Place order for a specific grid level
   */
  private async placeOrderForLevel(grid: GridState, levelIndex: number): Promise<void> {
    const level = grid.levels[levelIndex];
    if (level.status !== 'pending') {
      return;
    }

    const adapter = this.getAdapter(grid.config.brokerId);

    try {
      const result = await adapter.placeGridOrder({
        symbol: grid.config.symbol,
        exchange: grid.config.exchange,
        side: level.side,
        quantity: level.quantity,
        price: level.price,
        type: 'limit',
        productType: grid.config.productType,
        tag: `grid_${grid.config.id}_${levelIndex}`,
      });

      if (result.success && result.orderId) {
        // Update level status
        const updatedGrid = updateGridLevelStatus(grid, levelIndex, 'open', result.orderId);
        this.grids.set(grid.config.id, updatedGrid);

        this.emitEvent({
          type: 'level_placed',
          gridId: grid.config.id,
          level: updatedGrid.levels[levelIndex],
        });

        console.log(`[GridTradingService] Placed ${level.side} order at ${level.price}: ${result.orderId}`);
      } else {
        // Order failed
        const updatedGrid = updateGridLevelStatus(grid, levelIndex, 'error');
        updatedGrid.lastError = result.message;
        updatedGrid.errorCount += 1;
        this.grids.set(grid.config.id, updatedGrid);

        this.emitGridError(grid.config.id, result.message || 'Order placement failed');
      }
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Unknown error';
      const updatedGrid = updateGridLevelStatus(grid, levelIndex, 'error');
      updatedGrid.lastError = errorMsg;
      updatedGrid.errorCount += 1;
      this.grids.set(grid.config.id, updatedGrid);

      this.emitGridError(grid.config.id, errorMsg);
    }
  }

  /**
   * Check and place any necessary orders
   */
  private async checkAndPlaceOrders(grid: GridState): Promise<void> {
    const pendingLevels = getPendingLevels(grid);

    for (const level of pendingLevels) {
      // Only place orders for levels that are near current price
      const priceDistance = Math.abs(level.price - grid.currentPrice) / grid.currentPrice;
      if (priceDistance < 0.1) { // Within 10% of current price
        await this.placeOrderForLevel(grid, level.levelIndex);
      }
    }
  }

  /**
   * Cancel all open orders for a grid
   */
  private async cancelAllGridOrders(grid: GridState): Promise<void> {
    const openLevels = getOpenLevels(grid);
    const adapter = this.getAdapter(grid.config.brokerId);

    for (const level of openLevels) {
      if (level.orderId) {
        try {
          await adapter.cancelGridOrder(level.orderId, grid.config.symbol);
          const updatedGrid = updateGridLevelStatus(
            this.grids.get(grid.config.id)!,
            level.levelIndex,
            'cancelled'
          );
          this.grids.set(grid.config.id, updatedGrid);

          this.emitEvent({
            type: 'level_cancelled',
            gridId: grid.config.id,
            level: updatedGrid.levels[level.levelIndex],
          });
        } catch (error) {
          console.error(`[GridTradingService] Failed to cancel order ${level.orderId}:`, error);
        }
      }
    }
  }

  // ============================================================================
  // PRICE SUBSCRIPTION
  // ============================================================================

  /**
   * Subscribe to price updates for a grid
   */
  private async subscribeToPriceUpdates(grid: GridState): Promise<void> {
    const adapter = this.getAdapter(grid.config.brokerId);

    const unsubscribe = adapter.subscribeToPrice(
      grid.config.symbol,
      (price: number) => {
        this.updateGridPrice(grid.config.id, price);
      },
      grid.config.exchange
    );

    this.priceUnsubscribers.set(grid.config.id, unsubscribe);
  }

  /**
   * Stop price subscription for a grid
   */
  private stopPriceSubscription(gridId: string): void {
    const unsubscribe = this.priceUnsubscribers.get(gridId);
    if (unsubscribe) {
      unsubscribe();
      this.priceUnsubscribers.delete(gridId);
    }
  }

  // ============================================================================
  // ORDER MONITORING
  // ============================================================================

  /**
   * Start monitoring orders for a grid
   */
  private startOrderMonitoring(gridId: string): void {
    // Stop existing poller if any
    this.stopOrderMonitoring(gridId);

    // Start new poller
    const interval = setInterval(async () => {
      await this.pollOrderStatuses(gridId);
    }, this.orderPollInterval);

    this.orderPollers.set(gridId, interval);
  }

  /**
   * Stop order monitoring for a grid
   */
  private stopOrderMonitoring(gridId: string): void {
    const interval = this.orderPollers.get(gridId);
    if (interval) {
      clearInterval(interval);
      this.orderPollers.delete(gridId);
    }
  }

  /**
   * Poll order statuses for a grid
   */
  private async pollOrderStatuses(gridId: string): Promise<void> {
    const grid = this.getGrid(gridId);
    if (!grid || grid.status !== 'running') {
      return;
    }

    const adapter = this.getAdapter(grid.config.brokerId);
    const openLevels = getOpenLevels(grid);

    for (const level of openLevels) {
      if (!level.orderId) continue;

      try {
        const status = await adapter.getOrderStatus(level.orderId, grid.config.symbol);

        if (status.status === 'filled') {
          await this.processFilledOrder(
            gridId,
            level.orderId,
            status.filledQuantity,
            status.averagePrice
          );
        } else if (status.status === 'cancelled' || status.status === 'rejected') {
          const updatedGrid = updateGridLevelStatus(
            this.grids.get(gridId)!,
            level.levelIndex,
            'cancelled'
          );
          this.grids.set(gridId, updatedGrid);
        }
      } catch (error) {
        // Ignore polling errors
      }
    }
  }

  // ============================================================================
  // EVENTS
  // ============================================================================

  /**
   * Subscribe to all grid events
   */
  onEvent(callback: GridEventCallback): () => void {
    this.eventCallbacks.add(callback);
    return () => this.eventCallbacks.delete(callback);
  }

  /**
   * Subscribe to grid updates
   */
  onGridUpdate(callback: (grid: GridState) => void): () => void {
    this.gridUpdateCallbacks.add(callback);
    return () => this.gridUpdateCallbacks.delete(callback);
  }

  /**
   * Subscribe to order filled events
   */
  onOrderFilled(callback: (gridId: string, level: GridLevel) => void): () => void {
    this.orderFilledCallbacks.add(callback);
    return () => this.orderFilledCallbacks.delete(callback);
  }

  /**
   * Subscribe to grid errors
   */
  onGridError(callback: (gridId: string, error: string) => void): () => void {
    this.gridErrorCallbacks.add(callback);
    return () => this.gridErrorCallbacks.delete(callback);
  }

  /**
   * Emit an event to all subscribers
   */
  private emitEvent(event: GridEvent): void {
    this.eventCallbacks.forEach(cb => {
      try {
        cb(event);
      } catch (error) {
        console.error('[GridTradingService] Event callback error:', error);
      }
    });

    // Also trigger specific callbacks
    if (event.type === 'grid_updated') {
      this.gridUpdateCallbacks.forEach(cb => {
        try {
          cb(event.grid);
        } catch (error) {
          console.error('[GridTradingService] Grid update callback error:', error);
        }
      });
    }
  }

  /**
   * Emit a grid error
   */
  private emitGridError(gridId: string, error: string): void {
    this.emitEvent({ type: 'grid_error', gridId, error });

    this.gridErrorCallbacks.forEach(cb => {
      try {
        cb(gridId, error);
      } catch (err) {
        console.error('[GridTradingService] Grid error callback error:', err);
      }
    });
  }

  // ============================================================================
  // PERSISTENCE
  // ============================================================================

  /**
   * Persist a grid to storage (SQLite)
   */
  private async persistGrid(grid: GridState): Promise<void> {
    try {
      const key = `grid_${grid.config.id}`;
      await storageService.setJSON(key, grid);
    } catch (error) {
      console.error('[GridTradingService] Failed to persist grid:', error);
    }
  }

  /**
   * Load persisted grids (SQLite)
   */
  private async loadPersistedGrids(): Promise<void> {
    try {
      const gridKeys = await storageService.keysWithPrefix('grid_');

      for (const key of gridKeys) {
        const grid = await storageService.getJSON<GridState>(key);
        if (grid) {
          this.grids.set(grid.config.id, grid);

          // Don't auto-start grids, just load them
          if (grid.status === 'running') {
            grid.status = 'paused'; // Pause on load, user can resume
          }
        }
      }

      console.log(`[GridTradingService] Loaded ${this.grids.size} persisted grids`);
    } catch (error) {
      console.error('[GridTradingService] Failed to load persisted grids:', error);
    }
  }

  /**
   * Delete persisted grid (SQLite)
   */
  private async deletePersistedGrid(gridId: string): Promise<void> {
    try {
      await storageService.remove(`grid_${gridId}`);
    } catch (error) {
      console.error('[GridTradingService] Failed to delete persisted grid:', error);
    }
  }

  // ============================================================================
  // CLEANUP
  // ============================================================================

  /**
   * Dispose of all resources
   */
  dispose(): void {
    // Stop all grids
    for (const gridId of this.grids.keys()) {
      this.stopPriceSubscription(gridId);
      this.stopOrderMonitoring(gridId);
    }

    // Clear all callbacks
    this.eventCallbacks.clear();
    this.gridUpdateCallbacks.clear();
    this.orderFilledCallbacks.clear();
    this.gridErrorCallbacks.clear();
  }
}

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

let serviceInstance: GridTradingService | null = null;

/**
 * Get the singleton GridTradingService instance
 */
export function getGridTradingService(): GridTradingService {
  if (!serviceInstance) {
    serviceInstance = new GridTradingService();
  }
  return serviceInstance;
}

/**
 * Reset the service (for testing)
 */
export function resetGridTradingService(): void {
  if (serviceInstance) {
    serviceInstance.dispose();
    serviceInstance = null;
  }
}
