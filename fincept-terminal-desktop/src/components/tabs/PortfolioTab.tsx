import React, { useState, useEffect } from 'react';
import { Plus, Download, RefreshCw, Trash2, BarChart3, PieChart } from 'lucide-react';
import {
  portfolioService,
  Portfolio,
  PortfolioSummary,
  Transaction
} from '../../services/portfolioService';
import PositionsView from './portfolio/PositionsView';
import HistoryView from './portfolio/HistoryView';
import AnalyticsView from './portfolio/AnalyticsView';
import SectorsView from './portfolio/SectorsView';
import PerformanceView from './portfolio/PerformanceView';
import RiskMetricsView from './portfolio/RiskMetricsView';
import ReportsView from './portfolio/ReportsView';
import AlertsView from './portfolio/AlertsView';
import { getBloombergColors, formatCurrency, formatPercent, formatNumber } from './portfolio/utils';
import { useTerminalTheme } from '@/contexts/ThemeContext';

const PortfolioTab: React.FC = () => {
  const { colors: themeColors } = useTerminalTheme();
  const BLOOMBERG_COLORS = getBloombergColors();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolio, setSelectedPortfolio] = useState<Portfolio | null>(null);
  const [portfolioSummary, setPortfolioSummary] = useState<PortfolioSummary | null>(null);
  const [transactions, setTransactions] = useState<Transaction[]>([]);
  const [loading, setLoading] = useState(false);
  const [refreshing, setRefreshing] = useState(false);
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

  // Bloomberg colors from utils
  const { ORANGE, WHITE, RED, GREEN, GRAY, DARK_BG, PANEL_BG, CYAN, YELLOW } = BLOOMBERG_COLORS;

  // Initialize service and load portfolios
  useEffect(() => {
    const initService = async () => {
      setLoading(true);
      try {
        await portfolioService.initialize();
        await loadPortfolios();
      } catch (error) {
        console.error('[PortfolioTab] Initialization error:', error);
      } finally {
        setLoading(false);
      }
    };
    initService();
  }, []);

  // Update time
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
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
      console.error('[PortfolioTab] Error loading portfolios:', error);
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
      console.error('[PortfolioTab] Error loading portfolio summary:', error);
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

  // Handle portfolio selection
  useEffect(() => {
    if (selectedPortfolio) {
      loadPortfolioSummary(selectedPortfolio.id);
    }
  }, [selectedPortfolio]);

  // Create new portfolio
  const handleCreatePortfolio = async () => {
    if (!newPortfolioName || !newPortfolioOwner) {
      alert('Please fill in all required fields');
      return;
    }

    try {
      const portfolio = await portfolioService.createPortfolio(
        newPortfolioName,
        newPortfolioOwner,
        newPortfolioCurrency
      );

      await loadPortfolios();
      setSelectedPortfolio(portfolio);
      setShowCreatePortfolio(false);

      // Reset form
      setNewPortfolioName('');
      setNewPortfolioOwner('');
      setNewPortfolioCurrency('USD');
    } catch (error) {
      console.error('[PortfolioTab] Error creating portfolio:', error);
      alert('Failed to create portfolio');
    }
  };

  // Add asset to portfolio
  const handleAddAsset = async () => {
    if (!selectedPortfolio || !addAssetSymbol || !addAssetQuantity || !addAssetPrice) {
      alert('Please fill in all required fields');
      return;
    }

    try {
      await portfolioService.addAsset(
        selectedPortfolio.id,
        addAssetSymbol.toUpperCase(),
        parseFloat(addAssetQuantity),
        parseFloat(addAssetPrice)
      );

      await refreshPortfolioData();
      setShowAddAsset(false);

      // Reset form
      setAddAssetSymbol('');
      setAddAssetQuantity('');
      setAddAssetPrice('');
    } catch (error) {
      console.error('[PortfolioTab] Error adding asset:', error);
      alert('Failed to add asset');
    }
  };

  // Sell asset
  const handleSellAsset = async () => {
    if (!selectedPortfolio || !sellAssetSymbol || !sellAssetQuantity || !sellAssetPrice) {
      alert('Please fill in all required fields');
      return;
    }

    try {
      await portfolioService.sellAsset(
        selectedPortfolio.id,
        sellAssetSymbol.toUpperCase(),
        parseFloat(sellAssetQuantity),
        parseFloat(sellAssetPrice)
      );

      await refreshPortfolioData();
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

  // Delete portfolio
  const handleDeletePortfolio = async (portfolioId: string) => {
    if (!confirm('Are you sure you want to delete this portfolio? This action cannot be undone.')) {
      return;
    }

    try {
      await portfolioService.deletePortfolio(portfolioId);
      await loadPortfolios();

      if (selectedPortfolio?.id === portfolioId) {
        setSelectedPortfolio(portfolios.length > 1 ? portfolios[0] : null);
        setPortfolioSummary(null);
      }
    } catch (error) {
      console.error('[PortfolioTab] Error deleting portfolio:', error);
      alert('Failed to delete portfolio');
    }
  };

  // Export to CSV
  const handleExportCSV = async () => {
    if (!selectedPortfolio) return;

    try {
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
    } catch (error) {
      console.error('[PortfolioTab] Error exporting CSV:', error);
      alert('Failed to export CSV');
    }
  };

  // Get current portfolio currency
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
              FINCEPT PORTFOLIO MANAGER
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: refreshing ? ORANGE : GREEN, fontSize: '10px' }}>
              ● {refreshing ? 'UPDATING' : 'LIVE'}
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: CYAN, fontSize: '11px' }}>
              {currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC
            </span>
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
              NEW PORTFOLIO
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
              REFRESH
            </button>
            <button
              onClick={handleExportCSV}
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
              EXPORT CSV
            </button>
          </div>
        </div>

        {/* Portfolio Summary Bar */}
        {portfolioSummary && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px', marginTop: '8px' }}>
            <span style={{ color: GRAY }}>TOTAL VALUE:</span>
            <span style={{ color: YELLOW, fontWeight: 'bold' }}>
              {formatCurrency(portfolioSummary.total_market_value, currency)}
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: GRAY }}>DAY P&L:</span>
            <span style={{
              color: portfolioSummary.total_day_change >= 0 ? GREEN : RED,
              fontWeight: 'bold'
            }}>
              {formatCurrency(portfolioSummary.total_day_change, currency)} ({formatPercent(portfolioSummary.total_day_change_percent)})
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: GRAY }}>TOTAL P&L:</span>
            <span style={{
              color: portfolioSummary.total_unrealized_pnl >= 0 ? GREEN : RED,
              fontWeight: 'bold'
            }}>
              {formatCurrency(portfolioSummary.total_unrealized_pnl, currency)} ({formatPercent(portfolioSummary.total_unrealized_pnl_percent)})
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: GRAY }}>POSITIONS:</span>
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
            { key: "F6", label: "SECTORS", view: "SECTORS" },
            { key: "F7", label: "PERFORMANCE", view: "PERFORMANCE" },
            { key: "F8", label: "REBALANCE", view: "REBALANCE" },
            { key: "F9", label: "REPORTS", view: "REPORTS" },
            { key: "F10", label: "ALERTS", view: "ALERTS" },
            { key: "F11", label: "SETTINGS", view: "SETTINGS" },
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
              {/* Positions View */}
              {selectedView === 'POSITIONS' && (
                <PositionsView portfolioSummary={portfolioSummary} />
              )}

              {/* History View */}
              {selectedView === 'HISTORY' && (
                <HistoryView transactions={transactions} currency={currency} />
              )}

              {/* Analytics View */}
              {selectedView === 'ANALYTICS' && (
                <AnalyticsView portfolioSummary={portfolioSummary} />
              )}

              {/* Sectors View */}
              {selectedView === 'SECTORS' && (
                <SectorsView portfolioSummary={portfolioSummary} />
              )}

              {/* Performance View */}
              {selectedView === 'PERFORMANCE' && (
                <PerformanceView portfolioSummary={portfolioSummary} />
              )}

              {/* Risk Metrics View (for REBALANCE button - shows risk metrics) */}
              {selectedView === 'REBALANCE' && (
                <RiskMetricsView portfolioSummary={portfolioSummary} />
              )}

              {/* Reports View */}
              {selectedView === 'REPORTS' && (
                <ReportsView portfolioSummary={portfolioSummary} transactions={transactions} />
              )}

              {/* Alerts View */}
              {selectedView === 'ALERTS' && (
                <AlertsView portfolioSummary={portfolioSummary} />
              )}

              {/* Other views placeholder */}
              {!['POSITIONS', 'HISTORY', 'ANALYTICS', 'SECTORS', 'PERFORMANCE', 'REBALANCE', 'REPORTS', 'ALERTS'].includes(selectedView) && (
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

      {/* Create Portfolio Modal */}
      {showCreatePortfolio && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: DARK_BG,
            border: `2px solid ${ORANGE}`,
            padding: '24px',
            minWidth: '400px'
          }}>
            <div style={{ color: ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '16px' }}>
              CREATE NEW PORTFOLIO
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                PORTFOLIO NAME *
              </label>
              <input
                type="text"
                value={newPortfolioName}
                onChange={(e) => setNewPortfolioName(e.target.value)}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
                placeholder="My Portfolio"
              />
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                OWNER *
              </label>
              <input
                type="text"
                value={newPortfolioOwner}
                onChange={(e) => setNewPortfolioOwner(e.target.value)}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
                placeholder="John Doe"
              />
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                CURRENCY
              </label>
              <select
                value={newPortfolioCurrency}
                onChange={(e) => setNewPortfolioCurrency(e.target.value)}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
              >
                <option value="USD">USD - US Dollar</option>
                <option value="EUR">EUR - Euro</option>
                <option value="GBP">GBP - British Pound</option>
                <option value="INR">INR - Indian Rupee</option>
              </select>
            </div>

            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
              <button
                onClick={() => setShowCreatePortfolio(false)}
                style={{
                  background: GRAY,
                  color: 'black',
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                CANCEL
              </button>
              <button
                onClick={handleCreatePortfolio}
                style={{
                  background: ORANGE,
                  color: 'black',
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                CREATE
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Add Asset Modal */}
      {showAddAsset && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: DARK_BG,
            border: `2px solid ${GREEN}`,
            padding: '24px',
            minWidth: '400px'
          }}>
            <div style={{ color: GREEN, fontSize: '14px', fontWeight: 'bold', marginBottom: '16px' }}>
              ADD ASSET TO PORTFOLIO
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                SYMBOL *
              </label>
              <input
                type="text"
                value={addAssetSymbol}
                onChange={(e) => setAddAssetSymbol(e.target.value.toUpperCase())}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  textTransform: 'uppercase'
                }}
                placeholder="AAPL"
              />
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                QUANTITY *
              </label>
              <input
                type="number"
                value={addAssetQuantity}
                onChange={(e) => setAddAssetQuantity(e.target.value)}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
                placeholder="100"
                step="0.0001"
              />
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                BUY PRICE *
              </label>
              <input
                type="number"
                value={addAssetPrice}
                onChange={(e) => setAddAssetPrice(e.target.value)}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
                placeholder="150.00"
                step="0.01"
              />
            </div>

            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
              <button
                onClick={() => setShowAddAsset(false)}
                style={{
                  background: GRAY,
                  color: 'black',
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                CANCEL
              </button>
              <button
                onClick={handleAddAsset}
                style={{
                  background: GREEN,
                  color: 'black',
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                ADD
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Sell Asset Modal */}
      {showSellAsset && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: DARK_BG,
            border: `2px solid ${RED}`,
            padding: '24px',
            minWidth: '400px'
          }}>
            <div style={{ color: RED, fontSize: '14px', fontWeight: 'bold', marginBottom: '16px' }}>
              SELL ASSET FROM PORTFOLIO
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                SYMBOL *
              </label>
              <input
                type="text"
                value={sellAssetSymbol}
                onChange={(e) => setSellAssetSymbol(e.target.value.toUpperCase())}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px',
                  textTransform: 'uppercase'
                }}
                placeholder="AAPL"
              />
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                QUANTITY *
              </label>
              <input
                type="number"
                value={sellAssetQuantity}
                onChange={(e) => setSellAssetQuantity(e.target.value)}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
                placeholder="50"
                step="0.0001"
              />
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: GRAY, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                SELL PRICE *
              </label>
              <input
                type="number"
                value={sellAssetPrice}
                onChange={(e) => setSellAssetPrice(e.target.value)}
                style={{
                  width: '100%',
                  background: PANEL_BG,
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '8px',
                  fontSize: '11px'
                }}
                placeholder="180.00"
                step="0.01"
              />
            </div>

            <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
              <button
                onClick={() => setShowSellAsset(false)}
                style={{
                  background: GRAY,
                  color: 'black',
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                CANCEL
              </button>
              <button
                onClick={handleSellAsset}
                style={{
                  background: RED,
                  color: 'white',
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                SELL
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default PortfolioTab;
