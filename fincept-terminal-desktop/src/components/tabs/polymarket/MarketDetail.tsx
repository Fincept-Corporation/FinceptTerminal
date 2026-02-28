// MarketDetail.tsx — right-panel market detail for the Polymarket tab
import React, { useState } from 'react';
import {
  Activity, X, BarChart3, ExternalLink, RefreshCw,
} from 'lucide-react';
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip,
  ResponsiveContainer, Legend,
} from 'recharts';
import {
  PolymarketMarket, PolymarketTrade,
  PolymarketOrderBookEnriched, PolymarketTopHolders,
  PricePoint,
} from '@/services/polymarket/polymarketApiService';
import { C, OUTCOME_COLORS, fmtVol, parseJsonArr, parseNumArr, sectionHeader, statCell } from './tokens';

// ── Props ────────────────────────────────────────────────────────────────────
export interface MarketDetailProps {
  market: PolymarketMarket | null;
  livePrices: Record<string, number>;
  wsConnected: boolean;
  // chart
  priceHistory: PricePoint[];
  noPriceHistory: PricePoint[];
  priceChartLoading: boolean;
  priceChartError: string | null;
  chartInterval: string;
  onIntervalChange: (iv: string) => void;
  // order book
  yesOrderBook: PolymarketOrderBookEnriched | null;
  noOrderBook: PolymarketOrderBookEnriched | null;
  orderBookLoading: boolean;
  orderBookError: string | null;
  // holders
  topHolders: PolymarketTopHolders | null;
  holdersLoading: boolean;
  holdersError: string | null;
  // recent trades
  recentTrades: PolymarketTrade[];
  onClose: () => void;
}

