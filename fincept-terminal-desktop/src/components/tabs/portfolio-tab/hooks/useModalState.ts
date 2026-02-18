import { useState, useCallback } from 'react';
import { showError } from '@/utils/notifications';
import type { ImportResult } from '../types';

interface PortfolioOps {
  createPortfolio: (name: string, owner: string, currency: string) => Promise<any>;
  addAsset: (symbol: string, quantity: string, price: string) => Promise<void>;
  sellAsset: (symbol: string, quantity: string, price: string) => Promise<void>;
  deletePortfolio: (id: string) => Promise<void>;
}

export const useModalState = (operations: PortfolioOps) => {
  // Modal visibility
  const [showCreatePortfolio, setShowCreatePortfolio] = useState(false);
  const [showAddAsset, setShowAddAsset] = useState(false);
  const [showSellAsset, setShowSellAsset] = useState(false);
  const [showDeleteConfirm, setShowDeleteConfirm] = useState(false);
  const [showCreateIndex, setShowCreateIndex] = useState(false);
  const [showImportPortfolio, setShowImportPortfolio] = useState(false);
  const [portfolioToDelete, setPortfolioToDelete] = useState<string | null>(null);
  const [indexRefreshKey, setIndexRefreshKey] = useState(0);

  // Form state
  const [newPortfolioName, setNewPortfolioName] = useState('');
  const [newPortfolioOwner, setNewPortfolioOwner] = useState('');
  const [newPortfolioCurrency, setNewPortfolioCurrency] = useState('USD');
  const [addAssetSymbol, setAddAssetSymbol] = useState('');
  const [addAssetQuantity, setAddAssetQuantity] = useState('');
  const [addAssetPrice, setAddAssetPrice] = useState('');
  const [sellAssetSymbol, setSellAssetSymbol] = useState('');
  const [sellAssetQuantity, setSellAssetQuantity] = useState('');
  const [sellAssetPrice, setSellAssetPrice] = useState('');

  const handleCreatePortfolio = useCallback(async () => {
    try {
      await operations.createPortfolio(newPortfolioName, newPortfolioOwner, newPortfolioCurrency);
      setShowCreatePortfolio(false);
      setNewPortfolioName(''); setNewPortfolioOwner(''); setNewPortfolioCurrency('USD');
    } catch (error) {
      showError('Failed to create portfolio', [{ label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }]);
    }
  }, [operations, newPortfolioName, newPortfolioOwner, newPortfolioCurrency]);

  const handleAddAsset = useCallback(async () => {
    try {
      await operations.addAsset(addAssetSymbol, addAssetQuantity, addAssetPrice);
      setShowAddAsset(false);
      setAddAssetSymbol(''); setAddAssetQuantity(''); setAddAssetPrice('');
    } catch (error) {
      showError('Failed to add asset', [{ label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }]);
    }
  }, [operations, addAssetSymbol, addAssetQuantity, addAssetPrice]);

  const handleSellAsset = useCallback(async () => {
    try {
      await operations.sellAsset(sellAssetSymbol, sellAssetQuantity, sellAssetPrice);
      setShowSellAsset(false);
      setSellAssetSymbol(''); setSellAssetQuantity(''); setSellAssetPrice('');
    } catch (error) {
      showError('Failed to sell asset', [{ label: 'ERROR', value: error instanceof Error ? error.message : 'Unknown error' }]);
    }
  }, [operations, sellAssetSymbol, sellAssetQuantity, sellAssetPrice]);

  const handleDeletePortfolio = useCallback((portfolioId: string) => {
    setPortfolioToDelete(portfolioId);
    setShowDeleteConfirm(true);
  }, []);

  const confirmDeletePortfolio = useCallback(async () => {
    if (!portfolioToDelete) return;
    try { await operations.deletePortfolio(portfolioToDelete); } catch {}
    setShowDeleteConfirm(false);
    setPortfolioToDelete(null);
  }, [operations, portfolioToDelete]);

  const handleIndexCreated = useCallback(() => {
    setShowCreateIndex(false);
    setIndexRefreshKey(prev => prev + 1);
  }, []);

  const handleImportComplete = useCallback((_result: ImportResult) => {
    setShowImportPortfolio(false);
  }, []);

  return {
    showCreatePortfolio, setShowCreatePortfolio,
    showAddAsset, setShowAddAsset,
    showSellAsset, setShowSellAsset,
    showDeleteConfirm, setShowDeleteConfirm,
    showCreateIndex, setShowCreateIndex,
    portfolioToDelete,
    indexRefreshKey,
    // Create form
    newPortfolioName, setNewPortfolioName,
    newPortfolioOwner, setNewPortfolioOwner,
    newPortfolioCurrency, setNewPortfolioCurrency,
    // Add form
    addAssetSymbol, setAddAssetSymbol,
    addAssetQuantity, setAddAssetQuantity,
    addAssetPrice, setAddAssetPrice,
    // Sell form
    sellAssetSymbol, setSellAssetSymbol,
    sellAssetQuantity, setSellAssetQuantity,
    sellAssetPrice, setSellAssetPrice,
    // Handlers
    handleCreatePortfolio,
    handleAddAsset,
    handleSellAsset,
    handleDeletePortfolio,
    confirmDeletePortfolio,
    handleIndexCreated,
    showImportPortfolio, setShowImportPortfolio,
    handleImportComplete,
  };
};
