// MarketList.tsx — left-panel list for markets / events / sports / resolved
import React from 'react';
import { RefreshCw, AlertCircle, Info, Trophy, ChevronRight, CheckCircle } from 'lucide-react';
import { PolymarketMarket, PolymarketEvent, PolymarketSport } from '@/services/polymarket/polymarketApiService';
import { C, OUTCOME_COLORS, fmtVol, parseJsonArr, parseNumArr } from './tokens';

// ── Props ─────────────────────────────────────────────────────────────────────
export interface MarketListProps {
  activeView: 'markets' | 'events' | 'sports' | 'resolved';
  markets: PolymarketMarket[];
  events: PolymarketEvent[];
  sports: PolymarketSport[];
  resolvedEvents: PolymarketEvent[];
  selectedMarket: PolymarketMarket | null;
  selectedEvent: PolymarketEvent | null;
  selectedSport: PolymarketSport | null;
  selectedResolved: PolymarketEvent | null;
  livePrices: Record<string, number>;
  loading: boolean;
  error: string | null;
  onMarketClick: (m: PolymarketMarket) => void;
  onEventClick: (e: PolymarketEvent) => void;
  onSportClick: (s: PolymarketSport) => void;
  onSportBack: () => void;
  onResolvedClick: (e: PolymarketEvent) => void;
  isMarketTradeable: (m: PolymarketMarket) => boolean;
}

const MarketList: React.FC<MarketListProps> = ({
  activeView, markets, events, sports, resolvedEvents,
  selectedMarket, selectedEvent, selectedSport, selectedResolved,
  livePrices, loading, error,
  onMarketClick, onEventClick, onSportClick, onSportBack, onResolvedClick,
  isMarketTradeable,
}) => {
  if (loading) return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: 300, flexDirection: 'column', gap: 10 }}>
      <RefreshCw size={24} className="animate-spin" style={{ color: C.orange }} />
      <div style={{ fontSize: 10, color: C.muted, fontFamily: C.font }}>LOADING...</div>
    </div>
  );

  if (error) return (
    <div style={{ margin: 12, padding: 12, backgroundColor: '#330000', border: `1px solid ${C.red}`, display: 'flex', alignItems: 'flex-start', gap: 8 }}>
      <AlertCircle size={16} style={{ color: C.red, flexShrink: 0, marginTop: 1 }} />
      <div>
        <div style={{ fontSize: 10, fontWeight: 'bold', color: C.red, marginBottom: 2, fontFamily: C.font }}>ERROR</div>
        <div style={{ fontSize: 9, color: '#FF8888', fontFamily: C.font }}>{error}</div>
      </div>
    </div>
  );

  if (activeView === 'markets') {
    if (markets.length === 0) return (
      <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}>
        <Info size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
        <div style={{ fontSize: 10, fontFamily: C.font }}>NO MARKETS FOUND</div>
      </div>
    );
    return <>{markets.map(m => <MarketRow key={m.id} market={m} selected={selectedMarket?.id === m.id} livePrices={livePrices} isMarketTradeable={isMarketTradeable} onClick={() => onMarketClick(m)} />)}</>;
  }

  if (activeView === 'events') {
    if (events.length === 0) return (
      <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}>
        <Info size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
        <div style={{ fontSize: 10, fontFamily: C.font }}>NO EVENTS FOUND</div>
      </div>
    );
    return <>{events.map(e => <EventRow key={e.id} event={e} selected={selectedEvent?.id === e.id} onClick={() => onEventClick(e)} />)}</>;
  }

  if (activeView === 'resolved') {
    if (resolvedEvents.length === 0) return (
      <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}>
        <CheckCircle size={32} style={{ opacity: 0.3, margin: '0 auto 8px', color: C.green }} />
        <div style={{ fontSize: 10, fontFamily: C.font }}>NO RESOLVED EVENTS FOUND</div>
      </div>
    );
    return <>{resolvedEvents.map(e => <ResolvedEventRow key={e.id} event={e} selected={selectedResolved?.id === e.id} onClick={() => onResolvedClick(e)} />)}</>;
  }

  if (activeView === 'sports') {
    if (selectedSport) {
      return (
        <>
          <div style={{ padding: '6px 10px', borderBottom: `1px solid ${C.border}`, backgroundColor: C.header, display: 'flex', alignItems: 'center', gap: 8 }}>
            <button onClick={onSportBack}
              style={{ background: 'transparent', border: `1px solid ${C.border}`, color: C.muted, cursor: 'pointer', padding: '3px 8px', fontFamily: C.font, fontSize: 9, borderRadius: 1 }}>
              BACK
            </button>
            <span style={{ fontSize: 10, fontWeight: 'bold', color: C.orange, fontFamily: C.font }}>{selectedSport.sport?.toUpperCase()}</span>
            <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>{markets.length} MARKETS</span>
          </div>
          {markets.length === 0
            ? <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}><div style={{ fontSize: 10, fontFamily: C.font }}>NO MARKETS</div></div>
            : markets.map(m => <MarketRow key={m.id} market={m} selected={selectedMarket?.id === m.id} livePrices={livePrices} isMarketTradeable={isMarketTradeable} onClick={() => onMarketClick(m)} />)}
        </>
      );
    }
    return (
      <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: 1 }}>
        {sports.length === 0
          ? <div style={{ textAlign: 'center', padding: '60px 20px', color: C.muted }}><div style={{ fontSize: 10, fontFamily: C.font }}>NO SPORTS FOUND</div></div>
          : sports.map(s => <SportCard key={s.id} sport={s} onClick={() => onSportClick(s)} />)}
      </div>
    );
  }

  return null;
};

