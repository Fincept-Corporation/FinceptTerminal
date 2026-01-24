import React from 'react';
import type { GraphNodeData } from '../../types';
import { getSignalColor, getSignalLabel } from '../../utils/valuationSignals';

interface DetailPanelProps {
  nodeData: GraphNodeData | null;
  onClose: () => void;
}

const DetailPanel: React.FC<DetailPanelProps> = ({ nodeData, onClose }) => {
  if (!nodeData) return null;

  return (
    <div style={{
      position: 'absolute',
      right: 0,
      top: 0,
      bottom: 0,
      width: '380px',
      background: 'rgba(10, 10, 10, 0.95)',
      borderLeft: '1px solid var(--ft-color-text-muted)',
      padding: '16px',
      overflowY: 'auto',
      zIndex: 100,
      backdropFilter: 'blur(8px)',
    }}>
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
        <h3 style={{ color: 'var(--ft-color-primary)', fontSize: '14px', margin: 0 }}>{nodeData.label}</h3>
        <button
          onClick={onClose}
          style={{ background: 'none', border: '1px solid var(--ft-color-text-muted)', color: 'var(--ft-color-text)', cursor: 'pointer', padding: '4px 8px', borderRadius: '4px', fontSize: '11px' }}
        >
          X
        </button>
      </div>

      {/* Company Details */}
      {nodeData.category === 'company' && nodeData.company && (
        <CompanyDetails data={nodeData} />
      )}

      {/* Peer Details */}
      {nodeData.category === 'peer' && nodeData.peer && (
        <PeerDetails data={nodeData} />
      )}

      {/* Institutional Holder */}
      {nodeData.category === 'institutional' && nodeData.holder && (
        <HolderDetails data={nodeData} />
      )}

      {/* Insider */}
      {nodeData.category === 'insider' && nodeData.insider && (
        <InsiderDetails data={nodeData} />
      )}

      {/* Event */}
      {nodeData.category === 'event' && nodeData.event && (
        <EventDetails data={nodeData} />
      )}

      {/* Metrics */}
      {nodeData.category === 'metrics' && nodeData.metrics && (
        <MetricsDetails data={nodeData} />
      )}
    </div>
  );
};

