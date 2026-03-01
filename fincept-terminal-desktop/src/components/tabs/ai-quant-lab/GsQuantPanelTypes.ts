/**
 * GsQuantPanel - Type definitions and mode configuration
 */

import type { ElementType } from 'react';

export type AnalysisMode = 'risk_metrics' | 'portfolio' | 'greeks' | 'var_analysis' | 'stress_test' | 'backtest' | 'statistics';

export interface ModeConfig {
  id: AnalysisMode;
  label: string;
  icon: ElementType;
  description: string;
  detailedDescription: string;
  tips: string[];
}
