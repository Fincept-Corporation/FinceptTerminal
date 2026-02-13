/**
 * Research Panel - SEC Filings and Company Research
 *
 * Displays company research including SEC filings, financials,
 * risk factors, and AI-generated summaries.
 */

import React, { useState } from 'react';
import {
  FileText, Search, RefreshCw, AlertTriangle, ExternalLink,
  Building2, Loader2, ChevronDown,
  ChevronUp, Calendar, FileSearch, Shield, BarChart3,
} from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import { useCache, cacheKey } from '@/hooks/useCache';
import { validateSymbol } from '@/services/core/validators';
import {
  alphaArenaEnhancedService,
  type ResearchReport,
} from '../services/alphaArenaEnhancedService';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#3B82F6',
  YELLOW: '#EAB308',
  PURPLE: '#8B5CF6',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", monospace';

const RESEARCH_STYLES = `
  .research-panel *::-webkit-scrollbar { width: 6px; height: 6px; }
  .research-panel *::-webkit-scrollbar-track { background: #000000; }
  .research-panel *::-webkit-scrollbar-thumb { background: #2A2A2A; }
  @keyframes research-spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
`;

interface ResearchPanelProps {
  initialTicker?: string;
  onResearchLoaded?: (ticker: string) => void;
}

const FORM_TYPE_COLORS: Record<string, string> = {
  '10-K': FINCEPT.BLUE,
  '10-Q': FINCEPT.PURPLE,
  '8-K': FINCEPT.ORANGE,
  '4': FINCEPT.GREEN,
  'DEF 14A': FINCEPT.YELLOW,
};