// ── Market row ────────────────────────────────────────────────────────────────
interface MarketRowProps {
  market: PolymarketMarket;
  selected: boolean;
  livePrices: Record<string, number>;
  isMarketTradeable: (m: PolymarketMarket) => boolean;
  onClick: () => void;
}

const MarketRow: React.FC<MarketRowProps> = ({ market, selected, livePrices, isMarketTradeable, onClick }) => {
  const isTradeable   = isMarketTradeable(market);
  const outcomePrices = parseNumArr(market.outcomePrices);
  const outcomeNames  = parseJsonArr(market.outcomes);
  let tokenIds: string[] = [];
  try {
    const raw = market.clobTokenIds;
    tokenIds = typeof raw === 'string' ? JSON.parse(raw) : Array.isArray(raw) ? raw as string[] : [];
  } catch { /* ignore */ }

  const leadPrice   = outcomePrices.length > 0 ? Math.max(...outcomePrices) : 0;
  const leadIdx     = outcomePrices.findIndex(p => p === leadPrice);
  const leadName    = outcomeNames[leadIdx] ?? 'YES';
  const liveYes     = tokenIds[0] ? livePrices[tokenIds[0]] : undefined;
  const displayPrice = liveYes ?? leadPrice;

  return (
    <div
      onClick={onClick}
      style={{
        padding: '8px 10px', borderBottom: `1px solid ${C.borderFaint}`, cursor: 'pointer',
        backgroundColor: selected ? '#1A1200' : 'transparent',
        borderLeft: selected ? `2px solid ${C.orange}` : '2px solid transparent',
        transition: 'background-color 0.1s',
      }}
      onMouseEnter={e => { if (!selected) e.currentTarget.style.backgroundColor = C.hover; }}
      onMouseLeave={e => { if (!selected) e.currentTarget.style.backgroundColor = 'transparent'; }}
    >
      <div style={{ fontSize: 10, color: C.white, fontWeight: 'bold', marginBottom: 4, lineHeight: 1.3, fontFamily: C.font }}>
        {market.question}
      </div>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 8 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
          <span style={{ padding: '1px 5px', backgroundColor: `${OUTCOME_COLORS[0]}20`, color: OUTCOME_COLORS[0], fontSize: 9, fontWeight: 'bold', border: `1px solid ${OUTCOME_COLORS[0]}44`, fontFamily: C.font }}>
            {leadName.toUpperCase()} {(displayPrice * 100).toFixed(0)}%
            {liveYes !== undefined && <span style={{ color: C.orange }}> ●</span>}
          </span>
          <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>
            {fmtVol(market.volumeNum ?? market.volume ?? 0)}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
          {market.oneDayPriceChange != null && Math.abs(market.oneDayPriceChange) > 0.001 && (
            <span style={{ fontSize: 9, fontWeight: 'bold', fontFamily: C.font, color: market.oneDayPriceChange >= 0 ? C.green : C.red }}>
              {market.oneDayPriceChange >= 0 ? '+' : ''}{(market.oneDayPriceChange * 100).toFixed(1)}%
            </span>
          )}
          {isTradeable && <span style={{ width: 6, height: 6, borderRadius: '50%', backgroundColor: C.green, display: 'inline-block' }} />}
        </div>
      </div>
    </div>
  );
};

// ── Event row ─────────────────────────────────────────────────────────────────
const EventRow: React.FC<{ event: PolymarketEvent; selected: boolean; onClick: () => void }> = ({ event, selected, onClick }) => (
  <div
    onClick={onClick}
    style={{
      padding: '8px 10px', borderBottom: `1px solid ${C.borderFaint}`, cursor: 'pointer',
      backgroundColor: selected ? '#1A1200' : 'transparent',
      borderLeft: selected ? `2px solid ${C.orange}` : '2px solid transparent',
      transition: 'background-color 0.1s',
    }}
    onMouseEnter={e => { if (!selected) e.currentTarget.style.backgroundColor = C.hover; }}
    onMouseLeave={e => { if (!selected) e.currentTarget.style.backgroundColor = 'transparent'; }}
  >
    <div style={{ display: 'flex', alignItems: 'flex-start', gap: 8 }}>
      {event.image && (
        <img src={event.image} alt="" style={{ width: 28, height: 28, borderRadius: 2, objectFit: 'cover', flexShrink: 0, marginTop: 1 }} />
      )}
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ fontSize: 10, color: C.white, fontWeight: 'bold', marginBottom: 3, lineHeight: 1.3, fontFamily: C.font }}>
          {event.title}
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 9, color: C.faint, fontFamily: C.font }}>
          <span>MKTS: {event.markets?.length || 0}</span>
          {event.volume && <span>{fmtVol(parseFloat(String(event.volume)) || 0)}</span>}
          {event.endDate && <span>{new Date(event.endDate).toLocaleDateString()}</span>}
        </div>
      </div>
    </div>
  </div>
);

