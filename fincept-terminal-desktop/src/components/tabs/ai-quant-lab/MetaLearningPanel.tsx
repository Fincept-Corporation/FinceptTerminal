/**
 * Meta Learning Panel - AutoML, Model Selection & Ensemble
 * Connected to Python backend via Rust commands
 * Fincept Terminal Design - MAGENTA THEME
 */
import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Brain, Award, BarChart3, Layers, Play, Zap, TrendingUp, CheckCircle2,
  HelpCircle, X, Loader2, AlertCircle, Package, Target, Settings
} from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface ModelConfig {
  id: string;
  name: string;
  type: 'gradient_boosting' | 'neural_network' | 'ensemble';
  library: string;
  description: string;
  icon: any;
  helpText: string;
}

interface MetricResult {
  accuracy?: number;
  f1_score?: number;
  auc_roc?: number;
  mse?: number;
  rmse?: number;
  r2_score?: number;
}

interface ModelResult {
  model_key?: string;
  metrics: MetricResult;
  trained: boolean;
  error?: string;
}

interface RankingItem {
  model_id: string;
  model_key: string;
  score: number;
  metrics: MetricResult;
}

interface SelectionResult {
  success: boolean;
  results?: Record<string, ModelResult>;
  ranking?: RankingItem[];
  best_model?: string;
  trained_count?: number;
  task_type?: string;
  error?: string;
}

