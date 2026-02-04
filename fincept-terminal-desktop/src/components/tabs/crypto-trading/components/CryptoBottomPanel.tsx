// CryptoBottomPanel.tsx - Professional Terminal-Style Bottom Panel for Crypto Trading
import React from 'react';
import {
  ChevronDown,
  ChevronUp,
  Briefcase,
  FileText,
  History,
  Activity,
  BarChart3,
  Sparkles,
  Globe,
  Grid3X3,
  Bell
} from 'lucide-react';
import { FINCEPT } from '../constants';
import type { BottomPanelTabType, Position } from '../types';
import { PositionsTable } from '../../trading/core/PositionManager';
import { OrdersTable, ClosedOrders } from '../../trading/core/OrderManager';
import { AccountStats, FeesDisplay, MarginPanel } from '../../trading/core/AccountInfo';
import { StakingPanel, FuturesPanel } from '../../trading/exchange-specific/kraken';
import { PortfolioAggregator, ArbitrageDetector } from '../../trading/cross-exchange';
import { GridTradingPanel } from '../../trading/grid-trading';
import { MonitoringPanel } from '../../trading/monitoring/MonitoringPanel';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

interface CryptoBottomPanelProps {
  activeTab: BottomPanelTabType;
  positions: Position[];
  orders: any[];
  trades: any[];
  isMinimized: boolean;
  activeBroker: string | null;
  selectedSymbol: string;
  tickerData: any;
  realAdapter: any;
  onTabChange: (tab: BottomPanelTabType) => void;
  onMinimizeToggle: () => void;
}

const TAB_CONFIG: { id: BottomPanelTabType; label: string; icon: React.ReactNode }[] = [
  { id: 'positions', label: 'POSITIONS', icon: <Briefcase size={11} /> },
  { id: 'orders', label: 'ORDERS', icon: <FileText size={11} /> },
  { id: 'history', label: 'HISTORY', icon: <History size={11} /> },
  { id: 'trades', label: 'TRADES', icon: <Activity size={11} /> },
  { id: 'stats', label: 'STATS', icon: <BarChart3 size={11} /> },
  { id: 'features', label: 'FEATURES', icon: <Sparkles size={11} /> },
  { id: 'cross-exchange', label: 'CROSS-EX', icon: <Globe size={11} /> },
  { id: 'grid-trading', label: 'GRID', icon: <Grid3X3 size={11} /> },
  { id: 'monitoring', label: 'MONITOR', icon: <Bell size={11} /> },
];

