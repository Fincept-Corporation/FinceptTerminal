/**
 * EquityBottomPanel - Bottom panel with positions, holdings, orders, stats, grid trading, brokers
 */
import React from 'react';
import { ChevronUp, ChevronDown, Bell } from 'lucide-react';
import { PositionsPanel } from './PositionsPanel';
import { HoldingsPanel } from './HoldingsPanel';
import { OrdersPanel } from './OrdersPanel';
import { BrokersManagementPanel } from './BrokersManagementPanel';
import { GridTradingPanel } from '../../trading/grid-trading';
import { AlgoTradingPanel } from '../../trading/algo-trading';
import { MonitoringPanel } from '../../trading/monitoring/MonitoringPanel';
import { FINCEPT } from '../constants';
import type { EquityBottomPanelProps, BottomPanelTab } from '../types';

export const EquityBottomPanel: React.FC<EquityBottomPanelProps> = ({
  activeTab,
  isMinimized,
  positions,
  holdings,
  orders,
  funds,
  totalPositionValue,
  totalPositionPnl,
  totalHoldingsValue,
  selectedSymbol,
  selectedExchange,
  currentPrice,
  activeBroker,
  isPaper,
  adapter,
  onTabChange,
  onMinimizeToggle,
  fmtPrice,
}) => {
  return (
    <div style={{
      flex: isMinimized ? '0 0 32px' : activeTab === 'brokers' ? '0 0 350px' : '0 0 200px',
      minHeight: isMinimized ? '32px' : activeTab === 'brokers' ? '300px' : '150px',
      maxHeight: isMinimized ? '32px' : activeTab === 'brokers' ? '450px' : '220px',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      transition: 'all 0.2s ease'
    }}>
      {/* Bottom Panel Header */}
      <div style={{
        padding: '6px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{ display: 'flex', gap: '4px' }}>
          {([
            { id: 'positions', label: 'POSITIONS', count: positions.length },
            { id: 'holdings', label: 'HOLDINGS', count: holdings.length },
            { id: 'orders', label: 'ORDERS', count: orders.filter(o => ['PENDING', 'OPEN'].includes(o.status)).length },
            { id: 'history', label: 'HISTORY', count: null },
            { id: 'stats', label: 'STATS', count: null },
            { id: 'grid-trading', label: 'GRID BOT', count: null },
            { id: 'algo-trading', label: 'ALGO', count: null },
            { id: 'monitoring', label: 'MONITOR', count: null },
            { id: 'brokers', label: 'BROKERS', count: null },
          ] as const).map((tab) => (
            <button
              key={tab.id}
              onClick={() => onTabChange(tab.id as BottomPanelTab)}
              style={{
                padding: '6px 16px',
                backgroundColor: activeTab === tab.id ? FINCEPT.ORANGE : 'transparent',
                border: 'none',
                borderBottom: activeTab === tab.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                color: activeTab === tab.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                textTransform: 'uppercase',
                transition: 'all 0.15s'
              }}
            >
              {tab.label} {tab.count !== null && `(${tab.count})`}
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
            transition: 'all 0.15s'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            e.currentTarget.style.borderColor = FINCEPT.ORANGE;
            e.currentTarget.style.color = FINCEPT.ORANGE;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.borderColor = FINCEPT.BORDER;
            e.currentTarget.style.color = FINCEPT.GRAY;
          }}
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

      {/* Bottom Panel Content */}
      {!isMinimized && (
        <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
          {activeTab === 'positions' && <PositionsPanel />}
          {activeTab === 'holdings' && <HoldingsPanel />}
          {activeTab === 'orders' && <OrdersPanel />}
          {activeTab === 'history' && <OrdersPanel showHistory />}
          {activeTab === 'stats' && (
            <div style={{
              padding: '20px',
              display: 'grid',
              gridTemplateColumns: 'repeat(4, 1fr)',
              gap: '16px'
            }}>
              <div style={{
                padding: '16px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>TOTAL POSITIONS VALUE</div>
                <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.CYAN }}>
                  {fmtPrice(totalPositionValue)}
                </div>
              </div>
              <div style={{
                padding: '16px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>DAY P&L</div>
                <div style={{
                  fontSize: '18px',
                  fontWeight: 700,
                  color: totalPositionPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED
                }}>
                  {totalPositionPnl >= 0 ? '+' : '-'}{fmtPrice(Math.abs(totalPositionPnl))}
                </div>
              </div>
              <div style={{
                padding: '16px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>PORTFOLIO VALUE</div>
                <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.YELLOW }}>
                  {fmtPrice(totalHoldingsValue)}
                </div>
              </div>
              <div style={{
                padding: '16px',
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>AVAILABLE MARGIN</div>
                <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.PURPLE }}>
                  {fmtPrice(funds?.availableMargin || 0)}
                </div>
              </div>
            </div>
          )}

          {/* Grid Trading Tab */}
          {activeTab === 'grid-trading' && (
            <div style={{ padding: '16px', overflow: 'auto', height: '100%' }}>
              <GridTradingPanel
                symbol={selectedSymbol}
                exchange={selectedExchange}
                currentPrice={currentPrice}
                brokerType="stock"
                brokerId={activeBroker || ''}
                stockAdapter={adapter || undefined}
                variant="full"
              />
            </div>
          )}

          {/* Algo Trading Tab */}
          {activeTab === 'algo-trading' && (
            <div style={{ height: '100%', overflow: 'hidden' }}>
              <AlgoTradingPanel
                assetType="equity"
                symbol={selectedSymbol}
                currentPrice={currentPrice}
                activeBroker={activeBroker}
                isPaper={isPaper}
              />
            </div>
          )}

          {/* Monitoring Tab */}
          {activeTab === 'monitoring' && (
            <div style={{ height: '100%', overflow: 'hidden' }}>
              <MonitoringPanel
                provider={activeBroker || 'fyers'}
                symbol={selectedSymbol}
                assetType="equity"
              />
            </div>
          )}

          {/* Brokers Management Tab */}
          {activeTab === 'brokers' && (
            <div style={{ height: '100%', overflow: 'hidden' }}>
              <BrokersManagementPanel />
            </div>
          )}
        </div>
      )}
    </div>
  );
};