// ── Component ────────────────────────────────────────────────────────────────
const MarketDetail: React.FC<MarketDetailProps> = ({
  market, livePrices, wsConnected,
  priceHistory, noPriceHistory, priceChartLoading, priceChartError, chartInterval, onIntervalChange,
  yesOrderBook, noOrderBook, orderBookLoading, orderBookError,
  topHolders, holdersLoading, holdersError,
  recentTrades, onClose,
}) => {
  const [detailTab, setDetailTab] = useState<'chart' | 'orderbook' | 'holders' | 'trades'>('chart');
  const [tradeAmount, setTradeAmount]       = useState('');
  const [selectedOutcomeIndex, setSelectedOutcomeIndex] = useState(0);

  if (!market) return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', flexDirection: 'column', gap: 12 }}>
      <Activity size={40} style={{ color: C.faint, opacity: 0.4 }} />
      <div style={{ fontSize: 11, color: C.muted, fontFamily: C.font }}>SELECT A MARKET</div>
      <div style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>CLICK ANY ROW IN THE LIST</div>
    </div>
  );

  const parsedPrices  = parseNumArr(market.outcomePrices);
  const outcomeNames  = parseJsonArr(market.outcomes);
  let tokenIds: string[] = [];
  try {
    const raw = market.clobTokenIds;
    tokenIds = typeof raw === 'string' ? JSON.parse(raw) : Array.isArray(raw) ? raw as string[] : [];
  } catch { /* ignore */ }

  const outcomeItems = outcomeNames.map((name, i) => {
    const tokenId  = tokenIds[i];
    const livePrice = tokenId ? livePrices[tokenId] : undefined;
    return { name, price: livePrice ?? parsedPrices[i] ?? 0, isLive: livePrice !== undefined };
  });
  const detailMaxPrice = outcomeItems.length > 0 ? Math.max(...outcomeItems.map(o => o.price)) : 0;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      {/* Header */}
      <div style={{ padding: '8px 12px', borderBottom: `2px solid ${C.orange}`, backgroundColor: C.bg, flexShrink: 0 }}>
        <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', gap: 8, marginBottom: 6 }}>
          <div style={{ fontSize: 11, fontWeight: 'bold', color: C.orange, fontFamily: C.font, lineHeight: 1.3, flex: 1 }}>
            {market.question}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, flexShrink: 0 }}>
            {wsConnected && (
              <span style={{ fontSize: 9, color: C.green, display: 'flex', alignItems: 'center', gap: 3, fontFamily: C.font }}>
                <span style={{ width: 5, height: 5, borderRadius: '50%', backgroundColor: C.green, display: 'inline-block' }} /> LIVE
              </span>
            )}
            <button onClick={onClose}
              style={{ background: 'transparent', border: `1px solid ${C.border}`, color: C.muted, cursor: 'pointer', padding: '3px 6px', fontFamily: C.font, fontSize: 10, borderRadius: 2 }}>
              <X size={12} />
            </button>
          </div>
        </div>
        {/* Outcome chips */}
        <div style={{ display: 'flex', gap: 4, flexWrap: 'wrap' }}>
          {outcomeItems.map((item, i) => {
            const isLeading = item.price === detailMaxPrice && detailMaxPrice > 0;
            const col = isLeading ? OUTCOME_COLORS[0] : OUTCOME_COLORS[Math.min(i, OUTCOME_COLORS.length - 1) > 0 ? Math.min(i, OUTCOME_COLORS.length - 1) : 1];
            return (
              <div key={i} style={{ padding: '4px 10px', backgroundColor: `${col}18`, border: `1px solid ${col}44`, display: 'flex', alignItems: 'center', gap: 6 }}>
                <span style={{ fontSize: 9, color: C.muted, fontFamily: C.font, textTransform: 'uppercase' }}>{item.name}</span>
                <span style={{ fontSize: 15, fontWeight: 'bold', color: col, fontFamily: C.font }}>{(item.price * 100).toFixed(1)}%</span>
                {item.isLive && <span style={{ width: 5, height: 5, borderRadius: '50%', backgroundColor: C.orange, display: 'inline-block' }} />}
              </div>
            );
          })}
        </div>
      </div>

      {/* Stats bar */}
      {(() => {
        const v24h: number | null = (market as any).events?.[0]?.series?.[0]?.volume24hr ?? null;
        const eventOI: number | null = (market as any).events?.[0]?.openInterest ?? null;
        return (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 1, backgroundColor: C.border, flexShrink: 0 }}>
            {statCell('TOTAL VOL',   fmtVol(market.volumeNum ?? market.volume ?? 0))}
            {statCell('24H VOL',     v24h != null ? fmtVol(v24h) : 'N/A')}
            {statCell('LIQUIDITY',   fmtVol(market.liquidityNum ?? market.liquidity ?? 0))}
            {statCell('OPEN INT',    eventOI != null ? fmtVol(eventOI) : 'N/A')}
          </div>
        );
      })()}

      {/* Inner tab bar */}
      <div style={{ display: 'flex', borderBottom: `1px solid ${C.border}`, backgroundColor: C.bg, flexShrink: 0 }}>
        {(['chart', 'orderbook', 'holders', 'trades'] as const).map(tab => (
          <button key={tab} onClick={() => setDetailTab(tab)}
            style={{
              padding: '6px 12px', fontSize: 9, fontWeight: 'bold', fontFamily: C.font,
              background: detailTab === tab ? C.orange : 'transparent',
              color: detailTab === tab ? '#000' : C.muted,
              border: 'none', cursor: 'pointer', borderRight: `1px solid ${C.border}`,
              textTransform: 'uppercase', letterSpacing: '0.5px',
            }}>
            {tab}
          </button>
        ))}
        <div style={{ flex: 1 }} />
        {(market.slug || market.events?.[0]?.slug) && (
          <a href={`https://polymarket.com/event/${market.events?.[0]?.slug ?? market.slug}`}
            target="_blank" rel="noopener noreferrer"
            style={{ display: 'flex', alignItems: 'center', gap: 4, padding: '6px 12px', fontSize: 9, fontFamily: C.font, color: C.orange, textDecoration: 'none', borderLeft: `1px solid ${C.border}` }}>
            <ExternalLink size={10} /> POLYMARKET
          </a>
        )}
      </div>

      {/* Tab content */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {detailTab === 'chart'     && <ChartTab market={market} outcomeItems={outcomeItems} outcomeNames={outcomeNames} priceHistory={priceHistory} noPriceHistory={noPriceHistory} priceChartLoading={priceChartLoading} priceChartError={priceChartError} chartInterval={chartInterval} onIntervalChange={onIntervalChange} tradeAmount={tradeAmount} setTradeAmount={setTradeAmount} selectedOutcomeIndex={selectedOutcomeIndex} setSelectedOutcomeIndex={setSelectedOutcomeIndex} detailMaxPrice={detailMaxPrice} />}
        {detailTab === 'orderbook' && <OrderBookTab outcomeNames={outcomeNames} yesOrderBook={yesOrderBook} noOrderBook={noOrderBook} orderBookLoading={orderBookLoading} orderBookError={orderBookError} />}
        {detailTab === 'holders'   && <HoldersTab outcomeNames={outcomeNames} topHolders={topHolders} holdersLoading={holdersLoading} holdersError={holdersError} />}
        {detailTab === 'trades'    && <TradesTab recentTrades={recentTrades} />}
      </div>
    </div>
  );
};

