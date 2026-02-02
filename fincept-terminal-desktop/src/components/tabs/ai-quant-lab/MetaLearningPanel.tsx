/**
 * Meta Learning Panel - AutoML, Model Selection & Ensemble
 * Fincept Professional Design
 */
import React, { useState } from 'react';
import { Brain, Award, BarChart3, Layers, Play } from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', GREEN: '#00D66F', GRAY: '#787878',
  DARK_BG: '#000000', PANEL_BG: '#0F0F0F', BORDER: '#2A2A2A', PURPLE: '#9D4EDD', MUTED: '#4A4A4A'
};

export function MetaLearningPanel() {
  const [models] = useState(['LightGBM', 'XGBoost', 'CatBoost', 'LSTM', 'Transformer']);
  const [selectedModels, setSelectedModels] = useState<string[]>([]);
  const [bestModel, setBestModel] = useState<string | null>(null);

  return (
    <div className="space-y-6">
      <div className="flex items-center space-x-3">
        <div className="p-2 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
          <Brain size={24} style={{ color: FINCEPT.PURPLE }} />
        </div>
        <div>
          <h2 className="text-xl font-bold" style={{ color: FINCEPT.WHITE }}>Meta Learning & AutoML</h2>
          <p className="text-sm" style={{ color: FINCEPT.MUTED }}>Automated model selection and ensemble optimization</p>
        </div>
      </div>

      <div className="grid grid-cols-2 gap-6">
        <div className="p-6 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <h3 className="font-bold mb-4" style={{ color: FINCEPT.ORANGE }}>Model Selection</h3>
          <div className="space-y-2">
            {models.map(model => (
              <label key={model} className="flex items-center space-x-2 p-3 rounded-lg cursor-pointer" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                <input type="checkbox" className="form-checkbox" style={{ accentColor: FINCEPT.ORANGE }} onChange={(e) => e.target.checked ? setSelectedModels([...selectedModels, model]) : setSelectedModels(selectedModels.filter(m => m !== model))} />
                <span style={{ color: FINCEPT.WHITE }}>{model}</span>
              </label>
            ))}
          </div>
          <button className="w-full mt-4 px-6 py-3 rounded-lg font-semibold" style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.WHITE }}>
            <Play size={16} className="inline mr-2" />Run Model Selection
          </button>
        </div>

        <div className="p-6 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <h3 className="font-bold mb-4" style={{ color: FINCEPT.ORANGE }}>Results</h3>
          {bestModel ? (
            <div className="p-4 rounded-lg" style={{ backgroundColor: FINCEPT.GREEN + '20', border: `1px solid ${FINCEPT.GREEN}` }}>
              <div className="flex items-center space-x-2 mb-2">
                <Award size={20} style={{ color: FINCEPT.GREEN }} />
                <span className="font-bold" style={{ color: FINCEPT.GREEN }}>Best Model</span>
              </div>
              <div className="text-2xl font-bold" style={{ color: FINCEPT.WHITE }}>{bestModel}</div>
              <div className="text-sm mt-2" style={{ color: FINCEPT.MUTED }}>Score: 0.8542</div>
            </div>
          ) : (
            <div className="text-center py-20" style={{ color: FINCEPT.MUTED }}>Run model selection to see results</div>
          )}
        </div>
      </div>
    </div>
  );
}
