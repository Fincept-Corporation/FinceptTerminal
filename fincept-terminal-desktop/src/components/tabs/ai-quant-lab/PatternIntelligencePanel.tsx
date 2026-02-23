/**
 * Pattern Intelligence Panel
 * AI K-line pattern recognition powered by VisionQuant.
 * Sub-panel within the AI Quant Lab tab.
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  Search, Eye, TrendingUp, TrendingDown, Minus, BarChart3,
  AlertCircle, Loader2, Play, Settings2, Database,
  Target, Zap, Shield,
  ChevronDown, ChevronUp, RefreshCw, RotateCcw, Sliders
} from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import {
  visionQuantService,
  type SearchResult,
  type Scorecard,
  type BacktestResult,
  type IndexStatus,
  type PatternMatch,
  type SearchConfig,
  type ScoringConfig,
  type BacktestConfig,
  type SetupConfig,
} from '@/services/visionQuantService';

// ─── Default Configs ────────────────────────────────────────────────────────

const DEFAULT_SEARCH_CONFIG: Required<SearchConfig> = {
  isolation_days: 20,
  dtw_window: 5,
  w_dtw: 0.50,
  w_corr: 0.30,
  w_shape: 0.15,
  w_visual: 0.05,
  trend_bonus: 0.08,
  faiss_multiplier: 200,
  hq_dtw_threshold: 0.80,
  tb_upper: 0.05,
  tb_lower: 0.03,
  tb_max_hold: 20,
};

const DEFAULT_SCORING_CONFIG: Required<ScoringConfig> = {
  vision_breakpoints: [70, 55, 40],
  pe_thresholds: [15, 25, 40, 60],
  roe_thresholds: [20, 15, 10, 5],
  ma_period: 60,
  rsi_ranges: [30, 40, 60, 70],
  macd_fast: 12,
  macd_slow: 26,
  macd_signal: 9,
  ma_thresholds: [1.02, 1.00, 0.98],
  buy_threshold: 7,
  wait_threshold: 5,
};

const DEFAULT_BACKTEST_CONFIG: Required<BacktestConfig> = {
  stop_loss: 0.03,
  take_profit: 0.05,
  max_hold: 20,
  entry_rsi: 40,
  exit_rsi: 70,
  ma_period: 60,
  macd_fast: 12,
  macd_slow: 26,
  macd_signal: 9,
};

const DEFAULT_SETUP_CONFIG: Required<SetupConfig> = {
  batch_size: 32,
  chart_style: 'international',
  learning_rate: 0.001,
};

// ─── State ───────────────────────────────────────────────────────────────────

interface PanelState {
  symbol: string;
  searchDate: string;
  topK: number;
  lookback: number;

  isSearching: boolean;
  isScoring: boolean;
  isBacktesting: boolean;
  isSettingUp: boolean;
  indexStatus: IndexStatus | null;

  searchResult: SearchResult | null;
  scorecard: Scorecard | null;
  backtestResult: BacktestResult | null;
  error: string | null;

  btSymbol: string;
  btStart: string;
  btEnd: string;
  btCapital: string;

  showSetup: boolean;
  showBacktest: boolean;

  // Setup index params
  setupSymbols: string;
  setupStart: string;
  setupStride: number;
  setupWindow: number;
  setupEpochs: number;
}

const DEFAULT_STATE: PanelState = {
  symbol: 'AAPL',
  searchDate: '',
  topK: 10,
  lookback: 60,
  isSearching: false,
  isScoring: false,
  isBacktesting: false,
  isSettingUp: false,
  indexStatus: null,
  searchResult: null,
  scorecard: null,
  backtestResult: null,
  error: null,
  btSymbol: 'AAPL',
  btStart: '2023-01-01',
  btEnd: '2025-01-01',
  btCapital: '100000',
  showSetup: false,
  showBacktest: false,
  setupSymbols: '',
  setupStart: '2020-01-01',
  setupStride: 5,
  setupWindow: 60,
  setupEpochs: 30,
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

function ActionBadge({ action }: { action: string }) {
  const bg = action === 'BUY' ? '#16a34a' : action === 'SELL' ? '#dc2626' : '#ca8a04';
  return (
    <span
      className="px-2 py-0.5 rounded text-xs font-bold tracking-wider"
      style={{ backgroundColor: bg, color: '#fff' }}
    >
      {action}
    </span>
  );
}

function ScoreBar({ label, score, max, color }: { label: string; score: number; max: number; color: string }) {
  const pct = Math.min((score / max) * 100, 100);
  return (
    <div className="flex items-center gap-2 text-xs">
      <span className="w-28 text-right opacity-70">{label}</span>
      <div className="flex-1 h-2 rounded-full overflow-hidden" style={{ backgroundColor: 'rgba(255,255,255,0.1)' }}>
        <div className="h-full rounded-full transition-all" style={{ width: `${pct}%`, backgroundColor: color }} />
      </div>
      <span className="w-12 text-right font-mono">{score.toFixed(1)}/{max}</span>
    </div>
  );
}

function TBLabelIcon({ label }: { label?: number }) {
  if (label === 1) return <TrendingUp size={14} className="text-green-400" />;
  if (label === -1) return <TrendingDown size={14} className="text-red-400" />;
  return <Minus size={14} className="text-yellow-400" />;
}

function SectionHeader({
  icon,
  title,
  expanded,
  onToggle,
  primaryColor,
}: {
  icon: React.ReactNode;
  title: string;
  expanded: boolean;
  onToggle: () => void;
  primaryColor: string;
}) {
  return (
    <button
      onClick={onToggle}
      className="flex items-center gap-2 w-full text-left py-1"
      style={{ color: primaryColor }}
    >
      {icon}
      <span className="text-xs font-bold tracking-wider uppercase" style={{ letterSpacing: '0.08em' }}>{title}</span>
      {expanded ? <ChevronUp size={14} className="ml-auto opacity-50" /> : <ChevronDown size={14} className="ml-auto opacity-50" />}
    </button>
  );
}

function ParamRow({ children }: { children: React.ReactNode }) {
  return <div className="flex items-center gap-3 flex-wrap">{children}</div>;
}

function ParamLabel({ text }: { text: string }) {
  return (
    <span className="text-xs opacity-60 w-28 text-right flex-shrink-0" style={{ letterSpacing: '0.04em' }}>
      {text}
    </span>
  );
}

function NumInput({
  value,
  onChange,
  min,
  max,
  step,
  width = 'w-20',
  inputStyle,
}: {
  value: number;
  onChange: (v: number) => void;
  min?: number;
  max?: number;
  step?: number;
  width?: string;
  inputStyle: React.CSSProperties;
}) {
  return (
    <input
      type="number"
      value={value}
      onChange={e => onChange(parseFloat(e.target.value) || 0)}
      min={min}
      max={max}
      step={step}
      className={`px-2 py-1 rounded text-xs font-mono ${width}`}
      style={inputStyle}
    />
  );
}

function SliderInput({
  value,
  onChange,
  min,
  max,
  step,
  label,
  format,
  primaryColor,
}: {
  value: number;
  onChange: (v: number) => void;
  min: number;
  max: number;
  step: number;
  label: string;
  format?: (v: number) => string;
  primaryColor: string;
}) {
  return (
    <div className="flex items-center gap-2 text-xs">
      <span className="w-16 text-right opacity-60">{label}</span>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={e => onChange(parseFloat(e.target.value))}
        className="flex-1 h-1"
        style={{ accentColor: primaryColor }}
      />
      <span className="w-14 text-right font-mono">{format ? format(value) : value}</span>
    </div>
  );
}

// ─── Main Component ──────────────────────────────────────────────────────────

export function PatternIntelligencePanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [s, setS] = useState<PanelState>(DEFAULT_STATE);
  const [expandedSections, setExpandedSections] = useState<Set<string>>(new Set());
  const [searchConfig, setSearchConfig] = useState<Required<SearchConfig>>({ ...DEFAULT_SEARCH_CONFIG });
  const [scoringConfig, setScoringConfig] = useState<Required<ScoringConfig>>({ ...DEFAULT_SCORING_CONFIG });
  const [backtestConfig, setBacktestConfig] = useState<Required<BacktestConfig>>({ ...DEFAULT_BACKTEST_CONFIG });
  const [setupConfig, setSetupConfig] = useState<Required<SetupConfig>>({ ...DEFAULT_SETUP_CONFIG });

  const update = useCallback((patch: Partial<PanelState>) => {
    setS(prev => ({ ...prev, ...patch }));
  }, []);

  const toggleSection = useCallback((id: string) => {
    setExpandedSections(prev => {
      const next = new Set(prev);
      next.has(id) ? next.delete(id) : next.add(id);
      return next;
    });
  }, []);

  // Check index status on mount
  useEffect(() => {
    checkStatus();
  }, []);

  async function checkStatus() {
    try {
      const status = await visionQuantService.getStatus();
      update({ indexStatus: status, error: null });
      if (!status.index_ready) {
        update({ showSetup: true });
      }
    } catch (err: any) {
      update({ error: `Status check failed: ${err.message}` });
    }
  }

  async function handleSearch() {
    if (!s.symbol.trim()) return;
    update({ isSearching: true, error: null, searchResult: null, scorecard: null });
    try {
      const result = await visionQuantService.searchPatterns(
        s.symbol.trim().toUpperCase(),
        s.searchDate || undefined,
        s.topK,
        s.lookback,
        searchConfig,
      );
      update({ searchResult: result, isSearching: false });

      // Auto-score
      if (result.win_rate_stats?.win_rate != null) {
        update({ isScoring: true });
        try {
          const sc = await visionQuantService.getScore(
            s.symbol.trim().toUpperCase(),
            result.win_rate_stats.win_rate,
            s.searchDate || undefined,
            scoringConfig,
          );
          update({ scorecard: sc, isScoring: false });
        } catch {
          update({ isScoring: false });
        }
      }
    } catch (err: any) {
      update({ isSearching: false, error: err.message });
    }
  }

  async function handleBacktest() {
    if (!s.btSymbol.trim()) return;
    update({ isBacktesting: true, backtestResult: null, error: null });
    try {
      const result = await visionQuantService.runBacktest(
        s.btSymbol.trim().toUpperCase(),
        s.btStart.replace(/-/g, ''),
        s.btEnd.replace(/-/g, ''),
        parseFloat(s.btCapital) || 100000,
        backtestConfig,
      );
      update({ backtestResult: result, isBacktesting: false });
    } catch (err: any) {
      update({ isBacktesting: false, error: err.message });
    }
  }

  async function handleSetupIndex() {
    update({ isSettingUp: true, error: null });
    try {
      const symbols = s.setupSymbols.trim()
        ? s.setupSymbols.split(',').map(t => t.trim()).filter(Boolean)
        : undefined;
      await visionQuantService.setupIndex(
        symbols,
        s.setupStart || undefined,
        s.setupStride || undefined,
        s.setupEpochs || undefined,
        setupConfig,
      );
      await checkStatus();
      update({ isSettingUp: false, showSetup: false });
    } catch (err: any) {
      update({ isSettingUp: false, error: `Setup failed: ${err.message}` });
    }
  }

  function handleResetAll() {
    setSearchConfig({ ...DEFAULT_SEARCH_CONFIG });
    setScoringConfig({ ...DEFAULT_SCORING_CONFIG });
    setBacktestConfig({ ...DEFAULT_BACKTEST_CONFIG });
    setSetupConfig({ ...DEFAULT_SETUP_CONFIG });
    update({
      topK: 10,
      lookback: 60,
      setupStart: '2020-01-01',
      setupStride: 5,
      setupWindow: 60,
      setupEpochs: 30,
      setupSymbols: '',
    });
  }

  // Re-ranking weight normalization
  function handleWeightChange(key: keyof Pick<SearchConfig, 'w_dtw' | 'w_corr' | 'w_shape' | 'w_visual'>, value: number) {
    const clamped = Math.max(0, Math.min(1, value));
    const keys: Array<keyof Pick<SearchConfig, 'w_dtw' | 'w_corr' | 'w_shape' | 'w_visual'>> = ['w_dtw', 'w_corr', 'w_shape', 'w_visual'];
    const others = keys.filter(k => k !== key);
    const remaining = 1.0 - clamped;
    const otherSum = others.reduce((sum, k) => sum + searchConfig[k], 0);

    const updated = { ...searchConfig, [key]: clamped };
    if (otherSum > 0) {
      const scale = remaining / otherSum;
      for (const k of others) {
        updated[k] = Math.round(searchConfig[k] * scale * 1000) / 1000;
      }
    } else {
      const each = remaining / others.length;
      for (const k of others) {
        updated[k] = Math.round(each * 1000) / 1000;
      }
    }
    setSearchConfig(updated);
  }

  const borderColor = (colors as any).border || '#333';
  const primaryColor = colors.primary || '#FF8800';

  const inputStyle: React.CSSProperties = {
    backgroundColor: colors.background,
    color: colors.text,
    border: `1px solid ${borderColor}`,
    fontSize: fontSize.small,
    fontFamily,
  };

  const cardStyle: React.CSSProperties = {
    backgroundColor: colors.panel,
    border: `1px solid ${borderColor}`,
  };

  const sectionBg: React.CSSProperties = {
    backgroundColor: 'rgba(255,255,255,0.02)',
    border: `1px solid ${borderColor}`,
  };

  const weightSum = searchConfig.w_dtw + searchConfig.w_corr + searchConfig.w_shape + searchConfig.w_visual;

  return (
    <div className="flex flex-col h-full overflow-auto p-3 gap-3" style={{ color: colors.text, fontFamily }}>
      {/* Error banner */}
      {s.error && (
        <div className="flex items-center gap-2 px-3 py-2 rounded text-xs" style={{ backgroundColor: 'rgba(220,38,38,0.15)', border: '1px solid rgba(220,38,38,0.3)' }}>
          <AlertCircle size={14} className="text-red-400 flex-shrink-0" />
          <span className="text-red-300">{s.error}</span>
          <button onClick={() => update({ error: null })} className="ml-auto text-red-400 hover:text-red-300">&times;</button>
        </div>
      )}

      {/* Setup banner */}
      {s.showSetup && s.indexStatus && !s.indexStatus.index_ready && (
        <div className="rounded p-3" style={cardStyle}>
          <div className="flex items-center gap-2 mb-2">
            <Database size={16} style={{ color: primaryColor }} />
            <span className="text-sm font-semibold">First-Run Setup Required</span>
          </div>
          <p className="text-xs opacity-70 mb-3">
            The pattern intelligence engine needs to build its knowledge base. This will download historical data
            for 50 major US stocks, generate candlestick images, train the AI model, and build the search index.
            This is a one-time operation.
          </p>
          <button
            onClick={handleSetupIndex}
            disabled={s.isSettingUp}
            className="flex items-center gap-2 px-4 py-2 rounded text-xs font-semibold transition-all"
            style={{
              backgroundColor: s.isSettingUp ? 'rgba(255,255,255,0.1)' : primaryColor,
              color: s.isSettingUp ? colors.text : colors.background,
              opacity: s.isSettingUp ? 0.6 : 1,
            }}
          >
            {s.isSettingUp ? <Loader2 size={14} className="animate-spin" /> : <Zap size={14} />}
            {s.isSettingUp ? 'Building Index... (this may take 10-30 min)' : 'Build Index Now'}
          </button>
        </div>
      )}

      {/* ─── Search Section ─── */}
      <div className="rounded p-3" style={cardStyle}>
        <div className="flex items-center gap-2 mb-3">
          <Eye size={16} style={{ color: primaryColor }} />
          <span className="text-sm font-semibold">Pattern Search</span>
          {s.indexStatus && (
            <span className="ml-auto text-xs opacity-50">
              {s.indexStatus.n_records.toLocaleString()} patterns indexed
            </span>
          )}
        </div>

        <div className="flex items-center gap-2">
          <input
            type="text"
            value={s.symbol}
            onChange={e => update({ symbol: e.target.value.toUpperCase() })}
            onKeyDown={e => e.key === 'Enter' && handleSearch()}
            placeholder="Symbol (e.g. AAPL)"
            className="px-2 py-1.5 rounded text-xs w-32"
            style={inputStyle}
          />
          <input
            type="text"
            value={s.searchDate}
            onChange={e => update({ searchDate: e.target.value })}
            onKeyDown={e => e.key === 'Enter' && handleSearch()}
            placeholder="Date (optional, YYYYMMDD)"
            className="px-2 py-1.5 rounded text-xs w-48"
            style={inputStyle}
          />
          <NumInput value={s.topK} onChange={v => update({ topK: Math.max(1, Math.round(v)) })} min={1} max={50} step={1} width="w-16" inputStyle={inputStyle} />
          <span className="text-xs opacity-40">Top K</span>
          <NumInput value={s.lookback} onChange={v => update({ lookback: Math.max(10, Math.round(v)) })} min={10} max={200} step={5} width="w-16" inputStyle={inputStyle} />
          <span className="text-xs opacity-40">Bars</span>
          <button
            onClick={handleSearch}
            disabled={s.isSearching || !s.symbol.trim()}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded text-xs font-semibold transition-all"
            style={{
              backgroundColor: s.isSearching ? 'rgba(255,255,255,0.1)' : primaryColor,
              color: s.isSearching ? colors.text : colors.background,
            }}
          >
            {s.isSearching ? <Loader2 size={12} className="animate-spin" /> : <Search size={12} />}
            {s.isSearching ? 'Searching...' : 'Search'}
          </button>
          <button
            onClick={checkStatus}
            className="p-1.5 rounded opacity-50 hover:opacity-100 transition-opacity"
            title="Refresh status"
          >
            <RefreshCw size={12} />
          </button>
        </div>
      </div>

      {/* ═══════════════════════════════════════════════════════════════════ */}
      {/* ADVANCED SETTINGS SECTIONS                                         */}
      {/* ═══════════════════════════════════════════════════════════════════ */}

      {/* ─── Search Parameters ─── */}
      <div className="rounded p-3" style={sectionBg}>
        <SectionHeader
          icon={<Settings2 size={14} />}
          title="Search Parameters"
          expanded={expandedSections.has('search')}
          onToggle={() => toggleSection('search')}
          primaryColor={primaryColor}
        />
        {expandedSections.has('search') && (
          <div className="mt-3 flex flex-col gap-3">
            <ParamRow>
              <ParamLabel text="Isolation Days" />
              <NumInput value={searchConfig.isolation_days} onChange={v => setSearchConfig(p => ({ ...p, isolation_days: Math.max(1, Math.round(v)) }))} min={1} max={100} step={1} inputStyle={inputStyle} />
              <ParamLabel text="DTW Window" />
              <NumInput value={searchConfig.dtw_window} onChange={v => setSearchConfig(p => ({ ...p, dtw_window: Math.max(1, Math.round(v)) }))} min={1} max={50} step={1} inputStyle={inputStyle} />
              <ParamLabel text="FAISS Mult" />
              <NumInput value={searchConfig.faiss_multiplier} onChange={v => setSearchConfig(p => ({ ...p, faiss_multiplier: Math.max(10, Math.round(v)) }))} min={10} max={1000} step={10} inputStyle={inputStyle} />
            </ParamRow>
            <ParamRow>
              <ParamLabel text="HQ Threshold" />
              <NumInput value={searchConfig.hq_dtw_threshold} onChange={v => setSearchConfig(p => ({ ...p, hq_dtw_threshold: Math.max(0, Math.min(1, v)) }))} min={0} max={1} step={0.05} inputStyle={inputStyle} />
              <ParamLabel text="Trend Bonus" />
              <NumInput value={searchConfig.trend_bonus} onChange={v => setSearchConfig(p => ({ ...p, trend_bonus: Math.max(0, Math.min(0.5, v)) }))} min={0} max={0.5} step={0.01} inputStyle={inputStyle} />
            </ParamRow>

            <div className="border-t pt-2 mt-1" style={{ borderColor }}>
              <div className="flex items-center gap-2 mb-2">
                <span className="text-xs font-semibold opacity-70 uppercase tracking-wider">Re-Ranking Weights</span>
                <span className={`text-xs font-mono ml-auto ${Math.abs(weightSum - 1.0) < 0.01 ? 'text-green-400' : 'text-red-400'}`}>
                  Sum: {weightSum.toFixed(3)}
                </span>
              </div>
              <div className="flex flex-col gap-1">
                <SliderInput label="DTW" value={searchConfig.w_dtw} onChange={v => handleWeightChange('w_dtw', v)} min={0} max={1} step={0.01} format={v => v.toFixed(2)} primaryColor={primaryColor} />
                <SliderInput label="Corr" value={searchConfig.w_corr} onChange={v => handleWeightChange('w_corr', v)} min={0} max={1} step={0.01} format={v => v.toFixed(2)} primaryColor={primaryColor} />
                <SliderInput label="Shape" value={searchConfig.w_shape} onChange={v => handleWeightChange('w_shape', v)} min={0} max={1} step={0.01} format={v => v.toFixed(2)} primaryColor={primaryColor} />
                <SliderInput label="Visual" value={searchConfig.w_visual} onChange={v => handleWeightChange('w_visual', v)} min={0} max={1} step={0.01} format={v => v.toFixed(2)} primaryColor={primaryColor} />
              </div>
            </div>
          </div>
        )}
      </div>

      {/* ─── Triple Barrier ─── */}
      <div className="rounded p-3" style={sectionBg}>
        <SectionHeader
          icon={<Target size={14} />}
          title="Triple Barrier"
          expanded={expandedSections.has('barrier')}
          onToggle={() => toggleSection('barrier')}
          primaryColor={primaryColor}
        />
        {expandedSections.has('barrier') && (
          <div className="mt-3 flex flex-col gap-2">
            <SliderInput label="Take Profit" value={searchConfig.tb_upper} onChange={v => setSearchConfig(p => ({ ...p, tb_upper: v }))} min={0.01} max={0.20} step={0.005} format={v => `${(v * 100).toFixed(1)}%`} primaryColor="#16a34a" />
            <SliderInput label="Stop Loss" value={searchConfig.tb_lower} onChange={v => setSearchConfig(p => ({ ...p, tb_lower: v }))} min={0.01} max={0.15} step={0.005} format={v => `${(v * 100).toFixed(1)}%`} primaryColor="#dc2626" />
            <ParamRow>
              <ParamLabel text="Max Hold Days" />
              <NumInput value={searchConfig.tb_max_hold} onChange={v => setSearchConfig(p => ({ ...p, tb_max_hold: Math.max(1, Math.round(v)) }))} min={1} max={60} step={1} inputStyle={inputStyle} />
            </ParamRow>
          </div>
        )}
      </div>

      {/* ─── Scoring Thresholds ─── */}
      <div className="rounded p-3" style={sectionBg}>
        <SectionHeader
          icon={<Shield size={14} />}
          title="Scoring Thresholds"
          expanded={expandedSections.has('scoring')}
          onToggle={() => toggleSection('scoring')}
          primaryColor={primaryColor}
        />
        {expandedSections.has('scoring') && (
          <div className="mt-3 flex flex-col gap-3">
            {/* Vision Score */}
            <div>
              <span className="text-xs font-semibold opacity-70 mb-1 block">VISION SCORE (0-3)</span>
              <ParamRow>
                <ParamLabel text="Strong >=" />
                <NumInput value={scoringConfig.vision_breakpoints[0]} onChange={v => setScoringConfig(p => ({ ...p, vision_breakpoints: [Math.round(v), p.vision_breakpoints[1], p.vision_breakpoints[2]] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Moderate >=" />
                <NumInput value={scoringConfig.vision_breakpoints[1]} onChange={v => setScoringConfig(p => ({ ...p, vision_breakpoints: [p.vision_breakpoints[0], Math.round(v), p.vision_breakpoints[2]] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Weak >=" />
                <NumInput value={scoringConfig.vision_breakpoints[2]} onChange={v => setScoringConfig(p => ({ ...p, vision_breakpoints: [p.vision_breakpoints[0], p.vision_breakpoints[1], Math.round(v)] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
              </ParamRow>
            </div>

            {/* Fundamental */}
            <div className="border-t pt-2" style={{ borderColor }}>
              <span className="text-xs font-semibold opacity-70 mb-1 block">FUNDAMENTAL (0-4) - P/E Thresholds</span>
              <ParamRow>
                <ParamLabel text="Excellent <" />
                <NumInput value={scoringConfig.pe_thresholds[0]} onChange={v => setScoringConfig(p => ({ ...p, pe_thresholds: [Math.round(v), p.pe_thresholds[1], p.pe_thresholds[2], p.pe_thresholds[3]] }))} min={1} max={200} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Good <" />
                <NumInput value={scoringConfig.pe_thresholds[1]} onChange={v => setScoringConfig(p => ({ ...p, pe_thresholds: [p.pe_thresholds[0], Math.round(v), p.pe_thresholds[2], p.pe_thresholds[3]] }))} min={1} max={200} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Fair <" />
                <NumInput value={scoringConfig.pe_thresholds[2]} onChange={v => setScoringConfig(p => ({ ...p, pe_thresholds: [p.pe_thresholds[0], p.pe_thresholds[1], Math.round(v), p.pe_thresholds[3]] }))} min={1} max={200} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Poor <" />
                <NumInput value={scoringConfig.pe_thresholds[3]} onChange={v => setScoringConfig(p => ({ ...p, pe_thresholds: [p.pe_thresholds[0], p.pe_thresholds[1], p.pe_thresholds[2], Math.round(v)] }))} min={1} max={200} step={1} inputStyle={inputStyle} />
              </ParamRow>
            </div>

            <div>
              <span className="text-xs font-semibold opacity-70 mb-1 block">ROE Thresholds (%)</span>
              <ParamRow>
                <ParamLabel text="Excellent >=" />
                <NumInput value={scoringConfig.roe_thresholds[0]} onChange={v => setScoringConfig(p => ({ ...p, roe_thresholds: [Math.round(v), p.roe_thresholds[1], p.roe_thresholds[2], p.roe_thresholds[3]] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Good >=" />
                <NumInput value={scoringConfig.roe_thresholds[1]} onChange={v => setScoringConfig(p => ({ ...p, roe_thresholds: [p.roe_thresholds[0], Math.round(v), p.roe_thresholds[2], p.roe_thresholds[3]] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Fair >=" />
                <NumInput value={scoringConfig.roe_thresholds[2]} onChange={v => setScoringConfig(p => ({ ...p, roe_thresholds: [p.roe_thresholds[0], p.roe_thresholds[1], Math.round(v), p.roe_thresholds[3]] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Weak >=" />
                <NumInput value={scoringConfig.roe_thresholds[3]} onChange={v => setScoringConfig(p => ({ ...p, roe_thresholds: [p.roe_thresholds[0], p.roe_thresholds[1], p.roe_thresholds[2], Math.round(v)] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
              </ParamRow>
            </div>

            {/* Technical */}
            <div className="border-t pt-2" style={{ borderColor }}>
              <span className="text-xs font-semibold opacity-70 mb-1 block">TECHNICAL (0-3)</span>
              <ParamRow>
                <ParamLabel text="MA Period" />
                <NumInput value={scoringConfig.ma_period} onChange={v => setScoringConfig(p => ({ ...p, ma_period: Math.max(5, Math.round(v)) }))} min={5} max={200} step={1} inputStyle={inputStyle} />
                <ParamLabel text="MACD Fast" />
                <NumInput value={scoringConfig.macd_fast} onChange={v => setScoringConfig(p => ({ ...p, macd_fast: Math.max(2, Math.round(v)) }))} min={2} max={50} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Slow" />
                <NumInput value={scoringConfig.macd_slow} onChange={v => setScoringConfig(p => ({ ...p, macd_slow: Math.max(2, Math.round(v)) }))} min={2} max={100} step={1} inputStyle={inputStyle} />
                <ParamLabel text="Signal" />
                <NumInput value={scoringConfig.macd_signal} onChange={v => setScoringConfig(p => ({ ...p, macd_signal: Math.max(2, Math.round(v)) }))} min={2} max={50} step={1} inputStyle={inputStyle} />
              </ParamRow>
              <div className="mt-2">
                <span className="text-xs opacity-50 block mb-1">RSI Ranges</span>
                <ParamRow>
                  <ParamLabel text="Oversold <" />
                  <NumInput value={scoringConfig.rsi_ranges[0]} onChange={v => setScoringConfig(p => ({ ...p, rsi_ranges: [Math.round(v), p.rsi_ranges[1], p.rsi_ranges[2], p.rsi_ranges[3]] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                  <ParamLabel text="Low <" />
                  <NumInput value={scoringConfig.rsi_ranges[1]} onChange={v => setScoringConfig(p => ({ ...p, rsi_ranges: [p.rsi_ranges[0], Math.round(v), p.rsi_ranges[2], p.rsi_ranges[3]] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                  <ParamLabel text="High >" />
                  <NumInput value={scoringConfig.rsi_ranges[2]} onChange={v => setScoringConfig(p => ({ ...p, rsi_ranges: [p.rsi_ranges[0], p.rsi_ranges[1], Math.round(v), p.rsi_ranges[3]] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                  <ParamLabel text="Overbought >" />
                  <NumInput value={scoringConfig.rsi_ranges[3]} onChange={v => setScoringConfig(p => ({ ...p, rsi_ranges: [p.rsi_ranges[0], p.rsi_ranges[1], p.rsi_ranges[2], Math.round(v)] }))} min={0} max={100} step={1} inputStyle={inputStyle} />
                </ParamRow>
              </div>
              <div className="mt-2">
                <span className="text-xs opacity-50 block mb-1">MA Thresholds (price/MA ratio)</span>
                <ParamRow>
                  <ParamLabel text="Bullish >" />
                  <NumInput value={scoringConfig.ma_thresholds[0]} onChange={v => setScoringConfig(p => ({ ...p, ma_thresholds: [v, p.ma_thresholds[1], p.ma_thresholds[2]] }))} min={0.9} max={1.2} step={0.005} inputStyle={inputStyle} />
                  <ParamLabel text="Neutral >" />
                  <NumInput value={scoringConfig.ma_thresholds[1]} onChange={v => setScoringConfig(p => ({ ...p, ma_thresholds: [p.ma_thresholds[0], v, p.ma_thresholds[2]] }))} min={0.9} max={1.1} step={0.005} inputStyle={inputStyle} />
                  <ParamLabel text="Bearish >" />
                  <NumInput value={scoringConfig.ma_thresholds[2]} onChange={v => setScoringConfig(p => ({ ...p, ma_thresholds: [p.ma_thresholds[0], p.ma_thresholds[1], v] }))} min={0.8} max={1.1} step={0.005} inputStyle={inputStyle} />
                </ParamRow>
              </div>
            </div>

            {/* Action thresholds */}
            <div className="border-t pt-2" style={{ borderColor }}>
              <span className="text-xs font-semibold opacity-70 mb-1 block">ACTION THRESHOLDS</span>
              <ParamRow>
                <ParamLabel text="BUY >=" />
                <NumInput value={scoringConfig.buy_threshold} onChange={v => setScoringConfig(p => ({ ...p, buy_threshold: Math.max(0, Math.min(10, v)) }))} min={0} max={10} step={0.5} inputStyle={inputStyle} />
                <ParamLabel text="WAIT >=" />
                <NumInput value={scoringConfig.wait_threshold} onChange={v => setScoringConfig(p => ({ ...p, wait_threshold: Math.max(0, Math.min(10, v)) }))} min={0} max={10} step={0.5} inputStyle={inputStyle} />
                <span className="text-xs opacity-40">SELL &lt; {scoringConfig.wait_threshold}</span>
              </ParamRow>
            </div>
          </div>
        )}
      </div>

      {/* ─── Backtest Strategy ─── */}
      <div className="rounded p-3" style={sectionBg}>
        <SectionHeader
          icon={<Sliders size={14} />}
          title="Backtest Strategy"
          expanded={expandedSections.has('btstrategy')}
          onToggle={() => toggleSection('btstrategy')}
          primaryColor={primaryColor}
        />
        {expandedSections.has('btstrategy') && (
          <div className="mt-3 flex flex-col gap-2">
            <SliderInput label="Stop Loss" value={backtestConfig.stop_loss} onChange={v => setBacktestConfig(p => ({ ...p, stop_loss: v }))} min={0.005} max={0.15} step={0.005} format={v => `${(v * 100).toFixed(1)}%`} primaryColor="#dc2626" />
            <SliderInput label="Take Profit" value={backtestConfig.take_profit} onChange={v => setBacktestConfig(p => ({ ...p, take_profit: v }))} min={0.01} max={0.30} step={0.005} format={v => `${(v * 100).toFixed(1)}%`} primaryColor="#16a34a" />
            <ParamRow>
              <ParamLabel text="Max Hold" />
              <NumInput value={backtestConfig.max_hold} onChange={v => setBacktestConfig(p => ({ ...p, max_hold: Math.max(1, Math.round(v)) }))} min={1} max={120} step={1} inputStyle={inputStyle} />
              <span className="text-xs opacity-40">days</span>
              <ParamLabel text="Entry RSI <" />
              <NumInput value={backtestConfig.entry_rsi} onChange={v => setBacktestConfig(p => ({ ...p, entry_rsi: Math.max(0, Math.min(100, v)) }))} min={0} max={100} step={1} inputStyle={inputStyle} />
              <ParamLabel text="Exit RSI >" />
              <NumInput value={backtestConfig.exit_rsi} onChange={v => setBacktestConfig(p => ({ ...p, exit_rsi: Math.max(0, Math.min(100, v)) }))} min={0} max={100} step={1} inputStyle={inputStyle} />
            </ParamRow>
            <ParamRow>
              <ParamLabel text="MA Period" />
              <NumInput value={backtestConfig.ma_period} onChange={v => setBacktestConfig(p => ({ ...p, ma_period: Math.max(5, Math.round(v)) }))} min={5} max={200} step={1} inputStyle={inputStyle} />
              <ParamLabel text="MACD Fast" />
              <NumInput value={backtestConfig.macd_fast} onChange={v => setBacktestConfig(p => ({ ...p, macd_fast: Math.max(2, Math.round(v)) }))} min={2} max={50} step={1} inputStyle={inputStyle} />
              <ParamLabel text="Slow" />
              <NumInput value={backtestConfig.macd_slow} onChange={v => setBacktestConfig(p => ({ ...p, macd_slow: Math.max(2, Math.round(v)) }))} min={2} max={100} step={1} inputStyle={inputStyle} />
              <ParamLabel text="Signal" />
              <NumInput value={backtestConfig.macd_signal} onChange={v => setBacktestConfig(p => ({ ...p, macd_signal: Math.max(2, Math.round(v)) }))} min={2} max={50} step={1} inputStyle={inputStyle} />
            </ParamRow>
          </div>
        )}
      </div>

      {/* ─── Index Setup ─── */}
      <div className="rounded p-3" style={sectionBg}>
        <SectionHeader
          icon={<Database size={14} />}
          title="Index Setup"
          expanded={expandedSections.has('setup')}
          onToggle={() => toggleSection('setup')}
          primaryColor={primaryColor}
        />
        {expandedSections.has('setup') && (
          <div className="mt-3 flex flex-col gap-2">
            <ParamRow>
              <ParamLabel text="Start Date" />
              <input type="text" value={s.setupStart} onChange={e => update({ setupStart: e.target.value })} placeholder="YYYY-MM-DD" className="px-2 py-1 rounded text-xs w-28" style={inputStyle} />
              <ParamLabel text="Stride" />
              <NumInput value={s.setupStride} onChange={v => update({ setupStride: Math.max(1, Math.round(v)) })} min={1} max={30} step={1} inputStyle={inputStyle} />
              <span className="text-xs opacity-40">days</span>
              <ParamLabel text="Window" />
              <NumInput value={s.setupWindow} onChange={v => update({ setupWindow: Math.max(10, Math.round(v)) })} min={10} max={200} step={5} inputStyle={inputStyle} />
              <span className="text-xs opacity-40">bars</span>
            </ParamRow>
            <ParamRow>
              <ParamLabel text="Epochs" />
              <NumInput value={s.setupEpochs} onChange={v => update({ setupEpochs: Math.max(1, Math.round(v)) })} min={1} max={200} step={1} inputStyle={inputStyle} />
              <ParamLabel text="Batch Size" />
              <NumInput value={setupConfig.batch_size} onChange={v => setSetupConfig(p => ({ ...p, batch_size: Math.max(1, Math.round(v)) }))} min={1} max={256} step={1} inputStyle={inputStyle} />
              <ParamLabel text="Learn Rate" />
              <NumInput value={setupConfig.learning_rate} onChange={v => setSetupConfig(p => ({ ...p, learning_rate: Math.max(0.00001, v) }))} min={0.00001} max={0.1} step={0.0001} width="w-24" inputStyle={inputStyle} />
            </ParamRow>
            <ParamRow>
              <ParamLabel text="Chart Style" />
              <select
                value={setupConfig.chart_style}
                onChange={e => setSetupConfig(p => ({ ...p, chart_style: e.target.value as 'international' | 'chinese' }))}
                className="px-2 py-1 rounded text-xs"
                style={inputStyle}
              >
                <option value="international">International (Green=Up)</option>
                <option value="chinese">Chinese (Red=Up)</option>
              </select>
            </ParamRow>
            <div className="mt-1">
              <ParamLabel text="Symbols" />
              <textarea
                value={s.setupSymbols}
                onChange={e => update({ setupSymbols: e.target.value })}
                placeholder="Leave empty for default 50 S&P 500 stocks. Or enter comma-separated: AAPL,MSFT,GOOGL"
                className="w-full mt-1 px-2 py-1.5 rounded text-xs font-mono"
                style={{ ...inputStyle, minHeight: '40px', resize: 'vertical' }}
                rows={2}
              />
            </div>
          </div>
        )}
      </div>

      {/* ─── Model Info (read-only) ─── */}
      <div className="rounded p-3" style={sectionBg}>
        <SectionHeader
          icon={<Eye size={14} />}
          title="Model Info"
          expanded={expandedSections.has('model')}
          onToggle={() => toggleSection('model')}
          primaryColor={primaryColor}
        />
        {expandedSections.has('model') && (
          <div className="mt-3 grid grid-cols-4 gap-2 text-xs opacity-60">
            <span>Latent Dim: <span className="font-mono">1024</span></span>
            <span>Attention Heads: <span className="font-mono">8</span></span>
            <span>Image Size: <span className="font-mono">224x224</span></span>
            <span>Feature Dim: <span className="font-mono">256</span></span>
          </div>
        )}
      </div>

      {/* ─── Reset All ─── */}
      <button
        onClick={handleResetAll}
        className="flex items-center gap-2 px-3 py-1.5 rounded text-xs opacity-50 hover:opacity-100 transition-opacity self-start"
        style={{ border: `1px solid ${borderColor}` }}
      >
        <RotateCcw size={12} />
        Reset All to Defaults
      </button>

      {/* ═══════════════════════════════════════════════════════════════════ */}
      {/* RESULTS                                                            */}
      {/* ═══════════════════════════════════════════════════════════════════ */}

      {/* ─── Results ─── */}
      {s.searchResult && (
        <>
          {/* Win Rate Stats */}
          <div className="rounded p-3" style={cardStyle}>
            <div className="flex items-center gap-2 mb-2">
              <Target size={16} style={{ color: primaryColor }} />
              <span className="text-sm font-semibold">Win Rate Statistics</span>
              <span className="ml-auto text-xs opacity-50">
                Triple Barrier: +{(searchConfig.tb_upper * 100).toFixed(1)}% / -{(searchConfig.tb_lower * 100).toFixed(1)}% / {searchConfig.tb_max_hold}d
              </span>
            </div>
            <div className="grid grid-cols-4 gap-3 text-center">
              <StatBox
                label="Win Rate"
                value={`${s.searchResult.win_rate_stats.win_rate}%`}
                color={s.searchResult.win_rate_stats.win_rate >= 55 ? '#16a34a' : s.searchResult.win_rate_stats.win_rate >= 45 ? '#ca8a04' : '#dc2626'}
              />
              <StatBox label="Avg Return" value={`${s.searchResult.win_rate_stats.avg_return_pct > 0 ? '+' : ''}${s.searchResult.win_rate_stats.avg_return_pct}%`}
                color={s.searchResult.win_rate_stats.avg_return_pct >= 0 ? '#16a34a' : '#dc2626'} />
              <StatBox label="Avg Hold" value={`${s.searchResult.win_rate_stats.avg_holding_days}d`} color={primaryColor} />
              <StatBox label="W/L Ratio" value={`${s.searchResult.win_rate_stats.win_loss_ratio}`} color={primaryColor} />
            </div>
            <div className="flex items-center gap-4 mt-2 text-xs opacity-70 justify-center">
              <span className="flex items-center gap-1"><TrendingUp size={12} className="text-green-400" /> Bullish: {s.searchResult.win_rate_stats.bullish}</span>
              <span className="flex items-center gap-1"><Minus size={12} className="text-yellow-400" /> Neutral: {s.searchResult.win_rate_stats.neutral}</span>
              <span className="flex items-center gap-1"><TrendingDown size={12} className="text-red-400" /> Bearish: {s.searchResult.win_rate_stats.bearish}</span>
            </div>
          </div>

          {/* Scorecard */}
          {s.isScoring && (
            <div className="rounded p-3 flex items-center gap-2 text-xs" style={cardStyle}>
              <Loader2 size={14} className="animate-spin" style={{ color: primaryColor }} />
              <span>Computing multi-factor score...</span>
            </div>
          )}
          {s.scorecard && (
            <div className="rounded p-3" style={cardStyle}>
              <div className="flex items-center gap-2 mb-3">
                <Shield size={16} style={{ color: primaryColor }} />
                <span className="text-sm font-semibold">Scorecard</span>
                <ActionBadge action={s.scorecard.action} />
                <span className="ml-auto text-lg font-bold font-mono" style={{ color: primaryColor }}>
                  {s.scorecard.total_score.toFixed(1)}<span className="text-xs opacity-50">/10</span>
                </span>
              </div>
              <div className="flex flex-col gap-1.5">
                <ScoreBar label="Vision (V)" score={s.scorecard.vision.score} max={3} color="#a78bfa" />
                <ScoreBar label="Fundamental (F)" score={s.scorecard.fundamental.score} max={4} color="#60a5fa" />
                <ScoreBar label="Technical (Q)" score={s.scorecard.technical.score} max={3} color="#34d399" />
              </div>
              <div className="flex gap-4 mt-2 text-xs opacity-60">
                {s.scorecard.fundamental.pe != null && <span>P/E: {s.scorecard.fundamental.pe}</span>}
                {s.scorecard.fundamental.roe != null && <span>ROE: {s.scorecard.fundamental.roe}%</span>}
                {s.scorecard.technical.rsi != null && <span>RSI: {s.scorecard.technical.rsi}</span>}
              </div>
            </div>
          )}

          {/* Match Gallery */}
          <div className="rounded p-3" style={cardStyle}>
            <div className="flex items-center gap-2 mb-2">
              <Eye size={16} style={{ color: primaryColor }} />
              <span className="text-sm font-semibold">Top Pattern Matches</span>
              <span className="ml-auto text-xs opacity-50">{s.searchResult.matches.length} matches</span>
            </div>
            <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-2">
              {s.searchResult.matches.map((m, i) => (
                <MatchCard key={`${m.symbol}-${m.date}-${i}`} match={m} rank={i + 1} colors={colors} />
              ))}
            </div>
          </div>
        </>
      )}

      {/* ─── Backtest Section ─── */}
      <div className="rounded p-3" style={cardStyle}>
        <button
          onClick={() => update({ showBacktest: !s.showBacktest })}
          className="flex items-center gap-2 w-full text-left"
        >
          <BarChart3 size={16} style={{ color: primaryColor }} />
          <span className="text-sm font-semibold">Strategy Backtest</span>
          {s.showBacktest ? <ChevronUp size={14} className="ml-auto opacity-50" /> : <ChevronDown size={14} className="ml-auto opacity-50" />}
        </button>

        {s.showBacktest && (
          <div className="mt-3">
            <div className="flex items-center gap-2 flex-wrap">
              <input type="text" value={s.btSymbol} onChange={e => update({ btSymbol: e.target.value.toUpperCase() })} placeholder="Symbol" className="px-2 py-1.5 rounded text-xs w-24" style={inputStyle} />
              <input type="text" value={s.btStart} onChange={e => update({ btStart: e.target.value })} placeholder="Start (YYYY-MM-DD)" className="px-2 py-1.5 rounded text-xs w-32" style={inputStyle} />
              <input type="text" value={s.btEnd} onChange={e => update({ btEnd: e.target.value })} placeholder="End (YYYY-MM-DD)" className="px-2 py-1.5 rounded text-xs w-32" style={inputStyle} />
              <input type="text" value={s.btCapital} onChange={e => update({ btCapital: e.target.value })} placeholder="Capital" className="px-2 py-1.5 rounded text-xs w-24" style={inputStyle} />
              <button
                onClick={handleBacktest}
                disabled={s.isBacktesting}
                className="flex items-center gap-1.5 px-3 py-1.5 rounded text-xs font-semibold transition-all"
                style={{
                  backgroundColor: s.isBacktesting ? 'rgba(255,255,255,0.1)' : primaryColor,
                  color: s.isBacktesting ? colors.text : colors.background,
                }}
              >
                {s.isBacktesting ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} />}
                {s.isBacktesting ? 'Running...' : 'Run Backtest'}
              </button>
            </div>

            {s.backtestResult && (
              <div className="mt-3">
                <div className="grid grid-cols-4 gap-2 text-center mb-3">
                  <StatBox
                    label="Strategy Return"
                    value={`${s.backtestResult.return_pct > 0 ? '+' : ''}${s.backtestResult.return_pct}%`}
                    color={s.backtestResult.return_pct >= 0 ? '#16a34a' : '#dc2626'}
                  />
                  <StatBox
                    label="Buy & Hold"
                    value={`${s.backtestResult.buy_hold_return_pct > 0 ? '+' : ''}${s.backtestResult.buy_hold_return_pct}%`}
                    color={s.backtestResult.buy_hold_return_pct >= 0 ? '#16a34a' : '#dc2626'}
                  />
                  <StatBox label="Sharpe" value={`${s.backtestResult.sharpe_ratio}`} color={primaryColor} />
                  <StatBox label="Max DD" value={`${s.backtestResult.max_drawdown_pct}%`} color="#dc2626" />
                </div>
                <div className="flex gap-4 text-xs opacity-60 justify-center">
                  <span>Trades: {s.backtestResult.total_trades}</span>
                  <span>Win Rate: {s.backtestResult.win_rate}%</span>
                  <span>Avg Hold: {s.backtestResult.avg_hold_days}d</span>
                </div>

                {/* Trade list */}
                {s.backtestResult.trades.length > 0 && (
                  <div className="mt-3 max-h-40 overflow-auto">
                    <table className="w-full text-xs">
                      <thead>
                        <tr className="opacity-50 border-b" style={{ borderColor }}>
                          <th className="text-left py-1">Entry</th>
                          <th className="text-left py-1">Exit</th>
                          <th className="text-right py-1">Return</th>
                          <th className="text-right py-1">Days</th>
                          <th className="text-left py-1">Reason</th>
                        </tr>
                      </thead>
                      <tbody>
                        {s.backtestResult.trades.map((t, i) => (
                          <tr key={i} className="border-b" style={{ borderColor: 'rgba(255,255,255,0.05)' }}>
                            <td className="py-1">{t.entry_date}</td>
                            <td className="py-1">{t.exit_date}</td>
                            <td className="py-1 text-right font-mono" style={{ color: t.return_pct >= 0 ? '#16a34a' : '#dc2626' }}>
                              {t.return_pct > 0 ? '+' : ''}{t.return_pct}%
                            </td>
                            <td className="py-1 text-right">{t.hold_days}</td>
                            <td className="py-1 opacity-50">{t.exit_reason}</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}

// ─── Sub-components ──────────────────────────────────────────────────────────

function StatBox({ label, value, color }: { label: string; value: string; color: string }) {
  return (
    <div className="flex flex-col items-center gap-0.5 py-1.5 px-2 rounded" style={{ backgroundColor: 'rgba(255,255,255,0.03)' }}>
      <span className="text-lg font-bold font-mono" style={{ color }}>{value}</span>
      <span className="text-xs opacity-50">{label}</span>
    </div>
  );
}

function MatchCard({ match, rank, colors }: { match: PatternMatch; rank: number; colors: any }) {
  const scorePct = Math.round(match.score * 100);
  const scoreColor = scorePct >= 70 ? '#16a34a' : scorePct >= 50 ? '#ca8a04' : '#dc2626';

  return (
    <div
      className="rounded p-2 flex flex-col gap-1"
      style={{
        backgroundColor: 'rgba(255,255,255,0.03)',
        border: '1px solid rgba(255,255,255,0.06)',
      }}
    >
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-1.5">
          <span className="text-xs opacity-30">#{rank}</span>
          <span className="text-xs font-semibold">{match.symbol}</span>
          <span className="text-xs opacity-40">{match.date}</span>
        </div>
        <span className="text-xs font-bold font-mono" style={{ color: scoreColor }}>
          {scorePct}%
        </span>
      </div>
      <div className="flex items-center gap-3 text-xs opacity-60">
        {match.dtw_sim != null && <span>DTW: {(match.dtw_sim * 100).toFixed(0)}%</span>}
        {match.correlation != null && <span>Corr: {(match.correlation * 100).toFixed(0)}%</span>}
        {match.trend_match != null && (
          <span className={match.trend_match === 1 ? 'text-green-400' : 'text-red-400'}>
            {match.trend_match === 1 ? 'Trend OK' : 'Trend X'}
          </span>
        )}
      </div>
      {match.tb_label !== undefined && (
        <div className="flex items-center gap-2 text-xs">
          <TBLabelIcon label={match.tb_label} />
          <span className={
            match.tb_label === 1 ? 'text-green-400' :
            match.tb_label === -1 ? 'text-red-400' : 'text-yellow-400'
          }>
            {match.tb_hit_type === 'upper' ? 'Take Profit' :
             match.tb_hit_type === 'lower' ? 'Stop Loss' : 'Timeout'}
            {match.tb_hit_day ? ` (${match.tb_hit_day}d)` : ''}
          </span>
          {match.final_return_pct != null && (
            <span className="ml-auto font-mono" style={{ color: match.final_return_pct >= 0 ? '#16a34a' : '#dc2626' }}>
              {match.final_return_pct > 0 ? '+' : ''}{match.final_return_pct}%
            </span>
          )}
        </div>
      )}
    </div>
  );
}
