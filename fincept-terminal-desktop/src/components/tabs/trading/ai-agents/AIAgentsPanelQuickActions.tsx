import React from 'react';
import {
  Brain, Target, Shield, Sparkles, Loader2, AlertCircle,
  CheckCircle, Zap
} from 'lucide-react';

// Fincept color palette
export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

export interface QuickActionsViewProps {
  selectedSymbol: string;
  selectedModel: string;
  setSelectedModel: (model: string) => void;
  analysisType: 'quick' | 'comprehensive' | 'deep';
  setAnalysisType: (type: 'quick' | 'comprehensive' | 'deep') => void;
  strategy: 'momentum' | 'breakout' | 'reversal' | 'trend_following';
  setStrategy: (strategy: 'momentum' | 'breakout' | 'reversal' | 'trend_following') => void;
  isLoading: boolean;
  result: any;
  error: string | null;
  hasConfiguredKeys: boolean;
  availableModels: Array<{ value: string; label: string }>;
  onAnalyzeMarket: () => void;
  onGenerateSignal: () => void;
  onAssessRisk: () => void;
  onStartCompetition: () => void;
  onExecuteTrade: () => void;
}

export function QuickActionsView({
  selectedModel,
  setSelectedModel,
  analysisType,
  setAnalysisType,
  strategy,
  setStrategy,
  isLoading,
  result,
  error,
  hasConfiguredKeys,
  availableModels,
  onAnalyzeMarket,
  onGenerateSignal,
  onAssessRisk,
  onStartCompetition,
  onExecuteTrade
}: QuickActionsViewProps) {
  const models = availableModels;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      {/* Model Selection */}
      <div>
        <label style={{
          color: FINCEPT.GRAY,
          fontSize: '9px',
          fontWeight: '600',
          display: 'block',
          marginBottom: '4px',
          letterSpacing: '0.5px'
        }}>
          MODEL
        </label>
        <select
          value={selectedModel}
          onChange={(e) => setSelectedModel(e.target.value)}
          disabled={!hasConfiguredKeys}
          style={{
            width: '100%',
            background: FINCEPT.DARK_BG,
            border: `1px solid ${hasConfiguredKeys ? FINCEPT.BORDER : FINCEPT.RED}`,
            color: hasConfiguredKeys ? FINCEPT.WHITE : FINCEPT.GRAY,
            padding: '5px 8px',
            borderRadius: '2px',
            fontSize: '10px',
            cursor: hasConfiguredKeys ? 'pointer' : 'not-allowed',
            fontFamily: '"IBM Plex Mono", monospace',
            opacity: hasConfiguredKeys ? 1 : 0.6
          }}
        >
          {models.map(m => (
            <option key={m.value} value={m.value}>{m.label}</option>
          ))}
        </select>
        {!hasConfiguredKeys && (
          <div style={{ fontSize: '8px', color: FINCEPT.RED, marginTop: '3px' }}>
            Configure API keys in Settings → LLM Config
          </div>
        )}
      </div>

      {/* Quick Action Buttons */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
        {/* Analyze Market */}
        <div>
          <button
            onClick={onAnalyzeMarket}
            disabled={isLoading || !hasConfiguredKeys}
            style={{
              width: '100%',
              background: hasConfiguredKeys ? FINCEPT.BLUE : FINCEPT.GRAY,
              border: `1px solid ${hasConfiguredKeys ? FINCEPT.BLUE : FINCEPT.GRAY}`,
              color: FINCEPT.WHITE,
              padding: '6px',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: '600',
              cursor: (isLoading || !hasConfiguredKeys) ? 'not-allowed' : 'pointer',
              opacity: (isLoading || !hasConfiguredKeys) ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
              letterSpacing: '0.3px'
            }}
          >
            {isLoading ? <Loader2 size={12} className="animate-spin" /> : <Brain size={12} />}
            ANALYZE
          </button>
          <select
            value={analysisType}
            onChange={(e) => setAnalysisType(e.target.value as any)}
            style={{
              width: '100%',
              background: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              padding: '4px 6px',
              borderRadius: '2px',
              fontSize: '9px',
              marginTop: '3px',
              cursor: 'pointer'
            }}
          >
            <option value="quick">Quick</option>
            <option value="comprehensive">Comprehensive</option>
            <option value="deep">Deep</option>
          </select>
        </div>

        {/* Generate Signal */}
        <div>
          <button
            onClick={onGenerateSignal}
            disabled={isLoading || !hasConfiguredKeys}
            style={{
              width: '100%',
              background: hasConfiguredKeys ? FINCEPT.GREEN : FINCEPT.GRAY,
              border: `1px solid ${hasConfiguredKeys ? FINCEPT.GREEN : FINCEPT.GRAY}`,
              color: FINCEPT.DARK_BG,
              padding: '6px',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: '600',
              cursor: (isLoading || !hasConfiguredKeys) ? 'not-allowed' : 'pointer',
              opacity: (isLoading || !hasConfiguredKeys) ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
              letterSpacing: '0.3px'
            }}
          >
            {isLoading ? <Loader2 size={12} className="animate-spin" /> : <Target size={12} />}
            SIGNAL
          </button>
          <select
            value={strategy}
            onChange={(e) => setStrategy(e.target.value as any)}
            style={{
              width: '100%',
              background: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              padding: '4px 6px',
              borderRadius: '2px',
              fontSize: '9px',
              marginTop: '3px',
              cursor: 'pointer'
            }}
          >
            <option value="momentum">Momentum</option>
            <option value="breakout">Breakout</option>
            <option value="reversal">Reversal</option>
            <option value="trend_following">Trend</option>
          </select>
        </div>

        {/* Assess Risk */}
        <button
          onClick={onAssessRisk}
          disabled={isLoading || !hasConfiguredKeys}
          style={{
            width: '100%',
            background: hasConfiguredKeys ? FINCEPT.ORANGE : FINCEPT.GRAY,
            border: `1px solid ${hasConfiguredKeys ? FINCEPT.ORANGE : FINCEPT.GRAY}`,
            color: FINCEPT.DARK_BG,
            padding: '6px',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: '600',
            cursor: (isLoading || !hasConfiguredKeys) ? 'not-allowed' : 'pointer',
            opacity: (isLoading || !hasConfiguredKeys) ? 0.5 : 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            letterSpacing: '0.3px'
          }}
        >
          {isLoading ? <Loader2 size={12} className="animate-spin" /> : <Shield size={12} />}
          RISK
        </button>

        {/* Start Competition */}
        <button
          onClick={onStartCompetition}
          disabled={isLoading || availableModels.length < 2}
          style={{
            width: '100%',
            background: (availableModels.length >= 2) ? FINCEPT.PURPLE : FINCEPT.GRAY,
            border: `1px solid ${(availableModels.length >= 2) ? FINCEPT.PURPLE : FINCEPT.GRAY}`,
            color: FINCEPT.WHITE,
            padding: '6px',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: '600',
            cursor: (isLoading || availableModels.length < 2) ? 'not-allowed' : 'pointer',
            opacity: (isLoading || availableModels.length < 2) ? 0.5 : 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            letterSpacing: '0.3px'
          }}
        >
          {isLoading ? <Loader2 size={12} className="animate-spin" /> : <Sparkles size={12} />}
          COMPETE
        </button>
      </div>

      {!hasConfiguredKeys && (
        <div style={{ fontSize: '8px', color: FINCEPT.RED, marginTop: '4px', textAlign: 'center' }}>
          Configure API keys in Settings → LLM Config
        </div>
      )}

      {hasConfiguredKeys && availableModels.length < 2 && (
        <div style={{ fontSize: '8px', color: FINCEPT.YELLOW, marginTop: '4px', textAlign: 'center' }}>
          Add 2+ models for multi-model competition
        </div>
      )}

      {/* Error Display */}
      {error && (
        <div style={{
          background: `${FINCEPT.RED}10`,
          border: `1px solid ${FINCEPT.RED}`,
          borderRadius: '2px',
          padding: '6px 8px',
          display: 'flex',
          alignItems: 'flex-start',
          gap: '6px'
        }}>
          <AlertCircle size={12} color={FINCEPT.RED} style={{ flexShrink: 0, marginTop: '1px' }} />
          <div>
            <div style={{ color: FINCEPT.RED, fontSize: '9px', fontWeight: '600', letterSpacing: '0.3px' }}>ERROR</div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px', lineHeight: '1.4' }}>{error}</div>
          </div>
        </div>
      )}

      {/* Result Display */}
      {result && (
        <div style={{
          background: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '8px',
          maxHeight: '300px',
          overflow: 'auto'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            marginBottom: '6px',
            paddingBottom: '6px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <CheckCircle size={12} color={FINCEPT.GREEN} />
            <span style={{ color: FINCEPT.GREEN, fontSize: '9px', fontWeight: '600', letterSpacing: '0.5px' }}>
              {result.type === 'market_analysis' && 'MARKET ANALYSIS'}
              {result.type === 'trade_signal' && 'TRADE SIGNAL'}
              {result.type === 'risk_analysis' && 'RISK ASSESSMENT'}
              {result.type === 'competition' && 'MULTI-MODEL COMPETITION'}
              {result.type === 'trade_execution' && 'TRADE EXECUTED'}
            </span>

            {/* Execute Button for Trade Signals */}
            {result.type === 'trade_signal' && (
              <button
                onClick={onExecuteTrade}
                disabled={isLoading}
                style={{
                  marginLeft: 'auto',
                  background: FINCEPT.ORANGE,
                  border: `1px solid ${FINCEPT.ORANGE}`,
                  color: FINCEPT.DARK_BG,
                  padding: '3px 8px',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: '700',
                  cursor: isLoading ? 'not-allowed' : 'pointer',
                  opacity: isLoading ? 0.5 : 1,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '3px',
                  letterSpacing: '0.3px',
                  transition: 'all 0.2s'
                }}
              >
                {isLoading ? <Loader2 size={10} className="animate-spin" /> : <Zap size={10} />}
                EXECUTE
              </button>
            )}
          </div>

          {result.type === 'competition' ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {/* Consensus */}
              <div>
                <div style={{ color: FINCEPT.ORANGE, fontSize: '8px', fontWeight: '700', marginBottom: '4px' }}>
                  CONSENSUS ({result.data.consensus.total_models} MODELS)
                </div>
                <div style={{
                  background: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  padding: '6px',
                  fontSize: '9px',
                  color: FINCEPT.WHITE
                }}>
                  Action: <span style={{ color: FINCEPT.CYAN, fontWeight: '700' }}>
                    {result.data.consensus.action.toUpperCase()}
                  </span>
                  {' • '}
                  Confidence: <span style={{ color: FINCEPT.YELLOW, fontWeight: '700' }}>
                    {(result.data.consensus.confidence * 100).toFixed(0)}%
                  </span>
                  {' • '}
                  Agreement: <span style={{ color: FINCEPT.GREEN, fontWeight: '700' }}>
                    {result.data.consensus.agreement}/{result.data.consensus.total_models}
                  </span>
                </div>
              </div>

              {/* Model Results */}
              <div>
                <div style={{ color: FINCEPT.ORANGE, fontSize: '8px', fontWeight: '700', marginBottom: '4px' }}>
                  MODEL DECISIONS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                  {result.data.results.map((r: any, idx: number) => (
                    <div
                      key={idx}
                      style={{
                        background: FINCEPT.PANEL_BG,
                        border: `1px solid ${FINCEPT.BORDER}`,
                        borderRadius: '2px',
                        padding: '4px 6px',
                        fontSize: '9px'
                      }}
                    >
                      <span style={{ color: FINCEPT.CYAN, fontWeight: '700' }}>
                        {r.model.split(':')[1] || r.model}:
                      </span>
                      {' '}
                      <span style={{ color: FINCEPT.WHITE }}>
                        {r.decision?.action?.toUpperCase() || 'ANALYZING...'}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          ) : result.type === 'trade_execution' ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {/* Trade Success Message */}
              {result.data.success && (
                <div style={{
                  background: `${FINCEPT.GREEN}15`,
                  border: `1px solid ${FINCEPT.GREEN}`,
                  borderRadius: '2px',
                  padding: '6px',
                  fontSize: '9px',
                  color: FINCEPT.GREEN
                }}>
                  [OK] Trade executed successfully (Paper Trading)
                  {result.data.trade_id && <span style={{ marginLeft: '6px' }}>ID: {result.data.trade_id}</span>}
                </div>
              )}

              {/* Warnings */}
              {result.data.warnings && result.data.warnings.length > 0 && (
                <div style={{
                  background: `${FINCEPT.YELLOW}15`,
                  border: `1px solid ${FINCEPT.YELLOW}`,
                  borderRadius: '2px',
                  padding: '6px'
                }}>
                  {result.data.warnings.map((warn: string, idx: number) => (
                    <div key={idx} style={{ fontSize: '8px', color: FINCEPT.YELLOW, marginBottom: '2px' }}>
                      [WARN] {warn}
                    </div>
                  ))}
                </div>
              )}

              {/* Trade Details */}
              {result.data.trade_data && (
                <div style={{
                  background: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  padding: '6px',
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: '4px',
                  fontSize: '9px'
                }}>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Symbol:</span>
                    <span style={{ color: FINCEPT.WHITE, fontWeight: '700', marginLeft: '4px' }}>
                      {result.data.trade_data.symbol}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Side:</span>
                    <span style={{
                      color: result.data.trade_data.side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED,
                      fontWeight: '700',
                      marginLeft: '4px',
                      textTransform: 'uppercase'
                    }}>
                      {result.data.trade_data.side}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Entry:</span>
                    <span style={{ color: FINCEPT.CYAN, fontWeight: '700', marginLeft: '4px' }}>
                      ${result.data.trade_data.entry_price?.toFixed(2)}
                    </span>
                  </div>
                  <div>
                    <span style={{ color: FINCEPT.GRAY }}>Quantity:</span>
                    <span style={{ color: FINCEPT.WHITE, fontWeight: '700', marginLeft: '4px' }}>
                      {result.data.trade_data.quantity}
                    </span>
                  </div>
                  {result.data.trade_data.stop_loss && (
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Stop Loss:</span>
                      <span style={{ color: FINCEPT.RED, fontWeight: '700', marginLeft: '4px' }}>
                        ${result.data.trade_data.stop_loss.toFixed(2)}
                      </span>
                    </div>
                  )}
                  {result.data.trade_data.take_profit && (
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Take Profit:</span>
                      <span style={{ color: FINCEPT.GREEN, fontWeight: '700', marginLeft: '4px' }}>
                        ${result.data.trade_data.take_profit.toFixed(2)}
                      </span>
                    </div>
                  )}
                </div>
              )}

              {/* Execution Time */}
              {result.data.execution_time_ms && (
                <div style={{ fontSize: '8px', color: FINCEPT.GRAY, textAlign: 'right' }}>
                  Executed in {result.data.execution_time_ms}ms
                </div>
              )}
            </div>
          ) : (
            <div style={{
              color: FINCEPT.WHITE,
              fontSize: '10px',
              lineHeight: '1.5',
              whiteSpace: 'pre-wrap',
              fontFamily: '"IBM Plex Mono", monospace'
            }}>
              {result.data.analysis || result.data.signal || result.data.risk_analysis}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
