import { useState, useEffect, useRef, useCallback } from 'react';
import {
  portfolioService,
  Portfolio,
  PortfolioSummary,
  Transaction
} from '../../../../services/portfolio/portfolioService';
import type { PortfolioExportJSON, ImportMode, ImportResult } from '../types';
import { cacheService, CacheService } from '../../../../services/cache/cacheService';

const LAST_PORTFOLIO_KEY = 'fincept_last_selected_portfolio_id';
const REFRESH_INTERVAL_KEY = 'fincept_portfolio_refresh_interval';
const DEFAULT_REFRESH_MS = 60000; // 1 minute

// Cache configuration
const CACHE_CATEGORY = 'portfolio-data';
const CACHE_TTL = 300; // 5 minutes

export const REFRESH_OPTIONS = [
  { label: '1m', ms: 60_000 },
  { label: '5m', ms: 300_000 },
  { label: '10m', ms: 600_000 },
  { label: '30m', ms: 1_800_000 },
  { label: '1h', ms: 3_600_000 },
  { label: '3h', ms: 10_800_000 },
  { label: '1d', ms: 86_400_000 },
] as const;

function loadRefreshInterval(): number {
  try {
    const saved = localStorage.getItem(REFRESH_INTERVAL_KEY);
    if (saved) {
      const val = parseInt(saved, 10);
      if (val > 0) return val;
    }
  } catch { /* localStorage not available */ }
  return DEFAULT_REFRESH_MS;
}

