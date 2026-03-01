// QuantLib Statistics MCP Tools
// 52 tools for continuous distributions, discrete distributions, and time series analysis

import type { InternalTool } from '../../types';
import { statisticsContinuousTools } from './statisticsContinuous';
import { statisticsDiscreteTimeSeriesTools } from './statisticsDiscreteTimeSeries';

export const quantlibStatisticsTools: InternalTool[] = [
  ...statisticsContinuousTools,
  ...statisticsDiscreteTimeSeriesTools,
];
