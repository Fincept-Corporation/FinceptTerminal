import React, { useEffect, useState } from 'react';
import { ShieldAlert, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { portfolioService } from '@/services/portfolio/portfolioService';
import type { PortfolioSummary } from '@/services/portfolio/portfolioService';
import { usePortfolioMetrics } from '@/components/tabs/portfolio-tab/hooks/usePortfolioMetrics';

interface RiskMetricsWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

const MetricRow: React.FC<{ label: string; value: string | null; color?: string; tooltip?: string }> = ({ label, value, color }) => (
  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)' }}>
    <span style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>{label}</span>
    <span style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: color || 'var(--ft-color-text)' }}>
      {value ?? <span style={{ color: 'var(--ft-color-text-muted)' }}>—</span>}
    </span>
  </div>
);

export const RiskMetricsWidget: React.FC<RiskMetricsWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const [summary, setSummary] = useState<PortfolioSummary | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    const load = async () => {
      try {
        setLoading(true);
        await portfolioService.initialize();
        const portfolios = await portfolioService.getPortfolios();
        if (portfolios.length === 0) {
          if (!cancelled) { setSummary(null); setLoading(false); }
          return;
        }
        const s = await portfolioService.getPortfolioSummary(portfolios[0].id);
        if (!cancelled) { setSummary(s); setLoading(false); }
      } catch (e: any) {
        if (!cancelled) { setError(e?.message || 'Failed to load'); setLoading(false); }
      }
    };
    load();
    return () => { cancelled = true; };
  }, []);

  const metrics = usePortfolioMetrics(summary);

  const sharpeColor = metrics.sharpe == null ? undefined : metrics.sharpe >= 1 ? 'var(--ft-color-success)' : metrics.sharpe >= 0 ? 'var(--ft-color-warning)' : 'var(--ft-color-alert)';
  const betaColor = metrics.beta == null ? undefined : Math.abs(metrics.beta - 1) < 0.2 ? 'var(--ft-color-success)' : 'var(--ft-color-warning)';
  const riskColor = metrics.riskScore == null ? undefined : metrics.riskScore < 30 ? 'var(--ft-color-success)' : metrics.riskScore < 60 ? 'var(--ft-color-warning)' : 'var(--ft-color-alert)';

  const formatCurrency = (v: number) => v >= 1000 ? `$${(v / 1000).toFixed(1)}K` : `$${v.toFixed(0)}`;

  return (
    <BaseWidget
      id={id}
      title="RISK METRICS"
      onRemove={onRemove}
      onRefresh={() => window.location.reload()}
      isLoading={loading}
      error={error}
      headerColor="var(--ft-color-alert)"
    >
      {!summary ? (
        <div style={{ padding: '24px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
          <ShieldAlert size={32} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
          <div>No portfolio data</div>
          <div style={{ fontSize: 'var(--ft-font-size-tiny)', marginTop: '4px' }}>Add holdings to see risk metrics</div>
        </div>
      ) : (
        <div>
          {/* Risk Score Banner */}
          {metrics.riskScore != null && (
            <div style={{ padding: '10px 12px', backgroundColor: 'var(--ft-color-panel)', margin: '4px', borderRadius: '2px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>RISK SCORE</div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{ width: '80px', height: '6px', backgroundColor: 'var(--ft-border-color)', borderRadius: '3px', overflow: 'hidden' }}>
                  <div style={{ width: `${metrics.riskScore}%`, height: '100%', backgroundColor: riskColor, borderRadius: '3px' }} />
                </div>
                <span style={{ fontSize: 'var(--ft-font-size-heading)', fontWeight: 'bold', color: riskColor }}>{metrics.riskScore}/100</span>
              </div>
            </div>
          )}

          <MetricRow label="SHARPE RATIO" value={metrics.sharpe?.toFixed(2) ?? null} color={sharpeColor} />
          <MetricRow label="BETA (vs SPY)" value={metrics.beta?.toFixed(2) ?? null} color={betaColor} />
          <MetricRow label="VOLATILITY (Ann.)" value={metrics.volatility != null ? `${metrics.volatility.toFixed(1)}%` : null} color={metrics.volatility != null && metrics.volatility > 30 ? 'var(--ft-color-alert)' : 'var(--ft-color-text)'} />
          <MetricRow label="MAX DRAWDOWN" value={metrics.maxDrawdown != null ? `${metrics.maxDrawdown.toFixed(1)}%` : null} color={metrics.maxDrawdown != null && metrics.maxDrawdown > 20 ? 'var(--ft-color-alert)' : 'var(--ft-color-text)'} />
          <MetricRow label="VAR 95% (1-Day)" value={metrics.var95 != null ? formatCurrency(metrics.var95) : null} color="var(--ft-color-alert)" />
          <MetricRow label="TOP-3 CONCENTRATION" value={metrics.concentrationTop3 != null ? `${metrics.concentrationTop3.toFixed(1)}%` : null} color={metrics.concentrationTop3 != null && metrics.concentrationTop3 > 60 ? 'var(--ft-color-alert)' : 'var(--ft-color-text)'} />

          {onNavigate && (
            <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-alert)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
              <span>OPEN PORTFOLIO</span><ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
