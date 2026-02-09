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
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';

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
  const { t } = useTranslation('surfaceAnalytics');
  const { colors } = useTerminalTheme();
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
        return { label: 'DIVIDEND', color: colors.success };
      case 'split':
        return { label: 'SPLIT', color: colors.info };
      case 'spinoff':
        return { label: 'SPINOFF', color: colors.warning };
      case 'merger':
        return { label: 'MERGER', color: colors.info };
      default:
        return { label: type.toUpperCase(), color: colors.textMuted };
    }
  };

  if (!apiKey) {
    return (
      <div
        className="p-4 text-center"
        style={{
          backgroundColor: colors.background,
          border: `1px solid ${colors.textMuted}`,
          borderRadius: 'var(--ft-border-radius)',
        }}
      >
        <Building2 size={24} style={{ color: colors.textMuted, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: colors.textMuted }}>
          Configure Databento API key to access reference data
        </div>
      </div>
    );
  }

  return (
    <div
      style={{
        backgroundColor: colors.background,
        border: `1px solid ${colors.textMuted}`,
        borderRadius: 'var(--ft-border-radius)',
        overflow: 'hidden',
      }}
    >
      {/* Header with Tabs */}
      <div
        className="flex items-center justify-between p-2"
        style={{
          backgroundColor: colors.panel,
          borderBottom: `1px solid ${colors.textMuted}`,
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
                color: activeTab === id ? colors.background : colors.textMuted,
                border: `1px solid ${activeTab === id ? accentColor : colors.textMuted}`,
                borderRadius: 'var(--ft-border-radius)',
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
          style={{ color: loading ? colors.textMuted : accentColor }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
        </button>
      </div>

      {/* Symbols Badge */}
      <div className="flex flex-wrap gap-1 p-2" style={{ borderBottom: `1px solid ${colors.textMuted}` }}>
        <span className="text-[9px]" style={{ color: colors.textMuted }}>SYMBOLS:</span>
        {symbols.slice(0, 5).map(sym => (
          <span
            key={sym}
            className="px-1 py-0.5 text-[9px]"
            style={{
              backgroundColor: colors.textMuted,
              color: textColor,
              borderRadius: 'var(--ft-border-radius)',
            }}
          >
            {sym}
          </span>
        ))}
        {symbols.length > 5 && (
          <span className="text-[9px]" style={{ color: colors.textMuted }}>
            +{symbols.length - 5} more
          </span>
        )}
      </div>

      {/* Error Message */}
      {error && (
        <div
          className="flex items-center gap-2 m-2 p-2 text-xs"
          style={{
            backgroundColor: `${colors.alert}15`,
            border: `1px solid ${colors.alert}50`,
            borderRadius: 'var(--ft-border-radius)',
            color: colors.alert,
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
            <div className="text-xs text-center p-4" style={{ color: colors.textMuted }}>
              Click refresh to load security master data
            </div>
          ) : (
            securityData.map((sec, idx) => (
              <div
                key={`${sec.raw_symbol}-${idx}`}
                className="mb-2"
                style={{
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  borderRadius: 'var(--ft-border-radius)',
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
                    <span className="text-[9px]" style={{ color: colors.textMuted }}>
                      {sec.name || sec.description}
                    </span>
                  </div>
                  {expandedRow === sec.raw_symbol ? (
                    <ChevronUp size={12} style={{ color: colors.textMuted }} />
                  ) : (
                    <ChevronDown size={12} style={{ color: colors.textMuted }} />
                  )}
                </div>
                {expandedRow === sec.raw_symbol && (
                  <div className="grid grid-cols-2 gap-2 p-2 text-[9px]" style={{ borderTop: `1px solid ${colors.textMuted}` }}>
                    <div>
                      <span style={{ color: colors.textMuted }}>Exchange: </span>
                      <span style={{ color: textColor }}>{sec.exchange || 'N/A'}</span>
                    </div>
                    <div>
                      <span style={{ color: colors.textMuted }}>Currency: </span>
                      <span style={{ color: textColor }}>{sec.currency || 'USD'}</span>
                    </div>
                    <div>
                      <span style={{ color: colors.textMuted }}>Sector: </span>
                      <span style={{ color: textColor }}>{sec.sector || 'N/A'}</span>
                    </div>
                    <div>
                      <span style={{ color: colors.textMuted }}>Industry: </span>
                      <span style={{ color: textColor }}>{sec.industry || 'N/A'}</span>
                    </div>
                    <div>
                      <span style={{ color: colors.textMuted }}>Market Cap: </span>
                      <span style={{ color: colors.success }}>{formatMarketCap(sec.market_cap)}</span>
                    </div>
                    <div>
                      <span style={{ color: colors.textMuted }}>Type: </span>
                      <span style={{ color: textColor }}>{sec.security_type || 'Equity'}</span>
                    </div>
                  </div>
                )}
              </div>
            ))
          )
        ) : activeTab === 'corporate' ? (
          corporateActions.length === 0 ? (
            <div className="text-xs text-center p-4" style={{ color: colors.textMuted }}>
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
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.textMuted}`,
                    borderRadius: 'var(--ft-border-radius)',
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
                        borderRadius: 'var(--ft-border-radius)',
                      }}
                    >
                      {label}
                    </span>
                  </div>
                  <div className="flex items-center gap-3 text-[9px]">
                    {action.amount && (
                      <div className="flex items-center gap-1" style={{ color: colors.success }}>
                        <DollarSign size={10} />
                        <span>{action.amount.toFixed(4)} {action.currency}</span>
                      </div>
                    )}
                    {action.ratio && (
                      <div className="flex items-center gap-1" style={{ color: colors.info }}>
                        <Percent size={10} />
                        <span>{action.ratio}</span>
                      </div>
                    )}
                    <span style={{ color: colors.textMuted }}>{action.ex_date}</span>
                  </div>
                </div>
              );
            })
          )
        ) : (
          adjustmentFactors.length === 0 ? (
            <div className="text-xs text-center p-4" style={{ color: colors.textMuted }}>
              Click refresh to load adjustment factors
            </div>
          ) : (
            <div
              className="overflow-x-auto"
              style={{
                backgroundColor: colors.background,
                border: `1px solid ${colors.textMuted}`,
                borderRadius: 'var(--ft-border-radius)',
              }}
            >
              <table className="w-full text-[9px]">
                <thead>
                  <tr style={{ backgroundColor: colors.panel }}>
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
                      style={{ borderBottom: `1px solid ${colors.textMuted}` }}
                    >
                      <td className="p-1" style={{ color: textColor }}>{factor.raw_symbol}</td>
                      <td className="p-1" style={{ color: colors.textMuted }}>{factor.date}</td>
                      <td className="p-1 text-right" style={{ color: factor.split_factor !== 1 ? colors.info : textColor }}>
                        {factor.split_factor.toFixed(6)}
                      </td>
                      <td className="p-1 text-right" style={{ color: factor.dividend_factor !== 1 ? colors.success : textColor }}>
                        {factor.dividend_factor.toFixed(6)}
                      </td>
                      <td className="p-1 text-right" style={{ color: colors.warning }}>
                        {factor.cumulative_factor.toFixed(6)}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
              {adjustmentFactors.length > 20 && (
                <div className="p-1 text-[9px] text-center" style={{ color: colors.textMuted }}>
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