function CompanyDetails({ data }: { data: GraphNodeData }) {
  const company = data.company!;
  const valuation = data.valuation;
  const signalColor = valuation ? getSignalColor(valuation.action) : null;

  const fmt = (num: number): string => {
    if (num >= 1e12) return `$${(num / 1e12).toFixed(2)}T`;
    if (num >= 1e9) return `$${(num / 1e9).toFixed(2)}B`;
    if (num >= 1e6) return `$${(num / 1e6).toFixed(1)}M`;
    return `$${num.toLocaleString()}`;
  };

  return (
    <div>
      {/* Valuation Signal */}
      {valuation && signalColor && (
        <div style={{ background: signalColor.bg, border: `1px solid ${signalColor.border}`, borderRadius: '6px', padding: '10px', marginBottom: '12px' }}>
          <div style={{ fontSize: '12px', fontWeight: 'bold', color: signalColor.text, marginBottom: '4px' }}>
            {getSignalLabel(valuation.status)} - {valuation.action}
          </div>
          <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '6px' }}>
            Confidence: {valuation.confidence} | Score: {valuation.score.toFixed(1)}
          </div>
          {valuation.reasoning.map((r, i) => (
            <div key={i} style={{ fontSize: '9px', color: 'var(--ft-color-text)', marginBottom: '2px' }}>
              - {r}
            </div>
          ))}
        </div>
      )}

      {/* Company Info */}
      <SectionTitle title="Company Info" />
      <DetailRow label="Sector" value={company.sector} />
      <DetailRow label="Industry" value={company.industry} />
      <DetailRow label="Exchange" value={company.exchange} />
      <DetailRow label="Employees" value={company.employees?.toLocaleString() || 'N/A'} />
      {company.website && <DetailRow label="Website" value={company.website} />}

      {/* Valuation */}
      <SectionTitle title="Valuation" />
      <DetailRow label="Market Cap" value={fmt(company.market_cap)} />
      <DetailRow label="P/E Ratio" value={company.pe_ratio ? company.pe_ratio.toFixed(2) : 'N/A'} />
      <DetailRow label="Forward P/E" value={company.forward_pe ? company.forward_pe.toFixed(2) : 'N/A'} />
      <DetailRow label="P/B" value={company.price_to_book ? company.price_to_book.toFixed(2) : 'N/A'} />
      <DetailRow label="PEG" value={company.peg_ratio ? company.peg_ratio.toFixed(2) : 'N/A'} />
      <DetailRow label="EV" value={fmt(company.enterprise_value)} />

      {/* Financials */}
      <SectionTitle title="Financials" />
      <DetailRow label="Revenue" value={fmt(company.revenue)} />
      <DetailRow label="Rev Growth" value={`${(company.revenue_growth * 100).toFixed(1)}%`} />
      <DetailRow label="Profit Margin" value={`${(company.profit_margins * 100).toFixed(1)}%`} />
      <DetailRow label="ROE" value={`${(company.roe * 100).toFixed(1)}%`} />
      <DetailRow label="Free Cash Flow" value={fmt(company.free_cashflow)} />

      {/* Ownership */}
      <SectionTitle title="Ownership" />
      <DetailRow label="Insider %" value={`${company.insider_percent.toFixed(1)}%`} />
      <DetailRow label="Institutional %" value={`${company.institutional_percent.toFixed(1)}%`} />
      <DetailRow label="Shares Outstanding" value={(company.shares_outstanding / 1e9).toFixed(2) + 'B'} />

      {/* Executives */}
      {data.executives && data.executives.length > 0 && (
        <>
          <SectionTitle title="Executives" />
          {data.executives.slice(0, 8).map((exec, i) => (
            <div key={i} style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px', fontSize: '10px' }}>
              <span style={{ color: 'var(--ft-color-text)' }}>{exec.name}</span>
              <span style={{ color: 'var(--ft-color-text-muted)' }}>{exec.role}</span>
            </div>
          ))}
        </>
      )}

      {/* Description */}
      {company.description && company.description !== 'N/A' && (
        <>
          <SectionTitle title="Description" />
          <p style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', lineHeight: 1.4, maxHeight: '80px', overflow: 'hidden' }}>
            {company.description.slice(0, 300)}...
          </p>
        </>
      )}
    </div>
  );
}

function PeerDetails({ data }: { data: GraphNodeData }) {
  const peer = data.peer!;
  const val = data.peerValuation;
  const signalColor = val ? getSignalColor(val.action) : null;

  return (
    <div>
      {val && signalColor && (
        <div style={{ background: signalColor.bg, border: `1px solid ${signalColor.border}`, borderRadius: '4px', padding: '8px', marginBottom: '12px' }}>
          <div style={{ fontSize: '11px', fontWeight: 'bold', color: signalColor.text }}>
            {getSignalLabel(val.status)} - {val.action}
          </div>
          <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginTop: '4px' }}>
            Confidence: {val.confidence} | Score: {val.score.toFixed(1)}
          </div>
        </div>
      )}
      <DetailRow label="Industry" value={peer.industry} />
      <DetailRow label="Market Cap" value={`$${(peer.market_cap / 1e9).toFixed(1)}B`} />
      <DetailRow label="P/E" value={peer.pe_ratio > 0 ? peer.pe_ratio.toFixed(2) : 'N/A'} />
      <DetailRow label="P/B" value={peer.price_to_book > 0 ? peer.price_to_book.toFixed(2) : 'N/A'} />
      <DetailRow label="ROE" value={`${(peer.roe * 100).toFixed(1)}%`} />
      <DetailRow label="Rev Growth" value={`${(peer.revenue_growth * 100).toFixed(1)}%`} />
      <DetailRow label="Margin" value={`${(peer.profit_margins * 100).toFixed(1)}%`} />
    </div>
  );
}