// ── Chart tab ─────────────────────────────────────────────────────────────────
interface ChartTabProps {
  market: PolymarketMarket;
  outcomeItems: { name: string; price: number; isLive: boolean }[];
  outcomeNames: string[];
  priceHistory: PricePoint[];
  noPriceHistory: PricePoint[];
  priceChartLoading: boolean;
  priceChartError: string | null;
  chartInterval: string;
  onIntervalChange: (iv: string) => void;
  tradeAmount: string;
  setTradeAmount: (v: string) => void;
  selectedOutcomeIndex: number;
  setSelectedOutcomeIndex: (i: number) => void;
  detailMaxPrice: number;
}

const ChartTab: React.FC<ChartTabProps> = ({
  market, outcomeItems, outcomeNames,
  priceHistory, noPriceHistory, priceChartLoading, priceChartError, chartInterval, onIntervalChange,
  tradeAmount, setTradeAmount, selectedOutcomeIndex, setSelectedOutcomeIndex, detailMaxPrice,
}) => {
  const amount        = parseFloat(tradeAmount) || 0;
  const selectedItem  = outcomeItems[selectedOutcomeIndex] ?? outcomeItems[0];
  const price         = selectedItem?.price ?? 0;
  const shares        = price > 0 ? amount / price : 0;
  const maxReturn     = shares;
  const potentialProfit = maxReturn - amount;

  return (
    <div>
      {/* Chart */}
      <div style={{ borderBottom: `1px solid ${C.border}` }}>
        {sectionHeader('PROBABILITY HISTORY', (
          <div style={{ display: 'flex', gap: 2 }}>
            {(['1d', '1w', '1m', '6h', '1h', 'max'] as const).map(iv => (
              <button key={iv} onClick={() => onIntervalChange(iv)}
                style={{ padding: '2px 7px', fontSize: 9, fontFamily: C.font, background: chartInterval === iv ? C.orange : 'transparent', color: chartInterval === iv ? '#000' : C.muted, border: `1px solid ${chartInterval === iv ? C.orange : C.border}`, cursor: 'pointer', borderRadius: 1 }}>
                {iv.toUpperCase()}
              </button>
            ))}
          </div>
        ))}
        {priceChartLoading ? (
          <div style={{ height: 180, display: 'flex', alignItems: 'center', justifyContent: 'center', color: C.muted }}>
            <div style={{ textAlign: 'center' }}>
              <BarChart3 size={28} style={{ opacity: 0.3, margin: '0 auto 6px' }} />
              <div style={{ fontSize: 10, fontFamily: C.font }}>LOADING...</div>
            </div>
          </div>
        ) : priceHistory.length > 1 ? (() => {
          const noMap = new Map(noPriceHistory.map(pt => [pt.timestamp, pt.price]));
          const chartData = priceHistory.map(pt => {
            const ts = typeof pt.timestamp === 'number' ? pt.timestamp : Number(pt.timestamp) || 0;
            const ms = ts < 1e12 ? ts * 1000 : ts;
            const d  = new Date(ms);
            const label = chartInterval === '1d'
              ? d.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false })
              : d.toLocaleDateString('en-US', { month: 'short', day: 'numeric' });
            return { label, yes: typeof pt.price === 'number' ? pt.price : parseFloat(pt.price as any) || 0, no: noMap.get(pt.timestamp) ?? null };
          });
          const allP = chartData.flatMap(d => [d.yes, d.no ?? d.yes]);
          const minP = Math.min(...allP), maxP = Math.max(...allP);
          const pad  = (maxP - minP) * 0.05 || 0.02;
          const tickInterval = Math.max(1, Math.floor(chartData.length / 6));
          const yesLabel = outcomeNames[0] ?? 'Outcome 1';
          const noLabel  = outcomeNames[1] ?? 'Outcome 2';
          return (
            <ResponsiveContainer width="100%" height={180}>
              <LineChart data={chartData} margin={{ top: 6, right: 10, left: 2, bottom: 4 }}>
                <CartesianGrid strokeDasharray="2 4" stroke="#1a1a1a" vertical={false} />
                <XAxis dataKey="label" tick={{ fill: '#555', fontSize: 8, fontFamily: C.font }} tickLine={false} axisLine={{ stroke: '#222' }} interval={tickInterval} />
                <YAxis domain={[Math.max(0, minP - pad), Math.min(1, maxP + pad)]} tickFormatter={v => `${(v * 100).toFixed(0)}%`} tick={{ fill: '#555', fontSize: 8, fontFamily: C.font }} tickLine={false} axisLine={false} width={32} tickCount={5} />
                <Tooltip contentStyle={{ backgroundColor: '#111', border: `1px solid ${C.border}`, borderRadius: 2, fontFamily: C.font, fontSize: 9, padding: '5px 8px' }}
                  labelStyle={{ color: '#555', marginBottom: 3 }}
                  formatter={(val: number, name: string) => [`${(val * 100).toFixed(1)}%`, name === 'yes' ? yesLabel : name === 'no' ? noLabel : name]} />
                <Legend wrapperStyle={{ fontSize: 9, fontFamily: C.font, paddingTop: 2 }}
                  formatter={(value) => <span style={{ color: value === 'yes' ? OUTCOME_COLORS[0] : OUTCOME_COLORS[1] }}>{value === 'yes' ? yesLabel : noLabel}</span>} />
                <Line type="monotone" dataKey="yes" name="yes" stroke={OUTCOME_COLORS[0]} strokeWidth={1.5} dot={false} activeDot={{ r: 3 }} isAnimationActive={false} connectNulls />
                {noPriceHistory.length > 1 && <Line type="monotone" dataKey="no" name="no" stroke={OUTCOME_COLORS[1]} strokeWidth={1.5} dot={false} activeDot={{ r: 3 }} isAnimationActive={false} connectNulls />}
              </LineChart>
            </ResponsiveContainer>
          );
        })() : (
          <div style={{ height: 180, display: 'flex', alignItems: 'center', justifyContent: 'center', color: C.muted }}>
            <div style={{ textAlign: 'center' }}>
              <BarChart3 size={28} style={{ opacity: 0.3, margin: '0 auto 6px' }} />
              <div style={{ fontSize: 10, fontFamily: C.font }}>{priceChartError ?? 'NO PRICE DATA'}</div>
            </div>
          </div>
        )}
      </div>

      {/* Trade Calculator */}
      {outcomeItems.length > 0 && (
        <div style={{ borderBottom: `1px solid ${C.border}` }}>
          {sectionHeader('TRADE CALCULATOR', <span style={{ fontSize: 8, color: C.faint, fontFamily: C.font }}>READ-ONLY</span>)}
          <div style={{ padding: '10px 12px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12 }}>
            <div>
              <div style={{ fontSize: 8, color: C.faint, marginBottom: 5, fontFamily: C.font }}>SELECT OUTCOME</div>
              <div style={{ display: 'flex', gap: 3, marginBottom: 10, flexWrap: 'wrap' }}>
                {outcomeItems.map((item, i) => {
                  const isSelected = selectedOutcomeIndex === i;
                  const col = item.price === detailMaxPrice && detailMaxPrice > 0 ? OUTCOME_COLORS[0] : OUTCOME_COLORS[Math.min(i, OUTCOME_COLORS.length - 1) > 0 ? Math.min(i, OUTCOME_COLORS.length - 1) : 1];
                  return (
                    <button key={i} onClick={() => setSelectedOutcomeIndex(i)}
                      style={{ padding: '4px 10px', fontSize: 9, fontFamily: C.font, fontWeight: 'bold', background: isSelected ? col : 'transparent', color: isSelected ? '#000' : col, border: `1px solid ${col}`, cursor: 'pointer', borderRadius: 1 }}>
                      {item.name.toUpperCase()} {(item.price * 100).toFixed(0)}%
                    </button>
                  );
                })}
              </div>
              <div style={{ fontSize: 8, color: C.faint, marginBottom: 5, fontFamily: C.font }}>AMOUNT (USDC)</div>
              <div style={{ display: 'flex', alignItems: 'center', backgroundColor: C.bg, border: `1px solid ${C.border}`, marginBottom: 6 }}>
                <span style={{ padding: '0 8px', color: C.muted, fontSize: 13, fontWeight: 'bold', fontFamily: C.font }}>$</span>
                <input type="number" min="0" step="1" placeholder="0.00" value={tradeAmount} onChange={e => setTradeAmount(e.target.value)}
                  style={{ flex: 1, padding: '8px 6px', backgroundColor: 'transparent', border: 'none', outline: 'none', color: C.white, fontSize: 13, fontWeight: 'bold', fontFamily: C.font }} />
                <span style={{ padding: '0 8px', color: C.faint, fontSize: 9, fontFamily: C.font }}>USDC</span>
              </div>
              <div style={{ display: 'flex', gap: 3 }}>
                {[10, 50, 100, 500].map(v => (
                  <button key={v} onClick={() => setTradeAmount(String(v))}
                    style={{ flex: 1, padding: '3px 0', fontSize: 8, fontFamily: C.font, background: 'transparent', color: C.muted, border: `1px solid ${C.border}`, cursor: 'pointer', borderRadius: 1 }}>
                    ${v}
                  </button>
                ))}
              </div>
            </div>
            <div>
              <div style={{ fontSize: 8, color: C.faint, marginBottom: 8, fontFamily: C.font }}>ORDER SUMMARY</div>
              {[
                { label: 'OUTCOME',    value: selectedItem?.name?.toUpperCase() ?? '—', color: OUTCOME_COLORS[selectedOutcomeIndex] ?? C.white },
                { label: 'AVG PRICE',  value: price > 0 ? `$${price.toFixed(4)}` : '—',             color: C.white },
                { label: 'SHARES',     value: amount > 0 && price > 0 ? shares.toFixed(2) : '—',     color: C.white },
                { label: 'MAX RETURN', value: amount > 0 && price > 0 ? `$${maxReturn.toFixed(2)}` : '—', color: C.white },
                { label: 'PROFIT',     value: amount > 0 && price > 0 ? `$${potentialProfit.toFixed(2)}` : '—', color: potentialProfit > 0 ? C.green : C.muted },
                { label: 'ROI',        value: amount > 0 && price > 0 ? `${((potentialProfit / amount) * 100).toFixed(1)}%` : '—', color: potentialProfit > 0 ? C.green : C.muted },
              ].map(row => (
                <div key={row.label} style={{ display: 'flex', justifyContent: 'space-between', padding: '4px 0', borderBottom: `1px solid ${C.borderFaint}` }}>
                  <span style={{ fontSize: 8, color: C.faint, fontFamily: C.font }}>{row.label}</span>
                  <span style={{ fontSize: 10, fontWeight: 'bold', color: row.color, fontFamily: C.font }}>{row.value}</span>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* Additional stats */}
      {(() => {
        const vol1wk = market.volume1wk ?? null;
        const vol1mo = market.volume1mo ?? null;
        return (
          <>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 1, backgroundColor: C.border }}>
              {statCell('END DATE', market.endDate ? new Date(market.endDate).toLocaleDateString() : 'N/A')}
              {statCell('SPREAD', market.spread != null && !isNaN(Number(market.spread)) ? `${(Number(market.spread) * 100).toFixed(2)}%` : 'N/A')}
              {statCell('1WK VOL', vol1wk != null ? fmtVol(vol1wk) : 'N/A')}
              {statCell('1MO VOL', vol1mo != null ? fmtVol(vol1mo) : 'N/A')}
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 1, backgroundColor: C.border, marginTop: 1 }}>
              {statCell('STATUS', market.acceptingOrders ? 'ACCEPTING ORDERS' : market.closed ? 'CLOSED' : 'NOT ACCEPTING', market.acceptingOrders ? C.green : C.red)}
              {statCell('LAST TRADE', market.lastTradePrice != null ? `$${market.lastTradePrice.toFixed(4)}` : 'N/A')}
              {statCell('BEST BID', market.bestBid != null ? `$${market.bestBid.toFixed(4)}` : 'N/A', C.green)}
              {statCell('BEST ASK', market.bestAsk != null ? `$${market.bestAsk.toFixed(4)}` : 'N/A', C.red)}
            </div>
            {market.oneDayPriceChange != null && (
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: 1, backgroundColor: C.border, marginTop: 1 }}>
                {statCell('1D CHANGE', `${market.oneDayPriceChange >= 0 ? '+' : ''}${(market.oneDayPriceChange * 100).toFixed(2)}%`, market.oneDayPriceChange >= 0 ? C.green : C.red)}
                {statCell('RESOLUTION SOURCE', market.resolutionSource || 'N/A')}
              </div>
            )}
          </>
        );
      })()}
    </div>
  );
};

