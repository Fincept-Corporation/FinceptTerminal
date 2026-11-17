#pragma once
// Headless selftest for the chart kit: ChartAxisTicks, ChartKit decimation and
// formatting, ChartExport CSV, and one construct + render pass of
// TimeSeriesChart, CandleChart, Sparkline, SparklineDelegate, StatTile and
// KpiStrip (10k-point series, every style, panes, annotations, value modes).
// Invoked from main.cpp via --selftest-chart-kit. Returns 0 on success.

namespace fincept::ui {

int run_chart_kit_selftest();

} // namespace fincept::ui
