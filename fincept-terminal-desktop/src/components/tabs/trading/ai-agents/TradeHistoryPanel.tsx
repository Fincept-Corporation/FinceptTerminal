/**
 * Trade History Panel
 * Display agent trade history and performance metrics
 */

import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown, Activity, BarChart3, RefreshCw, Filter, ArrowUpDown } from 'lucide-react';
import agnoTradingService, { type AgentTrade, type AgentPerformance } from '../../../../services/agnoTradingService';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface TradeHistoryPanelProps {
  agentId?: string;
  autoRefresh?: boolean;
  refreshInterval?: number;
}

type SortField = 'date' | 'symbol' | 'pnl' | 'pnl_percent';
type SortDirection = 'asc' | 'desc';

export function TradeHistoryPanel({
  agentId,
  autoRefresh = true,
  refreshInterval = 10000
}: TradeHistoryPanelProps) {
  const [trades, setTrades] = useState<AgentTrade[]>([]);
  const [performance, setPerformance] = useState<AgentPerformance | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [filterStatus, setFilterStatus] = useState<string>('all');
  const [sortField, setSortField] = useState<SortField>('date');
  const [sortDirection, setSortDirection] = useState<SortDirection>('desc');
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    fetchData();

    if (autoRefresh) {
      const interval = setInterval(fetchData, refreshInterval);
      return () => clearInterval(interval);
    }
  }, [agentId, filterStatus, autoRefresh, refreshInterval]);

  const fetchData = async () => {
    try {
      setIsLoading(true);
      setError(null);

      // Fetch trades
      const tradesResponse = await agnoTradingService.getAgentTrades(
        agentId || 'all',
        100,
        filterStatus === 'all' ? undefined : filterStatus
      );

      if (tradesResponse.success && tradesResponse.data) {
        setTrades(tradesResponse.data.trades || []);
      }

      // Fetch performance if agent specified
      if (agentId) {
        const perfResponse = await agnoTradingService.getAgentPerformance(agentId);
        if (perfResponse.success && perfResponse.data) {
          setPerformance(perfResponse.data.performance);
        }
      }
    } catch (err) {
      console.error('Failed to fetch trade data:', err);
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const handleSort = (field: SortField) => {
    if (sortField === field) {
      setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc');
    } else {
      setSortField(field);
      setSortDirection('desc');
    }
  };

  const sortedTrades = [...trades].sort((a, b) => {
    const multiplier = sortDirection === 'asc' ? 1 : -1;

    switch (sortField) {
      case 'date':
        return (a.entry_timestamp - b.entry_timestamp) * multiplier;
      case 'symbol':
        return a.symbol.localeCompare(b.symbol) * multiplier;
      case 'pnl':
        return (a.pnl - b.pnl) * multiplier;
      case 'pnl_percent':
        return (a.pnl_percent - b.pnl_percent) * multiplier;
      default:
        return 0;
    }
  });

  return (
    <div style={{
      height: '100%',
      background: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderRadius: '4px',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        background: BLOOMBERG.HEADER_BG,
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        padding: '8px 10px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <BarChart3 size={14} color={BLOOMBERG.CYAN} />
          <span style={{
            color: BLOOMBERG.WHITE,
            fontSize: '11px',
            fontWeight: '600',
            letterSpacing: '0.5px'
          }}>
            TRADE HISTORY
          </span>
          {isLoading && (
            <RefreshCw size={10} color={BLOOMBERG.ORANGE} className="animate-spin" />
          )}
        </div>

        <div style={{ display: 'flex', gap: '4px' }}>
          {/* Status Filter */}
          {['all', 'open', 'closed'].map(status => (
            <button
              key={status}
              onClick={() => setFilterStatus(status)}
              style={{
                background: filterStatus === status ? BLOOMBERG.ORANGE : 'transparent',
                border: `1px solid ${filterStatus === status ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                color: filterStatus === status ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                padding: '2px 6px',
                borderRadius: '2px',
                fontSize: '8px',
                fontWeight: '700',
                cursor: 'pointer',
                transition: 'all 0.2s',
                textTransform: 'uppercase',
                letterSpacing: '0.3px'
              }}
            >
              {status}
            </button>
          ))}
        </div>
      </div>

      {/* Performance Summary */}
      {performance && (
        <div style={{
          background: BLOOMBERG.DARK_BG,
          borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '8px 10px',
          display: 'grid',
          gridTemplateColumns: 'repeat(4, 1fr)',
          gap: '8px'
        }}>
          <MetricCard
            label="TOTAL P&L"
            value={`$${performance.total_pnl.toFixed(2)}`}
            color={performance.total_pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
          />
          <MetricCard
            label="WIN RATE"
            value={`${performance.win_rate.toFixed(1)}%`}
            color={performance.win_rate >= 50 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
          />
          <MetricCard
            label="SHARPE"
            value={performance.sharpe_ratio.toFixed(2)}
            color={performance.sharpe_ratio >= 1 ? BLOOMBERG.GREEN : BLOOMBERG.ORANGE}
          />
          <MetricCard
            label="TRADES"
            value={performance.total_trades.toString()}
            color={BLOOMBERG.CYAN}
          />
          <MetricCard
            label="DAILY P&L"
            value={`$${performance.daily_pnl.toFixed(2)}`}
            color={performance.daily_pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
          />
          <MetricCard
            label="MAX DD"
            value={`${performance.max_drawdown.toFixed(2)}%`}
            color={BLOOMBERG.RED}
          />
          <MetricCard
            label="PROFIT FACTOR"
            value={performance.profit_factor.toFixed(2)}
            color={performance.profit_factor >= 1.5 ? BLOOMBERG.GREEN : BLOOMBERG.ORANGE}
          />
          <MetricCard
            label="STREAK"
            value={`${performance.consecutive_wins}W / ${performance.consecutive_losses}L`}
            color={BLOOMBERG.GRAY}
          />
        </div>
      )}

      {/* Error Display */}
      {error && (
        <div style={{
          background: `${BLOOMBERG.RED}15`,
          border: `1px solid ${BLOOMBERG.RED}`,
          borderRadius: '2px',
          padding: '8px',
          margin: '8px',
          color: BLOOMBERG.RED,
          fontSize: '9px'
        }}>
          {error}
        </div>
      )}

      {/* Trade Table */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '8px'
      }}>
        {trades.length === 0 && !isLoading && (
          <div style={{
            textAlign: 'center',
            color: BLOOMBERG.GRAY,
            fontSize: '10px',
            marginTop: '40px'
          }}>
            No trades yet
          </div>
        )}

        {trades.length > 0 && (
          <table style={{
            width: '100%',
            borderCollapse: 'collapse',
            fontSize: '9px',
            fontFamily: '"IBM Plex Mono", monospace'
          }}>
            <thead>
              <tr style={{
                borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                color: BLOOMBERG.GRAY
              }}>
                <SortableHeader
                  label="SYMBOL"
                  field="symbol"
                  currentField={sortField}
                  direction={sortDirection}
                  onSort={handleSort}
                />
                <th style={{ padding: '6px 4px', textAlign: 'left', fontSize: '8px', fontWeight: '700' }}>
                  SIDE
                </th>
                <th style={{ padding: '6px 4px', textAlign: 'right', fontSize: '8px', fontWeight: '700' }}>
                  ENTRY
                </th>
                <th style={{ padding: '6px 4px', textAlign: 'right', fontSize: '8px', fontWeight: '700' }}>
                  EXIT
                </th>
                <SortableHeader
                  label="P&L"
                  field="pnl"
                  currentField={sortField}
                  direction={sortDirection}
                  onSort={handleSort}
                  align="right"
                />
                <SortableHeader
                  label="P&L%"
                  field="pnl_percent"
                  currentField={sortField}
                  direction={sortDirection}
                  onSort={handleSort}
                  align="right"
                />
                <th style={{ padding: '6px 4px', textAlign: 'center', fontSize: '8px', fontWeight: '700' }}>
                  STATUS
                </th>
                <SortableHeader
                  label="DATE"
                  field="date"
                  currentField={sortField}
                  direction={sortDirection}
                  onSort={handleSort}
                  align="right"
                />
              </tr>
            </thead>
            <tbody>
              {sortedTrades.map((trade, idx) => (
                <TradeRow key={trade.id || idx} trade={trade} />
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}

// ============================================================================
// Metric Card Component
// ============================================================================

interface MetricCardProps {
  label: string;
  value: string;
  color: string;
}

function MetricCard({ label, value, color }: MetricCardProps) {
  return (
    <div style={{
      background: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderRadius: '2px',
      padding: '4px 6px',
      display: 'flex',
      flexDirection: 'column',
      gap: '2px'
    }}>
      <span style={{
        color: BLOOMBERG.GRAY,
        fontSize: '7px',
        fontWeight: '700',
        letterSpacing: '0.5px'
      }}>
        {label}
      </span>
      <span style={{
        color: color,
        fontSize: '10px',
        fontWeight: '600',
        fontFamily: '"IBM Plex Mono", monospace'
      }}>
        {value}
      </span>
    </div>
  );
}

// ============================================================================
// Sortable Header Component
// ============================================================================

interface SortableHeaderProps {
  label: string;
  field: SortField;
  currentField: SortField;
  direction: SortDirection;
  onSort: (field: SortField) => void;
  align?: 'left' | 'right' | 'center';
}

function SortableHeader({
  label,
  field,
  currentField,
  direction,
  onSort,
  align = 'left'
}: SortableHeaderProps) {
  const isActive = currentField === field;

  return (
    <th
      onClick={() => onSort(field)}
      style={{
        padding: '6px 4px',
        textAlign: align,
        fontSize: '8px',
        fontWeight: '700',
        cursor: 'pointer',
        userSelect: 'none',
        transition: 'color 0.2s'
      }}
      onMouseEnter={(e) => {
        e.currentTarget.style.color = BLOOMBERG.ORANGE;
      }}
      onMouseLeave={(e) => {
        e.currentTarget.style.color = isActive ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY;
      }}
    >
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '2px',
        justifyContent: align === 'right' ? 'flex-end' : align === 'center' ? 'center' : 'flex-start',
        color: isActive ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY
      }}>
        {label}
        {isActive && (
          <ArrowUpDown size={8} style={{
            transform: direction === 'asc' ? 'rotate(180deg)' : 'none',
            transition: 'transform 0.2s'
          }} />
        )}
      </div>
    </th>
  );
}

// ============================================================================
// Trade Row Component
// ============================================================================

interface TradeRowProps {
  trade: AgentTrade;
}

function TradeRow({ trade }: TradeRowProps) {
  const pnlColor = trade.pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED;
  const statusColor = trade.status === 'open' ? BLOOMBERG.CYAN :
                     trade.status === 'closed' ? BLOOMBERG.GRAY :
                     BLOOMBERG.YELLOW;

  return (
    <tr
      style={{
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        transition: 'background 0.2s'
      }}
      onMouseEnter={(e) => {
        e.currentTarget.style.background = BLOOMBERG.HOVER;
      }}
      onMouseLeave={(e) => {
        e.currentTarget.style.background = 'transparent';
      }}
    >
      {/* Symbol */}
      <td style={{
        padding: '6px 4px',
        color: BLOOMBERG.WHITE,
        fontWeight: '600'
      }}>
        {trade.symbol}
      </td>

      {/* Side */}
      <td style={{
        padding: '6px 4px',
        color: trade.side === 'buy' ? BLOOMBERG.GREEN : BLOOMBERG.RED,
        fontWeight: '700',
        textTransform: 'uppercase'
      }}>
        {trade.side}
      </td>

      {/* Entry Price */}
      <td style={{
        padding: '6px 4px',
        textAlign: 'right',
        color: BLOOMBERG.WHITE
      }}>
        ${trade.entry_price.toFixed(2)}
      </td>

      {/* Exit Price */}
      <td style={{
        padding: '6px 4px',
        textAlign: 'right',
        color: trade.exit_price ? BLOOMBERG.WHITE : BLOOMBERG.MUTED
      }}>
        {trade.exit_price ? `$${trade.exit_price.toFixed(2)}` : '-'}
      </td>

      {/* P&L */}
      <td style={{
        padding: '6px 4px',
        textAlign: 'right',
        color: pnlColor,
        fontWeight: '700'
      }}>
        {trade.pnl >= 0 ? '+' : ''}${trade.pnl.toFixed(2)}
      </td>

      {/* P&L % */}
      <td style={{
        padding: '6px 4px',
        textAlign: 'right',
        color: pnlColor,
        fontWeight: '700'
      }}>
        {trade.pnl_percent >= 0 ? '+' : ''}{trade.pnl_percent.toFixed(2)}%
      </td>

      {/* Status */}
      <td style={{
        padding: '6px 4px',
        textAlign: 'center'
      }}>
        <span style={{
          background: `${statusColor}20`,
          border: `1px solid ${statusColor}`,
          borderRadius: '2px',
          padding: '1px 4px',
          color: statusColor,
          fontSize: '7px',
          fontWeight: '700',
          textTransform: 'uppercase',
          letterSpacing: '0.3px'
        }}>
          {trade.status}
        </span>
      </td>

      {/* Date */}
      <td style={{
        padding: '6px 4px',
        textAlign: 'right',
        color: BLOOMBERG.GRAY,
        fontSize: '8px'
      }}>
        {new Date(trade.entry_timestamp * 1000).toLocaleDateString()}
      </td>
    </tr>
  );
}

export default TradeHistoryPanel;
