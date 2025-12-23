import React, { useState, useEffect } from 'react';
import { Plus, Download, RefreshCw, Trash2, BarChart3, PieChart } from 'lucide-react';
import PositionsView from './portfolio/PositionsView';
import HistoryView from './portfolio/HistoryView';
import AnalyticsView from './portfolio/AnalyticsView';
import SectorsView from './portfolio/SectorsView';
import PerformanceView from './portfolio/PerformanceView';
import RiskMetricsView from './portfolio/RiskMetricsView';
import ReportsView from './portfolio/ReportsView';
import AlertsView from './portfolio/AlertsView';
import ActiveManagementView from './portfolio/ActiveManagementView';
import CreatePortfolioModal from './modals/CreatePortfolioModal';
import AddAssetModal from './modals/AddAssetModal';
import SellAssetModal from './modals/SellAssetModal';
import { usePortfolioOperations } from './hooks/usePortfolioOperations';
import { getBloombergColors, formatCurrency, formatPercent } from './portfolio/utils';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { TimezoneSelector } from '../../common/TimezoneSelector';
import { useTranslation } from 'react-i18next';

const PortfolioTab: React.FC = () => {
  const { colors: themeColors } = useTerminalTheme();
  const { t } = useTranslation('portfolio');
  const BLOOMBERG_COLORS = getBloombergColors();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedView, setSelectedView] = useState('POSITIONS');

  // Modal states
  const [showCreatePortfolio, setShowCreatePortfolio] = useState(false);
  const [showAddAsset, setShowAddAsset] = useState(false);
  const [showSellAsset, setShowSellAsset] = useState(false);

  // Form states
  const [newPortfolioName, setNewPortfolioName] = useState('');
  const [newPortfolioOwner, setNewPortfolioOwner] = useState('');
  const [newPortfolioCurrency, setNewPortfolioCurrency] = useState('USD');

  const [addAssetSymbol, setAddAssetSymbol] = useState('');
  const [addAssetQuantity, setAddAssetQuantity] = useState('');
  const [addAssetPrice, setAddAssetPrice] = useState('');

  const [sellAssetSymbol, setSellAssetSymbol] = useState('');
  const [sellAssetQuantity, setSellAssetQuantity] = useState('');
  const [sellAssetPrice, setSellAssetPrice] = useState('');

  // Use custom hook for portfolio operations
  const {
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
  } = usePortfolioOperations();

  const { ORANGE, WHITE, RED, GREEN, GRAY, DARK_BG, PANEL_BG, CYAN, YELLOW } = BLOOMBERG_COLORS;

  // Update time
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Handle create portfolio
  const handleCreatePortfolio = async () => {
    try {
      await createPortfolio(newPortfolioName, newPortfolioOwner, newPortfolioCurrency);
      setShowCreatePortfolio(false);
      // Reset form
      setNewPortfolioName('');
      setNewPortfolioOwner('');
      setNewPortfolioCurrency('USD');
    } catch (error) {
      console.error('[PortfolioTab] Error creating portfolio:', error);
      alert(error instanceof Error ? error.message : 'Failed to create portfolio');
    }
  };

  // Handle add asset
  const handleAddAsset = async () => {
    try {
      await addAsset(addAssetSymbol, addAssetQuantity, addAssetPrice);
      setShowAddAsset(false);
      // Reset form
      setAddAssetSymbol('');
      setAddAssetQuantity('');
      setAddAssetPrice('');
    } catch (error) {
      console.error('[PortfolioTab] Error adding asset:', error);
      alert(error instanceof Error ? error.message : 'Failed to add asset');
    }
  };

  // Handle sell asset
  const handleSellAsset = async () => {
    try {
      await sellAsset(sellAssetSymbol, sellAssetQuantity, sellAssetPrice);
      setShowSellAsset(false);
      // Reset form
      setSellAssetSymbol('');
      setSellAssetQuantity('');
      setSellAssetPrice('');
    } catch (error) {
      console.error('[PortfolioTab] Error selling asset:', error);
      alert(error instanceof Error ? error.message : 'Failed to sell asset');
    }
  };

  // Handle delete portfolio
  const handleDeletePortfolio = async (portfolioId: string) => {
    if (!confirm('Are you sure you want to delete this portfolio? This action cannot be undone.')) {
      return;
    }

    try {
      await deletePortfolio(portfolioId);
    } catch (error) {
      console.error('[PortfolioTab] Error deleting portfolio:', error);
      alert('Failed to delete portfolio');
    }
  };

  const currency = selectedPortfolio?.currency || 'USD';

  return (
    <div style={{
      height: '100%',
      backgroundColor: DARK_BG,
      color: WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        *::-webkit-scrollbar { width: 8px; height: 8px; }
        *::-webkit-scrollbar-track { background: #1a1a1a; }
        *::-webkit-scrollbar-thumb { background: #404040; border-radius: 4px; }
        *::-webkit-scrollbar-thumb:hover { background: #525252; }
      `}</style>

      {/* Header Bar */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${GRAY}`,
        padding: '8px 12px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: ORANGE, fontWeight: 'bold', fontSize: '14px' }}>
              {t('title')}
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: refreshing ? ORANGE : GREEN, fontSize: '10px' }}>
              ● {refreshing ? t('header.updating') : t('header.live')}
            </span>
            <span style={{ color: WHITE }}>|</span>
            <TimezoneSelector compact />
          </div>

          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={() => setShowCreatePortfolio(true)}
              style={{
                background: ORANGE,
                color: 'black',
                border: 'none',
                padding: '4px 12px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Plus size={12} />
              {t('header.newPortfolio')}
            </button>
            <button
              onClick={refreshPortfolioData}
              disabled={!selectedPortfolio || refreshing}
              style={{
                background: selectedPortfolio && !refreshing ? GREEN : GRAY,
                color: 'black',
                border: 'none',
                padding: '4px 12px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: selectedPortfolio && !refreshing ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                opacity: selectedPortfolio && !refreshing ? 1 : 0.5
              }}
            >
              <RefreshCw size={12} />
              {t('header.refresh')}
            </button>
            <button
              onClick={exportToCSV}
              disabled={!selectedPortfolio}
              style={{
                background: selectedPortfolio ? CYAN : GRAY,
                color: 'black',
                border: 'none',
                padding: '4px 12px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: selectedPortfolio ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                opacity: selectedPortfolio ? 1 : 0.5
              }}
            >
              <Download size={12} />
              {t('header.exportCsv')}
            </button>
          </div>
        </div>

        {/* Portfolio Summary Bar */}
        {portfolioSummary && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px', marginTop: '8px' }}>
            <span style={{ color: GRAY }}>{t('summary.totalValue')}:</span>
            <span style={{ color: YELLOW, fontWeight: 'bold' }}>
              {formatCurrency(portfolioSummary.total_market_value, currency)}
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: GRAY }}>{t('summary.dayPnl')}:</span>
            <span style={{
              color: portfolioSummary.total_day_change >= 0 ? GREEN : RED,
              fontWeight: 'bold'
            }}>
              {formatCurrency(portfolioSummary.total_day_change, currency)} ({formatPercent(portfolioSummary.total_day_change_percent)})
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: GRAY }}>{t('summary.totalPnl')}:</span>
            <span style={{
              color: portfolioSummary.total_unrealized_pnl >= 0 ? GREEN : RED,
              fontWeight: 'bold'
            }}>
              {formatCurrency(portfolioSummary.total_unrealized_pnl, currency)} ({formatPercent(portfolioSummary.total_unrealized_pnl_percent)})
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: GRAY }}>{t('summary.positions')}:</span>
            <span style={{ color: CYAN }}>{portfolioSummary.total_positions}</span>
          </div>
        )}
      </div>

      {/* Function Keys Bar */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${GRAY}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '4px' }}>
          {[
            { key: "F1", label: "POSITIONS", view: "POSITIONS" },
            { key: "F2", label: "ADD ASSET", action: () => setShowAddAsset(true) },
            { key: "F3", label: "SELL ASSET", action: () => setShowSellAsset(true) },
            { key: "F4", label: "ANALYTICS", view: "ANALYTICS" },
            { key: "F5", label: "HISTORY", view: "HISTORY" },
            { key: "F6", label: "ACTIVE MGMT", view: "ACTIVE_MGMT" },
            { key: "F7", label: "PERFORMANCE", view: "PERFORMANCE" },
            { key: "F8", label: "REBALANCE", view: "REBALANCE" },
            { key: "F9", label: "REPORTS", view: "REPORTS" },
            { key: "F10", label: "ALERTS", view: "ALERTS" },
            { key: "F11", label: "SECTORS", view: "SECTORS" },
            { key: "F12", label: "HELP", view: "HELP" }
          ].map(item => (
            <button
              key={item.key}
              onClick={() => {
                if (item.action) {
                  if (selectedPortfolio) item.action();
                } else if (item.view) {
                  setSelectedView(item.view);
                }
              }}
              disabled={!selectedPortfolio && !item.view}
              style={{
                backgroundColor: selectedView === item.view ? ORANGE : DARK_BG,
                border: `1px solid ${GRAY}`,
                color: selectedView === item.view ? 'black' : WHITE,
                padding: '4px 6px',
                fontSize: '9px',
                cursor: selectedPortfolio || item.view ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '2px',
                opacity: !selectedPortfolio && !item.view ? 0.5 : 1
              }}
            >
              <span style={{ color: YELLOW }}>{item.key}:</span>
              <span>{item.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Sidebar - Portfolio List */}
        <div style={{
          width: '250px',
          backgroundColor: PANEL_BG,
          borderRight: `1px solid ${GRAY}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          <div style={{
            padding: '8px',
            backgroundColor: DARK_BG,
            borderBottom: `1px solid ${GRAY}`,
            color: ORANGE,
            fontSize: '11px',
            fontWeight: 'bold'
          }}>
            PORTFOLIOS ({portfolios.length})
          </div>

          <div style={{ flex: 1, overflow: 'auto' }}>
            {loading ? (
              <div style={{ padding: '16px', textAlign: 'center', color: GRAY, fontSize: '10px' }}>
                Loading portfolios...
              </div>
            ) : portfolios.length === 0 ? (
              <div style={{ padding: '16px', textAlign: 'center', color: GRAY, fontSize: '10px' }}>
                No portfolios yet.
                <br />
                Click NEW PORTFOLIO to create one.
              </div>
            ) : (
              portfolios.map(portfolio => (
                <div
                  key={portfolio.id}
                  onClick={() => setSelectedPortfolio(portfolio)}
                  style={{
                    padding: '12px',
                    backgroundColor: selectedPortfolio?.id === portfolio.id ? 'rgba(255,165,0,0.1)' : 'transparent',
                    borderLeft: selectedPortfolio?.id === portfolio.id ? `3px solid ${ORANGE}` : '3px solid transparent',
                    cursor: 'pointer',
                    borderBottom: `1px solid rgba(120,120,120,0.3)`,
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedPortfolio?.id !== portfolio.id) {
                      e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.05)';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (selectedPortfolio?.id !== portfolio.id) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                    <div style={{
                      color: selectedPortfolio?.id === portfolio.id ? ORANGE : WHITE,
                      fontSize: '11px',
                      fontWeight: 'bold'
                    }}>
                      {portfolio.name}
                    </div>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        handleDeletePortfolio(portfolio.id);
                      }}
                      style={{
                        background: 'transparent',
                        border: 'none',
                        color: RED,
                        cursor: 'pointer',
                        padding: '2px',
                        display: 'flex'
                      }}
                    >
                      <Trash2 size={12} />
                    </button>
                  </div>
                  <div style={{ fontSize: '9px', color: GRAY }}>
                    Owner: {portfolio.owner}
                  </div>
                  <div style={{ fontSize: '9px', color: CYAN }}>
                    Currency: {portfolio.currency}
                  </div>
                </div>
              ))
            )}
          </div>
        </div>

        {/* Center Content - Portfolio Details */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {!selectedPortfolio ? (
            <div style={{
              flex: 1,
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '16px'
            }}>
              <BarChart3 size={64} color={GRAY} />
              <div style={{ color: GRAY, fontSize: '14px' }}>
                Select a portfolio or create a new one to get started
              </div>
            </div>
          ) : !portfolioSummary ? (
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              color: GRAY,
              fontSize: '12px'
            }}>
              Loading portfolio data...
            </div>
          ) : (
            <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
              {selectedView === 'POSITIONS' && <PositionsView portfolioSummary={portfolioSummary} />}
              {selectedView === 'HISTORY' && <HistoryView transactions={transactions} currency={currency} />}
              {selectedView === 'ANALYTICS' && <AnalyticsView portfolioSummary={portfolioSummary} />}
              {selectedView === 'SECTORS' && <SectorsView portfolioSummary={portfolioSummary} />}
              {selectedView === 'PERFORMANCE' && <PerformanceView portfolioSummary={portfolioSummary} />}
              {selectedView === 'REBALANCE' && <RiskMetricsView portfolioSummary={portfolioSummary} />}
              {selectedView === 'REPORTS' && <ReportsView portfolioSummary={portfolioSummary} transactions={transactions} />}
              {selectedView === 'ALERTS' && <AlertsView portfolioSummary={portfolioSummary} />}
              {selectedView === 'ACTIVE_MGMT' && (
                <ActiveManagementView
                  portfolioId={selectedPortfolio.id}
                  portfolioData={{
                    returns: portfolioSummary.holdings.length > 0
                      ? portfolioSummary.holdings.map(h => (h.day_change_percent || 0) / 100)
                      : [],
                    weights: portfolioSummary.holdings.map(h => h.weight / 100)
                  }}
                />
              )}
              {!['POSITIONS', 'HISTORY', 'ANALYTICS', 'SECTORS', 'PERFORMANCE', 'REBALANCE', 'REPORTS', 'ALERTS', 'ACTIVE_MGMT'].includes(selectedView) && (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  height: '100%',
                  gap: '16px'
                }}>
                  <PieChart size={64} color={GRAY} />
                  <div style={{ color: GRAY, fontSize: '12px' }}>
                    {selectedView} view coming soon...
                  </div>
                </div>
              )}
            </div>
          )}
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${GRAY}`,
        backgroundColor: PANEL_BG,
        padding: '4px 12px',
        fontSize: '9px',
        color: GRAY,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span>Fincept Portfolio Manager v2.1.0 | Real-time tracking with yfinance integration</span>
          {portfolioSummary && (
            <span>
              Last Updated: {new Date(portfolioSummary.last_updated).toLocaleTimeString()} |
              Status: <span style={{ color: GREEN }}>CONNECTED</span>
            </span>
          )}
        </div>
      </div>

      {/* Modals */}
      <CreatePortfolioModal
        show={showCreatePortfolio}
        formState={{
          name: newPortfolioName,
          owner: newPortfolioOwner,
          currency: newPortfolioCurrency
        }}
        onNameChange={setNewPortfolioName}
        onOwnerChange={setNewPortfolioOwner}
        onCurrencyChange={setNewPortfolioCurrency}
        onClose={() => setShowCreatePortfolio(false)}
        onCreate={handleCreatePortfolio}
      />

      <AddAssetModal
        show={showAddAsset}
        formState={{
          symbol: addAssetSymbol,
          quantity: addAssetQuantity,
          price: addAssetPrice
        }}
        onSymbolChange={setAddAssetSymbol}
        onQuantityChange={setAddAssetQuantity}
        onPriceChange={setAddAssetPrice}
        onClose={() => setShowAddAsset(false)}
        onAdd={handleAddAsset}
      />

      <SellAssetModal
        show={showSellAsset}
        formState={{
          symbol: sellAssetSymbol,
          quantity: sellAssetQuantity,
          price: sellAssetPrice
        }}
        onSymbolChange={setSellAssetSymbol}
        onQuantityChange={setSellAssetQuantity}
        onPriceChange={setSellAssetPrice}
        onClose={() => setShowSellAsset(false)}
        onSell={handleSellAsset}
      />
    </div>
  );
};

export default PortfolioTab;
