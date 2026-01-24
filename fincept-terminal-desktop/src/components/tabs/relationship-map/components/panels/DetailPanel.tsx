import React from 'react';
import type { GraphNodeData } from '../../types';
import { FINCEPT, SIGNAL_COLORS } from '../../constants';

interface DetailPanelProps {
  nodeData: GraphNodeData | null;
  onClose: () => void;
}

function fmt(val: number | undefined, type: 'money' | 'pct' | 'num' = 'num'): string {
  if (val === undefined || val === null) return 'N/A';
  if (type === 'money') {
    if (Math.abs(val) >= 1e12) return `$${(val / 1e12).toFixed(2)}T`;
    if (Math.abs(val) >= 1e9) return `$${(val / 1e9).toFixed(2)}B`;
    if (Math.abs(val) >= 1e6) return `$${(val / 1e6).toFixed(1)}M`;
    return `$${val.toLocaleString()}`;
  }
  if (type === 'pct') return `${(val * 100).toFixed(1)}%`;
  if (Math.abs(val) < 1 && val !== 0) return `${(val * 100).toFixed(1)}%`;
  return val.toFixed(2);
}

const DetailPanel: React.FC<DetailPanelProps> = ({ nodeData, onClose }) => {
  if (!nodeData) return null;

  const renderContent = () => {
    if (nodeData.category === 'company' && nodeData.company) {
      const c = nodeData.company;
      const v = nodeData.valuation;
      return (
        <>
          <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '4px' }}>{c.ticker}</div>
          <div style={{ fontSize: '10px', color: FINCEPT.WHITE, marginBottom: '8px' }}>{c.name}</div>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '12px' }}>{c.sector} | {c.industry}</div>

          {v && (
            <div style={{ padding: '6px 8px', backgroundColor: SIGNAL_COLORS[v.action].bg, border: `1px solid ${SIGNAL_COLORS[v.action].border}`, borderRadius: '2px', marginBottom: '12px' }}>
              <div style={{ fontSize: '9px', fontWeight: 700, color: SIGNAL_COLORS[v.action].text }}>{v.status.replace(/_/g, ' ')} | {v.action}</div>
              <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginTop: '2px' }}>Confidence: {v.confidence} | Score: {v.score.toFixed(1)}</div>
            </div>
          )}

          <Section title="MARKET DATA">
            <Row label="MARKET CAP" value={fmt(c.market_cap, 'money')} />
            <Row label="PRICE" value={`$${c.current_price.toFixed(2)}`} />
            <Row label="P/E" value={c.pe_ratio ? c.pe_ratio.toFixed(1) : 'N/A'} />
            <Row label="FORWARD P/E" value={c.forward_pe ? c.forward_pe.toFixed(1) : 'N/A'} />
            <Row label="P/B" value={c.price_to_book ? c.price_to_book.toFixed(1) : 'N/A'} />
            <Row label="PEG" value={c.peg_ratio ? c.peg_ratio.toFixed(2) : 'N/A'} />
          </Section>
          <Section title="PROFITABILITY">
            <Row label="PROFIT MARGIN" value={fmt(c.profit_margins, 'pct')} />
            <Row label="OPERATING MARGIN" value={fmt(c.operating_margins, 'pct')} />
            <Row label="ROE" value={fmt(c.roe, 'pct')} />
            <Row label="ROA" value={fmt(c.roa, 'pct')} />
          </Section>
          <Section title="GROWTH">
            <Row label="REVENUE GROWTH" value={fmt(c.revenue_growth, 'pct')} color={c.revenue_growth > 0 ? FINCEPT.GREEN : FINCEPT.RED} />
            <Row label="EARNINGS GROWTH" value={fmt(c.earnings_growth, 'pct')} color={c.earnings_growth > 0 ? FINCEPT.GREEN : FINCEPT.RED} />
          </Section>
          <Section title="OWNERSHIP">
            <Row label="INSIDERS" value={`${c.insider_percent.toFixed(1)}%`} />
            <Row label="INSTITUTIONS" value={`${c.institutional_percent.toFixed(1)}%`} />
          </Section>
          {nodeData.executives && nodeData.executives.length > 0 && (
            <Section title="EXECUTIVES">
              {nodeData.executives.slice(0, 5).map((ex, i) => (
                <Row key={i} label={ex.role} value={ex.name} />
              ))}
            </Section>
          )}
        </>
      );
    }

    if (nodeData.category === 'peer' && nodeData.peer) {
      const p = nodeData.peer;
      const v = nodeData.peerValuation;
      return (
        <>
          <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.BLUE, marginBottom: '4px' }}>{p.ticker}</div>
          <div style={{ fontSize: '10px', color: FINCEPT.WHITE, marginBottom: '12px' }}>{p.name}</div>
          {v && (
            <div style={{ padding: '6px 8px', backgroundColor: SIGNAL_COLORS[v.action].bg, border: `1px solid ${SIGNAL_COLORS[v.action].border}`, borderRadius: '2px', marginBottom: '12px' }}>
              <div style={{ fontSize: '9px', fontWeight: 700, color: SIGNAL_COLORS[v.action].text }}>{v.action} | Score: {v.score.toFixed(1)}</div>
            </div>
          )}
          <Section title="METRICS">
            <Row label="MARKET CAP" value={fmt(p.market_cap, 'money')} />
            <Row label="P/E" value={p.pe_ratio ? p.pe_ratio.toFixed(1) : 'N/A'} />
            <Row label="P/B" value={p.price_to_book ? p.price_to_book.toFixed(1) : 'N/A'} />
            <Row label="ROE" value={fmt(p.roe, 'pct')} />
            <Row label="GROWTH" value={fmt(p.revenue_growth, 'pct')} color={p.revenue_growth > 0 ? FINCEPT.GREEN : FINCEPT.RED} />
          </Section>
        </>
      );
    }

    if (nodeData.category === 'institutional' && nodeData.holder) {
      const h = nodeData.holder;
      return (
        <>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.GREEN, marginBottom: '12px' }}>{h.name}</div>
          <Section title="HOLDINGS">
            <Row label="OWNERSHIP" value={`${h.percentage.toFixed(2)}%`} />
            <Row label="SHARES" value={h.shares.toLocaleString()} />
            <Row label="VALUE" value={fmt(h.value, 'money')} />
          </Section>
        </>
      );
    }

    if (nodeData.category === 'insider' && nodeData.insider) {
      const ins = nodeData.insider;
      return (
        <>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '4px' }}>{ins.name}</div>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '12px' }}>{ins.title}</div>
          <Section title="ACTIVITY">
            <Row label="SHARES" value={ins.shares ? ins.shares.toLocaleString() : 'N/A'} />
            {ins.last_transaction_date && <Row label="LAST TX" value={ins.last_transaction_date} />}
            {ins.last_transaction_type && <Row label="TYPE" value={ins.last_transaction_type.toUpperCase()} color={ins.last_transaction_type === 'buy' ? FINCEPT.GREEN : FINCEPT.RED} />}
          </Section>
        </>
      );
    }

    if (nodeData.category === 'event' && nodeData.event) {
      const ev = nodeData.event;
      return (
        <>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.RED, marginBottom: '4px' }}>{ev.form}</div>
          <div style={{ fontSize: '9px', color: FINCEPT.WHITE, marginBottom: '8px' }}>{ev.description}</div>
          <Section title="DETAILS">
            <Row label="DATE" value={ev.filing_date} />
            <Row label="CATEGORY" value={ev.category.toUpperCase()} />
            {ev.items.length > 0 && ev.items.map((item, i) => <Row key={i} label={`ITEM ${i + 1}`} value={item} />)}
          </Section>
        </>
      );
    }

    if (nodeData.category === 'metrics' && nodeData.metrics) {
      return (
        <>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '12px' }}>{nodeData.label}</div>
          <Section title="DATA">
            {Object.entries(nodeData.metrics).filter(([, v]) => v !== 0 && v !== '').map(([key, val]) => (
              <Row key={key} label={key.replace(/_/g, ' ').toUpperCase()} value={typeof val === 'number' ? fmt(val) : String(val)} />
            ))}
          </Section>
        </>
      );
    }

    return <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>No details available</div>;
  };

  return (
    <div style={{ position: 'absolute', top: '48px', right: 0, bottom: '28px', width: '300px', backgroundColor: FINCEPT.PANEL_BG, borderLeft: `1px solid ${FINCEPT.BORDER}`, zIndex: 80, overflowY: 'auto', display: 'flex', flexDirection: 'column' }}>
      <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>DETAILS</span>
        <button onClick={onClose} style={{ background: 'none', border: 'none', color: FINCEPT.GRAY, cursor: 'pointer', fontSize: '14px', padding: '2px 6px' }}>×</button>
      </div>
      <div style={{ padding: '12px', flex: 1 }}>
        {renderContent()}
      </div>
    </div>
  );
};

const Section: React.FC<{ title: string; children: React.ReactNode }> = ({ title, children }) => (
  <div style={{ marginBottom: '12px' }}>
    <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px', paddingBottom: '4px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>{title}</div>
    {children}
  </div>
);

const Row: React.FC<{ label: string; value: string; color?: string }> = ({ label, value, color }) => (
  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '2px 0' }}>
    <span style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.3px' }}>{label}</span>
    <span style={{ fontSize: '9px', color: color || FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>{value}</span>
  </div>
);

export default DetailPanel;
