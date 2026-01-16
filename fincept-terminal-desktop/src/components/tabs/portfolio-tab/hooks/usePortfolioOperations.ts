import { useState, useEffect } from 'react';
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

  // Initialize service and load portfolios
  useEffect(() => {
    const initService = async () => {
      setLoading(true);
      try {
        await portfolioService.initialize();
        await loadPortfolios();
      } catch (error) {
        console.error('[usePortfolioOperations] Initialization error:', error);
      } finally {
        setLoading(false);
      }
    };
    initService();
  }, []);

  // Auto-refresh portfolio data
  useEffect(() => {
    if (selectedPortfolio) {
      const refreshTimer = setInterval(() => {
        refreshPortfolioData();
      }, 60000); // Refresh every minute
      return () => clearInterval(refreshTimer);
    }
  }, [selectedPortfolio]);

  // Load portfolio summary when selection changes
  useEffect(() => {
    if (selectedPortfolio) {
      loadPortfolioSummary(selectedPortfolio.id);
    }
  }, [selectedPortfolio]);

  // Load portfolios
  const loadPortfolios = async () => {
    try {
      const result = await portfolioService.getPortfolios();
      setPortfolios(result);

      // Select first portfolio if none selected
      if (result.length > 0 && !selectedPortfolio) {
        setSelectedPortfolio(result[0]);
      }
    } catch (error) {
      console.error('[usePortfolioOperations] Error loading portfolios:', error);
    }
  };

  // Load portfolio summary
  const loadPortfolioSummary = async (portfolioId: string) => {
    try {
      setRefreshing(true);
      const summary = await portfolioService.getPortfolioSummary(portfolioId);
      setPortfolioSummary(summary);

      const txns = await portfolioService.getPortfolioTransactions(portfolioId, 20);
      setTransactions(txns);
    } catch (error) {
      console.error('[usePortfolioOperations] Error loading portfolio summary:', error);
    } finally {
      setRefreshing(false);
    }
  };

  // Refresh portfolio data
  const refreshPortfolioData = () => {
    if (selectedPortfolio) {
      loadPortfolioSummary(selectedPortfolio.id);
    }
  };

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
    await loadPortfolios();

    if (selectedPortfolio?.id === portfolioId) {
      setSelectedPortfolio(portfolios.length > 1 ? portfolios[0] : null);
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