// ── Order Book tab ────────────────────────────────────────────────────────────
interface OrderBookTabProps {
  outcomeNames: string[];
  yesOrderBook: PolymarketOrderBookEnriched | null;
  noOrderBook: PolymarketOrderBookEnriched | null;
  orderBookLoading: boolean;
  orderBookError: string | null;
}

const OrderBookTab: React.FC<OrderBookTabProps> = ({ outcomeNames, yesOrderBook, noOrderBook, orderBookLoading, orderBookError }) => {
  const fmtSize  = (s: number) => s >= 1000 ? `${(s / 1000).toFixed(1)}K` : s.toFixed(0);
  const fmtPrice = (p: number) => `$${p.toFixed(3)}`;

  if (orderBookLoading) return <div style={{ padding: 20, textAlign: 'center', color: C.faint, fontSize: 10, fontFamily: C.font }}>LOADING ORDER BOOK...</div>;
  if (orderBookError && !yesOrderBook && !noOrderBook) return <div style={{ padding: 20, textAlign: 'center', color: C.faint, fontSize: 10, fontFamily: C.font }}>{orderBookError}</div>;

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 1, backgroundColor: C.border }}>
      {([{ book: yesOrderBook, idx: 0 }, { book: noOrderBook, idx: 1 }] as const).map(({ book, idx }) => {
        const name       = outcomeNames[idx] ?? (idx === 0 ? 'YES' : 'NO');
        const labelColor = OUTCOME_COLORS[idx] ?? C.muted;
        if (!book) return (
          <div key={idx} style={{ backgroundColor: C.bg, padding: 20, textAlign: 'center', color: C.faint, fontSize: 9, fontFamily: C.font }}>
            <div style={{ color: labelColor, fontWeight: 'bold', marginBottom: 4 }}>{name.toUpperCase()}</div>
            NO DATA
          </div>
        );

        const bids    = [...(book.bids ?? [])].sort((a, b) => parseFloat(b.price) - parseFloat(a.price)).slice(0, 8);
        const asks    = [...(book.asks ?? [])].sort((a, b) => parseFloat(a.price) - parseFloat(b.price)).slice(0, 8);
        const bestBid = bids[0] ? parseFloat(bids[0].price) : null;
        const bestAsk = asks[0] ? parseFloat(asks[0].price) : null;
        const spread  = bestBid != null && bestAsk != null ? bestAsk - bestBid : null;
        const maxSize = Math.max(...bids.map(b => parseFloat(b.size) || 0), ...asks.map(a => parseFloat(a.size) || 0), 0.001);

        const renderLevel = (entry: { price: string; size: string }, side: 'bid' | 'ask') => {
          const p = parseFloat(entry.price), s = parseFloat(entry.size) || 0;
          const pct = Math.min((s / maxSize) * 100, 100);
          const isBid = side === 'bid';
          return (
            <div key={`${side}-${entry.price}`} style={{ position: 'relative', display: 'grid', gridTemplateColumns: '72px 1fr', padding: '3px 8px', borderBottom: `1px solid #080808` }}>
              <div style={{ position: 'absolute', top: 0, bottom: 0, right: 0, width: `${pct}%`, backgroundColor: isBid ? '#00D66F0D' : '#FF3B3B0D' }} />
              <span style={{ position: 'relative', fontSize: 10, fontWeight: 'bold', color: isBid ? C.green : C.red, fontFamily: C.font }}>{fmtPrice(p)}</span>
              <span style={{ position: 'relative', fontSize: 10, color: C.muted, textAlign: 'right', fontFamily: C.font }}>{fmtSize(s)}</span>
            </div>
          );
        };

        return (
          <div key={idx} style={{ backgroundColor: C.bg }}>
            <div style={{ padding: '5px 8px', borderBottom: `1px solid ${C.border}`, display: 'flex', justifyContent: 'space-between', backgroundColor: C.header }}>
              <span style={{ fontSize: 10, color: labelColor, fontWeight: 'bold', fontFamily: C.font, letterSpacing: '0.5px' }}>{name.toUpperCase()}</span>
              {book.last_trade_price && <span style={{ fontSize: 8, color: C.faint, fontFamily: C.font }}>LAST {fmtPrice(parseFloat(book.last_trade_price))}</span>}
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '72px 1fr', padding: '2px 8px', borderBottom: `1px solid ${C.borderFaint}` }}>
              <span style={{ fontSize: 8, color: C.faint, fontWeight: 'bold', fontFamily: C.font }}>PRICE</span>
              <span style={{ fontSize: 8, color: C.faint, fontWeight: 'bold', textAlign: 'right', fontFamily: C.font }}>QTY</span>
            </div>
            {asks.length === 0 ? <div style={{ padding: '5px 8px', fontSize: 9, color: '#333', fontFamily: C.font }}>no asks</div>
              : [...asks].reverse().map(a => renderLevel(a, 'ask'))}
            <div style={{ display: 'grid', gridTemplateColumns: '72px 1fr', padding: '3px 8px', backgroundColor: '#0C0C0C', borderTop: `1px solid ${C.border}`, borderBottom: `1px solid ${C.border}` }}>
              <span style={{ fontSize: 8, color: C.faint, fontWeight: 'bold', fontFamily: C.font }}>SPREAD</span>
              <span style={{ fontSize: 9, color: C.muted, textAlign: 'right', fontFamily: C.font }}>{spread != null ? fmtPrice(spread) : '—'}</span>
            </div>
            {bids.length === 0 ? <div style={{ padding: '5px 8px', fontSize: 9, color: '#333', fontFamily: C.font }}>no bids</div>
              : bids.map(b => renderLevel(b, 'bid'))}
          </div>
        );
      })}
    </div>
  );
};

