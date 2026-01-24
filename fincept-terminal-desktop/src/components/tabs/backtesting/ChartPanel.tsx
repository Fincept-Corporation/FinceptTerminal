/**
 * ChartPanel - Center panel with multi-tab chart views
 *
 * Tabs: EQUITY | DRAWDOWN | DISTRIBUTION | MONTHLY | ROLLING
 */

import React, { useState } from 'react';
import { F, panelStyle, tabStyle, ExtendedEquityPoint, MonthlyReturnsData, AdvancedMetrics, RollingMetric } from './backtestingStyles';
import EquityCurveChart from './EquityCurveChart';
import DrawdownChart from './DrawdownChart';
import ReturnsDistribution from './ReturnsDistribution';
import MonthlyReturnsHeatmap from './MonthlyReturnsHeatmap';
import RollingMetricsChart from './RollingMetricsChart';

type ChartTab = 'equity' | 'drawdown' | 'distribution' | 'monthly' | 'rolling';

interface ChartPanelProps {
  equity: ExtendedEquityPoint[];
  initialCapital: number;
  advancedMetrics?: AdvancedMetrics;
  monthlyReturns?: MonthlyReturnsData[];
  rollingSharpe?: RollingMetric[];
  rollingVolatility?: RollingMetric[];
  rollingDrawdown?: RollingMetric[];
}

const ChartPanel: React.FC<ChartPanelProps> = ({
  equity, initialCapital, advancedMetrics, monthlyReturns,
  rollingSharpe, rollingVolatility, rollingDrawdown,
}) => {
  const [activeTab, setActiveTab] = useState<ChartTab>('equity');

  const tabs: { key: ChartTab; label: string }[] = [
    { key: 'equity', label: 'EQUITY' },
    { key: 'drawdown', label: 'DRAWDOWN' },
    { key: 'distribution', label: 'DISTRIBUTION' },
    { key: 'monthly', label: 'MONTHLY' },
    { key: 'rolling', label: 'ROLLING' },
  ];

  return (
    <div style={{ ...panelStyle, display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Tab Bar */}
      <div style={{
        display: 'flex', borderBottom: `1px solid ${F.BORDER}`,
        backgroundColor: F.PANEL_BG, padding: '0 8px',
      }}>
        {tabs.map(tab => (
          <button
            key={tab.key}
            style={tabStyle(activeTab === tab.key)}
            onClick={() => setActiveTab(tab.key)}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Chart Content */}
      <div style={{ flex: 1, padding: '8px', minHeight: 0 }}>
        {activeTab === 'equity' && (
          <EquityCurveChart equity={equity} initialCapital={initialCapital} />
        )}
        {activeTab === 'drawdown' && (
          <DrawdownChart equity={equity} />
        )}
        {activeTab === 'distribution' && (
          <ReturnsDistribution
            histogram={advancedMetrics?.returnsHistogram || []}
          />
        )}
        {activeTab === 'monthly' && (
          <MonthlyReturnsHeatmap
            data={monthlyReturns || advancedMetrics?.monthlyReturns || []}
          />
        )}
        {activeTab === 'rolling' && (
          <RollingMetricsChart
            rollingSharpe={rollingSharpe || advancedMetrics?.rollingSharpe || []}
            rollingVolatility={rollingVolatility || advancedMetrics?.rollingVolatility || []}
            rollingDrawdown={rollingDrawdown || advancedMetrics?.rollingDrawdown || []}
          />
        )}
      </div>
    </div>
  );
};

export default ChartPanel;
