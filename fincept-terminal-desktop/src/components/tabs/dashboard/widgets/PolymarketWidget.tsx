import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown, Activity, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import polymarketService, { PolymarketMarket } from '@/services/polymarketService';

interface PolymarketWidgetProps {
  id: string;
  limit?: number;
  onRemove?: () => void;
  onNavigate?: () => void;
}

const BLOOMBERG_GREEN = '#00FF00';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_ORANGE = '#FFA500';
const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GRAY = '#787878';

export const PolymarketWidget: React.FC<PolymarketWidgetProps> = ({
  id,
  limit = 5,
  onRemove,
  onNavigate
}) => {
  const [markets, setMarkets] = useState<PolymarketMarket[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadMarkets = async () => {
    try {
      setLoading(true);
      setError(null);
      const data = await polymarketService.getMarkets({ limit, active: true });
      setMarkets(data || []);
    } catch (err) {
      setError('Failed to load markets');
      console.error('Polymarket widget error:', err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadMarkets();
    const interval = setInterval(loadMarkets, 60000); // Refresh every minute
    return () => clearInterval(interval);
  }, [limit]);

  const formatVolume = (volume: number) => {
    if (volume >= 1000000) return `$${(volume / 1000000).toFixed(1)}M`;
    if (volume >= 1000) return `$${(volume / 1000).toFixed(0)}K`;
    return `$${volume.toFixed(0)}`;
  };

  return (
    <BaseWidget
      id={id}
      title="POLYMARKET - TOP MARKETS"
      onRemove={onRemove}
      onRefresh={loadMarkets}
      isLoading={loading}
      error={error}
      headerColor="#9D4EDD"
    >
      <div style={{ padding: '4px' }}>
        {markets.slice(0, limit).map((market, index) => (
          <div
            key={market.id || index}
            style={{
              padding: '6px 8px',
              borderBottom: `1px solid #333`,
              display: 'flex',
              flexDirection: 'column',
              gap: '4px'
            }}
          >
            <div style={{
              fontSize: '10px',
              color: BLOOMBERG_WHITE,
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              whiteSpace: 'nowrap'
            }}>
              {market.question}
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div style={{ display: 'flex', gap: '12px' }}>
                <span style={{ fontSize: '10px', color: BLOOMBERG_GREEN }}>
                  YES: {((parseFloat(market.outcomePrices?.[0] || '0')) * 100).toFixed(0)}%
                </span>
                <span style={{ fontSize: '10px', color: BLOOMBERG_RED }}>
                  NO: {((parseFloat(market.outcomePrices?.[1] || '0')) * 100).toFixed(0)}%
                </span>
              </div>
              <span style={{ fontSize: '9px', color: BLOOMBERG_GRAY }}>
                Vol: {formatVolume(parseFloat(market.volume || '0'))}
              </span>
            </div>
          </div>
        ))}

        {markets.length === 0 && !loading && (
          <div style={{ padding: '12px', textAlign: 'center', color: BLOOMBERG_GRAY, fontSize: '10px' }}>
            No active markets found
          </div>
        )}

        {onNavigate && (
          <div
            onClick={onNavigate}
            style={{
              padding: '6px',
              textAlign: 'center',
              color: '#9D4EDD',
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px'
            }}
          >
            <span>Open Polymarket Tab</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
