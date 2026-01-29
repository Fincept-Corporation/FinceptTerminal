/**
 * Surface Analytics - Reference Data Panel Component
 * Display security master, corporate actions, and adjustment factors
 */

import React, { useState, useCallback } from 'react';
import {
  Building2,
  Calendar,
  TrendingUp,
  RefreshCw,
  ChevronDown,
  ChevronUp,
  AlertCircle,
  DollarSign,
  Percent,
  Globe,
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { FINCEPT_COLORS } from '../constants';

interface SecurityMaster {
  raw_symbol: string;
  instrument_id: string;
  security_type: string;
  exchange: string;
  currency: string;
  name: string;
  description: string;
  sector: string;
  industry: string;
  market_cap: number | null;
  shares_outstanding: number | null;
}

interface CorporateAction {
  raw_symbol: string;
  action_type: string;
  ex_date: string;
  record_date: string;
  payment_date: string;
  amount: number | null;
  ratio: string | null;
  currency: string;
  description: string;
}

interface AdjustmentFactor {
  raw_symbol: string;
  date: string;
  split_factor: number;
  dividend_factor: number;
  cumulative_factor: number;
}

interface ReferenceDataPanelProps {
  apiKey: string | null;
  symbols: string[];
  accentColor: string;
  textColor: string;
}

type TabType = 'security' | 'corporate' | 'adjustments';

export const ReferenceDataPanel: React.FC<ReferenceDataPanelProps> = ({
  apiKey,
  symbols,
  accentColor,
  textColor,
}) => {
  const [activeTab, setActiveTab] = useState<TabType>('security');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Data states
  const [securityData, setSecurityData] = useState<SecurityMaster[]>([]);
  const [corporateActions, setCorporateActions] = useState<CorporateAction[]>([]);
  const [adjustmentFactors, setAdjustmentFactors] = useState<AdjustmentFactor[]>([]);

  // Expanded rows for detail views
  const [expandedRow, setExpandedRow] = useState<string | null>(null);

  const fetchSecurityMaster = useCallback(async () => {
    if (!apiKey || symbols.length === 0) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_security_master', {
        apiKey,
        symbols,
        dataset: null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setSecurityData(parsed.securities || []);
      }
    } catch (err) {
      setError(`Failed to fetch security master: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, symbols]);

  const fetchCorporateActions = useCallback(async () => {
    if (!apiKey || symbols.length === 0) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_corporate_actions', {
        apiKey,
        symbols,
        startDate: null,
        endDate: null,
        actionType: null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setCorporateActions(parsed.actions || []);
      }
    } catch (err) {
      setError(`Failed to fetch corporate actions: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, symbols]);

  const fetchAdjustmentFactors = useCallback(async () => {
    if (!apiKey || symbols.length === 0) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_get_adjustment_factors', {
        apiKey,
        symbols,
        startDate: null,
        endDate: null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setAdjustmentFactors(parsed.factors || []);
      }
    } catch (err) {
      setError(`Failed to fetch adjustment factors: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, symbols]);

  const handleRefresh = useCallback(() => {
    switch (activeTab) {
      case 'security':
        fetchSecurityMaster();
        break;
      case 'corporate':
        fetchCorporateActions();
        break;
      case 'adjustments':
        fetchAdjustmentFactors();
        break;
    }
  }, [activeTab, fetchSecurityMaster, fetchCorporateActions, fetchAdjustmentFactors]);

  const formatMarketCap = (value: number | null): string => {
    if (!value) return 'N/A';
    if (value >= 1e12) return `$${(value / 1e12).toFixed(2)}T`;
    if (value >= 1e9) return `$${(value / 1e9).toFixed(2)}B`;
    if (value >= 1e6) return `$${(value / 1e6).toFixed(2)}M`;
    return `$${value.toLocaleString()}`;
  };

  const formatActionType = (type: string): { label: string; color: string } => {
    switch (type.toLowerCase()) {
      case 'dividend':
        return { label: 'DIVIDEND', color: FINCEPT_COLORS.GREEN };
      case 'split':
        return { label: 'SPLIT', color: FINCEPT_COLORS.CYAN };
      case 'spinoff':
        return { label: 'SPINOFF', color: FINCEPT_COLORS.YELLOW };
      case 'merger':
        return { label: 'MERGER', color: FINCEPT_COLORS.PURPLE };
      default:
        return { label: type.toUpperCase(), color: FINCEPT_COLORS.MUTED };
    }
  };

  if (!apiKey) {
    return (
      <div
        className="p-4 text-center"
        style={{
          backgroundColor: FINCEPT_COLORS.DARK_BG,
          border: `1px solid ${FINCEPT_COLORS.BORDER}`,
          borderRadius: '2px',
        }}
      >
        <Building2 size={24} style={{ color: FINCEPT_COLORS.MUTED, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
          Configure Databento API key to access reference data
        </div>
      </div>
    );
  }

  return (
    <div
      style={{
        backgroundColor: FINCEPT_COLORS.DARK_BG,
        border: `1px solid ${FINCEPT_COLORS.BORDER}`,
        borderRadius: '2px',
        overflow: 'hidden',
      }}
    >
      {/* Header with Tabs */}
      <div
        className="flex items-center justify-between p-2"
        style={{
          backgroundColor: FINCEPT_COLORS.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}`,
        }}
      >
        <div className="flex items-center gap-1">
          {[
            { id: 'security', label: 'SECURITY MASTER', icon: Building2 },
            { id: 'corporate', label: 'CORPORATE ACTIONS', icon: Calendar },
            { id: 'adjustments', label: 'ADJUSTMENTS', icon: TrendingUp },
          ].map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              onClick={() => setActiveTab(id as TabType)}
              className="flex items-center gap-1 px-2 py-1 text-[9px] font-bold"
              style={{
                backgroundColor: activeTab === id ? accentColor : 'transparent',
                color: activeTab === id ? FINCEPT_COLORS.BLACK : FINCEPT_COLORS.MUTED,
                border: `1px solid ${activeTab === id ? accentColor : FINCEPT_COLORS.BORDER}`,
                borderRadius: '2px',
              }}
            >
              <Icon size={10} />
              {label}
            </button>
          ))}
        </div>
        <button
          onClick={handleRefresh}
          disabled={loading || symbols.length === 0}
          style={{ color: loading ? FINCEPT_COLORS.MUTED : accentColor }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
        </button>
      </div>

      {/* Symbols Badge */}
      <div className="flex flex-wrap gap-1 p-2" style={{ borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}` }}>
        <span className="text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>SYMBOLS:</span>
        {symbols.slice(0, 5).map(sym => (
          <span
            key={sym}
            className="px-1 py-0.5 text-[9px]"
            style={{
              backgroundColor: FINCEPT_COLORS.BORDER,
              color: textColor,
              borderRadius: '2px',
            }}
          >
            {sym}
          </span>
        ))}
        {symbols.length > 5 && (
          <span className="text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
            +{symbols.length - 5} more
          </span>
        )}
      </div>

      {/* Error Message */}
      {error && (
        <div
          className="flex items-center gap-2 m-2 p-2 text-xs"
          style={{
            backgroundColor: `${FINCEPT_COLORS.RED}15`,
            border: `1px solid ${FINCEPT_COLORS.RED}50`,
            borderRadius: '2px',
            color: FINCEPT_COLORS.RED,
          }}
        >
          <AlertCircle size={12} />
          <span>{error}</span>
        </div>
      )}

      {/* Content Area */}
      <div className="p-2 max-h-64 overflow-y-auto">
        {loading ? (
          <div className="flex items-center justify-center p-4">
            <RefreshCw size={16} className="animate-spin" style={{ color: accentColor }} />
          </div>
        ) : activeTab === 'security' ? (
          securityData.length === 0 ? (
            <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
              Click refresh to load security master data
            </div>
          ) : (
            securityData.map((sec, idx) => (
              <div
                key={`${sec.raw_symbol}-${idx}`}
                className="mb-2"
                style={{
                  backgroundColor: FINCEPT_COLORS.BLACK,
                  border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                  borderRadius: '2px',
                }}
              >
                <div
                  className="flex items-center justify-between p-2 cursor-pointer"
                  onClick={() => setExpandedRow(expandedRow === sec.raw_symbol ? null : sec.raw_symbol)}
                >
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-bold" style={{ color: accentColor }}>
                      {sec.raw_symbol}
                    </span>
                    <span className="text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
                      {sec.name || sec.description}
                    </span>
                  </div>
                  {expandedRow === sec.raw_symbol ? (
                    <ChevronUp size={12} style={{ color: FINCEPT_COLORS.MUTED }} />
                  ) : (
                    <ChevronDown size={12} style={{ color: FINCEPT_COLORS.MUTED }} />
                  )}
                </div>
                {expandedRow === sec.raw_symbol && (
                  <div className="grid grid-cols-2 gap-2 p-2 text-[9px]" style={{ borderTop: `1px solid ${FINCEPT_COLORS.BORDER}` }}>
                    <div>
                      <span style={{ color: FINCEPT_COLORS.MUTED }}>Exchange: </span>
                      <span style={{ color: textColor }}>{sec.exchange || 'N/A'}</span>
                    </div>
                    <div>
                      <span style={{ color: FINCEPT_COLORS.MUTED }}>Currency: </span>
                      <span style={{ color: textColor }}>{sec.currency || 'USD'}</span>
                    </div>
                    <div>
                      <span style={{ color: FINCEPT_COLORS.MUTED }}>Sector: </span>
                      <span style={{ color: textColor }}>{sec.sector || 'N/A'}</span>
                    </div>
                    <div>
                      <span style={{ color: FINCEPT_COLORS.MUTED }}>Industry: </span>
                      <span style={{ color: textColor }}>{sec.industry || 'N/A'}</span>
                    </div>
                    <div>
                      <span style={{ color: FINCEPT_COLORS.MUTED }}>Market Cap: </span>
                      <span style={{ color: FINCEPT_COLORS.GREEN }}>{formatMarketCap(sec.market_cap)}</span>
                    </div>
                    <div>
                      <span style={{ color: FINCEPT_COLORS.MUTED }}>Type: </span>
                      <span style={{ color: textColor }}>{sec.security_type || 'Equity'}</span>
                    </div>
                  </div>
                )}
              </div>
            ))
          )
        ) : activeTab === 'corporate' ? (
          corporateActions.length === 0 ? (
            <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
              Click refresh to load corporate actions
            </div>
          ) : (
            corporateActions.map((action, idx) => {
              const { label, color } = formatActionType(action.action_type);
              return (
                <div
                  key={`${action.raw_symbol}-${action.ex_date}-${idx}`}
                  className="flex items-center justify-between p-2 mb-1"
                  style={{
                    backgroundColor: FINCEPT_COLORS.BLACK,
                    border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                    borderRadius: '2px',
                  }}
                >
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-bold" style={{ color: accentColor }}>
                      {action.raw_symbol}
                    </span>
                    <span
                      className="px-1 py-0.5 text-[8px] font-bold"
                      style={{
                        backgroundColor: `${color}20`,
                        color: color,
                        borderRadius: '2px',
                      }}
                    >
                      {label}
                    </span>
                  </div>
                  <div className="flex items-center gap-3 text-[9px]">
                    {action.amount && (
                      <div className="flex items-center gap-1" style={{ color: FINCEPT_COLORS.GREEN }}>
                        <DollarSign size={10} />
                        <span>{action.amount.toFixed(4)} {action.currency}</span>
                      </div>
                    )}
                    {action.ratio && (
                      <div className="flex items-center gap-1" style={{ color: FINCEPT_COLORS.CYAN }}>
                        <Percent size={10} />
                        <span>{action.ratio}</span>
                      </div>
                    )}
                    <span style={{ color: FINCEPT_COLORS.MUTED }}>{action.ex_date}</span>
                  </div>
                </div>
              );
            })
          )
        ) : (
          adjustmentFactors.length === 0 ? (
            <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
              Click refresh to load adjustment factors
            </div>
          ) : (
            <div
              className="overflow-x-auto"
              style={{
                backgroundColor: FINCEPT_COLORS.BLACK,
                border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                borderRadius: '2px',
              }}
            >
              <table className="w-full text-[9px]">
                <thead>
                  <tr style={{ backgroundColor: FINCEPT_COLORS.HEADER_BG }}>
                    <th className="p-1 text-left" style={{ color: accentColor }}>SYMBOL</th>
                    <th className="p-1 text-left" style={{ color: accentColor }}>DATE</th>
                    <th className="p-1 text-right" style={{ color: accentColor }}>SPLIT</th>
                    <th className="p-1 text-right" style={{ color: accentColor }}>DIVIDEND</th>
                    <th className="p-1 text-right" style={{ color: accentColor }}>CUMULATIVE</th>
                  </tr>
                </thead>
                <tbody>
                  {adjustmentFactors.slice(0, 20).map((factor, idx) => (
                    <tr
                      key={`${factor.raw_symbol}-${factor.date}-${idx}`}
                      style={{ borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}` }}
                    >
                      <td className="p-1" style={{ color: textColor }}>{factor.raw_symbol}</td>
                      <td className="p-1" style={{ color: FINCEPT_COLORS.MUTED }}>{factor.date}</td>
                      <td className="p-1 text-right" style={{ color: factor.split_factor !== 1 ? FINCEPT_COLORS.CYAN : textColor }}>
                        {factor.split_factor.toFixed(6)}
                      </td>
                      <td className="p-1 text-right" style={{ color: factor.dividend_factor !== 1 ? FINCEPT_COLORS.GREEN : textColor }}>
                        {factor.dividend_factor.toFixed(6)}
                      </td>
                      <td className="p-1 text-right" style={{ color: FINCEPT_COLORS.YELLOW }}>
                        {factor.cumulative_factor.toFixed(6)}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
              {adjustmentFactors.length > 20 && (
                <div className="p-1 text-[9px] text-center" style={{ color: FINCEPT_COLORS.MUTED }}>
                  Showing 20 of {adjustmentFactors.length} records
                </div>
              )}
            </div>
          )
        )}
      </div>
    </div>
  );
};
