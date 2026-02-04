/**
 * Meta Learning Panel - AutoML, Model Selection & Ensemble
 * Fincept Terminal Design - MAGENTA THEME
 */
import React, { useState } from 'react';
import { Brain, Award, BarChart3, Layers, Play, Zap, TrendingUp, CheckCircle2 } from 'lucide-react';

const FINCEPT = {
  MAGENTA: '#E91E8C',  // Primary theme color for Meta Learning
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  DARK_BG: '#0F0F0F',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F'
};

interface ModelConfig {
  id: string;
  name: string;
  type: 'gradient_boosting' | 'neural_network' | 'ensemble';
  description: string;
  icon: any;
}

export function MetaLearningPanel() {
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
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', backgroundColor: FINCEPT.DARK_BG }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Brain size={16} color={FINCEPT.MAGENTA} />
        <span style={{
          color: FINCEPT.MAGENTA,
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
            backgroundColor: FINCEPT.MAGENTA + '20',
            border: `1px solid ${FINCEPT.MAGENTA}`,
            color: FINCEPT.MAGENTA
          }}>
            {selectedModels.length} MODELS SELECTED
          </div>
        )}
        {bestModel && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: FINCEPT.GREEN + '20',
            border: `1px solid ${FINCEPT.GREEN}`,
            color: FINCEPT.GREEN
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
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.PANEL_BG,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.MAGENTA,
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
                    backgroundColor: isSelected ? FINCEPT.HOVER : 'transparent',
                    border: `1px solid ${isSelected ? FINCEPT.MAGENTA : FINCEPT.BORDER}`,
                    borderTop: idx === 0 ? `1px solid ${isSelected ? FINCEPT.MAGENTA : FINCEPT.BORDER}` : '0',
                    cursor: 'pointer',
                    transition: 'all 0.15s',
                    marginTop: idx === 0 ? '0' : '-1px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '10px'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                      e.currentTarget.style.borderColor = FINCEPT.MAGENTA;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }
                  }}
                >
                  {/* Checkbox */}
                  <div style={{
                    width: '16px',
                    height: '16px',
                    border: `1px solid ${isSelected ? FINCEPT.MAGENTA : FINCEPT.BORDER}`,
                    backgroundColor: isSelected ? FINCEPT.MAGENTA : 'transparent',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    transition: 'all 0.15s'
                  }}>
                    {isSelected && <CheckCircle2 size={12} color="#000000" />}
                  </div>

                  <Icon size={16} color={FINCEPT.MAGENTA} />

                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{
                      fontSize: '11px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      color: FINCEPT.WHITE,
                      marginBottom: '2px'
                    }}>
                      {model.name}
                    </div>
                    <div style={{
                      fontSize: '10px',
                      fontFamily: 'monospace',
                      color: FINCEPT.WHITE,
                      opacity: 0.6
                    }}>
                      {model.description}
                    </div>
                  </div>

                  <div style={{
                    fontSize: '9px',
                    fontFamily: 'monospace',
                    padding: '2px 6px',
                    backgroundColor: FINCEPT.MAGENTA + '20',
                    border: `1px solid ${FINCEPT.MAGENTA}`,
                    color: FINCEPT.MAGENTA
                  }}>
                    {model.type.replace('_', ' ').toUpperCase()}
                  </div>
                </div>
              );
            })}
          </div>

          {/* Action Button */}
          <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, padding: '12px' }}>
            <button
              onClick={runModelSelection}
              disabled={selectedModels.length === 0 || isRunning}
              style={{
                width: '100%',
                padding: '12px 16px',
                backgroundColor: selectedModels.length > 0 ? FINCEPT.MAGENTA : FINCEPT.DARK_BG,
                border: 'none',
                color: selectedModels.length > 0 ? '#000000' : FINCEPT.WHITE,
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
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG }}>
          <div style={{
            padding: '10px 16px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.MAGENTA,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: FINCEPT.PANEL_BG
          }}>
            AUTOML RESULTS
          </div>

          <div style={{ flex: 1, padding: '16px', overflowY: 'auto' }}>
            {bestModel ? (
              <div style={{
                padding: '16px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.GREEN}`,
                borderLeft: `3px solid ${FINCEPT.GREEN}`
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '12px' }}>
                  <div style={{
                    padding: '8px',
                    backgroundColor: FINCEPT.GREEN + '20',
                    border: `1px solid ${FINCEPT.GREEN}`
                  }}>
                    <Award size={20} color={FINCEPT.GREEN} />
                  </div>
                  <div>
                    <div style={{
                      fontSize: '10px',
                      fontFamily: 'monospace',
                      color: FINCEPT.WHITE,
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
                      color: FINCEPT.GREEN
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
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderLeft: `3px solid ${FINCEPT.MAGENTA}`
                  }}>
                    <div style={{
                      fontSize: '9px',
                      color: FINCEPT.WHITE,
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
                      color: FINCEPT.MAGENTA,
                      fontFamily: 'monospace'
                    }}>
                      85.42%
                    </div>
                  </div>

                  <div style={{
                    padding: '10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderLeft: `3px solid ${FINCEPT.MAGENTA}`
                  }}>
                    <div style={{
                      fontSize: '9px',
                      color: FINCEPT.WHITE,
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
                      color: FINCEPT.MAGENTA,
                      fontFamily: 'monospace'
                    }}>
                      0.8312
                    </div>
                  </div>

                  <div style={{
                    padding: '10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderLeft: `3px solid ${FINCEPT.MAGENTA}`
                  }}>
                    <div style={{
                      fontSize: '9px',
                      color: FINCEPT.WHITE,
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
                      color: FINCEPT.MAGENTA,
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
                    color: FINCEPT.MAGENTA,
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
                            backgroundColor: isBest ? FINCEPT.GREEN + '15' : FINCEPT.DARK_BG,
                            border: `1px solid ${isBest ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                            borderTop: idx === 0 ? `1px solid ${isBest ? FINCEPT.GREEN : FINCEPT.BORDER}` : '0',
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
                            color: FINCEPT.WHITE,
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
                              color: isBest ? FINCEPT.GREEN : FINCEPT.WHITE
                            }}>
                              {model.name}
                            </span>
                          </div>
                          <div style={{
                            fontSize: '10px',
                            fontFamily: 'monospace',
                            fontWeight: 700,
                            color: isBest ? FINCEPT.GREEN : FINCEPT.MAGENTA
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
                <Brain size={32} color={FINCEPT.BORDER} />
                <div style={{
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  color: FINCEPT.WHITE,
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
