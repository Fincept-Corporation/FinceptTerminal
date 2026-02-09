/**
 * Meta Learning Panel - AutoML, Model Selection & Ensemble
 * Fincept Terminal Design - MAGENTA THEME
 */
import React, { useState } from 'react';
import { Brain, Award, BarChart3, Layers, Play, Zap, TrendingUp, CheckCircle2 } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface ModelConfig {
  id: string;
  name: string;
  type: 'gradient_boosting' | 'neural_network' | 'ensemble';
  description: string;
  icon: any;
}

export function MetaLearningPanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const models: ModelConfig[] = [
    { id: 'lightgbm', name: 'LightGBM', type: 'gradient_boosting', description: 'Gradient boosting framework', icon: Zap },
    { id: 'xgboost', name: 'XGBoost', type: 'gradient_boosting', description: 'Extreme gradient boosting', icon: TrendingUp },
    { id: 'catboost', name: 'CatBoost', type: 'gradient_boosting', description: 'Categorical boosting', icon: BarChart3 },
    { id: 'lstm', name: 'LSTM', type: 'neural_network', description: 'Long short-term memory', icon: Brain },
    { id: 'transformer', name: 'Transformer', type: 'neural_network', description: 'Attention-based network', icon: Layers }
  ];

  const [selectedModels, setSelectedModels] = useState<string[]>([]);
  const [isRunning, setIsRunning] = useState(false);
  const [bestModel, setBestModel] = useState<string | null>(null);

  const toggleModel = (modelId: string) => {
    setSelectedModels(prev =>
      prev.includes(modelId)
        ? prev.filter(id => id !== modelId)
        : [...prev, modelId]
    );
  };

  const runModelSelection = () => {
    if (selectedModels.length === 0) return;
    setIsRunning(true);
    // Simulate model selection
    setTimeout(() => {
      setBestModel(selectedModels[Math.floor(Math.random() * selectedModels.length)]);
      setIsRunning(false);
    }, 2000);
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', backgroundColor: colors.background }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
        backgroundColor: colors.panel,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Brain size={16} color={colors.primary} />
        <span style={{
          color: colors.primary,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          META LEARNING & AUTOML
        </span>
        <div style={{ flex: 1 }} />
        {selectedModels.length > 0 && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: colors.primary + '20',
            border: `1px solid ${colors.primary}`,
            color: colors.primary
          }}>
            {selectedModels.length} MODELS SELECTED
          </div>
        )}
        {bestModel && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: colors.success + '20',
            border: `1px solid ${colors.success}`,
            color: colors.success
          }}>
            BEST MODEL FOUND
          </div>
        )}
      </div>

      {/* Main Content */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Left Panel - Model Selection */}
        <div style={{
          width: '400px',
          borderRight: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          backgroundColor: colors.panel,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.primary,
            fontFamily: 'monospace',
            letterSpacing: '0.5px'
          }}>
            SELECT MODELS FOR COMPARISON
          </div>

          {/* Model List - Joined Square Design */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
            {models.map((model, idx) => {
              const isSelected = selectedModels.includes(model.id);
              const Icon = model.icon;

              return (
                <div
                  key={model.id}
                  onClick={() => toggleModel(model.id)}
                  style={{
                    padding: '12px',
                    backgroundColor: isSelected ? '#1F1F1F' : 'transparent',
                    border: `1px solid ${isSelected ? colors.primary : 'var(--ft-border-color, #2A2A2A)'}`,
                    borderTop: idx === 0 ? `1px solid ${isSelected ? colors.primary : 'var(--ft-border-color, #2A2A2A)'}` : '0',
                    cursor: 'pointer',
                    transition: 'all 0.15s',
                    marginTop: idx === 0 ? '0' : '-1px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '10px'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = colors.background;
                      e.currentTarget.style.borderColor = colors.primary;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                    }
                  }}
                >
                  {/* Checkbox */}
                  <div style={{
                    width: '16px',
                    height: '16px',
                    border: `1px solid ${isSelected ? colors.primary : 'var(--ft-border-color, #2A2A2A)'}`,
                    backgroundColor: isSelected ? colors.primary : 'transparent',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    transition: 'all 0.15s'
                  }}>
                    {isSelected && <CheckCircle2 size={12} color="#000000" />}
                  </div>

                  <Icon size={16} color={colors.primary} />

                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{
                      fontSize: '11px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      color: colors.text,
                      marginBottom: '2px'
                    }}>
                      {model.name}
                    </div>
                    <div style={{
                      fontSize: '10px',
                      fontFamily: 'monospace',
                      color: colors.text,
                      opacity: 0.6
                    }}>
                      {model.description}
                    </div>
                  </div>

                  <div style={{
                    fontSize: '9px',
                    fontFamily: 'monospace',
                    padding: '2px 6px',
                    backgroundColor: colors.primary + '20',
                    border: `1px solid ${colors.primary}`,
                    color: colors.primary
                  }}>
                    {model.type.replace('_', ' ').toUpperCase()}
                  </div>
                </div>
              );
            })}
          </div>

          {/* Action Button */}
          <div style={{ borderTop: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`, padding: '12px' }}>
            <button
              onClick={runModelSelection}
              disabled={selectedModels.length === 0 || isRunning}
              style={{
                width: '100%',
                padding: '12px 16px',
                backgroundColor: selectedModels.length > 0 ? colors.primary : colors.background,
                border: 'none',
                color: selectedModels.length > 0 ? '#000000' : colors.text,
                opacity: selectedModels.length > 0 ? 1 : 0.5,
                fontSize: '11px',
                fontWeight: 700,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                cursor: selectedModels.length > 0 ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                transition: 'all 0.15s'
              }}
              onMouseEnter={(e) => {
                if (selectedModels.length > 0 && !isRunning) {
                  e.currentTarget.style.opacity = '0.9';
                }
              }}
              onMouseLeave={(e) => {
                if (selectedModels.length > 0) {
                  e.currentTarget.style.opacity = '1';
                }
              }}
            >
              <Play size={14} />
              {isRunning ? 'RUNNING AUTOML...' : 'RUN MODEL SELECTION'}
            </button>
          </div>
        </div>

        {/* Right Panel - Results */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', backgroundColor: colors.background }}>
          <div style={{
            padding: '10px 16px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.primary,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: colors.panel
          }}>
            AUTOML RESULTS
          </div>

          <div style={{ flex: 1, padding: '16px', overflowY: 'auto' }}>
            {bestModel ? (
              <div style={{
                padding: '16px',
                backgroundColor: colors.panel,
                border: `1px solid ${colors.success}`,
                borderLeft: `3px solid ${colors.success}`
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '12px' }}>
                  <div style={{
                    padding: '8px',
                    backgroundColor: colors.success + '20',
                    border: `1px solid ${colors.success}`
                  }}>
                    <Award size={20} color={colors.success} />
                  </div>
                  <div>
                    <div style={{
                      fontSize: '10px',
                      fontFamily: 'monospace',
                      color: colors.text,
                      opacity: 0.6,
                      marginBottom: '2px',
                      letterSpacing: '0.5px'
                    }}>
                      BEST PERFORMING MODEL
                    </div>
                    <div style={{
                      fontSize: '16px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      color: colors.success
                    }}>
                      {models.find(m => m.id === bestModel)?.name}
                    </div>
                  </div>
                </div>

                {/* Performance Metrics */}
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(3, 1fr)',
                  gap: '10px',
                  marginTop: '16px'
                }}>
                  <div style={{
                    padding: '10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    borderLeft: `3px solid ${colors.primary}`
                  }}>
                    <div style={{
                      fontSize: '9px',
                      color: colors.text,
                      opacity: 0.5,
                      marginBottom: '4px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px'
                    }}>
                      ACCURACY
                    </div>
                    <div style={{
                      fontSize: '16px',
                      fontWeight: 700,
                      color: colors.primary,
                      fontFamily: 'monospace'
                    }}>
                      85.42%
                    </div>
                  </div>

                  <div style={{
                    padding: '10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    borderLeft: `3px solid ${colors.primary}`
                  }}>
                    <div style={{
                      fontSize: '9px',
                      color: colors.text,
                      opacity: 0.5,
                      marginBottom: '4px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px'
                    }}>
                      F1 SCORE
                    </div>
                    <div style={{
                      fontSize: '16px',
                      fontWeight: 700,
                      color: colors.primary,
                      fontFamily: 'monospace'
                    }}>
                      0.8312
                    </div>
                  </div>

                  <div style={{
                    padding: '10px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    borderLeft: `3px solid ${colors.primary}`
                  }}>
                    <div style={{
                      fontSize: '9px',
                      color: colors.text,
                      opacity: 0.5,
                      marginBottom: '4px',
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px'
                    }}>
                      AUC-ROC
                    </div>
                    <div style={{
                      fontSize: '16px',
                      fontWeight: 700,
                      color: colors.primary,
                      fontFamily: 'monospace'
                    }}>
                      0.9124
                    </div>
                  </div>
                </div>

                {/* Model Ranking */}
                <div style={{ marginTop: '20px' }}>
                  <div style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: colors.primary,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px',
                    marginBottom: '10px'
                  }}>
                    MODEL RANKING
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '0' }}>
                    {selectedModels.map((modelId, idx) => {
                      const model = models.find(m => m.id === modelId);
                      if (!model) return null;
                      const isBest = modelId === bestModel;
                      const score = (0.85 - idx * 0.05).toFixed(4);

                      return (
                        <div
                          key={modelId}
                          style={{
                            padding: '8px 10px',
                            backgroundColor: isBest ? colors.success + '15' : colors.background,
                            border: `1px solid ${isBest ? colors.success : 'var(--ft-border-color, #2A2A2A)'}`,
                            borderTop: idx === 0 ? `1px solid ${isBest ? colors.success : 'var(--ft-border-color, #2A2A2A)'}` : '0',
                            marginTop: idx === 0 ? '0' : '-1px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '8px'
                          }}
                        >
                          <div style={{
                            fontSize: '11px',
                            fontWeight: 700,
                            fontFamily: 'monospace',
                            color: colors.text,
                            opacity: 0.5,
                            width: '24px'
                          }}>
                            #{idx + 1}
                          </div>
                          <div style={{ flex: 1 }}>
                            <span style={{
                              fontSize: '10px',
                              fontWeight: 700,
                              fontFamily: 'monospace',
                              color: isBest ? colors.success : colors.text
                            }}>
                              {model.name}
                            </span>
                          </div>
                          <div style={{
                            fontSize: '10px',
                            fontFamily: 'monospace',
                            fontWeight: 700,
                            color: isBest ? colors.success : colors.primary
                          }}>
                            {score}
                          </div>
                        </div>
                      );
                    })}
                  </div>
                </div>
              </div>
            ) : (
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                height: '100%',
                flexDirection: 'column',
                gap: '12px'
              }}>
                <Brain size={32} color={'var(--ft-border-color, #2A2A2A)'} />
                <div style={{
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  color: colors.text,
                  opacity: 0.5,
                  textAlign: 'center',
                  letterSpacing: '0.5px'
                }}>
                  SELECT MODELS AND RUN AUTOML
                  <br />
                  TO SEE COMPARISON RESULTS
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
