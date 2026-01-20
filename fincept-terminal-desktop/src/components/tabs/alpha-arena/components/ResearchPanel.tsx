/**
 * Research Panel - SEC Filings and Company Research
 *
 * Displays company research including SEC filings, financials,
 * risk factors, and AI-generated summaries.
 */

import React, { useState, useCallback } from 'react';
import {
  FileText, Search, RefreshCw, AlertTriangle, ExternalLink,
  Building2, DollarSign, TrendingUp, Loader2, ChevronDown,
  ChevronUp, Calendar, FileSearch, Shield, BarChart3,
} from 'lucide-react';
import {
  alphaArenaEnhancedService,
  type ResearchReport,
} from '../services/alphaArenaEnhancedService';

const COLORS = {
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
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
};

interface ResearchPanelProps {
  initialTicker?: string;
  onResearchLoaded?: (ticker: string) => void;
}

const FORM_TYPE_COLORS: Record<string, string> = {
  '10-K': COLORS.BLUE,
  '10-Q': COLORS.PURPLE,
  '8-K': COLORS.ORANGE,
  '4': COLORS.GREEN,
  'DEF 14A': COLORS.YELLOW,
};

const ResearchPanel: React.FC<ResearchPanelProps> = ({
  initialTicker = '',
  onResearchLoaded,
}) => {
  const [ticker, setTicker] = useState(initialTicker);
  const [searchTicker, setSearchTicker] = useState(initialTicker);
  const [report, setReport] = useState<ResearchReport | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [expandedSection, setExpandedSection] = useState<string | null>('filings');
  const [selectedFormTypes, setSelectedFormTypes] = useState<string[]>([]);

  const fetchResearch = useCallback(async (tickerToFetch: string) => {
    if (!tickerToFetch.trim()) {
      setError('Please enter a ticker symbol');
      return;
    }

    setIsLoading(true);
    setError(null);
    setTicker(tickerToFetch.toUpperCase());

    try {
      const result = await alphaArenaEnhancedService.getResearch(tickerToFetch);

      if (result.success && result.report) {
        setReport(result.report);
        onResearchLoaded?.(tickerToFetch);
      } else {
        setError(result.error || 'Failed to fetch research');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to fetch research');
    } finally {
      setIsLoading(false);
    }
  }, [onResearchLoaded]);

  const handleSearch = (e: React.FormEvent) => {
    e.preventDefault();
    fetchResearch(searchTicker);
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
    <div
      className="rounded-lg overflow-hidden"
      style={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}` }}
    >
      {/* Header */}
      <div className="px-4 py-3 border-b flex items-center justify-between" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex items-center gap-2">
          <FileSearch size={16} style={{ color: COLORS.YELLOW }} />
          <span className="font-semibold text-sm" style={{ color: COLORS.WHITE }}>
            Research & SEC Filings
          </span>
        </div>
        {report && (
          <button
            onClick={() => fetchResearch(ticker)}
            disabled={isLoading}
            className="p-1.5 rounded transition-colors hover:bg-[#1A1A1A]"
          >
            <RefreshCw
              size={14}
              className={isLoading ? 'animate-spin' : ''}
              style={{ color: COLORS.GRAY }}
            />
          </button>
        )}
      </div>

      {/* Search Bar */}
      <form onSubmit={handleSearch} className="px-4 py-3 border-b" style={{ borderColor: COLORS.BORDER }}>
        <div className="flex gap-2">
          <input
            type="text"
            value={searchTicker}
            onChange={(e) => setSearchTicker(e.target.value.toUpperCase())}
            placeholder="Enter ticker (e.g., AAPL)"
            className="flex-1 px-3 py-1.5 rounded text-sm"
            style={{
              backgroundColor: COLORS.CARD_BG,
              color: COLORS.WHITE,
              border: `1px solid ${COLORS.BORDER}`,
            }}
          />
          <button
            type="submit"
            disabled={isLoading}
            className="px-4 py-1.5 rounded text-sm font-medium flex items-center gap-1"
            style={{
              backgroundColor: COLORS.ORANGE,
              color: COLORS.DARK_BG,
              opacity: isLoading ? 0.5 : 1,
            }}
          >
            {isLoading ? (
              <Loader2 size={14} className="animate-spin" />
            ) : (
              <Search size={14} />
            )}
            Search
          </button>
        </div>
      </form>

      {/* Error */}
      {error && (
        <div className="px-4 py-2 flex items-center gap-2" style={{ backgroundColor: COLORS.RED + '10' }}>
          <AlertTriangle size={14} style={{ color: COLORS.RED }} />
          <span className="text-xs" style={{ color: COLORS.RED }}>{error}</span>
        </div>
      )}

      {/* Content */}
      <div className="max-h-96 overflow-y-auto">
        {isLoading ? (
          <div className="flex items-center justify-center py-8">
            <Loader2 size={24} className="animate-spin" style={{ color: COLORS.ORANGE }} />
          </div>
        ) : report ? (
          <>
            {/* Company Info */}
            {report.company_info && (
              <div className="px-4 py-3 border-b" style={{ borderColor: COLORS.BORDER }}>
                <div className="flex items-center gap-2 mb-2">
                  <Building2 size={14} style={{ color: COLORS.BLUE }} />
                  <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>
                    {report.company_info.name}
                  </span>
                  <span className="text-xs px-2 py-0.5 rounded" style={{ backgroundColor: COLORS.BORDER, color: COLORS.GRAY }}>
                    {report.company_info.ticker}
                  </span>
                </div>
                <div className="grid grid-cols-2 gap-2 text-xs">
                  <div>
                    <span style={{ color: COLORS.GRAY }}>Industry: </span>
                    <span style={{ color: COLORS.WHITE }}>{report.company_info.industry || 'N/A'}</span>
                  </div>
                  <div>
                    <span style={{ color: COLORS.GRAY }}>CIK: </span>
                    <span style={{ color: COLORS.WHITE }}>{report.company_info.cik}</span>
                  </div>
                  <div>
                    <span style={{ color: COLORS.GRAY }}>State: </span>
                    <span style={{ color: COLORS.WHITE }}>{report.company_info.state || 'N/A'}</span>
                  </div>
                  <div>
                    <span style={{ color: COLORS.GRAY }}>Fiscal Year End: </span>
                    <span style={{ color: COLORS.WHITE }}>{report.company_info.fiscal_year_end || 'N/A'}</span>
                  </div>
                </div>
              </div>
            )}

            {/* Summary */}
            {report.summary && (
              <div className="px-4 py-3 border-b" style={{ borderColor: COLORS.BORDER }}>
                <p className="text-xs leading-relaxed" style={{ color: COLORS.GRAY }}>
                  {report.summary}
                </p>
              </div>
            )}

            {/* SEC Filings Section */}
            <div className="border-b" style={{ borderColor: COLORS.BORDER }}>
              <button
                onClick={() => toggleSection('filings')}
                className="w-full px-4 py-2 flex items-center justify-between"
              >
                <div className="flex items-center gap-2">
                  <FileText size={14} style={{ color: COLORS.ORANGE }} />
                  <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>
                    SEC Filings ({report.recent_filings.length})
                  </span>
                </div>
                {expandedSection === 'filings' ? (
                  <ChevronUp size={14} style={{ color: COLORS.GRAY }} />
                ) : (
                  <ChevronDown size={14} style={{ color: COLORS.GRAY }} />
                )}
              </button>
              {expandedSection === 'filings' && (
                <div className="px-4 pb-3">
                  {/* Form Type Filters */}
                  <div className="flex flex-wrap gap-1 mb-2">
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
                        className="px-2 py-0.5 rounded text-xs"
                        style={{
                          backgroundColor: selectedFormTypes.includes(formType)
                            ? (FORM_TYPE_COLORS[formType] || COLORS.GRAY) + '30'
                            : COLORS.CARD_BG,
                          color: selectedFormTypes.includes(formType)
                            ? FORM_TYPE_COLORS[formType] || COLORS.GRAY
                            : COLORS.GRAY,
                          border: `1px solid ${selectedFormTypes.includes(formType) ? FORM_TYPE_COLORS[formType] || COLORS.GRAY : COLORS.BORDER}`,
                        }}
                      >
                        {formType}
                      </button>
                    ))}
                    {selectedFormTypes.length > 0 && (
                      <button
                        onClick={() => setSelectedFormTypes([])}
                        className="px-2 py-0.5 rounded text-xs"
                        style={{ color: COLORS.RED }}
                      >
                        Clear
                      </button>
                    )}
                  </div>
                  {/* Filings List */}
                  <div className="space-y-2 max-h-48 overflow-y-auto">
                    {filterFilings(report.recent_filings).map((filing, idx) => (
                      <div
                        key={idx}
                        className="p-2 rounded flex items-start justify-between"
                        style={{ backgroundColor: COLORS.CARD_BG }}
                      >
                        <div className="flex-1 min-w-0">
                          <div className="flex items-center gap-2 mb-1">
                            <span
                              className="text-xs px-1.5 py-0.5 rounded font-medium"
                              style={{
                                backgroundColor: (FORM_TYPE_COLORS[filing.form_type] || COLORS.GRAY) + '20',
                                color: FORM_TYPE_COLORS[filing.form_type] || COLORS.GRAY,
                              }}
                            >
                              {filing.form_type}
                            </span>
                            <span className="text-xs" style={{ color: COLORS.GRAY }}>
                              <Calendar size={10} className="inline mr-1" />
                              {formatDate(filing.filing_date)}
                            </span>
                          </div>
                          <p className="text-xs truncate" style={{ color: COLORS.WHITE }}>
                            {filing.description || `Filing ${filing.accession_number}`}
                          </p>
                        </div>
                        <a
                          href={`https://www.sec.gov/Archives/edgar/data/${report.company_info?.cik}/${filing.accession_number.replace(/-/g, '')}/${filing.primary_document}`}
                          target="_blank"
                          rel="noopener noreferrer"
                          className="p-1 rounded hover:bg-[#1A1A1A]"
                        >
                          <ExternalLink size={12} style={{ color: COLORS.GRAY }} />
                        </a>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>

            {/* Financials Section */}
            {report.financials.length > 0 && (
              <div className="border-b" style={{ borderColor: COLORS.BORDER }}>
                <button
                  onClick={() => toggleSection('financials')}
                  className="w-full px-4 py-2 flex items-center justify-between"
                >
                  <div className="flex items-center gap-2">
                    <BarChart3 size={14} style={{ color: COLORS.GREEN }} />
                    <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>
                      Financials
                    </span>
                  </div>
                  {expandedSection === 'financials' ? (
                    <ChevronUp size={14} style={{ color: COLORS.GRAY }} />
                  ) : (
                    <ChevronDown size={14} style={{ color: COLORS.GRAY }} />
                  )}
                </button>
                {expandedSection === 'financials' && (
                  <div className="px-4 pb-3">
                    <table className="w-full text-xs">
                      <thead>
                        <tr style={{ color: COLORS.GRAY }}>
                          <th className="text-left py-1">Period</th>
                          <th className="text-right py-1">Revenue</th>
                          <th className="text-right py-1">Net Income</th>
                          <th className="text-right py-1">EPS</th>
                        </tr>
                      </thead>
                      <tbody>
                        {report.financials.slice(0, 5).map((fin, idx) => (
                          <tr key={idx} className="border-t" style={{ borderColor: COLORS.BORDER }}>
                            <td className="py-1.5" style={{ color: COLORS.WHITE }}>{fin.period}</td>
                            <td className="text-right py-1.5" style={{ color: COLORS.WHITE }}>
                              {formatCurrency(fin.revenue)}
                            </td>
                            <td
                              className="text-right py-1.5"
                              style={{
                                color: fin.net_income && fin.net_income > 0 ? COLORS.GREEN : COLORS.RED,
                              }}
                            >
                              {formatCurrency(fin.net_income)}
                            </td>
                            <td className="text-right py-1.5" style={{ color: COLORS.WHITE }}>
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
                  className="w-full px-4 py-2 flex items-center justify-between"
                >
                  <div className="flex items-center gap-2">
                    <Shield size={14} style={{ color: COLORS.RED }} />
                    <span className="text-sm font-medium" style={{ color: COLORS.WHITE }}>
                      Risk Factors ({report.risk_factors.length})
                    </span>
                  </div>
                  {expandedSection === 'risks' ? (
                    <ChevronUp size={14} style={{ color: COLORS.GRAY }} />
                  ) : (
                    <ChevronDown size={14} style={{ color: COLORS.GRAY }} />
                  )}
                </button>
                {expandedSection === 'risks' && (
                  <div className="px-4 pb-3">
                    <ul className="space-y-1">
                      {report.risk_factors.map((risk, idx) => (
                        <li
                          key={idx}
                          className="text-xs pl-3 relative"
                          style={{ color: COLORS.GRAY }}
                        >
                          <span
                            className="absolute left-0 top-1.5 w-1.5 h-1.5 rounded-full"
                            style={{ backgroundColor: COLORS.RED }}
                          />
                          {risk}
                        </li>
                      ))}
                    </ul>
                  </div>
                )}
              </div>
            )}
          </>
        ) : (
          <div className="p-8 text-center">
            <FileSearch size={32} className="mx-auto mb-2 opacity-30" style={{ color: COLORS.GRAY }} />
            <p className="text-sm" style={{ color: COLORS.GRAY }}>Search for a company</p>
            <p className="text-xs mt-1" style={{ color: COLORS.GRAY }}>
              Enter a ticker to view SEC filings and research
            </p>
          </div>
        )}
      </div>

      {/* Footer */}
      {report && (
        <div className="px-4 py-2 border-t text-xs" style={{ borderColor: COLORS.BORDER, color: COLORS.GRAY }}>
          Generated: {formatDate(report.generated_at)} • Data from SEC EDGAR
        </div>
      )}
    </div>
  );
};

export default ResearchPanel;