export const usePortfolioOperations = () => {
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolio, _setSelectedPortfolio] = useState<Portfolio | null>(null);
  const [refreshIntervalMs, _setRefreshIntervalMs] = useState(loadRefreshInterval);

  const setRefreshIntervalMs = useCallback((ms: number) => {
    _setRefreshIntervalMs(ms);
    try { localStorage.setItem(REFRESH_INTERVAL_KEY, String(ms)); } catch { /* localStorage not available */ }
  }, []);

  // Wrap setter to persist selection to localStorage
  const setSelectedPortfolio = useCallback((p: Portfolio | null) => {
    _setSelectedPortfolio(p);
    if (p) {
      try { localStorage.setItem(LAST_PORTFOLIO_KEY, p.id); } catch { /* localStorage not available */ }
    } else {
      try { localStorage.removeItem(LAST_PORTFOLIO_KEY); } catch { /* localStorage not available */ }
    }
  }, []);
  const [portfolioSummary, setPortfolioSummary] = useState<PortfolioSummary | null>(null);
  const [transactions, setTransactions] = useState<Transaction[]>([]);
  const [loading, setLoading] = useState(false);
  const [refreshing, setRefreshing] = useState(false);

  // Track the currently loading portfolio ID to prevent race conditions
  const loadingPortfolioIdRef = useRef<string | null>(null);

  // ==================== FUNCTIONS (declared before useEffect) ====================

  // Load portfolios with cache
  const loadPortfolios = useCallback(async () => {
    try {
      const cacheKey = CacheService.key('portfolio', 'list');

      // Try cache first
      const cached = await cacheService.get<Portfolio[]>(cacheKey);
      if (cached && !cached.isStale) {
        setPortfolios(cached.data);
        return cached.data;
      }

      // Fetch fresh data
      const result = await portfolioService.getPortfolios();
      setPortfolios(result);

      // Update cache
      await cacheService.set(cacheKey, result, CACHE_CATEGORY, CACHE_TTL);

      return result;
    } catch (error) {
      console.error('[usePortfolioOperations] Error loading portfolios:', error);
      return [];
    }
  }, []);

  // Load portfolio summary with cache
  const loadPortfolioSummary = useCallback(async (portfolioId: string) => {
    try {
      setRefreshing(true);

      const summaryCacheKey = CacheService.key('portfolio', 'summary', portfolioId);
      const transactionsCacheKey = CacheService.key('portfolio', 'transactions', portfolioId);

      // Try cache first for summary
      const cachedSummary = await cacheService.get<PortfolioSummary>(summaryCacheKey);
      if (cachedSummary && !cachedSummary.isStale) {
        if (loadingPortfolioIdRef.current === portfolioId) {
          setPortfolioSummary(cachedSummary.data);
        }
      } else {
        // No cache — clear stale data and show spinner
        if (loadingPortfolioIdRef.current === portfolioId) {
          setPortfolioSummary(null);
          setTransactions([]);
        }
        // Fetch fresh summary
        const summary = await portfolioService.getPortfolioSummary(portfolioId);

        if (loadingPortfolioIdRef.current === portfolioId) {
          setPortfolioSummary(summary);
          // Update cache
          await cacheService.set(summaryCacheKey, summary, CACHE_CATEGORY, CACHE_TTL);
        } else {
          console.log(`[usePortfolioOperations] Discarding stale data for portfolio ${portfolioId}`);
          return;
        }
      }

      // Try cache first for transactions
      const cachedTransactions = await cacheService.get<Transaction[]>(transactionsCacheKey);
      if (cachedTransactions && !cachedTransactions.isStale) {
        if (loadingPortfolioIdRef.current === portfolioId) {
          setTransactions(cachedTransactions.data);
        }
      } else {
        // Fetch fresh transactions
        const txns = await portfolioService.getPortfolioTransactions(portfolioId, 20);

        if (loadingPortfolioIdRef.current === portfolioId) {
          setTransactions(txns);
          // Update cache
          await cacheService.set(transactionsCacheKey, txns, CACHE_CATEGORY, CACHE_TTL);
        } else {
          console.log(`[usePortfolioOperations] Discarding stale transactions for portfolio ${portfolioId}`);
        }
      }
    } catch (error) {
      console.error('[usePortfolioOperations] Error loading portfolio summary:', error);
    } finally {
      setRefreshing(false);
    }
  }, []);

  // Refresh portfolio data - invalidates cache and forces fresh fetch
  const refreshPortfolioData = useCallback(async () => {
    if (selectedPortfolio) {
      // Invalidate all portfolio caches
      await cacheService.invalidatePattern(`portfolio:summary:${selectedPortfolio.id}%`);
      await cacheService.invalidatePattern(`portfolio:transactions:${selectedPortfolio.id}%`);

      // Invalidate market data cache for all holdings
      if (portfolioSummary?.holdings) {
        for (const holding of portfolioSummary.holdings) {
          await cacheService.invalidatePattern(`market-historical:${holding.symbol}%`);
        }
      }

      loadPortfolioSummary(selectedPortfolio.id);
    }
  }, [selectedPortfolio, loadPortfolioSummary, portfolioSummary]);

  // Create new portfolio
  const createPortfolio = async (name: string, owner: string, currency: string) => {
    if (!name || !owner) {
      throw new Error('Please fill in all required fields');
    }

    const portfolio = await portfolioService.createPortfolio(name, owner, currency);

    // Invalidate portfolio list cache
    await cacheService.invalidatePattern('portfolio:list%');

    await loadPortfolios();
    setSelectedPortfolio(portfolio);
    return portfolio;
  };

  // Add asset to portfolio
  const addAsset = async (symbol: string, quantity: string, price: string) => {
    if (!selectedPortfolio || !symbol || !quantity || !price) {
      throw new Error('Please fill in all required fields');
    }

    await portfolioService.addAsset(
      selectedPortfolio.id,
      symbol.toUpperCase(),
      parseFloat(quantity),
      parseFloat(price)
    );

    // Invalidate cache for this portfolio
    await cacheService.invalidatePattern(`portfolio:summary:${selectedPortfolio.id}%`);
    await cacheService.invalidatePattern(`portfolio:transactions:${selectedPortfolio.id}%`);

    await refreshPortfolioData();
  };

  // Sell asset
  const sellAsset = async (symbol: string, quantity: string, price: string) => {
    if (!selectedPortfolio || !symbol || !quantity || !price) {
      throw new Error('Please fill in all required fields');
    }

    await portfolioService.sellAsset(
      selectedPortfolio.id,
      symbol.toUpperCase(),
      parseFloat(quantity),
      parseFloat(price)
    );

    // Invalidate cache for this portfolio
    await cacheService.invalidatePattern(`portfolio:summary:${selectedPortfolio.id}%`);
    await cacheService.invalidatePattern(`portfolio:transactions:${selectedPortfolio.id}%`);

    await refreshPortfolioData();
  };

  // Delete portfolio
  const deletePortfolio = async (portfolioId: string) => {
    await portfolioService.deletePortfolio(portfolioId);

    // Invalidate all caches related to this portfolio
    await cacheService.invalidatePattern('portfolio:list%');
    await cacheService.invalidatePattern(`portfolio:summary:${portfolioId}%`);
    await cacheService.invalidatePattern(`portfolio:transactions:${portfolioId}%`);

    const updatedPortfolios = await loadPortfolios();

    if (selectedPortfolio?.id === portfolioId) {
      // Use fresh list, exclude deleted portfolio
      const remaining = updatedPortfolios.filter(p => p.id !== portfolioId);
      setSelectedPortfolio(remaining.length > 0 ? remaining[0] : null);
      setPortfolioSummary(null);
    }
  };

  // Export to JSON
  const exportToJSON = async () => {
    if (!selectedPortfolio) return;
    const json = await portfolioService.exportPortfolioJSON(selectedPortfolio.id);
    const blob = new Blob([json], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${selectedPortfolio.name}_${new Date().toISOString().split('T')[0]}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  // Import portfolio from parsed data
  const importPortfolio = async (
    data: PortfolioExportJSON,
    mode: ImportMode,
    mergeTargetId?: string,
    onProgress?: (current: number, total: number) => void
  ): Promise<ImportResult> => {
    const result = await portfolioService.importPortfolio(data, mode, mergeTargetId, onProgress);

    // Invalidate caches
    await cacheService.invalidatePattern('portfolio:list%');
    await cacheService.invalidatePattern(`portfolio:summary:${result.portfolioId}%`);
    await cacheService.invalidatePattern(`portfolio:transactions:${result.portfolioId}%`);

    // Reload portfolio list
    const updated = await loadPortfolios();

    // Auto-select the imported/merged portfolio
    const imported = updated.find(p => p.id === result.portfolioId);
    if (imported) setSelectedPortfolio(imported);

    return result;
  };

  // Export to CSV
  const exportToCSV = async () => {
    if (!selectedPortfolio) return;

    const csv = await portfolioService.exportPortfolioCSV(selectedPortfolio.id);
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${selectedPortfolio.name}_${new Date().toISOString().split('T')[0]}.csv`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  // Update transaction
  const updateTransaction = async (
    transactionId: string,
    quantity: number,
    price: number,
    transactionDate: string,
    notes?: string
  ) => {
    await portfolioService.updateTransaction(transactionId, quantity, price, transactionDate, notes);

    // Invalidate cache for current portfolio
    if (selectedPortfolio) {
      await cacheService.invalidatePattern(`portfolio:summary:${selectedPortfolio.id}%`);
      await cacheService.invalidatePattern(`portfolio:transactions:${selectedPortfolio.id}%`);
    }

    await refreshPortfolioData();
  };

  // Delete transaction
  const deleteTransaction = async (transactionId: string) => {
    await portfolioService.deleteTransaction(transactionId);

    // Invalidate cache for current portfolio
    if (selectedPortfolio) {
      await cacheService.invalidatePattern(`portfolio:summary:${selectedPortfolio.id}%`);
      await cacheService.invalidatePattern(`portfolio:transactions:${selectedPortfolio.id}%`);
    }

    await refreshPortfolioData();
  };

  // ==================== EFFECTS (use functions declared above) ====================

  // Initialize service and load portfolios, restoring last selection
  useEffect(() => {
    let cancelled = false;

    const initService = async () => {
      setLoading(true);
      try {
        await portfolioService.initialize();
        if (cancelled) return;

        const result = await portfolioService.getPortfolios();
        if (cancelled) return;

        setPortfolios(result);

        if (result.length > 0) {
          // Restore last selected portfolio from localStorage
          let restored = false;
          try {
            const lastId = localStorage.getItem(LAST_PORTFOLIO_KEY);
            if (lastId) {
              const match = result.find(p => p.id === lastId);
              if (match) {
                setSelectedPortfolio(match);
                restored = true;
              }
            }
          } catch { /* localStorage not available */ }

          // Fallback to first portfolio if no saved selection
          if (!restored) {
            setSelectedPortfolio(result[0]);
          }
        }
      } catch (error) {
        console.error('[usePortfolioOperations] Initialization error:', error);
      } finally {
        if (!cancelled) {
          setLoading(false);
        }
      }
    };
    initService();

    return () => {
      cancelled = true;
    };
  }, [setSelectedPortfolio]);

  // Auto-refresh portfolio data only when tab is visible
  useEffect(() => {
    if (!selectedPortfolio) return;

    let refreshTimer: ReturnType<typeof setInterval> | null = null;

    const startRefresh = () => {
      if (!refreshTimer) {
        refreshTimer = setInterval(() => {
          refreshPortfolioData();
        }, refreshIntervalMs);
      }
    };

    const stopRefresh = () => {
      if (refreshTimer) {
        clearInterval(refreshTimer);
        refreshTimer = null;
      }
    };

    const handleVisibilityChange = () => {
      if (document.hidden) {
        stopRefresh();
      } else {
        startRefresh();
      }
    };

    if (!document.hidden) {
      startRefresh();
    }

    document.addEventListener('visibilitychange', handleVisibilityChange);

    return () => {
      stopRefresh();
      document.removeEventListener('visibilitychange', handleVisibilityChange);
    };
  }, [selectedPortfolio, refreshPortfolioData, refreshIntervalMs]);

  // Load portfolio summary when selection changes
  useEffect(() => {
    if (selectedPortfolio) {
      // Update the ref to track which portfolio we're loading
      loadingPortfolioIdRef.current = selectedPortfolio.id;

      // Load new portfolio data (cache hit = instant, no flicker)
      loadPortfolioSummary(selectedPortfolio.id);
    } else {
      // Clear data when no portfolio is selected
      setPortfolioSummary(null);
      setTransactions([]);
      loadingPortfolioIdRef.current = null;
    }
  }, [selectedPortfolio, loadPortfolioSummary]);

  // ==================== RETURN ====================

  return {
    portfolios,
    selectedPortfolio,
    setSelectedPortfolio,
    portfolioSummary,
    transactions,
    loading,
    refreshing,
    refreshIntervalMs,
    setRefreshIntervalMs,
    createPortfolio,
    addAsset,
    sellAsset,
    deletePortfolio,
    refreshPortfolioData,
    exportToCSV,
    exportToJSON,
    importPortfolio,
    updateTransaction,
    deleteTransaction
  };
};
