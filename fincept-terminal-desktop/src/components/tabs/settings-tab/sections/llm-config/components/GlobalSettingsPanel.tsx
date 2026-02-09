import React from 'react';
import { Settings as SettingsIcon } from 'lucide-react';
import type { GlobalSettingsPanelProps } from '../types';
import { createStyles } from '../styles';

export function GlobalSettingsPanel({
  colors,
  llmGlobalSettings,
  setLlmGlobalSettings,
}: GlobalSettingsPanelProps) {
  const styles = createStyles(colors);

  const handleTemperatureChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setLlmGlobalSettings({
      ...llmGlobalSettings,
      temperature: parseFloat(e.target.value)
    });
  };

  const handleMaxTokensChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const value = e.target.value;
    if (value === '' || /^\d+$/.test(value)) {
      setLlmGlobalSettings({
        ...llmGlobalSettings,
        max_tokens: value === '' ? 2048 : parseInt(value)
      });
    }
  };

  const handleSystemPromptChange = (e: React.ChangeEvent<HTMLTextAreaElement>) => {
    setLlmGlobalSettings({
      ...llmGlobalSettings,
      system_prompt: e.target.value
    });
  };

  return (
    <div style={styles.panel}>
      <div style={{ ...styles.panelHeader, ...styles.sectionDivider, marginBottom: '20px' }}>
        <SettingsIcon size={16} color={colors.primary} />
        GLOBAL SETTINGS
      </div>

      <div style={styles.flexColumn}>
        {/* Temperature */}
        <div>
          <label style={styles.label}>TEMPERATURE (Creativity)</label>
          <div style={styles.sliderContainer}>
            <input
              type="range"
              min="0"
              max="2"
              step="0.1"
              value={llmGlobalSettings.temperature}
              onChange={handleTemperatureChange}
              style={{ flex: 1, height: '6px', cursor: 'pointer' }}
            />
            <div style={styles.sliderValue as React.CSSProperties}>
              {llmGlobalSettings.temperature.toFixed(1)}
            </div>
          </div>
          <div style={{ color: '#666', fontSize: '10px', marginTop: '6px' }}>
            Lower = More focused, Higher = More creative
          </div>
        </div>

        {/* Max Tokens */}
        <div>
          <label style={styles.label}>MAX TOKENS (Response Length)</label>
          <input
            type="text"
            inputMode="numeric"
            value={String(llmGlobalSettings.max_tokens)}
            onChange={handleMaxTokensChange}
            style={styles.input}
          />
        </div>

        {/* System Prompt */}
        <div>
          <label style={styles.label}>SYSTEM PROMPT (Optional)</label>
          <textarea
            value={llmGlobalSettings.system_prompt}
            onChange={handleSystemPromptChange}
            rows={4}
            placeholder="Custom instructions for the AI assistant..."
            style={styles.textarea as React.CSSProperties}
          />
        </div>
      </div>
    </div>
  );
}
