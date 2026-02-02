/**
 * IndexHistoryView - Historical Index Performance Component
 * Displays historical snapshots and performance charts for custom indices
 */

import React, { useState, useEffect } from 'react';
import {
  TrendingUp,
  TrendingDown,
  Calendar,
  RefreshCw,
  Download,
  Trash2,
} from 'lucide-react';
import {
  FINCEPT,
  TYPOGRAPHY,
  SPACING,
  BORDERS,
  COMMON_STYLES,
  EFFECTS,
  getValueColor,
  formatPercentage,
  formatCurrency,
} from '../finceptStyles';
import { IndexSnapshot, CustomIndex } from './types';
import { customIndexService } from '../../../../services/portfolio/customIndexService';
import { showWarning, showSuccess, showError, showPrompt } from '@/utils/notifications';

interface IndexHistoryViewProps {
  index: CustomIndex;
}

const IndexHistoryView: React.FC<IndexHistoryViewProps> = ({ index }) => {
  const [snapshots, setSnapshots] = useState<IndexSnapshot[]>([]);
  const [loading, setLoading] = useState(false);
  const [selectedDays, setSelectedDays] = useState<number>(30);

  useEffect(() => {
    loadSnapshots();
  }, [index.id, selectedDays]);

  const loadSnapshots = async () => {
    setLoading(true);
    try {
      const data = await customIndexService.getSnapshots(index.id, selectedDays);
      setSnapshots(data);
    } catch (error) {
      console.error('Failed to load snapshots:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleCleanup = async () => {
    const days = await showPrompt('Delete snapshots older than how many days?', {
      title: 'Cleanup Snapshots',
      defaultValue: '365',
      placeholder: 'Enter number of days'
    });
    if (!days) return;

    const daysNum = parseInt(days);
    if (isNaN(daysNum) || daysNum < 1) {
      showWarning('Invalid number of days');
      return;
    }

    try {
      const deleted = await customIndexService.cleanupSnapshots(index.id, daysNum);
      showSuccess('Snapshots deleted', [
        { label: 'COUNT', value: `${deleted} old snapshots` }
      ]);
      loadSnapshots();
    } catch (error) {
      console.error('Failed to cleanup snapshots:', error);
      showError('Failed to cleanup snapshots');
    }
  };

  const exportHistory = () => {
    if (snapshots.length === 0) return;

    let csv = 'Date,Index Value,Day Change,Day Change %,Total Market Value,Divisor\n';
    snapshots.forEach(s => {
      csv += `${s.snapshot_date},${s.index_value.toFixed(2)},${s.day_change.toFixed(2)},${s.day_change_percent.toFixed(4)},${s.total_market_value.toFixed(2)},${s.divisor.toFixed(6)}\n`;
    });

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${index.name.replace(/\s+/g, '_')}_history.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  // Calculate performance metrics
  const calculatePerformance = () => {
    if (snapshots.length < 2) return null;

    const latest = snapshots[0];
    const oldest = snapshots[snapshots.length - 1];
    const totalReturn = ((latest.index_value - oldest.index_value) / oldest.index_value) * 100;

    // Find best and worst days
    let bestDay = snapshots[0];
    let worstDay = snapshots[0];

    snapshots.forEach(s => {
      if (s.day_change_percent > bestDay.day_change_percent) bestDay = s;
      if (s.day_change_percent < worstDay.day_change_percent) worstDay = s;
    });

    // Calculate volatility (simplified - standard deviation of daily returns)
    const returns = snapshots.map(s => s.day_change_percent);
    const avgReturn = returns.reduce((sum, r) => sum + r, 0) / returns.length;
    const variance = returns.reduce((sum, r) => sum + Math.pow(r - avgReturn, 2), 0) / returns.length;
    const volatility = Math.sqrt(variance);
    const annualizedVolatility = volatility * Math.sqrt(252);

    return {
      totalReturn,
      avgDailyReturn: avgReturn,
      volatility: annualizedVolatility,
      bestDay,
      worstDay,
      high: Math.max(...snapshots.map(s => s.index_value)),
      low: Math.min(...snapshots.map(s => s.index_value)),
    };
  };

  const performance = calculatePerformance();

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      {/* Header */}
      <div
        style={{
          ...COMMON_STYLES.sectionHeader,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: 0,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
          <Calendar size={14} />
          <span>HISTORICAL DATA - {index.name}</span>
        </div>
        <div style={{ display: 'flex', gap: SPACING.SMALL }}>
          <select
            value={selectedDays}
            onChange={e => setSelectedDays(parseInt(e.target.value))}
            style={{
              ...COMMON_STYLES.inputField,
              width: 'auto',
              padding: '4px 8px',
              fontSize: TYPOGRAPHY.TINY,
            }}
          >
            <option value={7}>7 Days</option>
            <option value={30}>30 Days</option>
            <option value={90}>90 Days</option>
            <option value={180}>180 Days</option>
            <option value={365}>1 Year</option>
          </select>
          <button
            onClick={loadSnapshots}
            style={{
              ...COMMON_STYLES.buttonSecondary,
              padding: '4px 8px',
            }}
            disabled={loading}
          >
            <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
          </button>
          <button
            onClick={exportHistory}
            style={{
              ...COMMON_STYLES.buttonSecondary,
              padding: '4px 8px',
            }}
            disabled={snapshots.length === 0}
          >
            <Download size={12} />
          </button>
          <button
            onClick={handleCleanup}
            style={{
              ...COMMON_STYLES.buttonSecondary,
              padding: '4px 8px',
              color: FINCEPT.RED,
            }}
          >
            <Trash2 size={12} />
          </button>
        </div>
      </div>

      {/* Performance Summary */}
      {performance && (
        <div
          style={{
            padding: SPACING.DEFAULT,
            borderBottom: BORDERS.STANDARD,
            display: 'grid',
            gridTemplateColumns: 'repeat(6, 1fr)',
            gap: SPACING.DEFAULT,
          }}
        >
          <div>
            <div style={COMMON_STYLES.dataLabel}>PERIOD RETURN</div>
            <div
              style={{
                color: getValueColor(performance.totalReturn),
                fontSize: TYPOGRAPHY.SUBHEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}
            >
              {formatPercentage(performance.totalReturn)}
            </div>
          </div>
          <div>
            <div style={COMMON_STYLES.dataLabel}>AVG DAILY</div>
            <div
              style={{
                color: getValueColor(performance.avgDailyReturn),
                fontSize: TYPOGRAPHY.BODY,
                fontFamily: TYPOGRAPHY.MONO,
              }}
            >
              {formatPercentage(performance.avgDailyReturn, 4)}
            </div>
          </div>
          <div>
            <div style={COMMON_STYLES.dataLabel}>VOLATILITY (Ann.)</div>
            <div
              style={{
                color: FINCEPT.YELLOW,
                fontSize: TYPOGRAPHY.BODY,
                fontFamily: TYPOGRAPHY.MONO,
              }}
            >
              {performance.volatility.toFixed(2)}%
            </div>
          </div>
          <div>
            <div style={COMMON_STYLES.dataLabel}>HIGH</div>
            <div
              style={{
                color: FINCEPT.GREEN,
                fontSize: TYPOGRAPHY.BODY,
                fontFamily: TYPOGRAPHY.MONO,
              }}
            >
              {formatCurrency(performance.high, '', 2)}
            </div>
          </div>
          <div>
            <div style={COMMON_STYLES.dataLabel}>LOW</div>
            <div
              style={{
                color: FINCEPT.RED,
                fontSize: TYPOGRAPHY.BODY,
                fontFamily: TYPOGRAPHY.MONO,
              }}
            >
              {formatCurrency(performance.low, '', 2)}
            </div>
          </div>
          <div>
            <div style={COMMON_STYLES.dataLabel}>DATA POINTS</div>
            <div
              style={{
                color: FINCEPT.CYAN,
                fontSize: TYPOGRAPHY.BODY,
                fontFamily: TYPOGRAPHY.MONO,
              }}
            >
              {snapshots.length}
            </div>
          </div>
        </div>
      )}

      {/* Best/Worst Days */}
      {performance && (
        <div
          style={{
            padding: SPACING.DEFAULT,
            borderBottom: BORDERS.STANDARD,
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: SPACING.LARGE,
          }}
        >
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
            }}
          >
            <TrendingUp size={16} style={{ color: FINCEPT.GREEN }} />
            <div>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: '2px' }}>BEST DAY</div>
              <div style={{ color: FINCEPT.WHITE, fontSize: TYPOGRAPHY.SMALL }}>
                {performance.bestDay.snapshot_date}
              </div>
              <div
                style={{
                  color: FINCEPT.GREEN,
                  fontSize: TYPOGRAPHY.BODY,
                  fontWeight: TYPOGRAPHY.BOLD,
                  fontFamily: TYPOGRAPHY.MONO,
                }}
              >
                {formatPercentage(performance.bestDay.day_change_percent)}
              </div>
            </div>
          </div>
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
            }}
          >
            <TrendingDown size={16} style={{ color: FINCEPT.RED }} />
            <div>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: '2px' }}>WORST DAY</div>
              <div style={{ color: FINCEPT.WHITE, fontSize: TYPOGRAPHY.SMALL }}>
                {performance.worstDay.snapshot_date}
              </div>
              <div
                style={{
                  color: FINCEPT.RED,
                  fontSize: TYPOGRAPHY.BODY,
                  fontWeight: TYPOGRAPHY.BOLD,
                  fontFamily: TYPOGRAPHY.MONO,
                }}
              >
                {formatPercentage(performance.worstDay.day_change_percent)}
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Snapshot Table */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        <div
          style={{
            ...COMMON_STYLES.tableHeader,
            display: 'grid',
            gridTemplateColumns: '120px 100px 80px 80px 100px 80px',
          }}
        >
          <span>DATE</span>
          <span style={{ textAlign: 'right' }}>VALUE</span>
          <span style={{ textAlign: 'right' }}>CHANGE</span>
          <span style={{ textAlign: 'right' }}>%</span>
          <span style={{ textAlign: 'right' }}>MKT VALUE</span>
          <span style={{ textAlign: 'right' }}>DIVISOR</span>
        </div>

        <div style={{ flex: 1, overflowY: 'auto' }}>
          {loading ? (
            <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.LARGE }}>
              <RefreshCw size={24} className="animate-spin" />
              <span style={{ marginTop: SPACING.SMALL }}>Loading history...</span>
            </div>
          ) : snapshots.length === 0 ? (
            <div style={{ ...COMMON_STYLES.emptyState, padding: SPACING.LARGE }}>
              <Calendar size={32} style={{ opacity: 0.5 }} />
              <span style={{ marginTop: SPACING.SMALL }}>No historical data available</span>
              <span style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginTop: SPACING.TINY }}>
                Snapshots are saved when you click the save button
              </span>
            </div>
          ) : (
            snapshots.map((snapshot, i) => (
              <div
                key={snapshot.id}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '120px 100px 80px 80px 100px 80px',
                  padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                  borderBottom: BORDERS.STANDARD,
                  backgroundColor: i % 2 === 1 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  fontSize: TYPOGRAPHY.BODY,
                }}
              >
                <span style={{ color: FINCEPT.WHITE }}>{snapshot.snapshot_date}</span>
                <span
                  style={{
                    textAlign: 'right',
                    color: FINCEPT.CYAN,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}
                >
                  {formatCurrency(snapshot.index_value, '', 2)}
                </span>
                <span
                  style={{
                    textAlign: 'right',
                    color: getValueColor(snapshot.day_change),
                    fontFamily: TYPOGRAPHY.MONO,
                  }}
                >
                  {snapshot.day_change >= 0 ? '+' : ''}
                  {snapshot.day_change.toFixed(2)}
                </span>
                <span
                  style={{
                    textAlign: 'right',
                    color: getValueColor(snapshot.day_change_percent),
                    fontFamily: TYPOGRAPHY.MONO,
                  }}
                >
                  {formatPercentage(snapshot.day_change_percent)}
                </span>
                <span
                  style={{
                    textAlign: 'right',
                    color: FINCEPT.WHITE,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}
                >
                  {formatCurrency(snapshot.total_market_value / 1000, '$', 0)}K
                </span>
                <span
                  style={{
                    textAlign: 'right',
                    color: FINCEPT.GRAY,
                    fontFamily: TYPOGRAPHY.MONO,
                    fontSize: TYPOGRAPHY.TINY,
                  }}
                >
                  {snapshot.divisor.toFixed(4)}
                </span>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
};

export default IndexHistoryView;
