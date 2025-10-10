import React from 'react';
import { X, Settings as SettingsIcon } from 'lucide-react';

interface LLMSettingsModalProps {
  isOpen: boolean;
  onClose: () => void;
  onSave?: () => void;
  onNavigateToSettings?: () => void;
}

const LLMSettingsModal: React.FC<LLMSettingsModalProps> = ({ isOpen, onClose, onNavigateToSettings }) => {
  // Bloomberg colors
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';

  if (!isOpen) return null;

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.8)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000,
      fontFamily: 'Consolas, monospace'
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        borderRadius: '4px',
        width: '90%',
        maxWidth: '600px',
        boxShadow: '0 4px 20px rgba(255, 165, 0, 0.3)',
        padding: '24px'
      }}>
        {/* Header */}
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '24px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <SettingsIcon size={20} color={BLOOMBERG_ORANGE} />
            <span style={{ color: BLOOMBERG_ORANGE, fontSize: '16px', fontWeight: 'bold' }}>
              LLM CONFIGURATION
            </span>
          </div>
          <button
            onClick={onClose}
            style={{
              backgroundColor: 'transparent',
              border: 'none',
              color: '#ff0000',
              cursor: 'pointer',
              padding: '4px'
            }}
          >
            <X size={20} />
          </button>
        </div>

        {/* Content */}
        <div style={{ padding: '32px', textAlign: 'center' }}>
          <div style={{
            marginBottom: '24px',
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            gap: '16px'
          }}>
            <SettingsIcon size={48} color={BLOOMBERG_ORANGE} />
            <h3 style={{ color: BLOOMBERG_WHITE, fontSize: '18px', fontWeight: 'bold', margin: 0 }}>
              Configure LLM Settings
            </h3>
            <p style={{ color: '#888', fontSize: '12px', maxWidth: '400px', lineHeight: '1.6', margin: 0 }}>
              LLM configuration has been moved to the Settings screen for a unified configuration experience.
              Please navigate to the Settings tab to configure your AI providers.
            </p>
          </div>

          <div style={{
            background: BLOOMBERG_DARK_BG,
            border: '1px solid #2a2a2a',
            borderRadius: '4px',
            padding: '16px',
            marginBottom: '24px'
          }}>
            <div style={{ color: '#888', fontSize: '11px', lineHeight: '1.6', textAlign: 'left' }}>
              <div style={{ marginBottom: '8px', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                What you can configure in Settings:
              </div>
              <ul style={{ margin: 0, paddingLeft: '20px' }}>
                <li>Select active LLM provider (OpenAI, Gemini, DeepSeek, Ollama, OpenRouter)</li>
                <li>Configure API keys and base URLs</li>
                <li>Set model names</li>
                <li>Adjust temperature and max tokens</li>
                <li>Customize system prompts</li>
              </ul>
            </div>
          </div>

          <div style={{ display: 'flex', gap: '12px', justifyContent: 'center' }}>
            <button
              onClick={() => {
                onClose();
                onNavigateToSettings?.();
              }}
              style={{
                backgroundColor: BLOOMBERG_ORANGE,
                color: 'black',
                border: 'none',
                padding: '12px 24px',
                fontSize: '12px',
                fontWeight: 'bold',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '8px'
              }}
            >
              <SettingsIcon size={16} />
              GO TO SETTINGS TAB
            </button>

            <button
              onClick={onClose}
              style={{
                backgroundColor: BLOOMBERG_DARK_BG,
                color: BLOOMBERG_WHITE,
                border: '1px solid #2a2a2a',
                padding: '12px 24px',
                fontSize: '12px',
                fontWeight: 'bold',
                cursor: 'pointer'
              }}
            >
              CANCEL
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default LLMSettingsModal;
