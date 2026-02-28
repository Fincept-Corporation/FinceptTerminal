// EventDetail.tsx — right-panel event detail for the Polymarket tab
import React from 'react';
import { Calendar, X } from 'lucide-react';
import { PolymarketEvent, PolymarketMarket } from '@/services/polymarket/polymarketApiService';
import { C, fmtVol, sectionHeader, statCell } from './tokens';

export interface EventDetailProps {
  event: PolymarketEvent | null;
  livePrices: Record<string, number>;
  onMarketClick: (market: PolymarketMarket) => void;
  onClose: () => void;
  formatVolume: (v: string | number) => string;
}

const EventDetail: React.FC<EventDetailProps> = ({ event, onMarketClick, onClose, formatVolume }) => {
  if (!event) return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', flexDirection: 'column', gap: 12 }}>
      <Calendar size={40} style={{ color: C.faint, opacity: 0.4 }} />
      <div style={{ fontSize: 11, color: C.muted, fontFamily: C.font }}>SELECT AN EVENT</div>
    </div>
  );

  return (
    <div style={{ height: '100%', overflow: 'auto' }}>
      {/* Header */}
      <div style={{ padding: '10px 12px', borderBottom: `2px solid ${C.orange}`, backgroundColor: C.header, display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', gap: 8 }}>
        <div style={{ flex: 1 }}>
          <div style={{ fontSize: 12, fontWeight: 'bold', color: C.orange, marginBottom: 4, fontFamily: C.font }}>{event.title}</div>
          {event.description && (
            <div style={{ fontSize: 10, color: C.muted, fontFamily: C.font, lineHeight: 1.4 }}>{event.description}</div>
          )}
        </div>
        <button onClick={onClose}
          style={{ background: 'transparent', border: `1px solid ${C.border}`, color: C.muted, cursor: 'pointer', padding: '3px 6px', fontFamily: C.font, fontSize: 10, flexShrink: 0, borderRadius: 2 }}>
          <X size={12} />
        </button>
      </div>

      {/* Stats */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 1, backgroundColor: C.border, margin: '0 0 1px' }}>
        {statCell('MARKETS', event.markets?.length ?? 0)}
        {statCell('VOLUME', event.volume ? formatVolume(event.volume) : 'N/A')}
        {statCell('END DATE', event.endDate ? new Date(event.endDate).toLocaleDateString() : 'N/A')}
      </div>

      {/* Markets list */}
      {event.markets && event.markets.length > 0 && (
        <>
          {sectionHeader(`MARKETS (${event.markets.length})`)}
          {event.markets.map((m: PolymarketMarket) => (
            <EventMarketRow key={m.id} market={m} onClick={() => onMarketClick(m)} />
          ))}
        </>
      )}
    </div>
  );
};

// ── Event market row ──────────────────────────────────────────────────────────
const EventMarketRow: React.FC<{ market: PolymarketMarket; onClick: () => void }> = ({ market, onClick }) => {
  let tokenIds: string[] = [];
  try {
    const raw = market.clobTokenIds;
    tokenIds = typeof raw === 'string' ? JSON.parse(raw) : Array.isArray(raw) ? raw as string[] : [];
  } catch { /* ignore */ }

  const prices = (() => {
    try {
      const raw = typeof market.outcomePrices === 'string' ? JSON.parse(market.outcomePrices) : market.outcomePrices;
      if (Array.isArray(raw)) return raw.map((x: any) => parseFloat(x) || 0);
    } catch { /* ignore */ }
    return [];
  })();

  const leadPrice = prices.length > 0 ? Math.max(...prices) : 0;

  return (
    <div
      onClick={onClick}
      style={{ padding: '8px 10px', borderBottom: `1px solid ${C.borderFaint}`, cursor: 'pointer', backgroundColor: 'transparent', borderLeft: '2px solid transparent', transition: 'background-color 0.1s' }}
      onMouseEnter={e => { e.currentTarget.style.backgroundColor = C.hover; e.currentTarget.style.borderLeftColor = C.orange; }}
      onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; e.currentTarget.style.borderLeftColor = 'transparent'; }}
    >
      <div style={{ fontSize: 10, color: C.white, fontWeight: 'bold', marginBottom: 4, lineHeight: 1.3, fontFamily: C.font }}>
        {market.question}
      </div>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 8 }}>
        <span style={{ padding: '1px 5px', backgroundColor: `#00D66F20`, color: '#00D66F', fontSize: 9, fontWeight: 'bold', border: `1px solid #00D66F44`, fontFamily: C.font }}>
          YES {(leadPrice * 100).toFixed(0)}%
        </span>
        <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>
          {fmtVol(market.volumeNum ?? market.volume ?? 0)}
        </span>
      </div>
    </div>
  );
};

export default EventDetail;
