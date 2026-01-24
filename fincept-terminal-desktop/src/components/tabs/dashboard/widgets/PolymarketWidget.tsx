import React, { useState, useEffect } from 'react';
import { ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import polymarketService, { PolymarketMarket } from '@/services/polymarket/polymarketService';
import { useTranslation } from 'react-i18next';

interface PolymarketWidgetProps {
  id: string;
  limit?: number;
  onRemove?: () => void;
  onNavigate?: () => void;
}


export const PolymarketWidget: React.FC<PolymarketWidgetProps> = ({
  id,
  limit = 5,
  onRemove,
  onNavigate
}) => {
  const { t } = useTranslation('dashboard');
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
      headerColor="var(--ft-color-purple)"
    >
      <div style={{ padding: '4px' }}>
        {markets.slice(0, limit).map((market, index) => (
          <div
            key={market.id || index}
            style={{
              padding: '6px 8px',
              borderBottom: '1px solid var(--ft-border-color)',
              display: 'flex',
              flexDirection: 'column',
              gap: '4px'
            }}
          >
            <div style={{
              fontSize: 'var(--ft-font-size-small)',
              color: 'var(--ft-color-text)',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              whiteSpace: 'nowrap'
            }}>
              {market.question}
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div style={{ display: 'flex', gap: '12px' }}>
                <span style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-success)' }}>
                  YES: {((parseFloat(market.outcomePrices?.[0] || '0')) * 100).toFixed(0)}%
                </span>
                <span style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-alert)' }}>
                  NO: {((parseFloat(market.outcomePrices?.[1] || '0')) * 100).toFixed(0)}%
                </span>
              </div>
              <span style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>
                Vol: {formatVolume(parseFloat(market.volume || '0'))}
              </span>
            </div>
          </div>
        ))}

        {markets.length === 0 && !loading && (
          <div style={{ padding: '12px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
            {t('widgets.noActiveMarkets')}
          </div>
        )}

        {onNavigate && (
          <div
            onClick={onNavigate}
            style={{
              padding: '6px',
              textAlign: 'center',
              color: 'var(--ft-color-purple)',
              fontSize: 'var(--ft-font-size-tiny)',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px'
            }}
          >
            <span>{t('widgets.openPolymarketTab')}</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