function HolderDetails({ data }: { data: GraphNodeData }) {
  const holder = data.holder!;
  return (
    <div>
      <DetailRow label="Ownership" value={`${holder.percentage.toFixed(2)}%`} />
      <DetailRow label="Shares" value={holder.shares.toLocaleString()} />
      <DetailRow label="Value" value={`$${(holder.value / 1e6).toFixed(1)}M`} />
      {holder.fund_family && <DetailRow label="Fund Family" value={holder.fund_family} />}
    </div>
  );
}

function InsiderDetails({ data }: { data: GraphNodeData }) {
  const insider = data.insider!;
  return (
    <div>
      <DetailRow label="Title" value={insider.title} />
      {insider.percentage > 0 && <DetailRow label="Ownership" value={`${insider.percentage.toFixed(2)}%`} />}
      {insider.last_transaction_date && <DetailRow label="Last Tx Date" value={insider.last_transaction_date} />}
      {insider.last_transaction_type && <DetailRow label="Last Activity" value={insider.last_transaction_type.toUpperCase()} />}

      {data.transactions && data.transactions.length > 0 && (
        <>
          <SectionTitle title="Recent Transactions" />
          {data.transactions.slice(0, 5).map((tx, i) => (
            <div key={i} style={{ fontSize: '9px', marginBottom: '4px', padding: '4px', background: 'rgba(255,255,255,0.03)', borderRadius: '3px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: tx.transaction_type === 'buy' ? 'var(--ft-color-success)' : 'var(--ft-color-alert)' }}>
                  {tx.transaction_type.toUpperCase()}
                </span>
                <span style={{ color: 'var(--ft-color-text-muted)' }}>{tx.date}</span>
              </div>
            </div>
          ))}
        </>
      )}
    </div>
  );
}

function EventDetails({ data }: { data: GraphNodeData }) {
  const event = data.event!;
  return (
    <div>
      <DetailRow label="Form" value={event.form} />
      <DetailRow label="Filing Date" value={event.filing_date} />
      <DetailRow label="Category" value={event.category} />
      {event.description && (
        <div style={{ marginTop: '8px', fontSize: '10px', color: 'var(--ft-color-text)', lineHeight: 1.4 }}>
          {event.description}
        </div>
      )}
      {event.items.length > 0 && (
        <>
          <SectionTitle title="Items" />
          {event.items.map((item, i) => (
            <div key={i} style={{ fontSize: '9px', color: 'var(--ft-color-text)', marginBottom: '3px' }}>
              - {item}
            </div>
          ))}
        </>
      )}
      {event.filing_url && (
        <div style={{ marginTop: '8px' }}>
          <a
            href={event.filing_url}
            target="_blank"
            rel="noopener noreferrer"
            style={{ fontSize: '10px', color: 'var(--ft-color-info)', textDecoration: 'underline' }}
          >
            View Filing on SEC
          </a>
        </div>
      )}
    </div>
  );
}

function MetricsDetails({ data }: { data: GraphNodeData }) {
  const metrics = data.metrics!;
  return (
    <div>
      {Object.entries(metrics).filter(([, v]) => v !== 0 && v !== '').map(([key, value]) => (
        <DetailRow key={key} label={key.replace(/_/g, ' ')} value={String(value)} />
      ))}
    </div>
  );
}

function SectionTitle({ title }: { title: string }) {
  return (
    <div style={{ fontSize: '10px', fontWeight: 'bold', color: 'var(--ft-color-primary)', marginTop: '12px', marginBottom: '6px', borderBottom: '1px solid rgba(120,120,120,0.2)', paddingBottom: '4px' }}>
      {title}
    </div>
  );
}

function DetailRow({ label, value }: { label: string; value: string }) {
  return (
    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
      <span style={{ fontSize: '10px', color: 'var(--ft-color-text-muted)' }}>{label}</span>
      <span style={{ fontSize: '10px', color: 'var(--ft-color-text)', fontWeight: 500 }}>{value}</span>
    </div>
  );
}

export default DetailPanel;
