/**
 * Alpha Arena Tab - Fincept Terminal Design (v3.0)
 *
 * AI Trading Competition Platform - Render-only component.
 * All state/logic lives in useAlphaArena hook.
 * Right panel extracted to AlphaArenaRightPanel.
 *
 * UI matches Fincept terminal aesthetic: sharp edges, monospace font,
 * inline styles, FINCEPT color palette.
 */

import React from 'react';
import {
  Trophy, Play, Pause, RotateCcw, Plus,
  TrendingUp, TrendingDown, Activity, Zap, AlertCircle, CheckCircle,
  ChevronRight, Loader2, Clock, Brain,
  RefreshCw, Check, Circle, History, Trash2, PlayCircle,
  StopCircle, XCircle,
} from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import { APP_VERSION } from '@/constants/version';
import { useAlphaArena, type AlphaArenaModel } from './useAlphaArena';
import AlphaArenaRightPanel from './AlphaArenaRightPanel';
import PerformanceChart from './components/PerformanceChart';
import { formatCurrency, formatPercent, getModelColor } from './types';
import type { CompetitionMode } from './types';

// =============================================================================
// Fincept Terminal Design Constants
// =============================================================================

const FINCEPT = {
  ORANGE: '#FF8800',
  ORANGE_HOVER: '#FF9900',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  YELLOW: '#FFD700',
  CYAN: '#00E5FF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  BORDER_LIGHT: '#3A3A3A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
} as const;

const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", "Monaco", monospace';

const TERMINAL_STYLES = `
  @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');
  .alpha-arena-terminal *::-webkit-scrollbar { width: 6px; height: 6px; }
  .alpha-arena-terminal *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  .alpha-arena-terminal *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; }
  .alpha-arena-terminal *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
  @keyframes alpha-pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
  }
  @keyframes alpha-spin {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
  }
`;

const TRADING_SYMBOLS = [
  { value: 'BTC/USD', label: 'Bitcoin (BTC/USD)' },
  { value: 'ETH/USD', label: 'Ethereum (ETH/USD)' },
  { value: 'BNB/USD', label: 'BNB (BNB/USD)' },
  { value: 'SOL/USD', label: 'Solana (SOL/USD)' },
  { value: 'XRP/USD', label: 'Ripple (XRP/USD)' },
  { value: 'ADA/USD', label: 'Cardano (ADA/USD)' },
  { value: 'DOGE/USD', label: 'Dogecoin (DOGE/USD)' },
  { value: 'AVAX/USD', label: 'Avalanche (AVAX/USD)' },
  { value: 'DOT/USD', label: 'Polkadot (DOT/USD)' },
  { value: 'POL/USD', label: 'Polygon (POL/USD)' },
  { value: 'LTC/USD', label: 'Litecoin (LTC/USD)' },
  { value: 'SHIB/USD', label: 'Shiba Inu (SHIB/USD)' },
  { value: 'TRX/USD', label: 'TRON (TRX/USD)' },
  { value: 'LINK/USD', label: 'Chainlink (LINK/USD)' },
  { value: 'UNI/USD', label: 'Uniswap (UNI/USD)' },
  { value: 'ATOM/USD', label: 'Cosmos (ATOM/USD)' },
  { value: 'XLM/USD', label: 'Stellar (XLM/USD)' },
  { value: 'ETC/USD', label: 'Ethereum Classic (ETC/USD)' },
  { value: 'BCH/USD', label: 'Bitcoin Cash (BCH/USD)' },
  { value: 'NEAR/USD', label: 'NEAR Protocol (NEAR/USD)' },
  { value: 'APT/USD', label: 'Aptos (APT/USD)' },
  { value: 'ARB/USD', label: 'Arbitrum (ARB/USD)' },
  { value: 'OP/USD', label: 'Optimism (OP/USD)' },
  { value: 'LDO/USD', label: 'Lido DAO (LDO/USD)' },
  { value: 'FIL/USD', label: 'Filecoin (FIL/USD)' },
  { value: 'ICP/USD', label: 'Internet Computer (ICP/USD)' },
  { value: 'INJ/USD', label: 'Injective (INJ/USD)' },
  { value: 'STX/USD', label: 'Stacks (STX/USD)' },
  { value: 'MKR/USD', label: 'Maker (MKR/USD)' },
  { value: 'AAVE/USD', label: 'Aave (AAVE/USD)' },
];

