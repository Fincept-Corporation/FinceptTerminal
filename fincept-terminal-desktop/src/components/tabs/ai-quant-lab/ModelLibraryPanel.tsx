/**
 * Model Library Panel - Fincept Professional Design
 * Browse and use Qlib's 15+ pre-trained models with full backend integration
 * Real-world finance application with working predictions
 */

import React, { useState, useEffect } from 'react';
import {
  Brain,
  Play,
  Download,
  Info,
  Star,
  TrendingUp,
  RefreshCw,
  Settings,
  BarChart2,
  Database,
  Cpu,
  Target,
  Zap,
  CheckCircle2,
  AlertCircle,
  Calendar,
  Filter
} from 'lucide-react';
import { qlibService, type QlibModel } from '@/services/aiQuantLab/qlibService';

// Fincept Professional Color Palette
const FINCEPT = {
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

export function ModelLibraryPanel() {
  const [models, setModels] = useState<QlibModel[]>([]);
  const [selectedModel, setSelectedModel] = useState<QlibModel | null>(null);
  const [loading, setLoading] = useState(true);
  const [filterType, setFilterType] = useState<'all' | 'tree_based' | 'neural_network'>('all');
  const [isPredicting, setIsPredicting] = useState(false);
  const [predictionResult, setPredictionResult] = useState<any>(null);

  // Prediction settings
  const [instruments, setInstruments] = useState('AAPL,MSFT,GOOGL,AMZN,TSLA');
  const [startDate, setStartDate] = useState('2024-01-01');
  const [endDate, setEndDate] = useState('2024-12-31');

  useEffect(() => {
    loadModels();
  }, []);

  const loadModels = async () => {
    setLoading(true);
    try {
      const response = await qlibService.listModels();
      console.log('[Model Library] Models response:', response);

      if (response.success && response.models) {
        setModels(response.models);
        if (response.models.length > 0) {
          setSelectedModel(response.models[0]);
        }
      } else {
        console.error('[Model Library] Failed to load models:', response.error);
      }
    } catch (error) {
      console.error('[Model Library] Error loading models:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleRunPredictions = async () => {
    if (!selectedModel) return;

    setIsPredicting(true);
    setPredictionResult(null);

    try {
      const instrumentList = instruments.split(',').map(s => s.trim());

      const result = await qlibService.trainModel(
        selectedModel.id,
        {
          instruments: instrumentList,
          start_time: startDate,
          end_time: endDate
        }
      );

      console.log('[Model Library] Prediction result:', result);
      setPredictionResult(result);
    } catch (error) {
      console.error('[Model Library] Prediction error:', error);
      alert('Failed to run predictions: ' + error);
    } finally {
      setIsPredicting(false);
    }
  };

  const filteredModels = models.filter(model => {
    if (filterType === 'all') return true;
    return model.type === filterType;
  });

  return (
    <div className="flex h-full" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      {/* Left Panel - Model List */}
      <div
        className="w-96 border-r overflow-auto flex-shrink-0"
        style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
      >
        <div className="p-4">
          {/* Header */}
          <div className="flex items-center justify-between mb-4 pb-3 border-b" style={{ borderColor: FINCEPT.BORDER }}>
            <div>
              <div className="flex items-center gap-2 mb-1">
                <Brain size={18} color={FINCEPT.ORANGE} />
                <h2 className="text-sm font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                  MODEL LIBRARY
                </h2>
              </div>
              <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                {filteredModels.length} Pre-trained Models Available
              </p>
            </div>
            <button
              onClick={loadModels}
              disabled={loading}
              className="p-1.5 hover:bg-opacity-80 transition-colors"
              style={{ backgroundColor: 'transparent' }}
              title="Refresh models"
            >
              <RefreshCw size={14} color={FINCEPT.GRAY} className={loading ? 'animate-spin' : ''} />
            </button>
          </div>

          {/* Filter */}
          <div className="mb-4">
            <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
              <Filter size={12} className="inline mr-1" />
              Model Type
            </label>
            <select
              value={filterType}
              onChange={(e) => setFilterType(e.target.value as any)}
              className="w-full px-3 py-2 rounded text-xs font-mono outline-none uppercase"
              style={{
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`
              }}
            >
              <option value="all">All Models</option>
              <option value="tree_based">Tree-Based</option>
              <option value="neural_network">Neural Networks</option>
            </select>
          </div>

          {/* Models List */}
          <div className="space-y-2">
            {loading ? (
              <div className="text-center py-8">
                <RefreshCw size={32} color={FINCEPT.ORANGE} className="animate-spin mx-auto mb-2" />
                <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                  LOADING MODELS...
                </p>
              </div>
            ) : filteredModels.length === 0 ? (
              <div className="text-center py-8">
                <AlertCircle size={32} color={FINCEPT.GRAY} className="mx-auto mb-2" />
                <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                  No models found
                </p>
              </div>
            ) : (
              filteredModels.map((model) => (
                <button
                  key={model.id}
                  onClick={() => setSelectedModel(model)}
                  className="w-full p-3 rounded text-left transition-all hover:bg-opacity-80"
                  style={{
                    backgroundColor: selectedModel?.id === model.id ? FINCEPT.DARK_BG : FINCEPT.HEADER_BG,
                    border: selectedModel?.id === model.id ? `1px solid ${FINCEPT.ORANGE}` : `1px solid ${FINCEPT.BORDER}`
                  }}
                >
                  <div className="flex items-start justify-between mb-2">
                    <span className="font-bold text-xs uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                      {model.name}
                    </span>
                    <span
                      className="text-xs px-1.5 py-0.5 rounded uppercase font-bold"
                      style={{
                        backgroundColor: model.type === 'tree_based' ? FINCEPT.GREEN : FINCEPT.CYAN,
                        color: FINCEPT.DARK_BG
                      }}
                    >
                      {model.type === 'tree_based' ? 'TREE' : 'NEURAL'}
                    </span>
                  </div>
                  <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                    {model.description}
                  </p>
                </button>
              ))
            )}
          </div>
        </div>
      </div>

      {/* Right Panel - Model Details */}
      {selectedModel ? (
        <div className="flex-1 overflow-auto p-6" style={{ backgroundColor: FINCEPT.DARK_BG }}>
          <div className="space-y-6 max-w-5xl">
            {/* Header */}
            <div>
              <h2 className="text-xl font-bold mb-2 uppercase tracking-wide flex items-center gap-3" style={{ color: FINCEPT.WHITE }}>
                {selectedModel.name}
                <span className="text-sm px-3 py-1 rounded font-bold" style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}>
                  MODEL ID: {selectedModel.id}
                </span>
              </h2>
              <p className="font-mono text-sm" style={{ color: FINCEPT.GRAY }}>
                {selectedModel.description}
              </p>
            </div>

            {/* Model Type Badge */}
            <div className="flex items-center gap-3">
              <div
                className="flex items-center gap-2 px-4 py-2 rounded"
                style={{ backgroundColor: FINCEPT.PANEL_BG, borderLeft: `3px solid ${selectedModel.type === 'tree_based' ? FINCEPT.GREEN : FINCEPT.CYAN}` }}
              >
                <Cpu size={16} color={selectedModel.type === 'tree_based' ? FINCEPT.GREEN : FINCEPT.CYAN} />
                <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                  {selectedModel.type === 'tree_based' ? 'GRADIENT BOOSTING' : 'DEEP LEARNING'}
                </span>
              </div>
            </div>

            {/* Key Features */}
            <div>
              <h3 className="text-xs font-bold uppercase tracking-wide mb-3" style={{ color: FINCEPT.GRAY }}>
                <Star size={12} className="inline mr-1" />
                KEY FEATURES
              </h3>
              <div className="grid grid-cols-2 gap-2">
                {selectedModel.features.map((feature, idx) => (
                  <div
                    key={idx}
                    className="flex items-center gap-2 p-3 rounded border"
                    style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
                  >
                    <CheckCircle2 size={14} color={FINCEPT.GREEN} />
                    <span className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                      {feature}
                    </span>
                  </div>
                ))}
              </div>
            </div>

            {/* Use Cases */}
            <div>
              <h3 className="text-xs font-bold uppercase tracking-wide mb-3" style={{ color: FINCEPT.GRAY }}>
                <Target size={12} className="inline mr-1" />
                USE CASES
              </h3>
              <div className="space-y-2">
                {selectedModel.use_cases.map((useCase, idx) => (
                  <div
                    key={idx}
                    className="flex items-center gap-2 p-3 rounded border"
                    style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
                  >
                    <TrendingUp size={14} color={FINCEPT.ORANGE} />
                    <span className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                      {useCase}
                    </span>
                  </div>
                ))}
              </div>
            </div>

            {/* Prediction Configuration */}
            <div className="pt-4 border-t" style={{ borderColor: FINCEPT.BORDER }}>
              <h3 className="text-xs font-bold uppercase tracking-wide mb-4" style={{ color: FINCEPT.GRAY }}>
                <Settings size={12} className="inline mr-1" />
                PREDICTION CONFIGURATION
              </h3>

              <div className="space-y-4">
                {/* Instruments */}
                <div>
                  <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
                    Instruments (Comma-separated)
                  </label>
                  <input
                    type="text"
                    value={instruments}
                    onChange={(e) => setInstruments(e.target.value)}
                    placeholder="AAPL,MSFT,GOOGL"
                    className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                    style={{
                      backgroundColor: FINCEPT.PANEL_BG,
                      color: FINCEPT.WHITE,
                      border: `1px solid ${FINCEPT.BORDER}`
                    }}
                  />
                </div>

                {/* Date Range */}
                <div className="grid grid-cols-2 gap-3">
                  <div>
                    <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
                      <Calendar size={12} className="inline mr-1" />
                      Start Date
                    </label>
                    <input
                      type="date"
                      value={startDate}
                      onChange={(e) => setStartDate(e.target.value)}
                      className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                      style={{
                        backgroundColor: FINCEPT.PANEL_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`
                      }}
                    />
                  </div>
                  <div>
                    <label className="block text-xs font-bold mb-2 uppercase tracking-wide" style={{ color: FINCEPT.GRAY }}>
                      <Calendar size={12} className="inline mr-1" />
                      End Date
                    </label>
                    <input
                      type="date"
                      value={endDate}
                      onChange={(e) => setEndDate(e.target.value)}
                      className="w-full px-3 py-2 rounded text-xs font-mono outline-none"
                      style={{
                        backgroundColor: FINCEPT.PANEL_BG,
                        color: FINCEPT.WHITE,
                        border: `1px solid ${FINCEPT.BORDER}`
                      }}
                    />
                  </div>
                </div>
              </div>
            </div>

            {/* Prediction Results */}
            {predictionResult && (
              <div
                className="p-4 rounded border"
                style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.GREEN }}
              >
                <div className="flex items-center gap-2 mb-3">
                  <CheckCircle2 size={16} color={FINCEPT.GREEN} />
                  <h3 className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                    PREDICTION RESULTS
                  </h3>
                </div>
                <pre
                  className="text-xs font-mono overflow-x-auto"
                  style={{ color: FINCEPT.WHITE }}
                >
                  {JSON.stringify(predictionResult, null, 2)}
                </pre>
              </div>
            )}

            {/* Action Buttons */}
            <div className="flex gap-3 pt-4 border-t" style={{ borderColor: FINCEPT.BORDER }}>
              <button
                onClick={handleRunPredictions}
                disabled={isPredicting || !instruments || !startDate || !endDate}
                className="flex-1 py-3 rounded font-bold uppercase text-sm tracking-wide hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed"
                style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
              >
                {isPredicting ? (
                  <>
                    <RefreshCw size={16} className="animate-spin" />
                    RUNNING PREDICTIONS...
                  </>
                ) : (
                  <>
                    <Play size={16} />
                    RUN PREDICTIONS
                  </>
                )}
              </button>
              <button
                className="px-6 py-3 rounded font-bold uppercase text-sm tracking-wide hover:bg-opacity-80 transition-colors flex items-center justify-center gap-2"
                style={{ backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.WHITE }}
              >
                <Info size={16} />
                DETAILS
              </button>
            </div>
          </div>
        </div>
      ) : (
        <div className="flex-1 flex items-center justify-center" style={{ backgroundColor: FINCEPT.DARK_BG }}>
          <div className="text-center max-w-md">
            <Brain size={64} color={FINCEPT.GRAY} className="mx-auto mb-4" />
            <h3 className="text-base font-bold uppercase tracking-wide mb-2" style={{ color: FINCEPT.WHITE }}>
              NO MODEL SELECTED
            </h3>
            <p className="text-sm font-mono" style={{ color: FINCEPT.GRAY }}>
              Select a model from the library to view details and run predictions
            </p>
          </div>
        </div>
      )}
    </div>
  );
}
