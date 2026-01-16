/**
 * Portfolio Management Tab - Bloomberg Terminal Style
 * Professional portfolio tracking and analytics interface
 */

import React, { useState, useEffect } from 'react';
import {
  Briefcase, TrendingUp, TrendingDown, DollarSign, BarChart3,
  RefreshCw, Plus, Download, Trash2, PieChart, Activity,
  Clock, AlertCircle, Target, RotateCcw, ArrowUpRight, ArrowDownRight, Info
} from 'lucide-react';
import { createPortfolioTabTour } from '../tours/portfolioTabTour';
import PositionsView from './portfolio/PositionsView';
import HistoryView from './portfolio/HistoryView';
import AnalyticsView from './portfolio/AnalyticsView';
import SectorsView from './portfolio/SectorsView';
import PerformanceView from './portfolio/PerformanceView';
import RiskMetricsView from './portfolio/RiskMetricsView';
import ReportsView from './portfolio/ReportsView';
import AlertsView from './portfolio/AlertsView';
import ActiveManagementView from './portfolio/ActiveManagementView';
import PortfolioOptimizationView from './portfolio/PortfolioOptimizationView';
import CreatePortfolioModal from './modals/CreatePortfolioModal';
import AddAssetModal from './modals/AddAssetModal';
import SellAssetModal from './modals/SellAssetModal';
import { usePortfolioOperations } from './hooks/usePortfolioOperations';
import { formatCurrency, formatPercent } from './portfolio/utils';
import { TabFooter } from '@/components/common/TabFooter';
import { BLOOMBERG, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES } from './bloombergStyles';

type SubTab = 'positions' | 'history' | 'analytics' | 'sectors' | 'performance' | 'risk' | 'reports' | 'alerts' | 'active-mgmt' | 'optimization';

const PortfolioTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeSubTab, setActiveSubTab] = useState<SubTab>('positions');

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

  // Sub-tab configurations
  const subTabs = [
    { id: 'positions' as SubTab, label: 'POSITIONS', icon: Briefcase },
    { id: 'history' as SubTab, label: 'HISTORY', icon: Clock },
    { id: 'analytics' as SubTab, label: 'ANALYTICS', icon: BarChart3 },
    { id: 'sectors' as SubTab, label: 'SECTORS', icon: PieChart },
    { id: 'performance' as SubTab, label: 'PERFORMANCE', icon: TrendingUp },
    { id: 'risk' as SubTab, label: 'RISK', icon: AlertCircle },
    { id: 'optimization' as SubTab, label: 'OPTIMIZATION', icon: Target },
    { id: 'active-mgmt' as SubTab, label: 'ACTIVE MGMT', icon: Activity },
    { id: 'reports' as SubTab, label: 'REPORTS', icon: BarChart3 },
    { id: 'alerts' as SubTab, label: 'ALERTS', icon: AlertCircle }
  ];

  return (
    <div style={{
      ...COMMON_STYLES.container,
    }}>
      {/* Bloomberg-Style Header */}
      <div style={{
        ...COMMON_STYLES.header,
        height: 'auto',
        padding: '6px 8px',
        gap: '6px',
        flexWrap: 'wrap'
      }}>
        {/* Title */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <Briefcase size={12} color={BLOOMBERG.ORANGE} style={{ filter: EFFECTS.ICON_GLOW_ORANGE }} />
          <span style={{
            fontSize: '10px',
            fontWeight: TYPOGRAPHY.BOLD,
            color: BLOOMBERG.ORANGE,
            letterSpacing: '0.5px',
            textShadow: EFFECTS.ORANGE_GLOW
          }}>
            PORTFOLIO
          </span>
        </div>

        <button
          onClick={() => {
            const tour = createPortfolioTabTour((tab: string) => setActiveSubTab(tab as any));
            tour.drive();
          }}
          style={{
            backgroundColor: BLOOMBERG.BLUE,
            color: BLOOMBERG.DARK_BG,
            border: 'none',
            padding: '2px 6px',
            fontSize: '8px',
            fontWeight: 'bold',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '2px',
            borderRadius: '2px'
          }}
          title="Start portfolio tour"
        >
          <Info size={10} />
          HELP
        </button>

        <div style={{ ...COMMON_STYLES.verticalDivider, height: '14px' }} />

        {/* Sub-tab Navigation */}
        <div id="portfolio-subtabs" style={{ display: 'flex', gap: '3px', flex: 1, overflowX: 'auto', overflowY: 'hidden' }}>
          {subTabs.map(tab => {
            const Icon = tab.icon;
            const isActive = activeSubTab === tab.id;
            return (
              <button
                key={tab.id}
                onClick={() => setActiveSubTab(tab.id)}
                disabled={!selectedPortfolio}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '2px',
                  padding: '3px 6px',
                  fontSize: '8px',
                  fontWeight: TYPOGRAPHY.SEMIBOLD,
                  letterSpacing: '0.3px',
                  color: !selectedPortfolio ? BLOOMBERG.MUTED : (isActive ? BLOOMBERG.DARK_BG : BLOOMBERG.CYAN),
                  backgroundColor: isActive ? BLOOMBERG.ORANGE : 'transparent',
                  border: isActive ? BORDERS.ORANGE : BORDERS.STANDARD,
                  borderRadius: '2px',
                  cursor: selectedPortfolio ? 'pointer' : 'not-allowed',
                  transition: EFFECTS.TRANSITION_FAST,
                  outline: 'none',
                  opacity: selectedPortfolio ? 1 : 0.5,
                  whiteSpace: 'nowrap',
                  flexShrink: 0
                }}
                onMouseEnter={(e) => {
                  if (!isActive && selectedPortfolio) {
                    e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                    e.currentTarget.style.color = BLOOMBERG.WHITE;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.color = selectedPortfolio ? BLOOMBERG.CYAN : BLOOMBERG.MUTED;
                  }
                }}
              >
                <Icon size={9} />
                {tab.label}
              </button>
            );
          })}
        </div>

        <div style={{ ...COMMON_STYLES.verticalDivider, height: '14px' }} />

        {/* Portfolio Selector & Actions */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          {portfolios.length > 0 && (
            <select
              id="portfolio-selector"
              value={selectedPortfolio?.id || ''}
              onChange={(e) => {
                const portfolio = portfolios.find(p => p.id === e.target.value);
                if (portfolio) setSelectedPortfolio(portfolio);
              }}
              style={{
                padding: '2px 4px',
                backgroundColor: BLOOMBERG.DARK_BG,
                border: BORDERS.STANDARD,
                color: BLOOMBERG.WHITE,
                fontSize: '8px',
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
                cursor: 'pointer',
                outline: 'none',
                minWidth: 'auto',
                maxWidth: '200px',
                width: 'auto'
              }}
            >
              <option value="" disabled>SELECT</option>
              {portfolios.map(portfolio => (
                <option key={portfolio.id} value={portfolio.id}>
                  {portfolio.name}
                </option>
              ))}
            </select>
          )}

          <button
            id="portfolio-create"
            onClick={() => setShowCreatePortfolio(true)}
            style={{
              padding: '2px 4px',
              backgroundColor: BLOOMBERG.DARK_BG,
              border: BORDERS.ORANGE,
              color: BLOOMBERG.ORANGE,
              fontSize: '8px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '2px',
              fontWeight: TYPOGRAPHY.BOLD
            }}
          >
            <Plus size={9} />
            NEW
          </button>

          {selectedPortfolio && (
            <>
              <button
                id="portfolio-buy"
                onClick={() => setShowAddAsset(true)}
                style={{
                  padding: '2px 4px',
                  backgroundColor: BLOOMBERG.DARK_BG,
                  border: BORDERS.STANDARD,
                  color: BLOOMBERG.GREEN,
                  fontSize: '8px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '2px'
                }}
              >
                <Plus size={9} />
                BUY
              </button>

              <button
                id="portfolio-sell"
                onClick={() => setShowSellAsset(true)}
                style={{
                  padding: '2px 4px',
                  backgroundColor: BLOOMBERG.DARK_BG,
                  border: BORDERS.STANDARD,
                  color: BLOOMBERG.RED,
                  fontSize: '8px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '2px'
                }}
              >
                <Trash2 size={9} />
                SELL
              </button>

              <button
                id="portfolio-refresh"
                onClick={refreshPortfolioData}
                disabled={refreshing}
                style={{
                  padding: '2px 4px',
                  backgroundColor: BLOOMBERG.DARK_BG,
                  border: BORDERS.STANDARD,
                  color: refreshing ? BLOOMBERG.MUTED : BLOOMBERG.CYAN,
                  fontSize: '8px',
                  cursor: refreshing ? 'wait' : 'pointer',
                  display: 'flex',
                  alignItems: 'center'
                }}
              >
                <RefreshCw size={9} className={refreshing ? 'animate-spin' : ''} />
              </button>

              <button
                id="portfolio-export"
                onClick={exportToCSV}
                style={{
                  padding: '2px 4px',
                  backgroundColor: BLOOMBERG.DARK_BG,
                  border: BORDERS.STANDARD,
                  color: BLOOMBERG.CYAN,
                  fontSize: '8px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '2px'
                }}
              >
                <Download size={9} />
              </button>
            </>
          )}
        </div>
      </div>

      {/* Sub-tab Content */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        {!selectedPortfolio ? (
          <div style={{
            height: '100%',
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            gap: SPACING.XLARGE,
            backgroundColor: BLOOMBERG.DARK_BG
          }}>
            <Briefcase size={80} color={BLOOMBERG.MUTED} style={{ opacity: 0.3 }} />
            <div style={{
              color: BLOOMBERG.GRAY,
              fontSize: TYPOGRAPHY.HEADING,
              textAlign: 'center',
              fontWeight: TYPOGRAPHY.SEMIBOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              fontFamily: TYPOGRAPHY.MONO
            }}>
              NO PORTFOLIO SELECTED
            </div>
            <div style={{
              color: BLOOMBERG.GRAY,
              fontSize: TYPOGRAPHY.DEFAULT,
              textAlign: 'center',
              fontFamily: TYPOGRAPHY.MONO
            }}>
              {portfolios.length === 0 ? 'Create a new portfolio to get started' : 'Select a portfolio from the dropdown above'}
            </div>
            {portfolios.length === 0 && (
              <button
                onClick={() => setShowCreatePortfolio(true)}
                style={{
                  marginTop: SPACING.LARGE,
                  padding: `${SPACING.MEDIUM} ${SPACING.LARGE}`,
                  backgroundColor: BLOOMBERG.ORANGE,
                  border: BORDERS.ORANGE,
                  color: BLOOMBERG.DARK_BG,
                  fontSize: TYPOGRAPHY.DEFAULT,
                  fontWeight: TYPOGRAPHY.BOLD,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  cursor: 'pointer',
                  transition: EFFECTS.TRANSITION_STANDARD,
                  fontFamily: TYPOGRAPHY.MONO
                }}
                onMouseEnter={(e) => e.currentTarget.style.opacity = '0.85'}
                onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
              >
                + CREATE PORTFOLIO
              </button>
            )}
          </div>
        ) : !portfolioSummary ? (
          <div style={{
            height: '100%',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: BLOOMBERG.GRAY,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.SEMIBOLD,
            letterSpacing: TYPOGRAPHY.WIDE,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            LOADING PORTFOLIO DATA...
          </div>
        ) : (
          <>
            {activeSubTab === 'positions' && <div id="positions-view"><PositionsView portfolioSummary={portfolioSummary} /></div>}
            {activeSubTab === 'history' && <div id="history-view"><HistoryView transactions={transactions} currency={currency} /></div>}
            {activeSubTab === 'analytics' && <div id="analytics-view"><AnalyticsView portfolioSummary={portfolioSummary} /></div>}
            {activeSubTab === 'sectors' && <div id="sectors-view"><SectorsView portfolioSummary={portfolioSummary} /></div>}
            {activeSubTab === 'performance' && <div id="performance-view"><PerformanceView portfolioSummary={portfolioSummary} /></div>}
            {activeSubTab === 'risk' && <div id="risk-view"><RiskMetricsView portfolioSummary={portfolioSummary} /></div>}
            {activeSubTab === 'optimization' && <div id="optimization-view"><PortfolioOptimizationView portfolioSummary={portfolioSummary} /></div>}
            {activeSubTab === 'reports' && <div id="reports-view"><ReportsView portfolioSummary={portfolioSummary} transactions={transactions} /></div>}
            {activeSubTab === 'alerts' && <div id="alerts-view"><AlertsView portfolioSummary={portfolioSummary} /></div>}
            {activeSubTab === 'active-mgmt' && (
              <div id="active-mgmt-view">
                <ActiveManagementView
                  portfolioId={selectedPortfolio.id}
                  portfolioData={{
                    returns: portfolioSummary.holdings.length > 0
                      ? portfolioSummary.holdings.map(h => (h.day_change_percent || 0) / 100)
                      : [],
                    weights: portfolioSummary.holdings.map(h => h.weight / 100)
                  }}
                />
              </div>
            )}
          </>
        )}
      </div>

      {/* Tab Footer */}
      <TabFooter
        tabName="PORTFOLIO MANAGER"
        backgroundColor={BLOOMBERG.HEADER_BG}
        borderColor={BLOOMBERG.ORANGE}
        leftInfo={
          portfolioSummary
            ? [
                {
                  label: 'Positions',
                  value: portfolioSummary.total_positions.toString(),
                  color: BLOOMBERG.WHITE
                },
                {
                  label: 'Total Value',
                  value: formatCurrency(portfolioSummary.total_market_value, currency),
                  color: BLOOMBERG.WHITE
                },
                {
                  label: 'Total P&L',
                  value: `${portfolioSummary.total_unrealized_pnl >= 0 ? '+' : ''}${formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}`,
                  color: portfolioSummary.total_unrealized_pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED
                }
              ]
            : []
        }
        statusInfo={
          <span style={{ color: BLOOMBERG.GRAY, fontFamily: TYPOGRAPHY.MONO }}>
            {selectedPortfolio ? `${selectedPortfolio.name} | ` : ''}
            Last Updated: {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </span>
        }
      />

      {/* Bloomberg Terminal CSS */}
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        ${COMMON_STYLES.scrollbarCSS}

        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }

        @keyframes priceFlash {
          0% { background-color: ${BLOOMBERG.YELLOW}40; }
          100% { background-color: transparent; }
        }
      `}</style>

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