// ── Resolved event row ────────────────────────────────────────────────────────
const ResolvedEventRow: React.FC<{ event: PolymarketEvent; selected: boolean; onClick: () => void }> = ({ event, selected, onClick }) => {
  // Find winner: the market outcome with price closest to 1
  const markets = event.markets ?? [];
  let winnerLabel: string | null = null;
  let winnerColor: string = C.green;
  if (markets.length > 0) {
    const m = markets[0];
    const prices  = parseNumArr(m.outcomePrices);
    const names   = parseJsonArr(m.outcomes);
    const maxIdx  = prices.reduce((best, p, i) => p > prices[best] ? i : best, 0);
    const maxPrice = prices[maxIdx] ?? 0;
    if (maxPrice >= 0.95) {
      winnerLabel = names[maxIdx] ?? null;
      winnerColor = winnerLabel?.toLowerCase() === 'no' ? C.red : C.green;
    }
  }

  return (
    <div
      onClick={onClick}
      style={{
        padding: '8px 10px', borderBottom: `1px solid ${C.borderFaint}`, cursor: 'pointer',
        backgroundColor: selected ? '#001A00' : 'transparent',
        borderLeft: selected ? `2px solid ${C.green}` : '2px solid transparent',
        transition: 'background-color 0.1s',
      }}
      onMouseEnter={e => { if (!selected) e.currentTarget.style.backgroundColor = C.hover; }}
      onMouseLeave={e => { if (!selected) e.currentTarget.style.backgroundColor = 'transparent'; }}
    >
      <div style={{ display: 'flex', alignItems: 'flex-start', gap: 8 }}>
        {event.image && (
          <img src={event.image} alt="" style={{ width: 26, height: 26, borderRadius: 2, objectFit: 'cover', flexShrink: 0, marginTop: 1 }} />
        )}
        <div style={{ flex: 1, minWidth: 0 }}>
          <div style={{ fontSize: 10, color: C.white, fontWeight: 'bold', marginBottom: 3, lineHeight: 1.3, fontFamily: C.font }}>
            {event.title}
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, flexWrap: 'wrap' }}>
            {winnerLabel && (
              <span style={{ display: 'inline-flex', alignItems: 'center', gap: 3, padding: '1px 6px', backgroundColor: `${winnerColor}22`, color: winnerColor, fontSize: 8, fontWeight: 'bold', border: `1px solid ${winnerColor}44`, fontFamily: C.font }}>
                <CheckCircle size={8} /> {winnerLabel.toUpperCase()}
              </span>
            )}
            <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>
              {event.endDate ? new Date(event.endDate).toLocaleDateString() : ''}
            </span>
            {event.volume && (
              <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>{fmtVol(parseFloat(String(event.volume)) || 0)}</span>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

// ── Sport card ────────────────────────────────────────────────────────────────
const SportCard: React.FC<{ sport: PolymarketSport; onClick: () => void }> = ({ sport, onClick }) => (
  <div
    onClick={onClick}
    style={{ backgroundColor: C.bg, border: `1px solid ${C.border}`, padding: '10px 12px', cursor: 'pointer', transition: 'background-color 0.1s', display: 'flex', alignItems: 'center', gap: 10, fontFamily: C.font }}
    onMouseEnter={e => e.currentTarget.style.backgroundColor = C.hover}
    onMouseLeave={e => e.currentTarget.style.backgroundColor = C.bg}
  >
    {sport.image && <img src={sport.image} alt={sport.sport} style={{ width: 32, height: 32, objectFit: 'cover', borderRadius: 2, flexShrink: 0 }} />}
    <div style={{ flex: 1 }}>
      <div style={{ fontSize: 11, fontWeight: 'bold', color: C.orange, marginBottom: 2 }}>{sport.sport ? sport.sport.toUpperCase() : 'UNKNOWN'}</div>
      <div style={{ display: 'flex', alignItems: 'center', gap: 4, fontSize: 9, color: C.faint }}>
        <Trophy size={9} /><span>CLICK TO VIEW MARKETS</span>
      </div>
    </div>
    <ChevronRight size={14} style={{ color: C.faint }} />
  </div>
);

export default MarketList;
