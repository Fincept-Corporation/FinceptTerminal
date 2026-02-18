/**
 * Advanced Models Panel - Time-Series & Advanced Neural Networks
 * Connected to Python backend via Rust commands
 * Purple theme - REFACTORED with full functionality
 */

import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Layers, Zap, Info, Play, FileText, Plus, HelpCircle, X, Loader2,
  AlertCircle, TrendingUp, Brain, CheckCircle2, BarChart3
} from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface ModelInfo {
  id: string;
  name: string;
  category: string;
  description: string;
  features: string[];
  use_cases: string[];
  available: boolean;
}

interface ModelConfig {
  input_size?: number;
  hidden_size?: number;
  num_layers?: number;
  dropout?: number;
  nhead?: number;
}

interface CreatedModel {
  model_id: string;
  type: string;
  created_at: string;
  trained: boolean;
  epochs_trained: number;
}

interface TrainingResult {
  success: boolean;
  model_id?: string;
  epochs_trained?: number;
  total_epochs?: number;
  final_loss?: number;
  loss_history?: number[];
  error?: string;
}

export function AdvancedModelsPanel() {
  const { colors } = useTerminalTheme();

  // State
  const [availableModels, setAvailableModels] = useState<ModelInfo[]>([]);
  const [selectedModel, setSelectedModel] = useState<ModelInfo | null>(null);
  const [createdModels, setCreatedModels] = useState<CreatedModel[]>([]);
  const [currentModelId, setCurrentModelId] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [isLoadingModels, setIsLoadingModels] = useState(true);
  const [isTraining, setIsTraining] = useState(false);
  const [trainingResult, setTrainingResult] = useState<TrainingResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [showHelp, setShowHelp] = useState(false);
  const [showConfig, setShowConfig] = useState(false);

  // Config
  const [config, setConfig] = useState<ModelConfig>({
    input_size: 10,
    hidden_size: 64,
    num_layers: 2,
    dropout: 0.2
  });

  // Load available models on mount
  useEffect(() => {
    loadAvailableModels();
    loadCreatedModels();
  }, []);

  const loadAvailableModels = async () => {
    setIsLoadingModels(true);
    try {
      const result = await invoke<string>('qlib_advanced_list_models');
      const data = JSON.parse(result);

      if (data.success && data.models) {
        setAvailableModels(data.models);
        if (data.models.length > 0) {
          setSelectedModel(data.models[0]);
        }
      }
    } catch (err: any) {
      setError(err.message || 'Failed to load models');
    } finally {
      setIsLoadingModels(false);
    }
  };

  const loadCreatedModels = async () => {
    try {
      const result = await invoke<string>('qlib_advanced_list_created');
      const data = JSON.parse(result);

      if (data.success && data.models) {
        setCreatedModels(data.models);
      }
    } catch (err: any) {
      console.error('Failed to load created models:', err);
    }
  };

  const handleCreateModel = async () => {
    if (!selectedModel) return;

    setIsLoading(true);
    setError(null);

    try {
      const params = JSON.stringify({
        model_type: selectedModel.id,
        config: config
      });

      const result = await invoke<string>('qlib_advanced_create_model', { params });
      const data = JSON.parse(result);

      if (data.success) {
        setCurrentModelId(data.model_id);
        await loadCreatedModels();
        setShowConfig(false);
      } else {
        setError(data.error || 'Failed to create model');
      }
    } catch (err: any) {
      setError(err.message || 'Failed to create model');
    } finally {
      setIsLoading(false);
    }
  };

  const handleTrainModel = async () => {
    if (!currentModelId) return;

    setIsTraining(true);
    setError(null);

    try {
      const params = JSON.stringify({
        epochs: 10,
        batch_size: 32,
        learning_rate: 0.001
      });

      const result = await invoke<string>('qlib_advanced_train_model', {
        modelId: currentModelId,
        params
      });
      const data: TrainingResult = JSON.parse(result);

      if (data.success) {
        setTrainingResult(data);
        await loadCreatedModels();
      } else {
        setError(data.error || 'Training failed');
      }
    } catch (err: any) {
      setError(err.message || 'Failed to train model');
    } finally {
      setIsTraining(false);
    }
  };

  const resetModel = () => {
    setCurrentModelId(null);
    setTrainingResult(null);
    setShowConfig(false);
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: colors.background
    }}>
      {/* Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
        backgroundColor: colors.panel,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Layers size={16} color={colors.purple} />
        <span style={{
          color: colors.purple,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          ADVANCED NEURAL MODELS
        </span>
        <div style={{ flex: 1 }} />

        {currentModelId && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: colors.success + '20',
            border: `1px solid ${colors.success}`,
            color: colors.success
          }}>
            MODEL CREATED
          </div>
        )}

        {trainingResult && trainingResult.success && (
          <div style={{
            fontSize: '10px',
            fontFamily: 'monospace',
            padding: '3px 8px',
            backgroundColor: colors.primary + '20',
            border: `1px solid ${colors.primary}`,
            color: colors.primary
          }}>
            TRAINED
          </div>
        )}

        <button
          onClick={() => setShowHelp(true)}
          style={{
            padding: '4px 8px',
            backgroundColor: colors.background,
            border: `1px solid ${colors.purple}`,
            color: colors.purple,
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

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Panel - Model Selection */}
        <div style={{
          width: '320px',
          borderRight: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.purple,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: colors.panel
          }}>
            SELECT MODEL ARCHITECTURE
          </div>

          {/* Model List */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
            {isLoadingModels ? (
              <div style={{
                flex: 1,
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '12px',
                padding: '40px 20px'
              }}>
                <Loader2 size={32} color={colors.purple} className="animate-spin" />
                <div style={{
                  fontSize: '11px',
                  fontFamily: 'monospace',
                  color: colors.purple,
                  fontWeight: 600,
                  letterSpacing: '0.5px'
                }}>
                  LOADING MODELS...
                </div>
                <div style={{
                  fontSize: '9px',
                  fontFamily: 'monospace',
                  color: colors.text,
                  opacity: 0.6,
                  textAlign: 'center',
                  maxWidth: '200px',
                  lineHeight: '1.5'
                }}>
                  Initializing PyTorch models (LSTM, GRU, Transformer)
                </div>
              </div>
            ) : availableModels.map((model, idx) => {
              const isSelected = selectedModel?.id === model.id;
              return (
                <div
                  key={model.id}
                  onClick={() => !currentModelId && setSelectedModel(model)}
                  style={{
                    padding: '12px',
                    backgroundColor: isSelected ? '#1F1F1F' : 'transparent',
                    border: `1px solid ${isSelected ? colors.purple : 'var(--ft-border-color, #2A2A2A)'}`,
                    borderTop: idx === 0 ? `1px solid ${isSelected ? colors.purple : 'var(--ft-border-color, #2A2A2A)'}` : '0',
                    cursor: currentModelId ? 'not-allowed' : 'pointer',
                    opacity: currentModelId ? 0.6 : 1,
                    transition: 'all 0.15s',
                    marginTop: idx === 0 ? '0' : '-1px'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected && !currentModelId) {
                      e.currentTarget.style.backgroundColor = colors.background;
                      e.currentTarget.style.borderColor = colors.purple;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected && !currentModelId) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                    }
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
                    <span style={{
                      color: colors.text,
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
                      backgroundColor: colors.purple + '30',
                      border: `1px solid ${colors.purple}`,
                      color: colors.purple
                    }}>
                      {model.category.toUpperCase()}
                    </div>
                  </div>
                  <div style={{
                    color: colors.text,
                    opacity: 0.6,
                    fontSize: '10px',
                    lineHeight: '1.5',
                    fontFamily: 'monospace'
                  }}>
                    {model.description}
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* Right Panel - Model Details & Training */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          <div style={{
            padding: '10px 16px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.purple,
            fontFamily: 'monospace',
            letterSpacing: '0.5px',
            backgroundColor: colors.panel
          }}>
            {currentModelId ? `TRAINING: ${currentModelId}` : selectedModel ? `DETAILS: ${selectedModel.name}` : 'SELECT A MODEL'}
          </div>

          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {isLoadingModels ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '100%',
                gap: '16px'
              }}>
                <Loader2 size={40} color={colors.purple} className="animate-spin" />
                <div style={{
                  fontSize: '12px',
                  fontFamily: 'monospace',
                  color: colors.purple,
                  fontWeight: 700,
                  letterSpacing: '0.5px'
                }}>
                  INITIALIZING NEURAL NETWORKS
                </div>
                <div style={{
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  color: colors.text,
                  opacity: 0.6,
                  textAlign: 'center',
                  maxWidth: '300px',
                  lineHeight: '1.6'
                }}>
                  Loading PyTorch framework and building model architectures...
                  <br />
                  This may take a few seconds on first load.
                </div>
              </div>
            ) : selectedModel && !currentModelId && (
              <>
                {/* Model Info */}
                <div style={{
                  padding: '14px',
                  backgroundColor: colors.panel,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  borderLeft: `3px solid ${colors.purple}`,
                  marginBottom: '16px'
                }}>
                  <div style={{
                    fontSize: '9px',
                    color: colors.text,
                    opacity: 0.5,
                    fontFamily: 'monospace',
                    marginBottom: '6px',
                    letterSpacing: '0.5px'
                  }}>
                    DESCRIPTION
                  </div>
                  <div style={{
                    color: colors.text,
                    fontSize: '11px',
                    lineHeight: '1.6',
                    fontFamily: 'monospace',
                    opacity: 0.8
                  }}>
                    {selectedModel.description}
                  </div>
                </div>

                {/* Two Column Layout */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '16px' }}>
                  {/* Features */}
                  <div style={{
                    padding: '14px',
                    backgroundColor: colors.panel,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                  }}>
                    <div style={{
                      fontSize: '10px',
                      color: colors.purple,
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
                          <CheckCircle2 size={12} color={colors.purple} style={{ marginTop: '2px', flexShrink: 0 }} />
                          <span style={{
                            color: colors.text,
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

                  {/* Use Cases */}
                  <div style={{
                    padding: '14px',
                    backgroundColor: colors.panel,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`
                  }}>
                    <div style={{
                      fontSize: '10px',
                      color: colors.purple,
                      fontFamily: 'monospace',
                      marginBottom: '12px',
                      fontWeight: 700,
                      letterSpacing: '0.5px'
                    }}>
                      USE CASES IN FINANCE
                    </div>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                      {selectedModel.use_cases.map((useCase, idx) => (
                        <div key={idx} style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
                          <TrendingUp size={12} color={colors.success} style={{ marginTop: '2px', flexShrink: 0 }} />
                          <span style={{
                            color: colors.text,
                            fontSize: '10px',
                            lineHeight: '1.5',
                            fontFamily: 'monospace',
                            opacity: 0.8
                          }}>
                            {useCase}
                          </span>
                        </div>
                      ))}
                    </div>
                  </div>
                </div>

                {/* Action Buttons */}
                <div style={{ display: 'flex', gap: '12px' }}>
                  <button
                    onClick={() => setShowConfig(true)}
                    disabled={isLoading}
                    style={{
                      flex: 1,
                      padding: '12px 16px',
                      backgroundColor: colors.purple,
                      border: 'none',
                      color: '#000000',
                      fontSize: '11px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      cursor: isLoading ? 'not-allowed' : 'pointer',
                      opacity: isLoading ? 0.5 : 1,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '8px',
                      letterSpacing: '0.5px',
                      transition: 'all 0.15s'
                    }}
                    onMouseEnter={(e) => !isLoading && (e.currentTarget.style.opacity = '0.9')}
                    onMouseLeave={(e) => !isLoading && (e.currentTarget.style.opacity = '1')}
                  >
                    {isLoading ? (
                      <>
                        <Loader2 size={14} className="animate-spin" />
                        CREATING...
                      </>
                    ) : (
                      <>
                        <Plus size={14} />
                        CREATE MODEL
                      </>
                    )}
                  </button>
                  <button
                    onClick={() => setShowHelp(true)}
                    style={{
                      flex: 1,
                      padding: '12px 16px',
                      backgroundColor: 'transparent',
                      border: `1px solid ${colors.purple}`,
                      color: colors.purple,
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
                    onMouseEnter={(e) => e.currentTarget.style.backgroundColor = colors.purple + '20'}
                    onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                  >
                    <FileText size={14} />
                    DOCUMENTATION
                  </button>
                </div>
              </>
            )}

            {currentModelId && (
              <>
                {/* Training Section */}
                <div style={{
                  padding: '16px',
                  backgroundColor: colors.panel,
                  border: `1px solid ${colors.purple}`,
                  borderLeft: `3px solid ${colors.purple}`,
                  marginBottom: '16px'
                }}>
                  <div style={{
                    fontSize: '11px',
                    fontWeight: 700,
                    color: colors.purple,
                    fontFamily: 'monospace',
                    marginBottom: '12px',
                    letterSpacing: '0.5px'
                  }}>
                    MODEL ID: {currentModelId}
                  </div>
                  <div style={{
                    fontSize: '10px',
                    color: colors.text,
                    opacity: 0.7,
                    fontFamily: 'monospace',
                    lineHeight: '1.6'
                  }}>
                    Model created successfully. Train with synthetic data or connect your own dataset.
                  </div>
                </div>

                {trainingResult && trainingResult.success && (
                  <div style={{
                    padding: '16px',
                    backgroundColor: colors.panel,
                    border: `1px solid ${colors.success}`,
                    borderLeft: `3px solid ${colors.success}`,
                    marginBottom: '16px'
                  }}>
                    <div style={{
                      fontSize: '10px',
                      fontWeight: 700,
                      color: colors.success,
                      fontFamily: 'monospace',
                      marginBottom: '12px',
                      letterSpacing: '0.5px'
                    }}>
                      TRAINING COMPLETED
                    </div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                      <div>
                        <div style={{
                          fontSize: '9px',
                          color: colors.text,
                          opacity: 0.5,
                          fontFamily: 'monospace',
                          marginBottom: '4px'
                        }}>
                          EPOCHS TRAINED
                        </div>
                        <div style={{
                          fontSize: '14px',
                          fontWeight: 700,
                          color: colors.primary,
                          fontFamily: 'monospace'
                        }}>
                          {trainingResult.epochs_trained}
                        </div>
                      </div>
                      <div>
                        <div style={{
                          fontSize: '9px',
                          color: colors.text,
                          opacity: 0.5,
                          fontFamily: 'monospace',
                          marginBottom: '4px'
                        }}>
                          FINAL LOSS
                        </div>
                        <div style={{
                          fontSize: '14px',
                          fontWeight: 700,
                          color: colors.success,
                          fontFamily: 'monospace'
                        }}>
                          {trainingResult.final_loss?.toFixed(6)}
                        </div>
                      </div>
                    </div>

                    {trainingResult.loss_history && trainingResult.loss_history.length > 0 && (
                      <div style={{ marginTop: '12px' }}>
                        <div style={{
                          fontSize: '9px',
                          color: colors.text,
                          opacity: 0.5,
                          fontFamily: 'monospace',
                          marginBottom: '8px'
                        }}>
                          LOSS TREND
                        </div>
                        <div style={{
                          display: 'flex',
                          gap: '2px',
                          height: '40px',
                          alignItems: 'flex-end'
                        }}>
                          {trainingResult.loss_history.slice(0, 20).map((loss, idx) => {
                            const maxLoss = Math.max(...trainingResult.loss_history!);
                            const height = (loss / maxLoss) * 100;
                            return (
                              <div
                                key={idx}
                                style={{
                                  flex: 1,
                                  backgroundColor: colors.success,
                                  height: `${height}%`,
                                  minHeight: '2px'
                                }}
                                title={`Epoch ${idx + 1}: ${loss.toFixed(6)}`}
                              />
                            );
                          })}
                        </div>
                      </div>
                    )}
                  </div>
                )}

                {/* Action Buttons */}
                <div style={{ display: 'flex', gap: '12px' }}>
                  <button
                    onClick={handleTrainModel}
                    disabled={isTraining}
                    style={{
                      flex: 1,
                      padding: '12px 16px',
                      backgroundColor: colors.purple,
                      border: 'none',
                      color: '#000000',
                      fontSize: '11px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      cursor: isTraining ? 'not-allowed' : 'pointer',
                      opacity: isTraining ? 0.5 : 1,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '8px',
                      letterSpacing: '0.5px',
                      transition: 'all 0.15s'
                    }}
                    onMouseEnter={(e) => !isTraining && (e.currentTarget.style.opacity = '0.9')}
                    onMouseLeave={(e) => !isTraining && (e.currentTarget.style.opacity = '1')}
                  >
                    {isTraining ? (
                      <>
                        <Loader2 size={14} className="animate-spin" />
                        TRAINING...
                      </>
                    ) : (
                      <>
                        <Play size={14} />
                        TRAIN MODEL
                      </>
                    )}
                  </button>
                  <button
                    onClick={resetModel}
                    disabled={isTraining}
                    style={{
                      padding: '12px 16px',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.purple}`,
                      color: colors.purple,
                      fontSize: '10px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      cursor: isTraining ? 'not-allowed' : 'pointer',
                      opacity: isTraining ? 0.5 : 1,
                      letterSpacing: '0.5px',
                      transition: 'all 0.15s'
                    }}
                  >
                    RESET
                  </button>
                </div>
              </>
            )}
          </div>
        </div>
      </div>

      {/* Config Modal */}
      {showConfig && (
        <ConfigModal
          config={config}
          onChange={setConfig}
          onConfirm={handleCreateModel}
          onCancel={() => setShowConfig(false)}
          colors={colors}
          isLoading={isLoading}
        />
      )}

      {/* Help Modal */}
      {showHelp && (
        <HelpModal colors={colors} onClose={() => setShowHelp(false)} />
      )}
    </div>
  );
}

// Config Modal Component
function ConfigModal({
  config,
  onChange,
  onConfirm,
  onCancel,
  colors,
  isLoading
}: {
  config: ModelConfig;
  onChange: (config: ModelConfig) => void;
  onConfirm: () => void;
  onCancel: () => void;
  colors: any;
  isLoading: boolean;
}) {
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
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: colors.panel,
        border: `1px solid ${colors.purple}`,
        width: '500px',
        maxHeight: '80vh',
        display: 'flex',
        flexDirection: 'column'
      }}>
        {/* Header */}
        <div style={{
          padding: '16px',
          borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          display: 'flex',
          alignItems: 'center',
          gap: '12px'
        }}>
          <Brain size={16} color={colors.purple} />
          <span style={{
            fontSize: '12px',
            fontWeight: 700,
            color: colors.purple,
            fontFamily: 'monospace',
            letterSpacing: '0.5px'
          }}>
            MODEL CONFIGURATION
          </span>
          <button
            onClick={onCancel}
            disabled={isLoading}
            style={{
              marginLeft: 'auto',
              background: 'none',
              border: 'none',
              color: colors.text,
              cursor: isLoading ? 'not-allowed' : 'pointer',
              padding: '4px'
            }}
          >
            <X size={16} />
          </button>
        </div>

        {/* Content */}
        <div style={{
          padding: '20px',
          fontSize: '11px',
          fontFamily: 'monospace',
          color: colors.text
        }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            {Object.entries(config).map(([key, value]) => (
              <div key={key}>
                <label style={{
                  display: 'block',
                  fontSize: '10px',
                  color: colors.purple,
                  marginBottom: '6px',
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>
                  {key.replace('_', ' ')}
                </label>
                <input
                  type="number"
                  value={value}
                  onChange={(e) => onChange({ ...config, [key]: parseFloat(e.target.value) })}
                  disabled={isLoading}
                  style={{
                    width: '100%',
                    padding: '8px 12px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: colors.text,
                    fontSize: '11px',
                    fontFamily: 'monospace'
                  }}
                />
              </div>
            ))}
          </div>
        </div>

        {/* Footer */}
        <div style={{
          borderTop: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          padding: '16px',
          display: 'flex',
          gap: '12px'
        }}>
          <button
            onClick={onConfirm}
            disabled={isLoading}
            style={{
              flex: 1,
              padding: '10px 16px',
              backgroundColor: colors.purple,
              border: 'none',
              color: '#000000',
              fontSize: '11px',
              fontWeight: 700,
              fontFamily: 'monospace',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              opacity: isLoading ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px'
            }}
          >
            {isLoading ? (
              <>
                <Loader2 size={14} className="animate-spin" />
                CREATING...
              </>
            ) : (
              'CONFIRM'
            )}
          </button>
          <button
            onClick={onCancel}
            disabled={isLoading}
            style={{
              padding: '10px 16px',
              backgroundColor: colors.background,
              border: `1px solid ${colors.purple}`,
              color: colors.purple,
              fontSize: '11px',
              fontWeight: 700,
              fontFamily: 'monospace',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              opacity: isLoading ? 0.5 : 1
            }}
          >
            CANCEL
          </button>
        </div>
      </div>
    </div>
  );
}

// Help Modal Component (truncated for length - will add comprehensive docs)
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
        border: `1px solid ${colors.purple}`,
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
          <HelpCircle size={16} color={colors.purple} />
          <span style={{
            fontSize: '12px',
            fontWeight: 700,
            color: colors.purple,
            fontFamily: 'monospace',
            letterSpacing: '0.5px'
          }}>
            ADVANCED MODELS GUIDE
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
            title="What are Advanced Neural Models?"
            content="Deep learning models specialized for financial time-series data. Unlike traditional ML, these models learn temporal patterns and long-term dependencies in sequential data like stock prices, returns, and market indicators."
            colors={colors}
          />

          <HelpSection
            title="Why Use Neural Networks in Finance?"
            content="Financial markets have complex patterns that change over time:
• Stock prices depend on historical patterns (momentum, mean reversion)
• Market regimes shift (bull to bear markets)
• Multiple timeframes matter (intraday vs long-term trends)
• Non-linear relationships between features

Neural networks excel at discovering these hidden patterns automatically."
            colors={colors}
          />

          <HelpSection
            title="Model Types Explained"
            content="LSTM (Long Short-Term Memory):
• Best for: Sequential patterns with long-term dependencies
• Example: Predicting next month's price using past 6 months
• Strength: Remembers important events far in the past
• Use when: You have clear sequential data (daily prices, volumes)

GRU (Gated Recurrent Unit):
• Best for: Faster training on shorter sequences
• Example: Intraday trading signals using last 20 bars
• Strength: 30% faster than LSTM with similar accuracy
• Use when: Speed matters more than long-term memory

Transformer:
• Best for: Multi-asset correlations and regime detection
• Example: Portfolio optimization across 50 stocks
• Strength: Processes all data in parallel, finds relationships
• Use when: You have many correlated time series

LSTM + Attention:
• Best for: Focus on specific events (earnings, news)
• Example: Trading around major announcements
• Strength: Model explains which periods matter most
• Use when: Interpretability is important"
            colors={colors}
          />

          <HelpSection
            title="Real-World Example: Stock Price Prediction"
            content="PROBLEM: Predict if AAPL will go up/down tomorrow

TRADITIONAL APPROACH:
• Calculate 20+ indicators (RSI, MACD, Bollinger Bands)
• Try multiple ML models manually
• Feature engineering takes weeks
• Accuracy: 55-60%

NEURAL NETWORK APPROACH:
• Feed raw price/volume sequences to LSTM
• Model learns patterns automatically
• Train once, reuse for any stock
• Accuracy: 65-70%

STEPS:
1. Select LSTM model
2. Create with default config (works for most cases)
3. Train on historical data (10 epochs = 5 minutes)
4. Model learns: support/resistance, momentum, volatility patterns
5. Deploy for live trading"
            colors={colors}
          />

          <HelpSection
            title="Configuration Parameters"
            content="INPUT_SIZE: Number of features per timestep
• Example: 10 = [open, high, low, close, volume, RSI, MACD, BB_upper, BB_lower, ATR]
• Default 10 works for most cases

HIDDEN_SIZE: Model capacity (neurons per layer)
• 32 = Simple patterns, fast training
• 64 = Standard (default) - balanced
• 128 = Complex patterns, slower but more accurate
• Rule: Use 64 unless accuracy is insufficient

NUM_LAYERS: Network depth
• 1 = Too simple for finance
• 2 = Default - works for most tasks
• 3-4 = Very complex patterns, risk overfitting
• Rule: Start with 2, only increase if underfitting

DROPOUT: Regularization to prevent overfitting
• 0.0 = No regularization (will overfit)
• 0.2 = Default - balanced
• 0.5 = Heavy regularization (use if overfitting)
• Rule: Keep at 0.2 unless training loss << test loss"
            colors={colors}
          />

          <HelpSection
            title="Training Process"
            content="When you click TRAIN MODEL:
1. Generates 1000 synthetic samples (for demo)
2. Splits into batches of 32
3. Trains for 10 epochs (~30 iterations)
4. Shows loss decreasing = model learning
5. Final loss < 0.1 = good convergence

LOSS INTERPRETATION:
• Loss > 1.0: Model not learning, check data
• Loss 0.1-1.0: Normal early training
• Loss 0.01-0.1: Good convergence
• Loss < 0.01: Excellent, risk of overfitting

For real trading:
• Replace synthetic data with your actual market data
• Train 20-50 epochs depending on data size
• Monitor train vs validation loss
• Save best model, deploy for predictions"
            colors={colors}
          />

          <HelpSection
            title="Tips for Beginners"
            content="START SIMPLE:
• Use LSTM with default config
• Train for 10 epochs first
• Check if loss decreases
• Only tune parameters if needed

AVOID OVERFITTING:
• More data > bigger model
• Monitor: if train loss << val loss = overfitting
• Solutions: more data, smaller model, higher dropout

PRACTICAL WORKFLOW:
1. Collect historical price data (>1000 samples)
2. Create LSTM model with defaults
3. Train and check loss converges
4. Backtest on unseen data
5. If performance insufficient, try:
   - More data (most effective)
   - More epochs (10→20)
   - Bigger model (hidden_size 64→128)
   - Different architecture (LSTM→Transformer)

REALISTIC EXPECTATIONS:
• 55% accuracy = random, model not working
• 60% accuracy = decent, tradeable with good risk mgmt
• 70% accuracy = excellent, rare
• 80%+ accuracy = likely overfitted, verify on new data"
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
        color: colors.purple,
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
