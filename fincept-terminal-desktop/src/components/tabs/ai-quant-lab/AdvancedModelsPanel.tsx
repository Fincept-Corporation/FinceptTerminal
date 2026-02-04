/**
 * Advanced Models Panel - Time-Series & Advanced Neural Networks
 * REFACTORED: Terminal-style UI with joined square design
 * Purple theme throughout - matches RL Trading style
 */

import React, { useState } from 'react';
import { Layers, Zap, Info, Play, FileText } from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  PURPLE: '#9D4EDD',
  MUTED: '#4A4A4A',
  HOVER: '#1F1F1F'
};

interface ModelInfo {
  id: string;
  name: string;
  category: string;
  desc: string;
  features: string[];
}

export function AdvancedModelsPanel() {
  const models: ModelInfo[] = [
    {
      id: 'lstm_ts',
      name: 'LSTM_TS',
      category: 'Time-Series',
      desc: 'LSTM with temporal features for sequential data',
      features: ['Long-term dependencies', 'Temporal patterns', 'Sequential modeling']
    },
    {
      id: 'gru_ts',
      name: 'GRU_TS',
      category: 'Time-Series',
      desc: 'Gated Recurrent Unit for time series',
      features: ['Faster than LSTM', 'Memory efficient', 'Good for shorter sequences']
    },
    {
      id: 'transformer_ts',
      name: 'Transformer_TS',
      category: 'Time-Series',
      desc: 'Transformer architecture for temporal data',
      features: ['Self-attention', 'Parallel processing', 'Long-range dependencies']
    },
    {
      id: 'localformer',
      name: 'Localformer',
      category: 'Attention',
      desc: 'Local attention mechanism for efficiency',
      features: ['Local attention', 'Reduced complexity', 'Efficient training']
    },
    {
      id: 'tcts',
      name: 'TCTS',
      category: 'Hybrid',
      desc: 'Temporal Convolutional Transformer System',
      features: ['CNN + Transformer', 'Multi-scale patterns', 'Hybrid approach']
    },
    {
      id: 'hist',
      name: 'HIST',
      category: 'Advanced',
      desc: 'Hierarchical Stock Transformer',
      features: ['Hierarchical structure', 'Multi-level features', 'Stock-specific']
    }
  ];

  const [selectedModel, setSelectedModel] = useState<ModelInfo>(models[0]);

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Layers size={16} color={FINCEPT.PURPLE} />
        <span style={{
          color: FINCEPT.PURPLE,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          ADVANCED NEURAL MODELS
        </span>
        <div style={{ flex: 1 }} />
        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: FINCEPT.PURPLE + '20',
          border: `1px solid ${FINCEPT.PURPLE}`,
          color: FINCEPT.PURPLE
        }}>
          {models.length} MODELS AVAILABLE
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Panel - Model Selection */}
        <div style={{
          width: '320px',
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.PURPLE,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: FINCEPT.PANEL_BG
          }}>
            SELECT MODEL ARCHITECTURE
          </div>

          {/* Model List - Joined Square Design */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
            {models.map((model, idx) => {
              const isSelected = selectedModel.id === model.id;
              return (
                <div
                  key={model.id}
                  onClick={() => setSelectedModel(model)}
                  style={{
                    padding: '12px',
                    backgroundColor: isSelected ? FINCEPT.HOVER : 'transparent',
                    border: `1px solid ${isSelected ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                    borderTop: idx === 0 ? `1px solid ${isSelected ? FINCEPT.PURPLE : FINCEPT.BORDER}` : '0',
                    cursor: 'pointer',
                    transition: 'all 0.15s',
                    marginTop: idx === 0 ? '0' : '-1px'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                      e.currentTarget.style.borderColor = FINCEPT.PURPLE;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
                    <span style={{
                      color: FINCEPT.WHITE,
                      fontSize: '12px',
                      fontWeight: 700,
                      fontFamily: 'monospace'
                    }}>
                      {model.name}
                    </span>
                    <div style={{
                      fontSize: '8px',
                      fontFamily: 'monospace',
                      padding: '2px 6px',
                      backgroundColor: FINCEPT.PURPLE + '30',
                      border: `1px solid ${FINCEPT.PURPLE}`,
                      color: FINCEPT.PURPLE
                    }}>
                      {model.category.toUpperCase()}
                    </div>
                  </div>
                  <div style={{
                    color: FINCEPT.WHITE,
                    opacity: 0.6,
                    fontSize: '10px',
                    lineHeight: '1.5',
                    fontFamily: 'monospace'
                  }}>
                    {model.desc}
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* Right Panel - Model Details */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          <div style={{
            padding: '10px 16px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.PURPLE,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: FINCEPT.PANEL_BG
          }}>
            MODEL DETAILS: {selectedModel.name}
          </div>

          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {/* Model Description */}
            <div style={{
              padding: '14px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderLeft: `3px solid ${FINCEPT.PURPLE}`,
              marginBottom: '16px'
            }}>
              <div style={{
                fontSize: '9px',
                color: FINCEPT.WHITE,
                opacity: 0.5,
                fontFamily: 'monospace',
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                DESCRIPTION
              </div>
              <div style={{
                color: FINCEPT.WHITE,
                fontSize: '11px',
                lineHeight: '1.6',
                fontFamily: 'monospace',
                opacity: 0.8
              }}>
                {selectedModel.desc}
              </div>
            </div>

            {/* Two Column Layout */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '16px' }}>
              {/* Key Features */}
              <div style={{
                padding: '14px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div style={{
                  fontSize: '10px',
                  color: FINCEPT.PURPLE,
                  fontFamily: 'monospace',
                  marginBottom: '12px',
                  fontWeight: 700,
                  letterSpacing: '0.5px'
                }}>
                  KEY FEATURES
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {selectedModel.features.map((feature, idx) => (
                    <div key={idx} style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
                      <div style={{
                        width: '4px',
                        height: '4px',
                        backgroundColor: FINCEPT.PURPLE,
                        marginTop: '6px',
                        flexShrink: 0
                      }} />
                      <span style={{
                        color: FINCEPT.WHITE,
                        fontSize: '10px',
                        lineHeight: '1.5',
                        fontFamily: 'monospace',
                        opacity: 0.8
                      }}>
                        {feature}
                      </span>
                    </div>
                  ))}
                </div>
              </div>

              {/* Architecture Info */}
              <div style={{
                padding: '14px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div style={{
                  fontSize: '10px',
                  color: FINCEPT.PURPLE,
                  fontFamily: 'monospace',
                  marginBottom: '12px',
                  fontWeight: 700,
                  letterSpacing: '0.5px'
                }}>
                  ARCHITECTURE INFO
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {[
                    { label: 'INPUT TYPE', value: 'Time-series' },
                    { label: 'OUTPUT', value: 'Predictions' },
                    { label: 'FRAMEWORK', value: 'PyTorch' },
                    { label: 'STATUS', value: 'AVAILABLE', color: FINCEPT.GREEN }
                  ].map((item, idx) => (
                    <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{
                        color: FINCEPT.WHITE,
                        opacity: 0.5,
                        fontSize: '9px',
                        fontFamily: 'monospace',
                        letterSpacing: '0.5px'
                      }}>
                        {item.label}
                      </span>
                      <span style={{
                        color: item.color || FINCEPT.WHITE,
                        fontSize: '10px',
                        fontFamily: 'monospace',
                        fontWeight: 600,
                        opacity: item.color ? 1 : 0.8
                      }}>
                        {item.value}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            </div>

            {/* Action Buttons */}
            <div style={{ display: 'flex', gap: '12px' }}>
              <button
                style={{
                  flex: 1,
                  padding: '12px 16px',
                  backgroundColor: FINCEPT.PURPLE,
                  border: 'none',
                  color: '#000000',
                  fontSize: '11px',
                  fontWeight: 700,
                  fontFamily: 'monospace',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                  letterSpacing: '0.5px',
                  transition: 'all 0.15s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.opacity = '0.9';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.opacity = '1';
                }}
              >
                <Play size={14} />
                TRAIN MODEL
              </button>
              <button
                style={{
                  flex: 1,
                  padding: '12px 16px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.PURPLE}`,
                  color: FINCEPT.PURPLE,
                  fontSize: '11px',
                  fontWeight: 700,
                  fontFamily: 'monospace',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                  letterSpacing: '0.5px',
                  transition: 'all 0.15s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = FINCEPT.PURPLE + '20';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                <FileText size={14} />
                DOCUMENTATION
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