const ResearchPanel: React.FC<ResearchPanelProps> = ({
  initialTicker = '',
  onResearchLoaded,
}) => {
  const [searchTicker, setSearchTicker] = useState(initialTicker);
  const [confirmedTicker, setConfirmedTicker] = useState(initialTicker || '');
  const [expandedSection, setExpandedSection] = useState<string | null>('filings');
  const [selectedFormTypes, setSelectedFormTypes] = useState<string[]>([]);
  const [hoveredBtn, setHoveredBtn] = useState<string | null>(null);
  const [hoveredFiling, setHoveredFiling] = useState<number | null>(null);

  // Cache: research report keyed on confirmedTicker
  const researchCache = useCache<ResearchReport>({
    key: cacheKey('alpha-arena', 'research', confirmedTicker),
    category: 'alpha-arena',
    fetcher: async () => {
      const validSym = validateSymbol(confirmedTicker);
      if (!validSym.valid) throw new Error(validSym.error);
      const result = await alphaArenaEnhancedService.getResearch(confirmedTicker);
      if (result.success && result.report) {
        onResearchLoaded?.(confirmedTicker);
        return result.report;
      }
      throw new Error(result.error || 'Failed to fetch research');
    },
    ttl: 600,
    enabled: !!confirmedTicker,
    staleWhileRevalidate: true,
  });

  const report = researchCache.data;
  const isLoading = researchCache.isLoading;
  const error = researchCache.error;

  const handleSearch = (e: React.FormEvent) => {
    e.preventDefault();
    const trimmed = searchTicker.trim().toUpperCase();
    if (!trimmed) return;
    setConfirmedTicker(trimmed);
  };

  const toggleSection = (section: string) => {
    setExpandedSection(expandedSection === section ? null : section);
  };

  const formatDate = (dateStr: string) => {
    try {
      return new Date(dateStr).toLocaleDateString();
    } catch {
      return dateStr;
    }
  };

  const formatCurrency = (value: number | null) => {
    if (value === null) return 'N/A';
    if (Math.abs(value) >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (Math.abs(value) >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toLocaleString()}`;
  };

  const filterFilings = (filings: ResearchReport['recent_filings']) => {
    if (selectedFormTypes.length === 0) return filings;
    return filings.filter(f => selectedFormTypes.includes(f.form_type));
  };

  return (
    <div className="research-panel" style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      overflow: 'hidden',
      fontFamily: TERMINAL_FONT,
    }}>
      <style>{RESEARCH_STYLES}</style>

      {/* Header */}
      <div style={{
        padding: '10px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <FileSearch size={16} style={{ color: FINCEPT.YELLOW }} />
          <span style={{ fontWeight: 600, fontSize: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
            RESEARCH & SEC FILINGS
          </span>
        </div>
        {report && (
          <button
            onClick={() => researchCache.refresh()}
            disabled={isLoading}
            onMouseEnter={() => setHoveredBtn('refresh')}
            onMouseLeave={() => setHoveredBtn(null)}
            style={{
              padding: '4px',
              backgroundColor: hoveredBtn === 'refresh' ? FINCEPT.HOVER : 'transparent',
              border: 'none',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
            }}
          >
            <RefreshCw
              size={14}
              style={{
                color: FINCEPT.GRAY,
                animation: isLoading ? 'research-spin 1s linear infinite' : 'none',
              }}
            />
          </button>
        )}
      </div>

      {/* Search Bar */}
      <form onSubmit={handleSearch} style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <div style={{ display: 'flex', gap: '8px' }}>
          <input
            type="text"
            value={searchTicker}
            onChange={(e) => setSearchTicker(e.target.value.toUpperCase())}
            placeholder="Enter ticker (e.g., AAPL)"
            style={{
              flex: 1,
              padding: '6px 12px',
              fontSize: '12px',
              fontFamily: TERMINAL_FONT,
              backgroundColor: FINCEPT.CARD_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              outline: 'none',
            }}
          />
          <button
            type="submit"
            disabled={isLoading}
            style={{
              padding: '6px 16px',
              fontSize: '11px',
              fontWeight: 600,
              fontFamily: TERMINAL_FONT,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              opacity: isLoading ? 0.5 : 1,
              letterSpacing: '0.5px',
            }}
          >
            {isLoading ? (
              <Loader2 size={14} style={{ animation: 'research-spin 1s linear infinite' }} />
            ) : (
              <Search size={14} />
            )}
            SEARCH
          </button>
        </div>
      </form>

      {/* Error */}
      {error && (
        <div style={{
          padding: '8px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          backgroundColor: FINCEPT.RED + '10',
        }}>
          <AlertTriangle size={14} style={{ color: FINCEPT.RED }} />
          <span style={{ fontSize: '11px', color: FINCEPT.RED, fontFamily: TERMINAL_FONT }}>{error.message}</span>
        </div>
      )}

      {/* Content */}
      <div style={{ maxHeight: '384px', overflowY: 'auto' }}>
        {isLoading ? (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px 0' }}>
            <Loader2 size={24} style={{ color: FINCEPT.ORANGE, animation: 'research-spin 1s linear infinite' }} />
          </div>
        ) : report ? (
          <>
            {/* Company Info */}
            {report.company_info && (
              <div style={{ padding: '12px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
                  <Building2 size={14} style={{ color: FINCEPT.BLUE }} />
                  <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>
                    {report.company_info.name}
                  </span>
                  <span style={{
                    fontSize: '10px',
                    padding: '1px 8px',
                    backgroundColor: FINCEPT.BORDER,
                    color: FINCEPT.GRAY,
                    fontFamily: TERMINAL_FONT,
                  }}>
                    {report.company_info.ticker}
                  </span>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '11px' }}>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Industry: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{report.company_info.industry || 'N/A'}</span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>CIK: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{report.company_info.cik}</span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>State: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{report.company_info.state || 'N/A'}</span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Fiscal Year End: </span>
                    <span style={{ color: FINCEPT.WHITE }}>{report.company_info.fiscal_year_end || 'N/A'}</span>
                  </div>
                </div>
              </div>
            )}

            {/* Summary */}
            {report.summary && (
              <div style={{ padding: '12px 16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <p style={{ fontSize: '11px', lineHeight: '1.5', color: FINCEPT.GRAY }}>
                  {report.summary}
                </p>
              </div>
            )}

            {/* SEC Filings Section */}
            <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <button
                onClick={() => toggleSection('filings')}
                style={{
                  width: '100%',
                  padding: '8px 16px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  background: 'none',
                  border: 'none',
                  cursor: 'pointer',
                  fontFamily: TERMINAL_FONT,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <FileText size={14} style={{ color: FINCEPT.ORANGE }} />
                  <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>
                    SEC FILINGS ({report.recent_filings.length})
                  </span>
                </div>
                {expandedSection === 'filings' ? (
                  <ChevronUp size={14} style={{ color: FINCEPT.GRAY }} />
                ) : (
                  <ChevronDown size={14} style={{ color: FINCEPT.GRAY }} />
                )}
              </button>
              {expandedSection === 'filings' && (
                <div style={{ padding: '0 16px 12px' }}>
                  {/* Form Type Filters */}
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px', marginBottom: '8px' }}>
                    {['10-K', '10-Q', '8-K', '4', 'DEF 14A'].map((formType) => (
                      <button
                        key={formType}
                        onClick={() => {
                          setSelectedFormTypes(prev =>
                            prev.includes(formType)
                              ? prev.filter(t => t !== formType)
                              : [...prev, formType]
                          );
                        }}
                        style={{
                          padding: '2px 8px',
                          fontSize: '10px',
                          fontFamily: TERMINAL_FONT,
                          backgroundColor: selectedFormTypes.includes(formType)
                            ? (FORM_TYPE_COLORS[formType] || FINCEPT.GRAY) + '30'
                            : FINCEPT.CARD_BG,
                          color: selectedFormTypes.includes(formType)
                            ? FORM_TYPE_COLORS[formType] || FINCEPT.GRAY
                            : FINCEPT.GRAY,
                          border: `1px solid ${selectedFormTypes.includes(formType) ? FORM_TYPE_COLORS[formType] || FINCEPT.GRAY : FINCEPT.BORDER}`,
                          cursor: 'pointer',
                        }}
                      >
                        {formType}
                      </button>
                    ))}
                    {selectedFormTypes.length > 0 && (
                      <button
                        onClick={() => setSelectedFormTypes([])}
                        style={{
                          padding: '2px 8px',
                          fontSize: '10px',
                          color: FINCEPT.RED,
                          background: 'none',
                          border: 'none',
                          cursor: 'pointer',
                          fontFamily: TERMINAL_FONT,
                        }}
                      >
                        Clear
                      </button>
                    )}
                  </div>
                  {/* Filings List */}
                  <div style={{ maxHeight: '192px', overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: '6px' }}>
                    {filterFilings(report.recent_filings).map((filing, idx) => (
                      <div
                        key={idx}
                        onMouseEnter={() => setHoveredFiling(idx)}
                        onMouseLeave={() => setHoveredFiling(null)}
                        style={{
                          padding: '8px',
                          display: 'flex',
                          alignItems: 'flex-start',
                          justifyContent: 'space-between',
                          backgroundColor: hoveredFiling === idx ? FINCEPT.HOVER : FINCEPT.CARD_BG,
                          border: `1px solid ${FINCEPT.BORDER}`,
                        }}
                      >
                        <div style={{ flex: 1, minWidth: 0 }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                            <span style={{
                              fontSize: '10px',
                              padding: '1px 6px',
                              fontWeight: 500,
                              fontFamily: TERMINAL_FONT,
                              backgroundColor: (FORM_TYPE_COLORS[filing.form_type] || FINCEPT.GRAY) + '20',
                              color: FORM_TYPE_COLORS[filing.form_type] || FINCEPT.GRAY,
                            }}>
                              {filing.form_type}
                            </span>
                            <span style={{ fontSize: '10px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
                              <Calendar size={10} />
                              {formatDate(filing.filing_date)}
                            </span>
                          </div>
                          <p style={{ fontSize: '11px', color: FINCEPT.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                            {filing.description || `Filing ${filing.accession_number}`}
                          </p>
                        </div>
                        <a
                          href={`https://www.sec.gov/Archives/edgar/data/${report.company_info?.cik}/${filing.accession_number.replace(/-/g, '')}/${filing.primary_document}`}
                          target="_blank"
                          rel="noopener noreferrer"
                          onClick={(e) => e.stopPropagation()}
                          onMouseEnter={() => setHoveredBtn(`link-${idx}`)}
                          onMouseLeave={() => setHoveredBtn(null)}
                          style={{
                            padding: '4px',
                            backgroundColor: hoveredBtn === `link-${idx}` ? FINCEPT.HOVER : 'transparent',
                            display: 'flex',
                            alignItems: 'center',
                            flexShrink: 0,
                          }}
                        >
                          <ExternalLink size={12} style={{ color: FINCEPT.GRAY }} />
                        </a>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>

            {/* Financials Section */}
            {report.financials.length > 0 && (
              <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <button
                  onClick={() => toggleSection('financials')}
                  style={{
                    width: '100%',
                    padding: '8px 16px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    background: 'none',
                    border: 'none',
                    cursor: 'pointer',
                    fontFamily: TERMINAL_FONT,
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <BarChart3 size={14} style={{ color: FINCEPT.GREEN }} />
                    <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>
                      FINANCIALS
                    </span>
                  </div>
                  {expandedSection === 'financials' ? (
                    <ChevronUp size={14} style={{ color: FINCEPT.GRAY }} />
                  ) : (
                    <ChevronDown size={14} style={{ color: FINCEPT.GRAY }} />
                  )}
                </button>
                {expandedSection === 'financials' && (
                  <div style={{ padding: '0 16px 12px' }}>
                    <table style={{ width: '100%', fontSize: '11px', borderCollapse: 'collapse' }}>
                      <thead>
                        <tr style={{ color: FINCEPT.GRAY }}>
                          <th style={{ textAlign: 'left', padding: '4px 0', fontWeight: 500, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Period</th>
                          <th style={{ textAlign: 'right', padding: '4px 0', fontWeight: 500, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Revenue</th>
                          <th style={{ textAlign: 'right', padding: '4px 0', fontWeight: 500, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>Net Income</th>
                          <th style={{ textAlign: 'right', padding: '4px 0', fontWeight: 500, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>EPS</th>
                        </tr>
                      </thead>
                      <tbody>
                        {report.financials.slice(0, 5).map((fin, idx) => (
                          <tr key={idx} style={{ borderTop: `1px solid ${FINCEPT.BORDER}` }}>
                            <td style={{ padding: '6px 0', color: FINCEPT.WHITE }}>{fin.period}</td>
                            <td style={{ textAlign: 'right', padding: '6px 0', color: FINCEPT.WHITE }}>
                              {formatCurrency(fin.revenue)}
                            </td>
                            <td style={{
                              textAlign: 'right',
                              padding: '6px 0',
                              color: fin.net_income && fin.net_income > 0 ? FINCEPT.GREEN : FINCEPT.RED,
                            }}>
                              {formatCurrency(fin.net_income)}
                            </td>
                            <td style={{ textAlign: 'right', padding: '6px 0', color: FINCEPT.WHITE }}>
                              {fin.eps !== null ? `$${fin.eps.toFixed(2)}` : 'N/A'}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
              </div>
            )}

            {/* Risk Factors Section */}
            {report.risk_factors.length > 0 && (
              <div>
                <button
                  onClick={() => toggleSection('risks')}
                  style={{
                    width: '100%',
                    padding: '8px 16px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    background: 'none',
                    border: 'none',
                    cursor: 'pointer',
                    fontFamily: TERMINAL_FONT,
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <Shield size={14} style={{ color: FINCEPT.RED }} />
                    <span style={{ fontSize: '12px', fontWeight: 500, color: FINCEPT.WHITE }}>
                      RISK FACTORS ({report.risk_factors.length})
                    </span>
                  </div>
                  {expandedSection === 'risks' ? (
                    <ChevronUp size={14} style={{ color: FINCEPT.GRAY }} />
                  ) : (
                    <ChevronDown size={14} style={{ color: FINCEPT.GRAY }} />
                  )}
                </button>
                {expandedSection === 'risks' && (
                  <div style={{ padding: '0 16px 12px' }}>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                      {report.risk_factors.map((risk, idx) => (
                        <div
                          key={idx}
                          style={{
                            fontSize: '11px',
                            paddingLeft: '12px',
                            position: 'relative',
                            color: FINCEPT.GRAY,
                          }}
                        >
                          <span style={{
                            position: 'absolute',
                            left: 0,
                            top: '6px',
                            width: '4px',
                            height: '4px',
                            backgroundColor: FINCEPT.RED,
                          }} />
                          {risk}
                        </div>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            )}
          </>
        ) : (
          <div style={{ padding: '32px 16px', textAlign: 'center' }}>
            <FileSearch size={32} style={{ color: FINCEPT.GRAY, opacity: 0.3, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '12px', color: FINCEPT.GRAY }}>Search for a company</p>
            <p style={{ fontSize: '10px', marginTop: '4px', color: FINCEPT.GRAY }}>
              Enter a ticker to view SEC filings and research
            </p>
          </div>
        )}
      </div>

      {/* Footer */}
      {report && (
        <div style={{
          padding: '6px 16px',
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          fontSize: '9px',
          backgroundColor: FINCEPT.HEADER_BG,
          color: FINCEPT.GRAY,
        }}>
          Generated: {formatDate(report.generated_at)} | Data from SEC EDGAR
        </div>
      )}
    </div>
  );
};

export default withErrorBoundary(ResearchPanel, { name: 'AlphaArena.ResearchPanel' });
