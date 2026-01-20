import React, { useState, useEffect } from 'react';
import { Search, TrendingUp, BarChart3, FileText, Users, Newspaper, RefreshCw, Info } from 'lucide-react';
import { useTranslation } from 'react-i18next';

// Import hooks
import { useStockData } from './hooks/useStockData';
import { useSymbolSearch } from './hooks/useSymbolSearch';
import { useNews } from './hooks/useNews';

// Import tab components
import { OverviewTab } from './tabs/OverviewTab';
import { FinancialsTab } from './tabs/FinancialsTab';
import { AnalysisTab } from './tabs/AnalysisTab';
import { TechnicalsTab } from './tabs/TechnicalsTab';
import { NewsTab } from './tabs/NewsTab';

// Import existing components
import { PeerComparisonPanel } from './components/PeerComparisonPanel';
import { TabFooter } from '@/components/common/TabFooter';
import { createEquityResearchTabTour } from '../tours/equityResearchTabTour';

// Import types and utilities
import type { ChartPeriod, ActiveTab } from './types';
import { formatNumber, getRecommendationColor, getRecommendationText } from './utils/formatters';

// Import Fincept styles
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES } from '../portfolio-tab/finceptStyles';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  BLUE: FINCEPT.BLUE,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  BORDER: FINCEPT.BORDER,
  HOVER: FINCEPT.HOVER,
};

