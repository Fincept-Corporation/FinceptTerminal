/**
 * Transaction Lock Manager
 *
 * Provides pessimistic locking to prevent race conditions in paper trading operations
 * Supports:
 * - Portfolio-level locks (for balance operations)
 * - Symbol-level locks (for position operations)
 * - Order-level locks (for order modifications)
 * - Deadlock prevention with timeout
 */

export class TransactionLockManager {
  private portfolioLocks: Map<string, Promise<void>> = new Map();
  private symbolLocks: Map<string, Promise<void>> = new Map();
  private orderLocks: Map<string, Promise<void>> = new Map();
  private lockTimeouts: Map<string, NodeJS.Timeout> = new Map();
  private lockResolvers: Map<string, () => void> = new Map(); // Store promise resolvers

  private readonly DEFAULT_TIMEOUT = 5000; // 5 seconds max lock hold time

  /**
   * Acquire a portfolio-level lock (for balance operations)
   */
  async acquirePortfolioLock(portfolioId: string, timeout: number = this.DEFAULT_TIMEOUT): Promise<() => void> {
    const lockKey = `portfolio:${portfolioId}`;
    return await this.acquireLock(this.portfolioLocks, lockKey, timeout);
  }

  /**
   * Acquire a symbol-level lock (for position operations)
   */
  async acquireSymbolLock(portfolioId: string, symbol: string, timeout: number = this.DEFAULT_TIMEOUT): Promise<() => void> {
    const lockKey = `symbol:${portfolioId}:${symbol}`;
    return await this.acquireLock(this.symbolLocks, lockKey, timeout);
  }

  /**
   * Acquire an order-level lock (for order modifications)
   */
  async acquireOrderLock(orderId: string, timeout: number = this.DEFAULT_TIMEOUT): Promise<() => void> {
    const lockKey = `order:${orderId}`;
    return await this.acquireLock(this.orderLocks, lockKey, timeout);
  }

  /**
   * Generic lock acquisition with timeout
   */
  private async acquireLock(
    lockMap: Map<string, Promise<void>>,
    lockKey: string,
    timeout: number
  ): Promise<() => void> {
    // Wait for existing lock to be released
    while (lockMap.has(lockKey)) {
      await lockMap.get(lockKey);
      // Small delay to prevent tight loop
      await new Promise(resolve => setTimeout(resolve, 1));
    }

    // Create new lock with resolver stored in separate map
    let releaseLock: () => void;
    const lockPromise = new Promise<void>(resolve => {
      releaseLock = resolve;
    });

    lockMap.set(lockKey, lockPromise);

    // Store the resolver function separately so we can call it later
    if (!this.lockResolvers) {
      this.lockResolvers = new Map();
    }
    this.lockResolvers.set(lockKey, releaseLock!);

    // Set timeout to auto-release lock (prevents deadlocks)
    const timeoutId = setTimeout(() => {
      console.warn(`[TransactionLock] Lock timeout for ${lockKey}, auto-releasing`);
      this.releaseLockInternal(lockMap, lockKey);
    }, timeout);

    this.lockTimeouts.set(lockKey, timeoutId);

    // Return release function
    return () => {
      this.releaseLockInternal(lockMap, lockKey);
    };
  }

  /**
   * Internal lock release
   */
  private releaseLockInternal(lockMap: Map<string, Promise<void>>, lockKey: string): void {
    const lockPromise = lockMap.get(lockKey);
    if (lockPromise) {
      // Clear timeout
      const timeoutId = this.lockTimeouts.get(lockKey);
      if (timeoutId) {
        clearTimeout(timeoutId);
        this.lockTimeouts.delete(lockKey);
      }

      // Remove lock
      lockMap.delete(lockKey);

      // Resolve promise to wake waiting operations using the stored resolver
      const resolver = this.lockResolvers?.get(lockKey);
      if (resolver) {
        resolver();
        this.lockResolvers.delete(lockKey);
      }
    }
  }

  /**
   * Execute a function with portfolio lock
   */
  async withPortfolioLock<T>(
    portfolioId: string,
    fn: () => Promise<T>,
    timeout?: number
  ): Promise<T> {
    const release = await this.acquirePortfolioLock(portfolioId, timeout);
    try {
      return await fn();
    } finally {
      release();
    }
  }

  /**
   * Execute a function with symbol lock
   */
  async withSymbolLock<T>(
    portfolioId: string,
    symbol: string,
    fn: () => Promise<T>,
    timeout?: number
  ): Promise<T> {
    const release = await this.acquireSymbolLock(portfolioId, symbol, timeout);
    try {
      return await fn();
    } finally {
      release();
    }
  }

  /**
   * Execute a function with order lock
   */
  async withOrderLock<T>(
    orderId: string,
    fn: () => Promise<T>,
    timeout?: number
  ): Promise<T> {
    const release = await this.acquireOrderLock(orderId, timeout);
    try {
      return await fn();
    } finally {
      release();
    }
  }

  /**
   * Execute a function with multiple locks (acquired in order to prevent deadlock)
   */
  async withMultipleLocks<T>(
    locks: Array<{ type: 'portfolio' | 'symbol' | 'order'; id: string; symbol?: string }>,
    fn: () => Promise<T>,
    timeout?: number
  ): Promise<T> {
    const releases: Array<() => void> = [];

    try {
      // Acquire locks in deterministic order to prevent deadlock
      const sortedLocks = [...locks].sort((a, b) => {
        const keyA = `${a.type}:${a.id}:${a.symbol || ''}`;
        const keyB = `${b.type}:${b.id}:${b.symbol || ''}`;
        return keyA.localeCompare(keyB);
      });

      for (const lock of sortedLocks) {
        let release: () => void;
        if (lock.type === 'portfolio') {
          release = await this.acquirePortfolioLock(lock.id, timeout);
        } else if (lock.type === 'symbol') {
          release = await this.acquireSymbolLock(lock.id, lock.symbol!, timeout);
        } else {
          release = await this.acquireOrderLock(lock.id, timeout);
        }
        releases.push(release);
      }

      return await fn();
    } finally {
      // Release in reverse order
      for (let i = releases.length - 1; i >= 0; i--) {
        releases[i]();
      }
    }
  }

  /**
   * Clear all locks (for cleanup/testing)
   */
  clearAllLocks(): void {
    // Clear all timeouts
    for (const timeoutId of this.lockTimeouts.values()) {
      clearTimeout(timeoutId);
    }

    // Resolve all pending locks to prevent memory leaks
    for (const resolver of this.lockResolvers.values()) {
      resolver();
    }

    this.portfolioLocks.clear();
    this.symbolLocks.clear();
    this.orderLocks.clear();
    this.lockTimeouts.clear();
    this.lockResolvers.clear();
  }

  /**
   * Get lock statistics (for debugging)
   */
  getLockStats(): {
    portfolioLocks: number;
    symbolLocks: number;
    orderLocks: number;
    totalLocks: number;
  } {
    return {
      portfolioLocks: this.portfolioLocks.size,
      symbolLocks: this.symbolLocks.size,
      orderLocks: this.orderLocks.size,
      totalLocks: this.portfolioLocks.size + this.symbolLocks.size + this.orderLocks.size,
    };
  }
}

// Singleton instance
export const transactionLockManager = new TransactionLockManager();