// ── Holders tab ───────────────────────────────────────────────────────────────
interface HoldersTabProps {
  outcomeNames: string[];
  topHolders: PolymarketTopHolders | null;
  holdersLoading: boolean;
  holdersError: string | null;
}

const HoldersTab: React.FC<HoldersTabProps> = ({ outcomeNames, topHolders, holdersLoading, holdersError }) => {
  if (holdersLoading) return <div style={{ padding: 20, textAlign: 'center', color: C.faint, fontSize: 10, fontFamily: C.font }}>LOADING HOLDERS...</div>;
  if (holdersError && (!topHolders || topHolders.holders.length === 0)) return <div style={{ padding: 20, textAlign: 'center', color: C.faint, fontSize: 10, fontFamily: C.font }}>{holdersError}</div>;
  if (!topHolders || topHolders.holders.length === 0) return <div style={{ padding: 20, textAlign: 'center', color: C.faint, fontSize: 10, fontFamily: C.font }}>NO HOLDER DATA</div>;

  return (
    <>
      {sectionHeader(`TOP HOLDERS (${topHolders.holders.length})`)}
      <div style={{ display: 'grid', gridTemplateColumns: '24px 1fr 70px 70px', padding: '3px 8px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, gap: 6 }}>
        {['#', 'HOLDER', 'OUTCOME', 'POSITION'].map((h, i) => (
          <span key={h} style={{ fontSize: 8, color: C.orange, fontWeight: 'bold', fontFamily: C.font, textAlign: i > 1 ? 'right' : 'left' }}>{h}</span>
        ))}
      </div>
      {topHolders.holders.slice(0, 20).map((holder, idx) => {
        const outcomeName  = outcomeNames[holder.outcomeIndex] ?? `Outcome ${holder.outcomeIndex}`;
        const outcomeColor = OUTCOME_COLORS[holder.outcomeIndex] ?? C.muted;
        const displayName  = holder.name ?? holder.pseudonym ?? `${holder.proxyWallet.slice(0, 6)}...${holder.proxyWallet.slice(-4)}`;
        return (
          <div key={idx} style={{ display: 'grid', gridTemplateColumns: '24px 1fr 70px 70px', padding: '5px 8px', borderBottom: `1px solid ${C.borderFaint}`, gap: 6, alignItems: 'center' }}>
            <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>{idx + 1}</span>
            <div style={{ display: 'flex', alignItems: 'center', gap: 5, overflow: 'hidden' }}>
              {holder.profileImage && <img src={holder.profileImage} alt="" style={{ width: 14, height: 14, borderRadius: '50%', flexShrink: 0 }} />}
              <span style={{ fontSize: 10, color: C.white, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', fontFamily: C.font }}>{displayName}</span>
            </div>
            <span style={{ fontSize: 8, color: outcomeColor, fontWeight: 'bold', textAlign: 'right', fontFamily: C.font, textTransform: 'uppercase' }}>{outcomeName}</span>
            <span style={{ fontSize: 10, color: C.white, fontWeight: 'bold', textAlign: 'right', fontFamily: C.font }}>{fmtVol(holder.amount)}</span>
          </div>
        );
      })}
    </>
  );
};

// ── Recent trades tab ─────────────────────────────────────────────────────────
const TradesTab: React.FC<{ recentTrades: PolymarketTrade[] }> = ({ recentTrades }) => {
  if (recentTrades.length === 0) return (
    <div style={{ padding: 20, textAlign: 'center', color: C.faint, fontSize: 10, fontFamily: C.font }}>NO RECENT TRADES</div>
  );
  return (
    <>
      {sectionHeader(`RECENT TRADES (${recentTrades.length})`)}
      <div style={{ display: 'grid', gridTemplateColumns: '50px 80px 70px 1fr', padding: '3px 8px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, gap: 6 }}>
        {['SIDE', 'PRICE', 'SHARES', 'TIME'].map((h, i) => (
          <span key={h} style={{ fontSize: 8, color: C.orange, fontWeight: 'bold', fontFamily: C.font, textAlign: i > 0 ? 'right' : 'left' }}>{h}</span>
        ))}
      </div>
      {recentTrades.slice(0, 30).map(trade => (
        <div key={trade.id} style={{ display: 'grid', gridTemplateColumns: '50px 80px 70px 1fr', padding: '4px 8px', borderBottom: `1px solid ${C.borderFaint}`, gap: 6, alignItems: 'center' }}>
          <span style={{ fontSize: 8, fontWeight: 'bold', fontFamily: C.font, textAlign: 'center', padding: '1px 4px', backgroundColor: trade.side === 'BUY' ? '#00D66F20' : '#FF3B3B20', color: trade.side === 'BUY' ? C.green : C.red, border: `1px solid ${trade.side === 'BUY' ? C.green : C.red}44` }}>
            {trade.side}
          </span>
          <span style={{ fontSize: 10, color: C.white, fontWeight: 'bold', textAlign: 'right', fontFamily: C.font }}>
            ${(() => { const p = parseFloat(trade.price); return !isNaN(p) ? p.toFixed(4) : '0.0000'; })()}
          </span>
          <span style={{ fontSize: 10, color: C.muted, textAlign: 'right', fontFamily: C.font }}>
            {(() => { const s = parseFloat(trade.size); return !isNaN(s) ? s.toFixed(2) : '0.00'; })()}
          </span>
          <span style={{ fontSize: 9, color: C.faint, textAlign: 'right', fontFamily: C.font }}>
            {new Date(trade.timestamp * 1000).toLocaleTimeString()}
          </span>
        </div>
      ))}
    </>
  );
};

export default MarketDetail;