const EquityResearchTab: React.FC = () => {
  const { t } = useTranslation('equityResearch');

  // State
  const [currentSymbol, setCurrentSymbol] = useState('AAPL');
  const [chartPeriod, setChartPeriod] = useState<ChartPeriod>('6M');
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeTab, setActiveTab] = useState<ActiveTab>('overview');

  // Custom hooks
  const {
    stockInfo,
    quoteData,
    historicalData,
    financials,
    technicalsData,
    loading,
    technicalsLoading,
    fetchStockData,
    computeTechnicals,
  } = useStockData();

  const {
    searchSymbol,
    setSearchSymbol,
    searchResults,
    showSearchDropdown,
    setShowSearchDropdown,
    searchLoading,
    handleSearchInput,
    handleSelectSymbol,
    searchContainerRef,
  } = useSymbolSearch();

  const {
    newsData,
    newsLoading,
    fetchNews,
  } = useNews();

  // Update time every second
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Load initial data
  useEffect(() => {
    fetchStockData(currentSymbol, chartPeriod);
  }, [currentSymbol, chartPeriod, fetchStockData]);

  // Compute technicals when historical data changes
  useEffect(() => {
    if (historicalData.length > 0) {
      computeTechnicals(historicalData);
    }
  }, [historicalData, computeTechnicals]);

  // Fetch news when news tab is activated or symbol changes
  useEffect(() => {
    if (activeTab === 'news' && currentSymbol) {
      fetchNews(currentSymbol, stockInfo?.company_name);
    }
  }, [activeTab, currentSymbol, stockInfo?.company_name, fetchNews]);

  const handleSearch = () => {
    if (searchSymbol.trim()) {
      setCurrentSymbol(searchSymbol.trim().toUpperCase());
      setSearchSymbol('');
    }
  };

  const currentPrice = quoteData?.price || stockInfo?.current_price || 0;
  const priceChange = quoteData?.change || 0;
  const priceChangePercent = quoteData?.change_percent || 0;

  return (
    <div style={{
      ...COMMON_STYLES.container,
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* Fincept-Style Header */}
      <div style={{
        ...COMMON_STYLES.header,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexWrap: 'wrap',
        gap: SPACING.MEDIUM,
      }}>
        {/* Left Section */}
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.LARGE, flex: 1 }}>
          {/* Title with Icon */}
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            <TrendingUp size={16} color={COLORS.ORANGE} />
            <span style={{
              color: COLORS.ORANGE,
              fontWeight: TYPOGRAPHY.BOLD,
              fontSize: TYPOGRAPHY.SUBHEADING,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase',
            }}>
              RESEARCH
            </span>
          </div>

          {/* Help Button */}
          <button
            onClick={() => {
              const tour = createEquityResearchTabTour((tab: string) => setActiveTab(tab as ActiveTab));
              tour.drive();
            }}
            style={{
              backgroundColor: COLORS.BLUE,
              color: COLORS.DARK_BG,
              border: 'none',
              padding: '4px 8px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              borderRadius: '0'
            }}
            title="Start interactive tour"
          >
            <Info size={12} />
            HELP
          </button>

          {/* Divider */}
          <div style={COMMON_STYLES.verticalDivider} />

          {/* Symbol Search */}
          <div id="research-search" style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            <div ref={searchContainerRef} style={{ position: 'relative', display: 'flex', alignItems: 'center' }}>
              <Search size={14} style={{
                position: 'absolute',
                left: '8px',
                pointerEvents: 'none',
                color: COLORS.GRAY,
                zIndex: 1
              }} />
              {searchLoading && (
                <div style={{
                  position: 'absolute',
                  right: '8px',
                  width: '12px',
                  height: '12px',
                  border: `2px solid ${COLORS.BORDER}`,
                  borderTop: `2px solid ${COLORS.ORANGE}`,
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite',
                  zIndex: 1
                }} />
              )}
              <input
                type="text"
                placeholder="SYMBOL..."
                value={searchSymbol}
                style={{
                  ...COMMON_STYLES.inputField,
                  paddingLeft: '28px',
                  paddingRight: searchLoading ? '28px' : '8px',
                  width: '160px',
                  height: '28px',
                  padding: `${SPACING.SMALL} ${SPACING.SMALL} ${SPACING.SMALL} 28px`,
                  fontSize: TYPOGRAPHY.BODY,
                  textTransform: 'uppercase',
                }}
                onInput={(e) => {
                  const val = (e.target as HTMLInputElement).value;
                  setSearchSymbol(val);
                  handleSearchInput(val);
                }}
                onKeyDown={(e) => {
                  if (e.key === 'Enter') {
                    handleSearch();
                    setShowSearchDropdown(false);
                  }
                }}
                onFocus={(e) => {
                  e.currentTarget.style.borderColor = COLORS.ORANGE;
                  if (searchResults.length > 0) setShowSearchDropdown(true);
                }}
                onBlur={(e) => e.currentTarget.style.borderColor = COLORS.BORDER}
              />
              {/* Search Results Dropdown */}
              {showSearchDropdown && searchResults.length > 0 && (
                <div style={{
                  position: 'absolute',
                  top: '100%',
                  left: 0,
                  width: '320px',
                  maxHeight: '300px',
                  overflowY: 'auto',
                  backgroundColor: COLORS.PANEL_BG,
                  border: BORDERS.STANDARD,
                  borderColor: COLORS.ORANGE,
                  zIndex: 1000,
                  marginTop: '2px',
                }}>
                  {searchResults.map((result, idx) => (
                    <div
                      key={`${result.symbol}-${idx}`}
                      onMouseDown={() => handleSelectSymbol(result.symbol, setCurrentSymbol)}
                      style={{
                        padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                        cursor: 'pointer',
                        borderBottom: idx < searchResults.length - 1 ? BORDERS.STANDARD : 'none',
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        transition: EFFECTS.TRANSITION_FAST,
                      }}
                      onMouseEnter={(e) => e.currentTarget.style.backgroundColor = COLORS.HOVER}
                      onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                    >
                      <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
                        <span style={{ color: COLORS.ORANGE, fontWeight: TYPOGRAPHY.BOLD, fontSize: TYPOGRAPHY.BODY }}>
                          {result.symbol}
                        </span>
                        <span style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.SMALL, maxWidth: '220px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                          {result.name}
                        </span>
                      </div>
                      <span style={{
                        color: COLORS.CYAN,
                        fontSize: '9px',
                        textTransform: 'uppercase',
                        padding: '2px 6px',
                        backgroundColor: `${COLORS.CYAN}20`,
                        borderRadius: '2px',
                      }}>
                        {result.type}
                      </span>
                    </div>
                  ))}
                </div>
              )}
            </div>
            <button
              onClick={handleSearch}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                fontSize: TYPOGRAPHY.SMALL,
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.TINY,
              }}
              onMouseEnter={(e) => e.currentTarget.style.opacity = '0.85'}
              onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
            >
              <Search size={12} />
              GO
            </button>
            <button
              onClick={() => fetchStockData(currentSymbol, chartPeriod, true)}
              style={{
                ...COMMON_STYLES.buttonSecondary,
                padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
                fontSize: TYPOGRAPHY.SMALL,
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.TINY,
              }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = COLORS.HOVER}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
            >
              <RefreshCw size={12} />
              REFRESH
            </button>
          </div>

          {/* Divider */}
          <div style={COMMON_STYLES.verticalDivider} />

          {/* Tab Navigation Buttons */}
          <div id="research-tabs" style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
            {[
              { id: 'overview', label: 'OVERVIEW', icon: TrendingUp },
              { id: 'financials', label: 'FINANCIALS', icon: BarChart3 },
              { id: 'analysis', label: 'ANALYSIS', icon: FileText },
              { id: 'technicals', label: 'TECHNICALS', icon: BarChart3 },
              { id: 'peers', label: 'PEERS', icon: Users },
              { id: 'news', label: 'NEWS', icon: Newspaper },
            ].map((tab) => {
              const Icon = tab.icon;
              return (
                <button
                  key={tab.id}
                  onClick={() => setActiveTab(tab.id as ActiveTab)}
                  style={{
                    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                    backgroundColor: activeTab === tab.id ? COLORS.ORANGE : 'transparent',
                    border: BORDERS.STANDARD,
                    borderColor: activeTab === tab.id ? COLORS.ORANGE : COLORS.BORDER,
                    color: activeTab === tab.id ? COLORS.DARK_BG : COLORS.GRAY,
                    fontSize: TYPOGRAPHY.SMALL,
                    fontWeight: TYPOGRAPHY.BOLD,
                    cursor: 'pointer',
                    textTransform: 'uppercase',
                    letterSpacing: TYPOGRAPHY.NORMAL,
                    transition: EFFECTS.TRANSITION_FAST,
                    display: 'flex',
                    alignItems: 'center',
                    gap: SPACING.SMALL,
                  }}
                  onMouseEnter={(e) => {
                    if (activeTab !== tab.id) {
                      e.currentTarget.style.backgroundColor = COLORS.HOVER;
                      e.currentTarget.style.borderColor = COLORS.ORANGE;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (activeTab !== tab.id) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = COLORS.BORDER;
                    }
                  }}
                  title={tab.label}
                >
                  <Icon size={12} />
                  <span>{tab.label}</span>
                </button>
              );
            })}
          </div>
        </div>

        {/* Right Section - Status */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: SPACING.DEFAULT,
          fontSize: TYPOGRAPHY.BODY,
        }}>
          <span style={{ color: COLORS.GRAY, fontSize: TYPOGRAPHY.SMALL }}>
            {currentTime.toLocaleTimeString()}
          </span>
          <div style={COMMON_STYLES.verticalDivider} />
          <span style={{ color: COLORS.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>
            {currentSymbol}
          </span>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.TINY,
            padding: `${SPACING.TINY} ${SPACING.SMALL}`,
            backgroundColor: loading ? `${COLORS.YELLOW}15` : `${COLORS.GREEN}15`,
            border: `1px solid ${loading ? COLORS.YELLOW : COLORS.GREEN}`,
          }}>
            <div style={{
              width: '6px',
              height: '6px',
              borderRadius: '50%',
              backgroundColor: loading ? COLORS.YELLOW : COLORS.GREEN,
              animation: loading ? 'pulse 1.5s ease-in-out infinite' : 'none',
            }} />
            <span style={{
              color: loading ? COLORS.YELLOW : COLORS.GREEN,
              fontWeight: TYPOGRAPHY.BOLD,
              fontSize: TYPOGRAPHY.SMALL,
            }}>
              {loading ? 'LOADING' : 'LIVE'}
            </span>
          </div>
        </div>
      </div>

      {/* Ticker Bar - Stock Price and Info */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        borderBottom: BORDERS.STANDARD,
        padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
        flexShrink: 0,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        {/* Left - Company Info */}
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.LARGE }}>
          <div>
            <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM, marginBottom: SPACING.TINY }}>
              <span style={{
                color: COLORS.ORANGE,
                fontSize: TYPOGRAPHY.LARGE,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: TYPOGRAPHY.WIDE,
              }}>
                {currentSymbol}
              </span>
              <span style={{
                color: COLORS.WHITE,
                fontSize: TYPOGRAPHY.SUBHEADING,
                fontWeight: TYPOGRAPHY.SEMIBOLD,
              }}>
                {stockInfo?.company_name || 'Loading...'}
              </span>
              {stockInfo?.sector && (
                <span style={{
                  color: COLORS.BLUE,
                  fontSize: TYPOGRAPHY.SMALL,
                  padding: `${SPACING.TINY} ${SPACING.SMALL}`,
                  backgroundColor: `${COLORS.BLUE}15`,
                  border: `1px solid ${COLORS.BLUE}`,
                  fontWeight: TYPOGRAPHY.SEMIBOLD,
                  textTransform: 'uppercase',
                  letterSpacing: TYPOGRAPHY.NORMAL,
                }}>
                  {stockInfo.sector}
                </span>
              )}
              {stockInfo?.recommendation_key && (
                <span style={{
                  color: getRecommendationColor(stockInfo.recommendation_key, COLORS),
                  fontSize: TYPOGRAPHY.SMALL,
                  padding: `${SPACING.TINY} ${SPACING.SMALL}`,
                  backgroundColor: `${getRecommendationColor(stockInfo.recommendation_key, COLORS)}20`,
                  border: `1px solid ${getRecommendationColor(stockInfo.recommendation_key, COLORS)}`,
                  fontWeight: TYPOGRAPHY.BOLD,
                  textTransform: 'uppercase',
                }}>
                  {getRecommendationText(stockInfo.recommendation_key)}
                </span>
              )}
            </div>
            <div style={{
              fontSize: TYPOGRAPHY.SMALL,
              color: COLORS.GRAY,
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
            }}>
              <span>{stockInfo?.exchange || 'N/A'}</span>
              <span>•</span>
              <span>{stockInfo?.industry || 'N/A'}</span>
              <span>•</span>
              <span>{stockInfo?.country || 'N/A'}</span>
            </div>
          </div>
        </div>

        {/* Right - Price Info */}
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'flex-end',
          gap: SPACING.TINY,
        }}>
          <div style={{
            fontSize: '28px',
            fontWeight: TYPOGRAPHY.BOLD,
            color: COLORS.WHITE,
            fontFamily: TYPOGRAPHY.MONO,
            letterSpacing: TYPOGRAPHY.TIGHT,
          }}>
            ${formatNumber(currentPrice)}
          </div>
          <div style={{
            fontSize: TYPOGRAPHY.SUBHEADING,
            color: priceChange >= 0 ? COLORS.GREEN : COLORS.RED,
            fontWeight: TYPOGRAPHY.BOLD,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.TINY,
          }}>
            <span>{priceChange >= 0 ? '▲' : '▼'}</span>
            <span>${Math.abs(priceChange).toFixed(2)}</span>
            <span>({priceChangePercent.toFixed(2)}%)</span>
          </div>
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        scrollbarColor: 'rgba(120, 120, 120, 0.3) transparent',
        scrollbarWidth: 'thin',
        minHeight: 0,
        backgroundColor: COLORS.DARK_BG,
      }} className="custom-scrollbar">

        {/* Overview Tab */}
        {activeTab === 'overview' && (
          <OverviewTab
            currentSymbol={currentSymbol}
            stockInfo={stockInfo}
            quoteData={quoteData}
            historicalData={historicalData}
            chartPeriod={chartPeriod}
            setChartPeriod={setChartPeriod}
            loading={loading}
          />
        )}

        {/* Financials Tab */}
        {activeTab === 'financials' && (
          <FinancialsTab
            financials={financials}
            loading={loading}
          />
        )}

        {/* Analysis Tab */}
        {activeTab === 'analysis' && (
          <AnalysisTab
            currentSymbol={currentSymbol}
            stockInfo={stockInfo}
          />
        )}

        {/* Technicals Tab */}
        {activeTab === 'technicals' && (
          <TechnicalsTab
            technicalsData={technicalsData}
            technicalsLoading={technicalsLoading}
            historicalData={historicalData}
          />
        )}

        {/* Peers Tab */}
        {activeTab === 'peers' && (
          <div id="research-peer-comparison" style={{ padding: '8px' }}>
            <PeerComparisonPanel initialSymbol={currentSymbol} />
          </div>
        )}

        {/* News Tab */}
        {activeTab === 'news' && (
          <NewsTab
            newsData={newsData}
            newsLoading={newsLoading}
            currentSymbol={currentSymbol}
            stockInfo={stockInfo}
            onRefresh={() => fetchNews(currentSymbol, stockInfo?.company_name)}
          />
        )}
      </div>

      {/* Footer / Status Bar */}
      <TabFooter
        tabName="EQUITY RESEARCH"
        leftInfo={[
          { label: 'Real-time data analysis and company fundamentals', color: COLORS.GRAY },
          { label: 'Data Source: YFinance', color: COLORS.GRAY },
        ]}
        statusInfo={
          <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
            <span style={{ color: COLORS.CYAN, fontWeight: 'bold' }}>{currentSymbol}</span>
            {stockInfo?.exchange && <span>{stockInfo.exchange}</span>}
            {currentPrice > 0 && (
              <>
                <span>Price: ${currentPrice.toFixed(2)}</span>
                {priceChange !== 0 && (
                  <span style={{ color: priceChange >= 0 ? COLORS.GREEN : COLORS.RED }}>
                    {priceChange >= 0 ? '▲' : '▼'} {Math.abs(priceChangePercent).toFixed(2)}%
                  </span>
                )}
              </>
            )}
            <span style={{ color: loading ? COLORS.YELLOW : COLORS.GREEN, fontWeight: 'bold' }}>
              {loading ? 'LOADING...' : 'READY'}
            </span>
          </div>
        }
        backgroundColor={COLORS.PANEL_BG}
        borderColor={COLORS.BORDER}
      />

      {/* Custom Scrollbar Styles */}
      <style>{`
        .custom-scrollbar::-webkit-scrollbar {
          width: 4px;
          height: 4px;
        }
        .custom-scrollbar::-webkit-scrollbar-track {
          background: transparent;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb {
          background: rgba(120, 120, 120, 0.3);
          border-radius: 2px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb:hover {
          background: rgba(120, 120, 120, 0.6);
        }
        .custom-scrollbar::-webkit-scrollbar-corner {
          background: transparent;
        }
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>
    </div>
  );
};

export default EquityResearchTab;