const COMPETITION_MODES: { value: CompetitionMode; label: string; desc: string }[] = [
  { value: 'baseline', label: 'Baseline', desc: 'Standard trading with balanced risk' },
  { value: 'monk', label: 'Monk', desc: 'Conservative - capital preservation' },
  { value: 'situational', label: 'Situational', desc: 'Competitive - models aware of others' },
  { value: 'max_leverage', label: 'Max Leverage', desc: 'Aggressive - maximum positions' },
];

// =============================================================================
// Component
// =============================================================================

const AlphaArenaTab: React.FC = () => {
  const arena = useAlphaArena();

  // =============================================================================
  // Render: Terminal Header
  // =============================================================================

  const renderHeader = () => (
    <div style={{
      backgroundColor: FINCEPT.HEADER_BG,
      borderBottom: `2px solid ${FINCEPT.ORANGE}`,
      padding: '6px 12px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      flexShrink: 0,
      boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      fontFamily: TERMINAL_FONT,
    }}>
      {/* Left Section */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Trophy size={18} style={{ color: FINCEPT.ORANGE, filter: `drop-shadow(0 0 4px ${FINCEPT.ORANGE})` }} />
          <span style={{
            color: FINCEPT.ORANGE,
            fontWeight: 700,
            fontSize: '14px',
            letterSpacing: '0.5px',
            textShadow: `0 0 10px ${FINCEPT.ORANGE}40`,
          }}>
            ALPHA ARENA
          </span>
        </div>

        <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

        <span style={{
          fontSize: '9px',
          padding: '2px 8px',
          backgroundColor: `${FINCEPT.PURPLE}20`,
          border: `1px solid ${FINCEPT.PURPLE}40`,
          color: FINCEPT.PURPLE,
          fontWeight: 600,
          letterSpacing: '0.5px',
        }}>
          AI TRADING COMPETITION
        </span>
      </div>

      {/* Right Section */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
        {arena.competitionId && (
          <>
            {arena.latestPrice !== null && (
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                padding: '3px 8px',
                backgroundColor: `${FINCEPT.GREEN}10`,
                border: `1px solid ${FINCEPT.GREEN}25`,
              }}>
                <Zap size={10} style={{ color: FINCEPT.GREEN }} />
                <span style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.GREEN, fontFamily: TERMINAL_FONT }}>
                  ${arena.latestPrice.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </span>
              </div>
            )}
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '3px 8px',
              backgroundColor: FINCEPT.BORDER,
            }}>
              <Activity size={12} style={{ color: arena.isAutoRunning ? FINCEPT.GREEN : FINCEPT.GRAY }} />
              <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>Cycle: {arena.cycleCount}</span>
            </div>
            <div style={{
              padding: '3px 8px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              backgroundColor: arena.status === 'running' ? `${FINCEPT.GREEN}20` : `${FINCEPT.GRAY}20`,
              color: arena.status === 'running' ? FINCEPT.GREEN : FINCEPT.GRAY,
              border: `1px solid ${arena.status === 'running' ? FINCEPT.GREEN : FINCEPT.GRAY}40`,
            }}>
              {arena.isAutoRunning ? 'AUTO-RUNNING' : arena.status?.toUpperCase() || 'IDLE'}
            </div>

            <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
          </>
        )}
        <button
          onClick={() => arena.loadLLMConfigs()}
          style={{
            padding: '5px 7px',
            backgroundColor: 'transparent',
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            transition: 'all 0.15s',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
            e.currentTarget.style.color = FINCEPT.WHITE;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = FINCEPT.GRAY;
          }}
          title="Refresh LLM configs"
        >
          <RefreshCw size={14} />
        </button>
      </div>
    </div>
  );

  // =============================================================================
  // Render: Progress Panel
  // =============================================================================

  const renderProgressPanel = () => {
    if (!arena.isLoading && arena.progressSteps.length === 0) return null;

    return (
      <div style={{
        padding: '12px',
        marginBottom: '8px',
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.ORANGE}40`,
        boxShadow: `0 0 20px ${FINCEPT.ORANGE}10`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '10px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Loader2 size={16} style={{ color: FINCEPT.ORANGE, animation: 'alpha-spin 1s linear infinite' }} />
            <span style={{ fontWeight: 600, fontSize: '12px', color: FINCEPT.WHITE }}>
              {arena.currentProgress ? arena.currentProgress.message : 'Processing...'}
            </span>
          </div>
          <button
            onClick={arena.handleCancel}
            disabled={arena.isCancelling}
            style={{
              padding: '4px 10px',
              backgroundColor: FINCEPT.RED,
              color: FINCEPT.WHITE,
              border: 'none',
              fontSize: '10px',
              fontWeight: 600,
              cursor: arena.isCancelling ? 'not-allowed' : 'pointer',
              opacity: arena.isCancelling ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            {arena.isCancelling ? <Loader2 size={10} style={{ animation: 'alpha-spin 1s linear infinite' }} /> : <XCircle size={10} />}
            {arena.isCancelling ? 'Stopping...' : 'Cancel'}
          </button>
        </div>

        {arena.currentProgress && (
          <div style={{ marginBottom: '10px' }}>
            <div style={{ height: '4px', overflow: 'hidden', backgroundColor: FINCEPT.BORDER }}>
              <div style={{
                height: '100%',
                transition: 'width 0.3s ease-out',
                width: `${(arena.currentProgress.step / arena.currentProgress.total_steps) * 100}%`,
                backgroundColor: arena.currentProgress.event_type === 'error' ? FINCEPT.RED :
                                 arena.currentProgress.event_type === 'complete' ? FINCEPT.GREEN : FINCEPT.ORANGE,
              }} />
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '4px' }}>
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                Step {arena.currentProgress.step} of {arena.currentProgress.total_steps}
              </span>
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                {Math.round((arena.currentProgress.step / arena.currentProgress.total_steps) * 100)}%
              </span>
            </div>
          </div>
        )}

        <div style={{ maxHeight: '120px', overflowY: 'auto' }}>
          {arena.progressSteps.map((step, idx) => (
            <div key={idx} style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '10px', marginBottom: '2px' }}>
              {step.event_type === 'complete' ? (
                <Check size={10} style={{ color: FINCEPT.GREEN }} />
              ) : step.event_type === 'error' ? (
                <AlertCircle size={10} style={{ color: FINCEPT.RED }} />
              ) : step.event_type === 'warning' ? (
                <AlertCircle size={10} style={{ color: FINCEPT.YELLOW }} />
              ) : step.event_type === 'step' ? (
                <Circle size={10} style={{ color: FINCEPT.ORANGE }} />
              ) : (
                <ChevronRight size={10} style={{ color: FINCEPT.GRAY }} />
              )}
              <span style={{
                color: step.event_type === 'error' ? FINCEPT.RED :
                       step.event_type === 'complete' ? FINCEPT.GREEN :
                       step.event_type === 'warning' ? FINCEPT.YELLOW : FINCEPT.GRAY,
              }}>
                {step.message}
              </span>
            </div>
          ))}
        </div>
      </div>
    );
  };

  // =============================================================================
  // Render: Past Competitions Panel
  // =============================================================================

  const renderPastCompetitions = () => (
    <div style={{ padding: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <History size={16} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontWeight: 700, fontSize: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>PAST COMPETITIONS</span>
          <span style={{
            fontSize: '9px',
            padding: '1px 6px',
            backgroundColor: FINCEPT.BORDER,
            color: FINCEPT.GRAY,
            fontWeight: 600,
          }}>
            {arena.pastCompetitions.length}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <button
            onClick={() => arena.pastCompetitionsCache.refresh()}
            disabled={arena.loadingPastCompetitions}
            style={{
              padding: '4px 6px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
            }}
            title="Refresh"
          >
            <RefreshCw size={12} style={arena.loadingPastCompetitions ? { animation: 'alpha-spin 1s linear infinite' } : {}} />
          </button>
          <button
            onClick={() => arena.setShowPastCompetitions(false)}
            style={{
              fontSize: '10px',
              padding: '4px 10px',
              backgroundColor: FINCEPT.BORDER,
              color: FINCEPT.WHITE,
              border: 'none',
              cursor: 'pointer',
              fontWeight: 600,
            }}
          >
            Close
          </button>
        </div>
      </div>

      {arena.loadingPastCompetitions ? (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '32px 0' }}>
          <Loader2 size={24} style={{ color: FINCEPT.ORANGE, animation: 'alpha-spin 1s linear infinite' }} />
        </div>
      ) : arena.pastCompetitions.length === 0 ? (
        <div style={{ textAlign: 'center', padding: '32px 0' }}>
          <History size={32} style={{ color: FINCEPT.GRAY, opacity: 0.5, margin: '0 auto 8px' }} />
          <p style={{ fontSize: '11px', color: FINCEPT.GRAY }}>No past competitions found</p>
        </div>
      ) : (
        <div style={{ maxHeight: '400px', overflowY: 'auto' }}>
          {arena.pastCompetitions.map(comp => (
            <div
              key={comp.competition_id}
              onClick={() => arena.handleResumeCompetition(comp)}
              style={{
                padding: '10px',
                marginBottom: '4px',
                cursor: 'pointer',
                backgroundColor: FINCEPT.CARD_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                transition: 'all 0.15s',
              }}
              onMouseEnter={e => {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                e.currentTarget.style.borderColor = `${FINCEPT.ORANGE}40`;
              }}
              onMouseLeave={e => {
                e.currentTarget.style.backgroundColor = FINCEPT.CARD_BG;
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <Trophy size={12} style={{ color: FINCEPT.ORANGE }} />
                  <span style={{ fontWeight: 600, fontSize: '11px', color: FINCEPT.WHITE }}>
                    {comp.config.competition_name}
                  </span>
                </div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <span style={{
                    fontSize: '8px',
                    padding: '1px 6px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    backgroundColor:
                      comp.status === 'running' ? `${FINCEPT.GREEN}20` :
                      comp.status === 'completed' ? `${FINCEPT.BLUE}20` :
                      comp.status === 'failed' ? `${FINCEPT.RED}20` :
                      `${FINCEPT.GRAY}20`,
                    color:
                      comp.status === 'running' ? FINCEPT.GREEN :
                      comp.status === 'completed' ? FINCEPT.BLUE :
                      comp.status === 'failed' ? FINCEPT.RED :
                      FINCEPT.GRAY,
                  }}>
                    {comp.status.toUpperCase()}
                  </span>
                  <button
                    onClick={(e) => arena.handleDeleteCompetition(comp.competition_id, e)}
                    style={{
                      padding: '2px 4px',
                      backgroundColor: 'transparent',
                      border: 'none',
                      cursor: 'pointer',
                      color: FINCEPT.RED,
                      display: 'flex',
                      alignItems: 'center',
                    }}
                    title="Delete competition"
                  >
                    <Trash2 size={11} />
                  </button>
                </div>
              </div>

              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '6px', fontSize: '10px' }}>
                <div>
                  <span style={{ color: FINCEPT.GRAY }}>Cycles: </span>
                  <span style={{ color: FINCEPT.WHITE }}>{comp.cycle_count}</span>
                </div>
                <div>
                  <span style={{ color: FINCEPT.GRAY }}>Models: </span>
                  <span style={{ color: FINCEPT.WHITE }}>{comp.config.models.length}</span>
                </div>
                <div>
                  <span style={{ color: FINCEPT.GRAY }}>Capital: </span>
                  <span style={{ color: FINCEPT.WHITE }}>${comp.config.initial_capital.toLocaleString()}</span>
                </div>
              </div>

              <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginTop: '6px' }}>
                <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                  {comp.config.symbols.join(', ')}
                </span>
                <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>|</span>
                <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                  {new Date(comp.created_at).toLocaleDateString()}
                </span>
              </div>

              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '3px', marginTop: '6px' }}>
                {comp.config.models.map((model, idx) => (
                  <span
                    key={idx}
                    style={{
                      fontSize: '8px',
                      padding: '1px 5px',
                      backgroundColor: FINCEPT.BORDER,
                      color: FINCEPT.GRAY,
                      fontWeight: 600,
                    }}
                  >
                    {model.name}
                  </span>
                ))}
              </div>

              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'flex-end',
                marginTop: '6px',
                paddingTop: '6px',
                borderTop: `1px solid ${FINCEPT.BORDER}`,
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', color: FINCEPT.ORANGE }}>
                  <PlayCircle size={10} />
                  <span>Click to resume</span>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );

  // =============================================================================
  // Render: Create Competition Panel
  // =============================================================================

  const renderCreatePanel = () => (
    <div style={{ padding: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
        <Plus size={16} style={{ color: FINCEPT.ORANGE }} />
        <span style={{ fontWeight: 700, fontSize: '12px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>CREATE COMPETITION</span>
      </div>

      {/* Competition Name */}
      <div style={{ marginBottom: '10px' }}>
        <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>COMPETITION NAME</label>
        <input
          type="text"
          value={arena.competitionName}
          onChange={e => arena.setCompetitionName(e.target.value)}
          style={{
            width: '100%',
            padding: '8px 10px',
            backgroundColor: FINCEPT.HEADER_BG,
            color: FINCEPT.WHITE,
            border: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '12px',
            fontFamily: TERMINAL_FONT,
            outline: 'none',
            boxSizing: 'border-box',
          }}
          onFocus={e => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
          onBlur={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
        />
      </div>

      {/* Symbol & Mode */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '10px' }}>
        <div>
          <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>TRADING SYMBOL</label>
          <select
            value={arena.symbol}
            onChange={e => arena.setSymbol(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: FINCEPT.HEADER_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '12px',
              fontFamily: TERMINAL_FONT,
              outline: 'none',
            }}
          >
            {TRADING_SYMBOLS.map(s => (
              <option key={s.value} value={s.value} style={{ backgroundColor: FINCEPT.HEADER_BG }}>{s.label}</option>
            ))}
          </select>
        </div>
        <div>
          <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>COMPETITION MODE</label>
          <select
            value={arena.mode}
            onChange={e => arena.setMode(e.target.value as CompetitionMode)}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: FINCEPT.HEADER_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '12px',
              fontFamily: TERMINAL_FONT,
              outline: 'none',
            }}
          >
            {COMPETITION_MODES.map(m => (
              <option key={m.value} value={m.value} style={{ backgroundColor: FINCEPT.HEADER_BG }}>{m.label}</option>
            ))}
          </select>
        </div>
      </div>

      {/* Capital & Interval */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '10px' }}>
        <div>
          <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>INITIAL CAPITAL (USD)</label>
          <input
            type="text"
            inputMode="numeric"
            value={arena.initialCapital}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) arena.setInitialCapital(Number(v) || 0); }}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: FINCEPT.HEADER_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '12px',
              fontFamily: TERMINAL_FONT,
              outline: 'none',
              boxSizing: 'border-box',
            }}
            onFocus={e => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
            onBlur={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
          />
        </div>
        <div>
          <label style={{ display: 'block', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>CYCLE INTERVAL (SECONDS)</label>
          <input
            type="text"
            inputMode="numeric"
            value={arena.cycleInterval}
            onChange={e => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) arena.setCycleInterval(Math.min(Number(v) || 0, 600)); }}
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: FINCEPT.HEADER_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '12px',
              fontFamily: TERMINAL_FONT,
              outline: 'none',
              boxSizing: 'border-box',
            }}
            onFocus={e => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
            onBlur={e => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
          />
        </div>
      </div>

      {/* LLM Model Selection */}
      <div style={{ marginBottom: '10px' }}>
        <label style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '6px', fontWeight: 600, letterSpacing: '0.5px' }}>
          <Brain size={10} />
          SELECT AI MODELS (from Settings - min 2 required)
        </label>

        {arena.configuredLLMs.length === 0 ? (
          <div style={{
            padding: '16px',
            textAlign: 'center',
            backgroundColor: FINCEPT.CARD_BG,
            border: `1px solid ${FINCEPT.RED}40`,
          }}>
            <AlertCircle size={24} style={{ color: FINCEPT.RED, margin: '0 auto 8px' }} />
            <p style={{ fontSize: '11px', color: FINCEPT.RED, marginBottom: '4px' }}>No AI models configured</p>
            <p style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
              Go to Settings → LLM Config → My Model Library to add models with API keys
            </p>
          </div>
        ) : (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
            {arena.configuredLLMs.map((model: AlphaArenaModel) => {
              const isSelected = arena.selectedModels.some(m => m.id === model.id);
              return (
                <button
                  key={model.id}
                  type="button"
                  onClick={() => arena.toggleModelSelection(model)}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    padding: '8px 10px',
                    textAlign: 'left',
                    cursor: 'pointer',
                    backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : FINCEPT.CARD_BG,
                    border: `1px solid ${isSelected ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                    transition: 'all 0.15s',
                  }}
                >
                  <div style={{
                    width: '16px',
                    height: '16px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    backgroundColor: isSelected ? FINCEPT.ORANGE : FINCEPT.BORDER,
                    flexShrink: 0,
                  }}>
                    {isSelected && <CheckCircle size={10} style={{ color: FINCEPT.DARK_BG }} />}
                  </div>
                  <div style={{ flex: 1, minWidth: 0, overflow: 'hidden' }}>
                    <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                      {model.display_name}
                    </div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                      {model.provider} | {model.model_id}
                    </div>
                  </div>
                </button>
              );
            })}
          </div>
        )}
        <p style={{ fontSize: '9px', marginTop: '6px', color: FINCEPT.GRAY }}>
          Selected: {arena.selectedModels.length} model(s)
        </p>
      </div>

      {/* Actions */}
      <div style={{ display: 'flex', gap: '6px', paddingTop: '8px' }}>
        <button
          onClick={() => arena.setShowCreatePanel(false)}
          style={{
            padding: '8px 14px',
            backgroundColor: FINCEPT.BORDER,
            color: FINCEPT.GRAY,
            border: 'none',
            fontSize: '11px',
            fontWeight: 600,
            cursor: 'pointer',
          }}
        >
          CANCEL
        </button>
        <button
          onClick={arena.handleCreateCompetition}
          disabled={arena.isLoading || arena.selectedModels.length < 2 || arena.configuredLLMs.length === 0}
          style={{
            flex: 1,
            padding: '8px 14px',
            backgroundColor: FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: 'none',
            fontSize: '11px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            cursor: (arena.isLoading || arena.selectedModels.length < 2) ? 'not-allowed' : 'pointer',
            opacity: (arena.isLoading || arena.selectedModels.length < 2) ? 0.5 : 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
          }}
        >
          {arena.isLoading ? <Loader2 size={14} style={{ animation: 'alpha-spin 1s linear infinite' }} /> : <Zap size={14} />}
          CREATE COMPETITION
        </button>
      </div>
    </div>
  );

  // =============================================================================
  // Render: Competition Controls
  // =============================================================================

  const renderControls = () => (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      gap: '6px',
      padding: '8px 10px',
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
    }}>
      {arena.isLoading ? (
        <button
          onClick={arena.handleCancel}
          disabled={arena.isCancelling}
          style={{
            padding: '6px 12px',
            backgroundColor: FINCEPT.RED,
            color: FINCEPT.WHITE,
            border: 'none',
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            cursor: arena.isCancelling ? 'not-allowed' : 'pointer',
            opacity: arena.isCancelling ? 0.5 : 1,
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
          }}
        >
          {arena.isCancelling ? <Loader2 size={12} style={{ animation: 'alpha-spin 1s linear infinite' }} /> : <StopCircle size={12} />}
          {arena.isCancelling ? 'STOPPING...' : 'STOP'}
        </button>
      ) : (
        <button
          onClick={arena.handleRunCycle}
          disabled={arena.isAutoRunning}
          style={{
            padding: '6px 12px',
            backgroundColor: FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: 'none',
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            cursor: arena.isAutoRunning ? 'not-allowed' : 'pointer',
            opacity: arena.isAutoRunning ? 0.5 : 1,
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
          }}
        >
          <Play size={12} />
          RUN CYCLE
        </button>
      )}

      <button
        onClick={arena.handleToggleAutoRun}
        disabled={arena.isLoading}
        style={{
          padding: '6px 12px',
          backgroundColor: arena.isAutoRunning ? `${FINCEPT.RED}20` : `${FINCEPT.GREEN}20`,
          color: arena.isAutoRunning ? FINCEPT.RED : FINCEPT.GREEN,
          border: `1px solid ${arena.isAutoRunning ? FINCEPT.RED : FINCEPT.GREEN}40`,
          fontSize: '10px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          cursor: arena.isLoading ? 'not-allowed' : 'pointer',
          opacity: arena.isLoading ? 0.5 : 1,
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
        }}
      >
        {arena.isAutoRunning ? <Pause size={12} /> : <Zap size={12} />}
        {arena.isAutoRunning ? 'STOP AUTO' : 'AUTO RUN'}
      </button>

      <button
        onClick={arena.handleReset}
        style={{
          padding: '6px 12px',
          backgroundColor: FINCEPT.BORDER,
          color: FINCEPT.GRAY,
          border: 'none',
          fontSize: '10px',
          fontWeight: 600,
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
        }}
        onMouseEnter={(e) => { e.currentTarget.style.color = FINCEPT.WHITE; }}
        onMouseLeave={(e) => { e.currentTarget.style.color = FINCEPT.GRAY; }}
      >
        <RotateCcw size={12} />
        RESET
      </button>

      <div style={{ flex: 1 }} />

      <div style={{
        fontSize: '9px',
        padding: '4px 8px',
        backgroundColor: FINCEPT.CARD_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        color: FINCEPT.GRAY,
        display: 'flex',
        alignItems: 'center',
        gap: '4px',
      }}>
        <Clock size={10} />
        INTERVAL: {arena.cycleInterval}s
      </div>
    </div>
  );

  // =============================================================================
  // Render: Leaderboard
  // =============================================================================

  const renderLeaderboard = () => (
    <div style={{
      overflow: 'hidden',
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
    }}>
      {/* Header */}
      <div style={{
        padding: '8px 12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Trophy size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ fontWeight: 700, fontSize: '11px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>LEADERBOARD</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          {arena.latestPrice !== null && (
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              padding: '2px 6px',
              backgroundColor: `${FINCEPT.GREEN}15`,
              border: `1px solid ${FINCEPT.GREEN}30`,
            }}>
              <Activity size={9} style={{ color: FINCEPT.GREEN }} />
              <span style={{ fontSize: '9px', fontWeight: 600, color: FINCEPT.GREEN, fontFamily: TERMINAL_FONT }}>
                {arena.symbol.split('/')[0]} ${arena.latestPrice.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
              </span>
            </div>
          )}
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Cycle {arena.cycleCount}</span>
        </div>
      </div>

      {arena.leaderboard.length === 0 ? (
        <div style={{ padding: '32px', textAlign: 'center', color: FINCEPT.GRAY }}>
          <Trophy size={32} style={{ opacity: 0.3, margin: '0 auto 8px' }} />
          <p style={{ fontSize: '11px' }}>No competition data yet</p>
        </div>
      ) : (
        <div>
          {arena.leaderboard.map(entry => {
            const isPositive = entry.total_pnl >= 0;
            const modelColor = getModelColor(entry.model_name);

            return (
              <div
                key={entry.model_name}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                  padding: '10px 12px',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  transition: 'background-color 0.15s',
                  cursor: 'default',
                }}
                onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
                onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
              >
                {/* Rank */}
                <div style={{
                  width: '28px',
                  height: '28px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  fontSize: '12px',
                  fontWeight: 700,
                  backgroundColor: entry.rank === 1 ? `${FINCEPT.YELLOW}20` : entry.rank === 2 ? `${FINCEPT.GRAY}20` : entry.rank === 3 ? `${FINCEPT.ORANGE}20` : FINCEPT.BORDER,
                  color: entry.rank === 1 ? FINCEPT.YELLOW : entry.rank === 2 ? FINCEPT.GRAY : entry.rank === 3 ? FINCEPT.ORANGE : FINCEPT.GRAY,
                }}>
                  {entry.rank}
                </div>

                {/* Color Bar */}
                <div style={{ width: '3px', height: '32px', backgroundColor: modelColor }} />

                {/* Model Info */}
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    <span style={{ fontWeight: 600, fontSize: '11px', color: FINCEPT.WHITE, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                      {entry.model_name}
                    </span>
                    {entry.rank === 1 && <Trophy size={12} style={{ color: FINCEPT.YELLOW }} />}
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '10px', fontSize: '9px', color: FINCEPT.GRAY }}>
                    <span>{entry.trades_count} trades</span>
                    <span>{entry.positions_count} positions</span>
                    {entry.win_rate !== undefined && (
                      <span>{(entry.win_rate * 100).toFixed(0)}% win</span>
                    )}
                  </div>
                </div>

                {/* P&L */}
                <div style={{ textAlign: 'right' }}>
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '4px' }}>
                    {isPositive ? (
                      <TrendingUp size={12} style={{ color: FINCEPT.GREEN }} />
                    ) : (
                      <TrendingDown size={12} style={{ color: FINCEPT.RED }} />
                    )}
                    <span style={{ fontSize: '11px', fontWeight: 600, color: isPositive ? FINCEPT.GREEN : FINCEPT.RED }}>
                      {formatPercent(entry.return_pct)}
                    </span>
                  </div>
                  <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                    {formatCurrency(entry.portfolio_value)}
                  </span>
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );

  // =============================================================================
  // Render: Performance Chart
  // =============================================================================

  const renderPerformanceChart = () => (
    <PerformanceChart snapshots={arena.snapshots} height={250} />
  );

  // =============================================================================
  // Render: Status Bar (Terminal Footer)
  // =============================================================================

  const renderStatusBar = () => (
    <div style={{
      borderTop: `1px solid ${FINCEPT.BORDER}`,
      backgroundColor: FINCEPT.HEADER_BG,
      padding: '4px 12px',
      fontSize: '9px',
      color: FINCEPT.GRAY,
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      flexShrink: 0,
      fontFamily: TERMINAL_FONT,
    }}>
      {/* Left - Branding */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <span style={{ color: FINCEPT.ORANGE, fontWeight: 700 }}>FINCEPT</span>
        <span>v{APP_VERSION}</span>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span>ALPHA ARENA</span>
      </div>

      {/* Right - Status */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span style={{ color: FINCEPT.GRAY }}>ID:</span>
          <span style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>
            {arena.competitionId ? arena.competitionId.slice(0, 8) + '...' : 'NONE'}
          </span>
        </div>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span>Models: {arena.selectedModels.length}</span>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span>LLMs: {arena.configuredLLMs.length}</span>
        {arena.selectedBroker && (
          <>
            <span style={{ color: FINCEPT.BORDER }}>|</span>
            <span style={{ color: FINCEPT.ORANGE, fontWeight: 600 }}>Broker: {arena.selectedBroker}</span>
          </>
        )}
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span>Cycle: {arena.cycleCount}</span>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span>Decisions: {arena.decisions.length}</span>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>
          {arena.rightPanelTab.toUpperCase()}
        </span>
      </div>
    </div>
  );

  // =============================================================================
  // Main Render
  // =============================================================================

  return (
    <div
      className="alpha-arena-terminal"
      style={{
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        backgroundColor: FINCEPT.DARK_BG,
        color: FINCEPT.WHITE,
        fontFamily: TERMINAL_FONT,
      }}
    >
      <style>{TERMINAL_STYLES}</style>

      {renderHeader()}

      {/* Notifications */}
      {arena.error && (
        <div style={{
          margin: '0 12px',
          marginTop: '4px',
          padding: '8px 10px',
          backgroundColor: `${FINCEPT.RED}20`,
          border: `1px solid ${FINCEPT.RED}40`,
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          color: FINCEPT.RED,
        }}>
          <AlertCircle size={14} />
          <span style={{ fontSize: '11px', flex: 1 }}>{arena.error}</span>
          <button
            onClick={() => arena.setError(null)}
            style={{ background: 'none', border: 'none', color: FINCEPT.RED, cursor: 'pointer', fontSize: '14px', fontWeight: 700 }}
          >
            x
          </button>
        </div>
      )}
      {arena.success && (
        <div style={{
          margin: '0 12px',
          marginTop: '4px',
          padding: '8px 10px',
          backgroundColor: `${FINCEPT.GREEN}20`,
          border: `1px solid ${FINCEPT.GREEN}40`,
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          color: FINCEPT.GREEN,
        }}>
          <CheckCircle size={14} />
          <span style={{ fontSize: '11px' }}>{arena.success}</span>
        </div>
      )}

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', padding: '8px', gap: '8px', overflowY: 'auto' }}>
          {!arena.competitionId && !arena.showCreatePanel && !arena.showPastCompetitions && (
            <div style={{
              padding: '32px',
              textAlign: 'center',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
            }}>
              <Trophy size={48} style={{ color: FINCEPT.ORANGE, opacity: 0.5, margin: '0 auto 16px' }} />
              <h3 style={{ fontSize: '14px', fontWeight: 700, marginBottom: '8px', color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>NO ACTIVE COMPETITION</h3>
              <p style={{ fontSize: '11px', marginBottom: '16px', color: FINCEPT.GRAY }}>
                Create a new AI trading competition or resume a past competition.
              </p>
              <div style={{ display: 'flex', justifyContent: 'center', gap: '8px' }}>
                <button
                  onClick={() => arena.setShowCreatePanel(true)}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    fontWeight: 700,
                    fontSize: '11px',
                    letterSpacing: '0.5px',
                    cursor: 'pointer',
                  }}
                >
                  <Plus size={14} />
                  CREATE COMPETITION
                </button>
                {arena.pastCompetitions.length > 0 && (
                  <button
                    onClick={() => arena.setShowPastCompetitions(true)}
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      padding: '8px 16px',
                      backgroundColor: FINCEPT.CARD_BG,
                      color: FINCEPT.WHITE,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      fontWeight: 600,
                      fontSize: '11px',
                      cursor: 'pointer',
                      transition: 'all 0.15s',
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = `${FINCEPT.ORANGE}40`;
                      e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                      e.currentTarget.style.backgroundColor = FINCEPT.CARD_BG;
                    }}
                  >
                    <History size={14} />
                    PAST COMPETITIONS ({arena.pastCompetitions.length})
                  </button>
                )}
              </div>
              {arena.configuredLLMs.length === 0 && (
                <p style={{ fontSize: '9px', marginTop: '16px', color: FINCEPT.RED }}>
                  No AI models configured. Go to Settings → LLM Config → My Model Library first.
                </p>
              )}
            </div>
          )}

          {!arena.competitionId && arena.showPastCompetitions && renderPastCompetitions()}

          {renderProgressPanel()}

          {arena.showCreatePanel && renderCreatePanel()}

          {arena.competitionId && (
            <>
              {renderControls()}
              {renderPerformanceChart()}
              {renderLeaderboard()}
            </>
          )}
        </div>

        {/* Right Panel */}
        <AlphaArenaRightPanel
          rightPanelTab={arena.rightPanelTab}
          setRightPanelTab={arena.setRightPanelTab}
          decisions={arena.decisions}
          competitionId={arena.competitionId}
          symbol={arena.symbol}
          latestPrice={arena.latestPrice}
          selectedBroker={arena.selectedBroker}
          setSelectedBroker={arena.setSelectedBroker}
          refreshCompetitionData={arena.refreshCompetitionData}
        />
      </div>

      {renderStatusBar()}
    </div>
  );
};

export default withErrorBoundary(AlphaArenaTab, { name: 'AlphaArena' });