export function MetaLearningPanel() {
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  // Available models list
  const [availableModels, setAvailableModels] = useState<ModelConfig[]>([]);
  const [selectedModels, setSelectedModels] = useState<string[]>([]);
  const [isRunning, setIsRunning] = useState(false);
  const [selectionResult, setSelectionResult] = useState<SelectionResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [showHelp, setShowHelp] = useState(false);
  const [taskType, setTaskType] = useState<'classification' | 'regression'>('classification');

  // Load available models on mount
  useEffect(() => {
    loadAvailableModels();
  }, []);

  const loadAvailableModels = async () => {
    try {
      const result = await invoke<string>('qlib_meta_list_models');
      const data = JSON.parse(result);

      if (data.success && data.models) {
        // Map backend models to frontend with icons and help text
        const models: ModelConfig[] = data.models.map((m: any) => ({
          ...m,
          icon: getModelIcon(m.id),
          helpText: getModelHelpText(m.id)
        }));
        setAvailableModels(models);
      } else {
        setError(data.error || 'Failed to load models');
      }
    } catch (err: any) {
      setError(err.message || 'Failed to connect to backend');
    }
  };

  const getModelIcon = (modelId: string) => {
    switch (modelId) {
      case 'lightgbm': return Zap;
      case 'xgboost': return TrendingUp;
      case 'catboost': return BarChart3;
      case 'random_forest': return Layers;
      default: return Brain;
    }
  };

  const getModelHelpText = (modelId: string): string => {
    const helpTexts: Record<string, string> = {
      'lightgbm': 'Fast gradient boosting by Microsoft. Best for: Large datasets, speed-critical applications. Example: Predicting stock returns with 100+ features.',
      'xgboost': 'Extreme gradient boosting. Best for: High accuracy, structured data. Example: Credit risk modeling, fraud detection.',
      'catboost': 'Gradient boosting with categorical features. Best for: Data with categories (sectors, industries). Example: Sector rotation strategy.',
      'random_forest': 'Ensemble of decision trees. Best for: Interpretable models, feature importance. Example: Factor-based stock selection.'
    };
    return helpTexts[modelId] || 'Machine learning model for predictions';
  };

  const toggleModel = (modelId: string) => {
    setSelectedModels(prev =>
      prev.includes(modelId)
        ? prev.filter(id => id !== modelId)
        : [...prev, modelId]
    );
  };

  const runModelSelection = async () => {
    if (selectedModels.length === 0) return;

    setIsRunning(true);
    setError(null);

    try {
      const params = JSON.stringify({
        model_ids: selectedModels,
        task_type: taskType,
        metric: 'auto'
      });

      const result = await invoke<string>('qlib_meta_run_selection', { params });
      const data: SelectionResult = JSON.parse(result);

      if (data.success) {
        setSelectionResult(data);
      } else {
        setError(data.error || 'Model selection failed');
      }
    } catch (err: any) {
      setError(err.message || 'Failed to run model selection');
    } finally {
      setIsRunning(false);
    }
  };

  const resetSelection = () => {
    setSelectionResult(null);
    setSelectedModels([]);
    setError(null);
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

        {/* Task Type Selector */}
        <select
          value={taskType}
          onChange={(e) => setTaskType(e.target.value as 'classification' | 'regression')}
          disabled={isRunning || selectionResult !== null}
          style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '4px 8px',
            backgroundColor: colors.background,
            border: `1px solid ${colors.primary}`,
            color: colors.primary,
            cursor: isRunning || selectionResult ? 'not-allowed' : 'pointer',
            opacity: isRunning || selectionResult ? 0.5 : 1
          }}
        >
          <option value="classification">CLASSIFICATION</option>
          <option value="regression">REGRESSION</option>
        </select>

        {selectedModels.length > 0 && !selectionResult && (
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

        {selectionResult && selectionResult.best_model && (
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

        <button
          onClick={() => setShowHelp(true)}
          style={{
            padding: '4px 8px',
            backgroundColor: colors.background,
            border: `1px solid ${colors.primary}`,
            color: colors.primary,
            cursor: 'pointer',
            fontSize: '10px',
            fontFamily: 'monospace',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <HelpCircle size={12} />
          SHOW GUIDE
        </button>
      </div>

      {/* Error Banner */}
      {error && (
        <div style={{
          padding: '12px 16px',
          backgroundColor: '#EF444420',
          border: '1px solid #EF4444',
          borderLeft: '3px solid #EF4444',
          color: '#EF4444',
          fontSize: '11px',
          fontFamily: 'monospace',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <AlertCircle size={14} />
          {error}
          <button
            onClick={() => setError(null)}
            style={{
              marginLeft: 'auto',
              background: 'none',
              border: 'none',
              color: '#EF4444',
              cursor: 'pointer'
            }}
          >
            <X size={14} />
          </button>
        </div>
      )}

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

          {availableModels.length === 0 ? (
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              flexDirection: 'column',
              gap: '12px',
              padding: '20px'
            }}>
              <Package size={32} color={'var(--ft-border-color, #2A2A2A)'} />
              <div style={{
                fontSize: '11px',
                fontFamily: 'monospace',
                color: colors.text,
                opacity: 0.5,
                textAlign: 'center'
              }}>
                NO MODELS AVAILABLE
                <br />
                Install scikit-learn, lightgbm, xgboost, or catboost
              </div>
            </div>
          ) : (
            <>
              {/* Model List */}
              <div style={{ flex: 1, overflowY: 'auto', padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
                {availableModels.map((model, idx) => {
                  const isSelected = selectedModels.includes(model.id);
                  const Icon = model.icon;

                  return (
                    <div
                      key={model.id}
                      onClick={() => !selectionResult && toggleModel(model.id)}
                      style={{
                        padding: '12px',
                        backgroundColor: isSelected ? '#1F1F1F' : 'transparent',
                        border: `1px solid ${isSelected ? colors.primary : 'var(--ft-border-color, #2A2A2A)'}`,
                        borderTop: idx === 0 ? `1px solid ${isSelected ? colors.primary : 'var(--ft-border-color, #2A2A2A)'}` : '0',
                        cursor: selectionResult ? 'not-allowed' : 'pointer',
                        opacity: selectionResult ? 0.6 : 1,
                        transition: 'all 0.15s',
                        marginTop: idx === 0 ? '0' : '-1px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '10px'
                      }}
                      onMouseEnter={(e) => {
                        if (!isSelected && !selectionResult) {
                          e.currentTarget.style.backgroundColor = colors.background;
                          e.currentTarget.style.borderColor = colors.primary;
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (!isSelected && !selectionResult) {
                          e.currentTarget.style.backgroundColor = 'transparent';
                          e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                        }
                      }}
                      title={model.helpText}
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
                        {model.library.toUpperCase()}
                      </div>
                    </div>
                  );
                })}
              </div>

              {/* Action Buttons */}
              <div style={{ borderTop: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`, padding: '12px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
                <button
                  onClick={runModelSelection}
                  disabled={selectedModels.length === 0 || isRunning || selectionResult !== null}
                  style={{
                    width: '100%',
                    padding: '12px 16px',
                    backgroundColor: selectedModels.length > 0 && !selectionResult ? colors.primary : colors.background,
                    border: 'none',
                    color: selectedModels.length > 0 && !selectionResult ? '#000000' : colors.text,
                    opacity: selectedModels.length > 0 && !selectionResult && !isRunning ? 1 : 0.5,
                    fontSize: '11px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px',
                    cursor: selectedModels.length > 0 && !isRunning && !selectionResult ? 'pointer' : 'not-allowed',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '8px',
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedModels.length > 0 && !isRunning && !selectionResult) {
                      e.currentTarget.style.opacity = '0.9';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (selectedModels.length > 0 && !selectionResult) {
                      e.currentTarget.style.opacity = '1';
                    }
                  }}
                >
                  {isRunning ? (
                    <>
                      <Loader2 size={14} className="animate-spin" />
                      TRAINING MODELS...
                    </>
                  ) : (
                    <>
                      <Play size={14} />
                      RUN MODEL SELECTION
                    </>
                  )}
                </button>

                {selectionResult && (
                  <button
                    onClick={resetSelection}
                    style={{
                      width: '100%',
                      padding: '10px 16px',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.primary}`,
                      color: colors.primary,
                      fontSize: '10px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      letterSpacing: '0.5px',
                      cursor: 'pointer',
                      transition: 'all 0.15s'
                    }}
                  >
                    RESET SELECTION
                  </button>
                )}
              </div>
            </>
          )}
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
            {selectionResult && selectionResult.ranking && selectionResult.ranking.length > 0 ? (
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
                      {availableModels.find(m => m.id === selectionResult.best_model)?.name || selectionResult.best_model}
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
                  {selectionResult.ranking[0].metrics.accuracy !== undefined && (
                    <MetricCard
                      label="ACCURACY"
                      value={`${(selectionResult.ranking[0].metrics.accuracy * 100).toFixed(2)}%`}
                      colors={colors}
                    />
                  )}
                  {selectionResult.ranking[0].metrics.f1_score !== undefined && (
                    <MetricCard
                      label="F1 SCORE"
                      value={selectionResult.ranking[0].metrics.f1_score.toFixed(4)}
                      colors={colors}
                    />
                  )}
                  {selectionResult.ranking[0].metrics.auc_roc !== undefined && (
                    <MetricCard
                      label="AUC-ROC"
                      value={selectionResult.ranking[0].metrics.auc_roc.toFixed(4)}
                      colors={colors}
                    />
                  )}
                  {selectionResult.ranking[0].metrics.r2_score !== undefined && (
                    <MetricCard
                      label="R² SCORE"
                      value={selectionResult.ranking[0].metrics.r2_score.toFixed(4)}
                      colors={colors}
                    />
                  )}
                  {selectionResult.ranking[0].metrics.mse !== undefined && (
                    <MetricCard
                      label="MSE"
                      value={selectionResult.ranking[0].metrics.mse.toExponential(4)}
                      colors={colors}
                    />
                  )}
                  {selectionResult.ranking[0].metrics.rmse !== undefined && (
                    <MetricCard
                      label="RMSE"
                      value={selectionResult.ranking[0].metrics.rmse.toFixed(4)}
                      colors={colors}
                    />
                  )}
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
                    {selectionResult.ranking.map((item, idx) => {
                      const model = availableModels.find(m => m.id === item.model_id);
                      const isBest = idx === 0;

                      return (
                        <div
                          key={item.model_id}
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
                              {model?.name || item.model_id}
                            </span>
                          </div>
                          <div style={{
                            fontSize: '10px',
                            fontFamily: 'monospace',
                            fontWeight: 700,
                            color: isBest ? colors.success : colors.primary
                          }}>
                            {item.score.toFixed(4)}
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
                <Target size={32} color={'var(--ft-border-color, #2A2A2A)'} />
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

      {/* Help Modal */}
      {showHelp && (
        <HelpModal colors={colors} onClose={() => setShowHelp(false)} />
      )}
    </div>
  );
}

// Helper component for metric cards
function MetricCard({ label, value, colors }: { label: string; value: string; colors: any }) {
  return (
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
        {label}
      </div>
      <div style={{
        fontSize: '16px',
        fontWeight: 700,
        color: colors.primary,
        fontFamily: 'monospace'
      }}>
        {value}
      </div>
    </div>
  );
}

// Help Modal Component
function HelpModal({ colors, onClose }: { colors: any; onClose: () => void }) {
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
      padding: '20px'
    }}>
      <div style={{
        backgroundColor: colors.panel,
        border: `1px solid ${colors.primary}`,
        maxWidth: '800px',
        maxHeight: '80vh',
        width: '100%',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden'
      }}>
        {/* Header */}
        <div style={{
          padding: '16px',
          borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          display: 'flex',
          alignItems: 'center',
          gap: '12px'
        }}>
          <HelpCircle size={16} color={colors.primary} />
          <span style={{
            fontSize: '12px',
            fontWeight: 700,
            color: colors.primary,
            fontFamily: 'monospace',
            letterSpacing: '0.5px'
          }}>
            META LEARNING GUIDE
          </span>
          <button
            onClick={onClose}
            style={{
              marginLeft: 'auto',
              background: 'none',
              border: 'none',
              color: colors.text,
              cursor: 'pointer',
              padding: '4px'
            }}
          >
            <X size={16} />
          </button>
        </div>

        {/* Content */}
        <div style={{
          flex: 1,
          overflowY: 'auto',
          padding: '20px',
          fontSize: '11px',
          fontFamily: 'monospace',
          color: colors.text,
          lineHeight: '1.6'
        }}>
          <HelpSection
            title="What is Meta Learning & AutoML?"
            content="Meta Learning is 'learning how to learn' - the system automatically finds the best machine learning model for your task. AutoML automates the process of model selection, training, and evaluation so you don't need to manually test dozens of models."
            colors={colors}
          />

          <HelpSection
            title="Why Use Meta Learning in Finance?"
            content="Financial markets are complex and change constantly. Different models work better for different tasks:
• Stock price prediction might work best with XGBoost
• Volatility forecasting might prefer LightGBM
• Sector rotation could benefit from Random Forest

Meta Learning tests multiple models automatically and tells you which performs best, saving weeks of manual experimentation."
            colors={colors}
          />

          <HelpSection
            title="Real-World Use Cases"
            content="1. Portfolio Optimization
   Problem: Which stocks will outperform next quarter?
   Solution: Train 4 models, find LightGBM gives 85% accuracy vs 72% for others

2. Risk Management
   Problem: Predict which trades might lose money
   Solution: Compare models, discover CatBoost detects risky trades 20% better

3. Trading Signal Generation
   Problem: When to buy/sell based on 100+ indicators?
   Solution: AutoML finds Random Forest handles feature interactions best"
            colors={colors}
          />

          <HelpSection
            title="Understanding Model Types"
            content="GRADIENT BOOSTING MODELS (LightGBM, XGBoost, CatBoost):
• How: Build many weak models, each fixing previous mistakes
• Best for: High accuracy, handling lots of features
• Example: Predicting stock returns with 100+ technical indicators

ENSEMBLE MODELS (Random Forest):
• How: Average predictions from many decision trees
• Best for: Stable predictions, understanding feature importance
• Example: Stock screening - which features matter most?"
            colors={colors}
          />

          <HelpSection
            title="Key Metrics Explained"
            content="CLASSIFICATION (Predict categories: Buy/Sell/Hold):
• Accuracy: % of correct predictions (85% = correct 85% of time)
• F1 Score: Balance between precision and recall (0-1, higher better)
• AUC-ROC: Model's ability to distinguish classes (0-1, >0.8 is good)

REGRESSION (Predict numbers: stock price, returns):
• MSE/RMSE: Average prediction error (lower is better)
• R² Score: How well model explains data (0-1, >0.7 is good)"
            colors={colors}
          />

          <HelpSection
            title="How to Use This Panel"
            content="STEP 1: Choose Task Type
• Classification: Predicting categories (buy/sell, up/down)
• Regression: Predicting numbers (price, return, volatility)

STEP 2: Select Models
• Check 2-4 models to compare
• Read model descriptions for guidance
• More models = more comprehensive but slower

STEP 3: Run Model Selection
• System trains all selected models on synthetic data
• Shows which performs best
• See full ranking and metrics

STEP 4: Analyze Results
• Best model is highlighted
• Review metrics to understand performance
• Use insights for real trading strategies"
            colors={colors}
          />

          <HelpSection
            title="Tips for Beginners"
            content="• Start with 2-3 models to understand differences
• Classification tasks: Look for accuracy >70%, AUC >0.7
• Regression tasks: Look for R² >0.5, lower RMSE
• LightGBM is often fastest for experimentation
• XGBoost often gives best accuracy
• Random Forest gives most interpretable results

Remember: These results are on synthetic data. Real performance depends on your actual market data quality and strategy design."
            colors={colors}
          />
        </div>
      </div>
    </div>
  );
}

// Help Section Component
function HelpSection({ title, content, colors }: { title: string; content: string; colors: any }) {
  return (
    <div style={{ marginBottom: '20px' }}>
      <div style={{
        fontSize: '11px',
        fontWeight: 700,
        color: colors.primary,
        marginBottom: '8px',
        letterSpacing: '0.5px'
      }}>
        {title}
      </div>
      <div style={{
        fontSize: '10px',
        color: colors.text,
        opacity: 0.8,
        whiteSpace: 'pre-wrap'
      }}>
        {content}
      </div>
    </div>
  );
}
