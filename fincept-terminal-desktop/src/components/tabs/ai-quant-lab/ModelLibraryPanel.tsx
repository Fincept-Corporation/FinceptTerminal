/**
 * Model Library Panel
 * Browse and use Qlib's pre-trained models
 */

import React, { useState, useEffect } from 'react';
import { Brain, Play, Download, Info, Star, TrendingUp } from 'lucide-react';
import { qlibService, type QlibModel } from '@/services/aiQuantLab/qlibService';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
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

  useEffect(() => {
    loadModels();
  }, []);

  const loadModels = async () => {
    setLoading(true);
    try {
      const response = await qlibService.listModels();
      if (response.success && response.models) {
        setModels(response.models);
        if (response.models.length > 0) {
          setSelectedModel(response.models[0]);
        }
      }
    } catch (error) {
      console.error('Failed to load models:', error);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="flex h-full">
      {/* Models List */}
      <div className="w-1/3 border-r overflow-auto" style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}>
        <div className="p-4 space-y-2">
          <h3 className="text-sm font-semibold mb-3 uppercase" style={{ color: BLOOMBERG.GRAY }}>
            PRE-TRAINED MODELS ({models.length})
          </h3>
          {models.map((model) => (
            <button
              key={model.id}
              onClick={() => setSelectedModel(model)}
              className="w-full p-4 rounded text-left transition-colors hover:bg-opacity-80"
              style={{
                backgroundColor: selectedModel?.id === model.id ? BLOOMBERG.HEADER_BG : BLOOMBERG.DARK_BG,
                border: selectedModel?.id === model.id ? `1px solid ${BLOOMBERG.ORANGE}` : `1px solid ${BLOOMBERG.BORDER}`
              }}
            >
              <div className="flex items-start justify-between mb-2">
                <span className="font-medium" style={{ color: BLOOMBERG.WHITE }}>
                  {model.name}
                </span>
                <span className="text-xs px-2 py-0.5 rounded uppercase" style={{
                  backgroundColor: model.type === 'tree_based' ? BLOOMBERG.GREEN : BLOOMBERG.ORANGE,
                  color: BLOOMBERG.DARK_BG
                }}>
                  {model.type}
                </span>
              </div>
              <p className="text-xs" style={{ color: BLOOMBERG.GRAY }}>
                {model.description}
              </p>
            </button>
          ))}
        </div>
      </div>

      {/* Model Details */}
      {selectedModel && (
        <div className="flex-1 overflow-auto p-6" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
          <div className="space-y-6">
            <div>
              <h2 className="text-2xl font-bold mb-2 uppercase" style={{ color: BLOOMBERG.WHITE }}>
                {selectedModel.name}
              </h2>
              <p style={{ color: BLOOMBERG.GRAY }}>{selectedModel.description}</p>
            </div>

            {/* Features */}
            <div>
              <h3 className="font-semibold mb-3 uppercase" style={{ color: BLOOMBERG.WHITE }}>Key Features</h3>
              <div className="grid grid-cols-2 gap-2">
                {selectedModel.features.map((feature, idx) => (
                  <div key={idx} className="flex items-center gap-2 p-3 rounded" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
                    <Star size={16} color={BLOOMBERG.ORANGE} />
                    <span className="text-sm" style={{ color: BLOOMBERG.WHITE }}>{feature}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Use Cases */}
            <div>
              <h3 className="font-semibold mb-3 uppercase" style={{ color: BLOOMBERG.WHITE }}>Use Cases</h3>
              <div className="space-y-2">
                {selectedModel.use_cases.map((useCase, idx) => (
                  <div key={idx} className="flex items-center gap-2 p-3 rounded" style={{ backgroundColor: BLOOMBERG.PANEL_BG }}>
                    <TrendingUp size={16} color={BLOOMBERG.GREEN} />
                    <span className="text-sm" style={{ color: BLOOMBERG.WHITE }}>{useCase}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Actions */}
            <div className="flex gap-3">
              <button
                className="flex-1 py-3 rounded font-semibold uppercase hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2"
                style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
              >
                <Play size={18} />
                Run Predictions
              </button>
              <button
                className="px-6 py-3 rounded hover:bg-opacity-80 transition-colors"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, color: BLOOMBERG.WHITE }}
              >
                <Info size={18} />
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
