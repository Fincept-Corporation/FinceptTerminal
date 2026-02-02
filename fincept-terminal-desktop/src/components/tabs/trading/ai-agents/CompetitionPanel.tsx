/**
 * Competition Panel - NOF1 Alpha Arena Style
 * Ultra-simple: Select models → Start → Watch them trade
 */

import React, { useState, useEffect } from 'react';
import { Play, Square } from 'lucide-react';
import { agnoTradingService } from '../../../../services/trading/agnoTradingService';

const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  BG: '#0F0F0F',
  BORDER: '#2A2A2A'
};

interface Model {
  value: string;
  label: string;
}

export function CompetitionPanel({ selectedSymbol }: { selectedSymbol: string }) {
  const [models, setModels] = useState<Model[]>([]);
  const [selected, setSelected] = useState<string[]>([]);
  const [isRunning, setIsRunning] = useState(false);
  const [teamId, setTeamId] = useState<string | null>(null);
  const [price, setPrice] = useState(67000);
  const [error, setError] = useState<string | null>(null);

  // Load models from database
  useEffect(() => {
    (async () => {
      try {
        const { sqliteService } = await import('../../../../services/core/sqliteService');
        const configs = await sqliteService.getLLMConfigs();
        const modelList = configs
          .filter(c => c.api_key?.trim())
          .map(c => ({
            value: `${c.provider}:${c.model}`,
            label: `${c.provider} - ${c.model}`
          }));
        setModels(modelList);
        setSelected(modelList.slice(0, 2).map(m => m.value)); // Auto-select first 2
      } catch (err) {
        setError('Failed to load models. Configure API keys in Settings.');
      }
    })();
  }, []);

  // Start competition
  const start = async () => {
    if (selected.length < 2) {
      setError('Select at least 2 models');
      return;
    }

    try {
      setError(null);

      // Create team
      const res = await agnoTradingService.createCompetition(
        'Competition',
        selected,
        'trading'
      );

      if (!res.success) throw new Error(res.error);

      const tid = res.data!.team_id;
      setTeamId(tid);
      setIsRunning(true);

      // Start price feed via Rust WebSocket
      const { websocketBridge, configureProvider } = await import('../../../../services/trading/websocketBridge');

      await configureProvider('kraken', 'wss://ws.kraken.com/v2');
      await websocketBridge.connect('kraken');
      await websocketBridge.subscribe('kraken', selectedSymbol, 'ticker');

      await websocketBridge.onTicker(async (data) => {
        if (data.provider !== 'kraken' || data.symbol !== selectedSymbol.replace('/', '')) return;
        if (!isRunning) return;
        setPrice(data.price);

        try {
          await agnoTradingService.runCompetition(tid, selectedSymbol, 'signal');
        } catch (e) {
          console.error('[Competition] Trade error:', e);
        }
      });

    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
      setIsRunning(false);
    }
  };

  // Stop competition
  const stop = async () => {
    setIsRunning(false);
    const { websocketBridge } = await import('../../../../services/trading/websocketBridge');
    await websocketBridge.unsubscribe('kraken', selectedSymbol, 'ticker');
  };

  return (
    <div style={{
      background: COLORS.BG,
      border: `1px solid ${COLORS.BORDER}`,
      padding: '20px',
      borderRadius: '4px',
      height: '100%'
    }}>
      {/* Header */}
      <div style={{ marginBottom: '20px' }}>
        <h2 style={{ color: COLORS.ORANGE, fontSize: '16px', marginBottom: '8px' }}>
          AI TRADING COMPETITION
        </h2>
        <div style={{ color: COLORS.GRAY, fontSize: '12px' }}>
          {selectedSymbol} · ${price.toLocaleString()}
        </div>
      </div>

      {error && (
        <div style={{
          background: '#FF3B3B20',
          border: `1px solid ${COLORS.RED}`,
          padding: '10px',
          borderRadius: '4px',
          color: COLORS.RED,
          fontSize: '12px',
          marginBottom: '20px'
        }}>
          {error}
        </div>
      )}

      {/* Model Selection */}
      <div style={{ marginBottom: '20px' }}>
        <div style={{ color: COLORS.WHITE, fontSize: '13px', marginBottom: '10px' }}>
          SELECT MODELS (minimum 2)
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          {models.map(m => (
            <label key={m.value} style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '8px',
              background: selected.includes(m.value) ? '#FF880020' : 'transparent',
              border: `1px solid ${selected.includes(m.value) ? COLORS.ORANGE : COLORS.BORDER}`,
              borderRadius: '4px',
              cursor: 'pointer',
              fontSize: '12px',
              color: COLORS.WHITE
            }}>
              <input
                type="checkbox"
                checked={selected.includes(m.value)}
                onChange={(e) => {
                  if (e.target.checked) {
                    setSelected([...selected, m.value]);
                  } else {
                    setSelected(selected.filter(s => s !== m.value));
                  }
                }}
                disabled={isRunning}
              />
              {m.label}
            </label>
          ))}
        </div>
      </div>

      {/* Start/Stop Button */}
      <button
        onClick={isRunning ? stop : start}
        disabled={!isRunning && selected.length < 2}
        style={{
          width: '100%',
          padding: '12px',
          background: isRunning ? COLORS.RED : COLORS.GREEN,
          color: '#000',
          border: 'none',
          borderRadius: '4px',
          fontSize: '14px',
          fontWeight: '700',
          cursor: selected.length >= 2 || isRunning ? 'pointer' : 'not-allowed',
          opacity: selected.length >= 2 || isRunning ? 1 : 0.5,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '8px'
        }}
      >
        {isRunning ? <Square size={16} /> : <Play size={16} />}
        {isRunning ? 'STOP COMPETITION' : 'START COMPETITION'}
      </button>

      {isRunning && (
        <div style={{
          marginTop: '20px',
          padding: '12px',
          background: '#00D66F20',
          border: `1px solid ${COLORS.GREEN}`,
          borderRadius: '4px',
          color: COLORS.GREEN,
          fontSize: '12px',
          textAlign: 'center'
        }}>
          LIVE · Models trading at ${price.toLocaleString()}
        </div>
      )}
    </div>
  );
}
