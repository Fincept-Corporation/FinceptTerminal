import { useState, useEffect, useRef, useCallback } from 'react';
import {
  portfolioService,
  Portfolio,
  PortfolioSummary,
  Transaction
} from '../../../../services/portfolio/portfolioService';

export const usePortfolioOperations = () => {
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolio, setSelectedPortfolio] = useState<Portfolio | null>(null);
  const [portfolioSummary, setPortfolioSummary] = useState<PortfolioSummary | null>(null);
  const [transactions, setTransactions] = useState<Transaction[]>([]);
  const [loading, setLoading] = useState(false);
  const [refreshing, setRefreshing] = useState(false);

  // Track the currently loading portfolio ID to prevent race conditions
  const loadingPortfolioIdRef = useRef<string | null>(null);

  // ==================== FUNCTIONS (declared before useEffect) ====================

  // Load portfolios
  const loadPortfolios = useCallback(async () => {
    try {
      const result = await portfolioService.getPortfolios();
      setPortfolios(result);
      return result;
    } catch (error) {
      console.error('[usePortfolioOperations] Error loading portfolios:', error);
      return [];
    }
  }, []);

  // Load portfolio summary
  const loadPortfolioSummary = useCallback(async (portfolioId: string) => {
    try {
      setRefreshing(true);
      const summary = await portfolioService.getPortfolioSummary(portfolioId);

      // Only update state if this portfolio is still the one being loaded (prevent race conditions)
      if (loadingPortfolioIdRef.current === portfolioId) {
        setPortfolioSummary(summary);
      } else {
        console.log(`[usePortfolioOperations] Discarding stale data for portfolio ${portfolioId}`);
        return; // Don't fetch transactions if portfolio changed
      }

      const txns = await portfolioService.getPortfolioTransactions(portfolioId, 20);

      // Only update state if this portfolio is still the one being loaded (prevent race conditions)
      if (loadingPortfolioIdRef.current === portfolioId) {
        setTransactions(txns);
      } else {
        console.log(`[usePortfolioOperations] Discarding stale transactions for portfolio ${portfolioId}`);
      }
    } catch (error) {
      console.error('[usePortfolioOperations] Error loading portfolio summary:', error);
    } finally {
      setRefreshing(false);
    }
  }, []);

  // Refresh portfolio data
  const refreshPortfolioData = useCallback(() => {
    if (selectedPortfolio) {
      loadPortfolioSummary(selectedPortfolio.id);
    }
  }, [selectedPortfolio, loadPortfolioSummary]);

  // Create new portfolio
  const createPortfolio = async (name: string, owner: string, currency: string) => {
    if (!name || !owner) {
      throw new Error('Please fill in all required fields');
    }

    const portfolio = await portfolioService.createPortfolio(name, owner, currency);
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

    await refreshPortfolioData();
  };

  // Delete portfolio
  const deletePortfolio = async (portfolioId: string) => {
    await portfolioService.deletePortfolio(portfolioId);
    const updatedPortfolios = await loadPortfolios();

    if (selectedPortfolio?.id === portfolioId) {
      // Use fresh list, exclude deleted portfolio
      const remaining = updatedPortfolios.filter(p => p.id !== portfolioId);
      setSelectedPortfolio(remaining.length > 0 ? remaining[0] : null);
      setPortfolioSummary(null);
    }
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

  // ==================== EFFECTS (use functions declared above) ====================

  // Initialize service and load portfolios
  useEffect(() => {
    const initService = async () => {
      setLoading(true);
      try {
        await portfolioService.initialize();
        const result = await portfolioService.getPortfolios();
        setPortfolios(result);
        // Select first portfolio on initial load
        if (result.length > 0) {
          setSelectedPortfolio(result[0]);
        }
      } catch (error) {
        console.error('[usePortfolioOperations] Initialization error:', error);
      } finally {
        setLoading(false);
      }
    };
    initService();
  }, []);

  // Auto-refresh portfolio data only when tab is visible
  useEffect(() => {
    if (!selectedPortfolio) return;

    let refreshTimer: ReturnType<typeof setInterval> | null = null;

    const startRefresh = () => {
      if (!refreshTimer) {
        refreshTimer = setInterval(() => {
          refreshPortfolioData();
        }, 60000);
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

    // Start refresh only if page is visible
    if (!document.hidden) {
      startRefresh();
    }

    document.addEventListener('visibilitychange', handleVisibilityChange);

    return () => {
      stopRefresh();
      document.removeEventListener('visibilitychange', handleVisibilityChange);
    };
  }, [selectedPortfolio, refreshPortfolioData]);

  // Load portfolio summary when selection changes
  useEffect(() => {
    if (selectedPortfolio) {
      // Clear previous portfolio data immediately when switching
      setPortfolioSummary(null);
      setTransactions([]);

      // Update the ref to track which portfolio we're loading
      loadingPortfolioIdRef.current = selectedPortfolio.id;

      // Load new portfolio data
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
    createPortfolio,
    addAsset,
    sellAsset,
    deletePortfolio,
    refreshPortfolioData,
    exportToCSV
  };
};
