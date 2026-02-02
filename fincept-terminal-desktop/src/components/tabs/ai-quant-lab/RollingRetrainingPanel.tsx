/**
 * Rolling Retraining Panel - Automated Model Retraining
 * Fincept Professional Design
 */
import React, { useState } from 'react';
import { RefreshCw, Calendar, Clock, CheckCircle2, Play } from 'lucide-react';

const FINCEPT = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', GREEN: '#00D66F', GRAY: '#787878',
  DARK_BG: '#000000', PANEL_BG: '#0F0F0F', BORDER: '#2A2A2A', CYAN: '#00E5FF', MUTED: '#4A4A4A'
};

export function RollingRetrainingPanel() {
  const [frequency, setFrequency] = useState('daily');
  const [schedules] = useState([
    { model: 'Model_A', frequency: 'daily', lastRun: '2026-02-02 10:00', status: 'completed' },
    { model: 'Model_B', frequency: 'weekly', lastRun: '2026-02-01 14:30', status: 'completed' }
  ]);

  return (
    <div className="space-y-6">
      <div className="flex items-center space-x-3">
        <div className="p-2 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
          <RefreshCw size={24} style={{ color: FINCEPT.CYAN }} />
        </div>
        <div>
          <h2 className="text-xl font-bold" style={{ color: FINCEPT.WHITE }}>Rolling Retraining</h2>
          <p className="text-sm" style={{ color: FINCEPT.MUTED }}>Automated model retraining schedules</p>
        </div>
      </div>

      <div className="grid grid-cols-2 gap-6">
        <div className="p-6 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <h3 className="font-bold mb-4" style={{ color: FINCEPT.ORANGE }}>Create Schedule</h3>
          <div className="space-y-4">
            <div>
              <label className="block text-sm mb-2" style={{ color: FINCEPT.GRAY }}>Frequency</label>
              <select value={frequency} onChange={(e) => setFrequency(e.target.value)} className="w-full px-4 py-2 rounded-lg" style={{ backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.WHITE }}>
                <option value="hourly">Hourly</option>
                <option value="daily">Daily</option>
                <option value="weekly">Weekly</option>
              </select>
            </div>
            <button className="w-full px-6 py-3 rounded-lg font-semibold" style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.WHITE }}>
              <Play size={16} className="inline mr-2" />Create Schedule
            </button>
          </div>
        </div>

        <div className="p-6 rounded-lg" style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
          <h3 className="font-bold mb-4" style={{ color: FINCEPT.ORANGE }}>Active Schedules</h3>
          <div className="space-y-3">
            {schedules.map((sched, idx) => (
              <div key={idx} className="p-3 rounded-lg" style={{ backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}` }}>
                <div className="flex justify-between items-start mb-2">
                  <span className="font-bold" style={{ color: FINCEPT.WHITE }}>{sched.model}</span>
                  <CheckCircle2 size={16} style={{ color: FINCEPT.GREEN }} />
                </div>
                <div className="text-xs space-y-1" style={{ color: FINCEPT.GRAY }}>
                  <div>Frequency: {sched.frequency}</div>
                  <div>Last run: {sched.lastRun}</div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
