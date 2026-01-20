// CryptoBottomPanel.tsx - Bottom Panel with Tabs for Crypto Trading
import React from 'react';
import { ChevronDown, ChevronUp } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { BottomPanelTabType, Position } from '../types';
import { PositionsTable } from '../../trading/core/PositionManager';
import { OrdersTable, ClosedOrders } from '../../trading/core/OrderManager';
import { AccountStats, FeesDisplay, MarginPanel } from '../../trading/core/AccountInfo';
import { StakingPanel, FuturesPanel } from '../../trading/exchange-specific/kraken';
import { PortfolioAggregator, ArbitrageDetector } from '../../trading/cross-exchange';
import { GridTradingPanel } from '../../trading/grid-trading';

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
  return (
    <div style={{
      flex: isMinimized ? '0 0 40px' : 1,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      transition: 'flex 0.3s ease'
    }}>
      <div style={{
        padding: '6px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{ display: 'flex', gap: '4px' }}>
          {(['positions', 'orders', 'history', 'trades', 'stats', 'features', 'cross-exchange', 'grid-trading'] as BottomPanelTabType[]).map((tab) => (
            <button
              key={tab}
              onClick={() => onTabChange(tab)}
              style={{
                padding: '6px 16px',
                backgroundColor: activeTab === tab ? FINCEPT.ORANGE : 'transparent',
                border: 'none',
                color: activeTab === tab ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                textTransform: 'uppercase',
                transition: 'all 0.2s'
              }}
            >
              {tab} {tab === 'positions' && `(${positions.length})`} {tab === 'orders' && `(${orders.length})`}
            </button>
          ))}
        </div>

        {/* Minimize/Maximize Button */}
        <button
          onClick={onMinimizeToggle}
          style={{
            padding: '4px 8px',
            backgroundColor: 'transparent',
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            fontSize: '10px',
            fontWeight: 600,
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            borderRadius: '2px',
            transition: 'all 0.2s'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            e.currentTarget.style.borderColor = FINCEPT.GRAY;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.borderColor = FINCEPT.BORDER;
          }}
          title={isMinimized ? 'Maximize Panel' : 'Minimize Panel'}
        >
          {isMinimized ? (
            <>
              <ChevronUp size={14} />
              <span>MAXIMIZE</span>
            </>
          ) : (
            <>
              <ChevronDown size={14} />
              <span>MINIMIZE</span>
            </>
          )}
        </button>
      </div>

      {!isMinimized && (
        <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          {/* Positions Tab */}
          {activeTab === 'positions' && <PositionsTable />}

          {/* Orders Tab */}
          {activeTab === 'orders' && <OrdersTable />}

          {/* History Tab (Closed Orders) */}
          {activeTab === 'history' && <ClosedOrders />}

          {/* Trades Tab */}
          {activeTab === 'trades' && (
            trades.length === 0 ? (
              <div style={{ padding: '40px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
                No trade history
              </div>
            ) : (
              <table style={{ width: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                    <th style={{ padding: '8px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 600 }}>TIME</th>
                    <th style={{ padding: '8px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 600 }}>SYMBOL</th>
                    <th style={{ padding: '8px', textAlign: 'center', color: FINCEPT.GRAY, fontWeight: 600 }}>SIDE</th>
                    <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 600 }}>QTY</th>
                    <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 600 }}>PRICE</th>
                    <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 600 }}>VALUE</th>
                    <th style={{ padding: '8px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 600 }}>FEE</th>
                  </tr>
                </thead>
                <tbody>
                  {trades.map((trade, idx) => (
                    <tr key={idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                      <td style={{ padding: '8px', color: FINCEPT.GRAY, fontSize: '9px' }}>
                        {new Date(trade.timestamp).toLocaleTimeString()}
                      </td>
                      <td style={{ padding: '8px', color: FINCEPT.WHITE }}>{trade.symbol}</td>
                      <td style={{ padding: '8px', textAlign: 'center' }}>
                        <span style={{
                          padding: '2px 8px',
                          backgroundColor: trade.side === 'buy' ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
                          color: trade.side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED,
                          fontSize: '9px',
                          fontWeight: 700
                        }}>
                          {trade.side.toUpperCase()}
                        </span>
                      </td>
                      <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.WHITE }}>
                        {trade.amount?.toFixed(4) || '0.0000'}
                      </td>
                      <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.YELLOW }}>
                        ${trade.price?.toFixed(2) || '0.00'}
                      </td>
                      <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.CYAN }}>
                        ${trade.cost?.toFixed(2) || '0.00'}
                      </td>
                      <td style={{ padding: '8px', textAlign: 'right', color: FINCEPT.RED }}>
                        ${trade.fee?.cost?.toFixed(2) || '0.00'}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
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
              overflow: 'hidden'
            }}>
              {/* Left: Account Statistics */}
              <div style={{ height: '100%', overflow: 'auto' }}>
                <AccountStats />
              </div>

              {/* Right: Fees & Margin */}
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                gap: '12px',
                height: '100%',
                overflow: 'auto'
              }}>
                <FeesDisplay />
                <MarginPanel />
              </div>
            </div>
          )}

          {/* Features Tab - Exchange-Specific Features */}
          {activeTab === 'features' && (
            <div style={{ padding: '16px', overflow: 'auto' }}>
              {/* Kraken-Specific Features */}
              {activeBroker === 'kraken' && (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                  <StakingPanel />
                  <FuturesPanel />
                </div>
              )}

              {/* HyperLiquid - Already shown in order form area */}
              {activeBroker === 'hyperliquid' && (
                <div style={{
                  padding: '40px',
                  textAlign: 'center',
                  color: FINCEPT.GRAY,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '4px'
                }}>
                  <div style={{ fontSize: '14px', fontWeight: 600, marginBottom: '8px' }}>
                    HyperLiquid Features
                  </div>
                  <div style={{ fontSize: '11px' }}>
                    HyperLiquid-specific features (Vault Manager, Leverage Control) are available in the order form panel on the right.
                  </div>
                </div>
              )}

              {/* No exchange-specific features */}
              {activeBroker !== 'kraken' && activeBroker !== 'hyperliquid' && (
                <div style={{
                  padding: '40px',
                  textAlign: 'center',
                  color: FINCEPT.GRAY,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '4px'
                }}>
                  <div style={{ fontSize: '14px', fontWeight: 600, marginBottom: '8px' }}>
                    No Exchange-Specific Features
                  </div>
                  <div style={{ fontSize: '11px' }}>
                    The selected exchange does not have additional features available in this panel.
                  </div>
                </div>
              )}
            </div>
          )}

          {/* Cross-Exchange Tab - Multi-Exchange Portfolio & Arbitrage */}
          {activeTab === 'cross-exchange' && (
            <div style={{ padding: '16px', overflow: 'auto' }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                <PortfolioAggregator />
                <ArbitrageDetector />
              </div>
            </div>
          )}

          {/* Grid Trading Tab */}
          {activeTab === 'grid-trading' && (
            <div style={{ padding: '16px', overflow: 'auto', height: '100%' }}>
              <GridTradingPanel
                symbol={selectedSymbol}
                currentPrice={tickerData?.last || tickerData?.close || 0}
                brokerType="crypto"
                brokerId={activeBroker || ''}
                cryptoAdapter={realAdapter || undefined}
                variant="full"
              />
            </div>
          )}
        </div>
      )}
    </div>
  );
}

export default CryptoBottomPanel;
