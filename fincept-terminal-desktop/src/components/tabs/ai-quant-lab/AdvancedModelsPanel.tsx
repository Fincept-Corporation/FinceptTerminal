/**
 * Advanced Models Panel - Time-Series & Advanced Neural Networks
 * Showcase for LSTM_TS, Transformer_TS, Localformer, TCTS, HIST, KRNN
 * Fincept Professional Design
 */

import React, { useState } from 'react';
import { Layers, TrendingUp, Zap, BarChart3, Brain, Info } from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  PURPLE: '#9D4EDD',
  MUTED: '#4A4A4A'
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
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center space-x-3">
        <div className="p-2 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
          <Layers size={24} style={{ color: FINCEPT.PURPLE }} />
        </div>
        <div>
          <h2 className="text-xl font-bold" style={{ color: FINCEPT.WHITE }}>
            Advanced Models
          </h2>
          <p className="text-sm" style={{ color: FINCEPT.MUTED }}>
            Time-series & advanced neural network architectures
          </p>
        </div>
      </div>

      {/* Model Grid */}
      <div className="grid grid-cols-3 gap-4">
        {models.map(model => (
          <button
            key={model.id}
            onClick={() => setSelectedModel(model)}
            className="p-4 rounded-lg text-left transition-all"
            style={{
              backgroundColor: selectedModel.id === model.id ? FINCEPT.ORANGE + '30' : FINCEPT.PANEL_BG,
              border: `2px solid ${selectedModel.id === model.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`
            }}
          >
            <div className="font-bold mb-1" style={{ color: FINCEPT.WHITE }}>
              {model.name}
            </div>
            <div className="text-xs mb-2 px-2 py-1 rounded inline-block" style={{ backgroundColor: FINCEPT.ORANGE + '30', color: FINCEPT.ORANGE }}>
              {model.category}
            </div>
            <div className="text-xs mt-2" style={{ color: FINCEPT.MUTED }}>
              {model.desc}
            </div>
          </button>
        ))}
      </div>

      {/* Model Details */}
      <div className="p-6 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
        <div className="flex items-center justify-between mb-4">
          <h3 className="font-bold" style={{ color: FINCEPT.ORANGE }}>
            Model Details: {selectedModel.name}
          </h3>
          <div className="px-3 py-1 rounded-lg text-sm" style={{ backgroundColor: FINCEPT.PURPLE + '30', color: FINCEPT.PURPLE }}>
            {selectedModel.category}
          </div>
        </div>

        <p className="mb-4" style={{ color: FINCEPT.GRAY }}>
          {selectedModel.desc}
        </p>

        <div className="grid grid-cols-2 gap-6 mb-6">
          <div>
            <h4 className="text-sm font-bold mb-2" style={{ color: FINCEPT.ORANGE }}>
              Key Features
            </h4>
            <ul className="space-y-2">
              {selectedModel.features.map((feature, idx) => (
                <li key={idx} className="flex items-start space-x-2 text-sm">
                  <div className="w-1.5 h-1.5 rounded-full mt-1.5" style={{ backgroundColor: FINCEPT.PURPLE }} />
                  <span style={{ color: FINCEPT.WHITE }}>{feature}</span>
                </li>
              ))}
            </ul>
          </div>

          <div>
            <h4 className="text-sm font-bold mb-2" style={{ color: FINCEPT.ORANGE }}>
              Architecture Info
            </h4>
            <div className="space-y-2 text-sm">
              <div className="flex justify-between">
                <span style={{ color: FINCEPT.GRAY }}>Input Type:</span>
                <span style={{ color: FINCEPT.WHITE }}>Time-series</span>
              </div>
              <div className="flex justify-between">
                <span style={{ color: FINCEPT.GRAY }}>Output:</span>
                <span style={{ color: FINCEPT.WHITE }}>Predictions</span>
              </div>
              <div className="flex justify-between">
                <span style={{ color: FINCEPT.GRAY }}>Framework:</span>
                <span style={{ color: FINCEPT.WHITE }}>PyTorch</span>
              </div>
              <div className="flex justify-between">
                <span style={{ color: FINCEPT.GRAY }}>Training:</span>
                <span style={{ color: FINCEPT.GREEN }}>Available</span>
              </div>
            </div>
          </div>
        </div>

        <div className="flex space-x-4">
          <button
            className="flex-1 flex items-center justify-center space-x-2 px-6 py-3 rounded-lg font-semibold"
            style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.WHITE }}
          >
            <Zap size={16} />
            <span>Train Model</span>
          </button>
          <button
            className="flex-1 flex items-center justify-center space-x-2 px-6 py-3 rounded-lg font-semibold"
            style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.WHITE }}
          >
            <Info size={16} />
            <span>View Documentation</span>
          </button>
        </div>
      </div>
    </div>
  );
}