export function CryptoBottomPanel({
  activeTab,
  positions,
  orders,
  trades,
  isMinimized,
  activeBroker,
  selectedSymbol,
  tickerData,
  realAdapter,
  onTabChange,
  onMinimizeToggle,
}: CryptoBottomPanelProps) {
  const getTabCount = (tabId: BottomPanelTabType): number | null => {
    if (tabId === 'positions') return positions.length;
    if (tabId === 'orders') return orders.length;
    if (tabId === 'trades') return trades.length;
    return null;
  };

  return (
    <div style={{
      flex: isMinimized ? '0 0 32px' : 1,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      transition: 'flex 0.2s ease',
      backgroundColor: FINCEPT.PANEL_BG,
      borderTop: `1px solid ${FINCEPT.BORDER}`,
    }}>
      {/* Tab Header - Sharp edges, terminal style */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
      }}>
        {/* Tabs */}
        <div style={{ display: 'flex', gap: '1px', overflow: 'auto' }}>
          {TAB_CONFIG.map((tab) => {
            const count = getTabCount(tab.id);
            const isActive = activeTab === tab.id;
            return (
              <button
                key={tab.id}
                onClick={() => onTabChange(tab.id)}
                style={{
                  padding: '8px 12px',
                  backgroundColor: isActive ? FINCEPT.ORANGE : 'transparent',
                  border: 'none',
                  borderBottom: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  color: isActive ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  cursor: 'pointer',
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '5px',
                  transition: 'all 0.15s',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
                onMouseEnter={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    e.currentTarget.style.color = FINCEPT.WHITE;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.color = FINCEPT.GRAY;
                  }
                }}
              >
                {tab.icon}
                {tab.label}
                {count !== null && count > 0 && (
                  <span style={{
                    padding: '1px 5px',
                    backgroundColor: isActive ? FINCEPT.DARK_BG : FINCEPT.BORDER,
                    color: isActive ? FINCEPT.ORANGE : FINCEPT.WHITE,
                    fontSize: '8px',
                    fontWeight: 700,
                    marginLeft: '2px',
                  }}>
                    {count}
                  </span>
                )}
              </button>
            );
          })}
        </div>

        {/* Minimize/Maximize Button */}
        <button
          onClick={onMinimizeToggle}
          style={{
            padding: '6px 10px',
            backgroundColor: 'transparent',
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            fontSize: '9px',
            fontWeight: 600,
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            marginRight: '8px',
            transition: 'all 0.15s',
            fontFamily: '"IBM Plex Mono", monospace',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            e.currentTarget.style.borderColor = FINCEPT.GRAY;
            e.currentTarget.style.color = FINCEPT.WHITE;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.borderColor = FINCEPT.BORDER;
            e.currentTarget.style.color = FINCEPT.GRAY;
          }}
          title={isMinimized ? 'Expand Panel' : 'Collapse Panel'}
        >
          {isMinimized ? (
            <>
              <ChevronUp size={12} />
              <span>EXPAND</span>
            </>
          ) : (
            <>
              <ChevronDown size={12} />
              <span>COLLAPSE</span>
            </>
          )}
        </button>
      </div>

      {!isMinimized && (
        <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          {/* Positions Tab */}
          {activeTab === 'positions' && (
            <ErrorBoundary name="PositionsTable" variant="minimal">
              <PositionsTable />
            </ErrorBoundary>
          )}

          {/* Orders Tab */}
          {activeTab === 'orders' && (
            <ErrorBoundary name="OrdersTable" variant="minimal">
              <OrdersTable />
            </ErrorBoundary>
          )}

          {/* History Tab (Closed Orders) */}
          {activeTab === 'history' && (
            <ErrorBoundary name="ClosedOrders" variant="minimal">
              <ClosedOrders />
            </ErrorBoundary>
          )}

          {/* Trades Tab */}
          {activeTab === 'trades' && (
            trades.length === 0 ? (
              <div style={{
                padding: '40px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
                fontSize: '11px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                <Activity size={24} color={FINCEPT.BORDER} style={{ marginBottom: '12px' }} />
                <div style={{ fontWeight: 600, marginBottom: '4px' }}>NO TRADE HISTORY</div>
                <div style={{ fontSize: '9px' }}>Executed trades will appear here</div>
              </div>
            ) : (
              <div style={{ flex: 1, overflow: 'auto' }}>
                <table style={{
                  width: '100%',
                  fontSize: '10px',
                  borderCollapse: 'collapse',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}>
                  <thead>
                    <tr style={{
                      backgroundColor: FINCEPT.HEADER_BG,
                      borderBottom: `1px solid ${FINCEPT.BORDER}`,
                      position: 'sticky',
                      top: 0,
                    }}>
                      <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 600, fontSize: '9px', letterSpacing: '0.5px' }}>TIME</th>
                      <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 600, fontSize: '9px', letterSpacing: '0.5px' }}>SYMBOL</th>
                      <th style={{ padding: '8px 10px', textAlign: 'center', color: FINCEPT.GRAY, fontWeight: 600, fontSize: '9px', letterSpacing: '0.5px' }}>SIDE</th>
                      <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 600, fontSize: '9px', letterSpacing: '0.5px' }}>QTY</th>
                      <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 600, fontSize: '9px', letterSpacing: '0.5px' }}>PRICE</th>
                      <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 600, fontSize: '9px', letterSpacing: '0.5px' }}>VALUE</th>
                      <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 600, fontSize: '9px', letterSpacing: '0.5px' }}>FEE</th>
                    </tr>
                  </thead>
                  <tbody>
                    {trades.map((trade, idx) => (
                      <tr
                        key={idx}
                        style={{ borderBottom: `1px solid ${FINCEPT.BORDER}30` }}
                        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
                        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                      >
                        <td style={{ padding: '8px 10px', color: FINCEPT.GRAY, fontSize: '9px' }}>
                          {new Date(trade.timestamp).toLocaleTimeString()}
                        </td>
                        <td style={{ padding: '8px 10px', color: FINCEPT.WHITE, fontWeight: 600 }}>{trade.symbol}</td>
                        <td style={{ padding: '8px 10px', textAlign: 'center' }}>
                          <span style={{
                            padding: '2px 8px',
                            backgroundColor: trade.side === 'buy' ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
                            color: trade.side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED,
                            fontSize: '8px',
                            fontWeight: 700,
                            border: `1px solid ${trade.side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED}40`,
                          }}>
                            {trade.side.toUpperCase()}
                          </span>
                        </td>
                        <td style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.WHITE }}>
                          {trade.amount?.toFixed(6) || '0.000000'}
                        </td>
                        <td style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.YELLOW }}>
                          ${trade.price?.toLocaleString('en-US', { minimumFractionDigits: 2 }) || '0.00'}
                        </td>
                        <td style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.CYAN }}>
                          ${trade.cost?.toLocaleString('en-US', { minimumFractionDigits: 2 }) || '0.00'}
                        </td>
                        <td style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.RED, fontSize: '9px' }}>
                          -${trade.fee?.cost?.toFixed(4) || '0.0000'}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )
          )}

          {/* Stats Tab */}
          {activeTab === 'stats' && (
            <div style={{
              display: 'grid',
              gridTemplateColumns: '1fr 300px',
              gap: '12px',
              padding: '12px',
              height: '100%',
              overflow: 'hidden',
            }}>
              <div style={{ height: '100%', overflow: 'auto' }}>
                <ErrorBoundary name="AccountStats" variant="minimal">
                  <AccountStats />
                </ErrorBoundary>
              </div>
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                gap: '12px',
                height: '100%',
                overflow: 'auto',
              }}>
                <ErrorBoundary name="FeesDisplay" variant="minimal">
                  <FeesDisplay />
                </ErrorBoundary>
                <ErrorBoundary name="MarginPanel" variant="minimal">
                  <MarginPanel />
                </ErrorBoundary>
              </div>
            </div>
          )}

          {/* Features Tab */}
          {activeTab === 'features' && (
            <div style={{ padding: '16px', overflow: 'auto' }}>
              {activeBroker === 'kraken' && (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                  <ErrorBoundary name="StakingPanel" variant="minimal">
                    <StakingPanel />
                  </ErrorBoundary>
                  <ErrorBoundary name="FuturesPanel" variant="minimal">
                    <FuturesPanel />
                  </ErrorBoundary>
                </div>
              )}

              {activeBroker === 'hyperliquid' && (
                <div style={{
                  padding: '40px',
                  textAlign: 'center',
                  color: FINCEPT.GRAY,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                }}>
                  <Sparkles size={24} color={FINCEPT.CYAN} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '12px', fontWeight: 600, marginBottom: '6px', color: FINCEPT.WHITE }}>
                    HYPERLIQUID FEATURES
                  </div>
                  <div style={{ fontSize: '10px' }}>
                    Vault Manager and Leverage Control available in order panel
                  </div>
                </div>
              )}

              {activeBroker !== 'kraken' && activeBroker !== 'hyperliquid' && (
                <div style={{
                  padding: '40px',
                  textAlign: 'center',
                  color: FINCEPT.GRAY,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                }}>
                  <Sparkles size={24} color={FINCEPT.BORDER} style={{ marginBottom: '12px' }} />
                  <div style={{ fontSize: '12px', fontWeight: 600, marginBottom: '6px', color: FINCEPT.WHITE }}>
                    NO EXCHANGE FEATURES
                  </div>
                  <div style={{ fontSize: '10px' }}>
                    Selected exchange has no additional features
                  </div>
                </div>
              )}
            </div>
          )}

          {/* Cross-Exchange Tab */}
          {activeTab === 'cross-exchange' && (
            <div style={{ padding: '16px', overflow: 'auto' }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                <ErrorBoundary name="PortfolioAggregator" variant="minimal">
                  <PortfolioAggregator />
                </ErrorBoundary>
                <ErrorBoundary name="ArbitrageDetector" variant="minimal">
                  <ArbitrageDetector />
                </ErrorBoundary>
              </div>
            </div>
          )}

          {/* Grid Trading Tab */}
          {activeTab === 'grid-trading' && (
            <div style={{ padding: '16px', overflow: 'auto', height: '100%' }}>
              <ErrorBoundary name="GridTradingPanel" variant="minimal">
                <GridTradingPanel
                  symbol={selectedSymbol}
                  currentPrice={tickerData?.last || tickerData?.close || 0}
                  brokerType="crypto"
                  brokerId={activeBroker || ''}
                  cryptoAdapter={realAdapter || undefined}
                  variant="full"
                />
              </ErrorBoundary>
            </div>
          )}

          {/* Monitoring Tab */}
          {activeTab === 'monitoring' && (
            <div style={{ height: '100%', overflow: 'hidden' }}>
              <ErrorBoundary name="MonitoringPanel" variant="minimal">
                <MonitoringPanel
                  provider={activeBroker || 'kraken'}
                  symbol={selectedSymbol}
                  assetType="crypto"
                />
              </ErrorBoundary>
            </div>
          )}

        </div>
      )}
    </div>
  );
}

export default CryptoBottomPanel;
