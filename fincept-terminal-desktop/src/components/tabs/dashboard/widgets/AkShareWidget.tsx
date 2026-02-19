import React from 'react';
import { TrendingUp, TrendingDown, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { invoke } from '@tauri-apps/api/core';
import { useCache } from '@/hooks/useCache';

interface AkShareWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface ChineseIndex {
  symbol: string;
  name: string;
  price: number;
  change: number;
  change_pct: number;
}

interface AkShareData {
  indices: ChineseIndex[];
  lastUpdate: string;
}

// Major Chinese indices from stock_zh_index_spot_em (East Money A-share index spot)
// Fields: 代码 (code), 名称 (name), 最新价 (price), 涨跌幅 (% chg), 涨跌额 (chg amt)
const CN_INDEX_KEYWORDS = [
  { keyword: '上证指数', name: 'Shanghai Comp.', symbol: '000001' },
  { keyword: '深证成指', name: 'Shenzhen Comp.', symbol: '399001' },
  { keyword: '沪深300', name: 'CSI 300', symbol: '000300' },
  { keyword: '上证50', name: 'SSE 50', symbol: '000016' },
  { keyword: '创业板指', name: 'ChiNext', symbol: '399006' },
  { keyword: '中证500', name: 'CSI 500', symbol: '000905' },
  { keyword: '中证1000', name: 'CSI 1000', symbol: '000852' },
  { keyword: '科创50', name: 'STAR 50', symbol: '000688' },
  { keyword: '深证100', name: 'SZSE 100', symbol: '399004' },
  { keyword: '中小100', name: 'SME 100', symbol: '399005' },
  { keyword: '上证180', name: 'SSE 180', symbol: '000010' },
  { keyword: '创业板50', name: 'ChiNext 50', symbol: '399673' },
];

export const AkShareWidget: React.FC<AkShareWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<AkShareData>({
    key: 'widget:akshare-indices',
    category: 'markets',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        // Use stock_zh_index_spot_em (ak.stock_zh_index_spot_em) which returns
        // Chinese A-share indices with fields: 代码, 名称, 最新价, 涨跌幅, 涨跌额
        const result = await invoke<any>('akshare_query', {
          script: 'akshare_stocks_realtime.py',
          endpoint: 'stock_zh_index_spot_em',
          args: null,
        });
        const rawData: any[] = result?.data ?? [];
        const indices: ChineseIndex[] = [];

        for (const idx of CN_INDEX_KEYWORDS) {
          const row = rawData.find((r: any) => {
            // Match by code first (代码 field), then by name keyword
            const code = String(r['代码'] ?? r['code'] ?? '');
            if (code === idx.symbol) return true;
            const name = String(r['名称'] ?? r['name'] ?? '');
            return name.includes(idx.keyword);
          });
          if (row) {
            const price = parseFloat(row['最新价'] ?? row['price'] ?? 0);
            const change = parseFloat(row['涨跌额'] ?? row['change'] ?? 0);
            const changePct = parseFloat(row['涨跌幅'] ?? row['change_pct'] ?? 0);
            if (price > 0) {
              indices.push({ symbol: idx.symbol, name: idx.name, price, change, change_pct: changePct });
            }
          }
        }

        return { indices, lastUpdate: new Date().toLocaleTimeString() };
      } catch {
        return { indices: [], lastUpdate: '' };
      }
    }
  });

  return (
    <BaseWidget
      id={id}
      title="CHINA MARKETS"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-alert)"
    >
      <div style={{ padding: '4px' }}>
        {/* Header */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 70px 65px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
          <span>INDEX</span>
          <span style={{ textAlign: 'right' }}>PRICE</span>
          <span style={{ textAlign: 'right' }}>CHANGE%</span>
        </div>

        {(!data || data.indices.length === 0) ? (
          <div style={{ padding: '20px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-tiny)' }}>
            No data available
          </div>
        ) : data.indices.map(idx => {
          const pos = idx.change_pct >= 0;
          const clr = pos ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';
          return (
            <div key={idx.symbol} style={{ display: 'grid', gridTemplateColumns: '1fr 70px 65px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
              <div>
                <div style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{idx.name}</div>
                <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>{idx.symbol}</div>
              </div>
              <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                {idx.price.toLocaleString('en-US', { maximumFractionDigits: 2 })}
              </span>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '3px' }}>
                {pos ? <TrendingUp size={10} color={clr} /> : <TrendingDown size={10} color={clr} />}
                <span style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: clr }}>
                  {pos ? '+' : ''}{idx.change_pct.toFixed(2)}%
                </span>
              </div>
            </div>
          );
        })}

        {data?.lastUpdate && (
          <div style={{ padding: '4px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', textAlign: 'right' }}>
            Updated: {data.lastUpdate}
          </div>
        )}

        {onNavigate && (
          <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-alert)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
            <span>OPEN AKSHARE DATA</span><ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
