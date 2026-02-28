// ResolvedDetail.tsx — right-panel detail for resolved/closed Polymarket events
import React from 'react';
import { CheckCircle, X, Calendar, TrendingUp, BarChart2 } from 'lucide-react';
import { PolymarketEvent, PolymarketMarket } from '@/services/polymarket/polymarketApiService';
import { C, fmtVol, sectionHeader, statCell, parseJsonArr, parseNumArr } from './tokens';

export interface ResolvedDetailProps {
  event: PolymarketEvent | null;
  onClose: () => void;
}

const ResolvedDetail: React.FC<ResolvedDetailProps> = ({ event, onClose }) => {
  if (!event) return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', flexDirection: 'column', gap: 12 }}>
      <CheckCircle size={40} style={{ color: C.faint, opacity: 0.4 }} />
      <div style={{ fontSize: 11, color: C.muted, fontFamily: C.font }}>SELECT A RESOLVED EVENT</div>
      <div style={{ fontSize: 9, color: C.faint, fontFamily: C.font }}>Browse 200,000+ past markets</div>
    </div>
  );

  const markets = event.markets ?? [];
  const totalVolume = parseFloat(String(event.volume)) || 0;
  const endDate = event.endDate ? new Date(event.endDate) : null;

  return (
    <div style={{ height: '100%', overflow: 'auto' }}>
      {/* Header */}
      <div style={{ padding: '10px 12px', borderBottom: `2px solid ${C.green}`, backgroundColor: C.header, display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', gap: 8 }}>
        <div style={{ flex: 1 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginBottom: 4 }}>
            <span style={{ display: 'inline-flex', alignItems: 'center', gap: 4, padding: '2px 7px', backgroundColor: `${C.green}22`, color: C.green, fontSize: 8, fontWeight: 'bold', border: `1px solid ${C.green}44`, fontFamily: C.font }}>
              <CheckCircle size={8} /> RESOLVED
            </span>
            {event.tags && event.tags.length > 0 && (
              <span style={{ fontSize: 8, color: C.faint, fontFamily: C.font, textTransform: 'uppercase' }}>{event.tags[0].label}</span>
            )}
          </div>
          <div style={{ fontSize: 12, fontWeight: 'bold', color: C.white, marginBottom: 4, fontFamily: C.font, lineHeight: 1.3 }}>{event.title}</div>
          {event.description && (
            <div style={{ fontSize: 10, color: C.muted, fontFamily: C.font, lineHeight: 1.4 }}>{event.description}</div>
          )}
        </div>
        <button onClick={onClose}
          style={{ background: 'transparent', border: `1px solid ${C.border}`, color: C.muted, cursor: 'pointer', padding: '3px 6px', fontFamily: C.font, fontSize: 10, flexShrink: 0, borderRadius: 2 }}>
          <X size={12} />
        </button>
      </div>

      {/* Stats strip */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 1, backgroundColor: C.border, margin: '0 0 1px' }}>
        {statCell('MARKETS', markets.length, C.white)}
        {statCell('TOTAL VOLUME', fmtVol(totalVolume), C.green)}
        {statCell('CLOSED', endDate ? endDate.toLocaleDateString() : 'N/A', C.muted)}
      </div>

      {/* Resolved markets */}
      {markets.length > 0 && (
        <>
          {sectionHeader(`MARKET OUTCOMES (${markets.length})`)}
          {markets.map(m => <ResolvedMarketRow key={m.id} market={m} />)}
        </>
      )}

      {/* Event image */}
      {event.image && (
        <div style={{ padding: '12px', borderTop: `1px solid ${C.border}` }}>
          <img src={event.image} alt={event.title}
            style={{ width: '100%', maxHeight: 160, objectFit: 'cover', borderRadius: 2, opacity: 0.7 }} />
        </div>
      )}
    </div>
  );
};

// ── Resolved market row ───────────────────────────────────────────────────────
const ResolvedMarketRow: React.FC<{ market: PolymarketMarket }> = ({ market }) => {
  const prices  = parseNumArr(market.outcomePrices);
  const names   = parseJsonArr(market.outcomes);
  const vol     = parseFloat(String(market.volumeNum ?? market.volume ?? 0)) || 0;

  // Determine winning outcome (price ≥ 0.95 → resolved)
  const winIdx   = prices.findIndex(p => p >= 0.95);
  const winName  = winIdx >= 0 ? (names[winIdx] ?? 'YES') : null;
  const isYesWin = winName?.toLowerCase() === 'yes';
  const isNoWin  = winName?.toLowerCase() === 'no';
  const winColor = winName == null ? C.muted : isNoWin ? C.red : C.green;

  return (
    <div style={{ padding: '8px 12px', borderBottom: `1px solid ${C.borderFaint}` }}>
      <div style={{ fontSize: 10, color: C.white, fontWeight: 'bold', marginBottom: 5, lineHeight: 1.3, fontFamily: C.font }}>
        {market.question}
      </div>

      {/* Outcome bars */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: 3, marginBottom: 6 }}>
        {names.map((name, i) => {
          const pct   = Math.round((prices[i] ?? 0) * 100);
          const isWin = i === winIdx;
          const color = name.toLowerCase() === 'no' ? C.red : isWin ? C.green : C.muted;
          return (
            <div key={i}>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 2 }}>
                <span style={{ fontSize: 9, fontWeight: 'bold', color: isWin ? color : C.faint, fontFamily: C.font, display: 'flex', alignItems: 'center', gap: 4 }}>
                  {isWin && <CheckCircle size={8} style={{ color }} />}
                  {name.toUpperCase()}
                </span>
                <span style={{ fontSize: 9, fontWeight: 'bold', color: isWin ? color : C.faint, fontFamily: C.font }}>{pct}%</span>
              </div>
              <div style={{ height: 3, backgroundColor: C.border, borderRadius: 1 }}>
                <div style={{ height: '100%', width: `${pct}%`, backgroundColor: isWin ? color : C.border, borderRadius: 1, transition: 'width 0.3s' }} />
              </div>
            </div>
          );
        })}
      </div>

      {/* Footer row */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 8 }}>
        {winName ? (
          <span style={{ display: 'inline-flex', alignItems: 'center', gap: 3, padding: '1px 6px', backgroundColor: `${winColor}22`, color: winColor, fontSize: 8, fontWeight: 'bold', border: `1px solid ${winColor}44`, fontFamily: C.font }}>
            <CheckCircle size={8} /> WINNER: {winName.toUpperCase()}
          </span>
        ) : (
          <span style={{ fontSize: 8, color: C.faint, fontFamily: C.font }}>OUTCOME UNCLEAR</span>
        )}
        <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
          {vol > 0 && (
            <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font, display: 'flex', alignItems: 'center', gap: 3 }}>
              <BarChart2 size={9} /> {fmtVol(vol)}
            </span>
          )}
          {market.endDate && (
            <span style={{ fontSize: 9, color: C.faint, fontFamily: C.font, display: 'flex', alignItems: 'center', gap: 3 }}>
              <Calendar size={9} /> {new Date(market.endDate).toLocaleDateString()}
            </span>
          )}
        </div>
      </div>
    </div>
  );
};

export default ResolvedDetail;
