# Equity Research - Momentum Indicators Module

Professional momentum indicators analysis with interactive candlestick charts, real-time parameter adjustment, and buy/sell signal generation.

## Features

✅ **Professional Candlestick Charts**
- Powered by TradingView's lightweight-charts
- High performance with large datasets
- Synchronized main chart and indicator panel
- Interactive crosshair and time range selection

✅ **9 Momentum Indicators**
1. **RSI** (Relative Strength Index)
2. **MACD** (Moving Average Convergence Divergence)
3. **Stochastic Oscillator**
4. **CCI** (Commodity Channel Index)
5. **ROC** (Rate of Change)
6. **Williams %R**
7. **Awesome Oscillator**
8. **TSI** (True Strength Index)
9. **Ultimate Oscillator**

✅ **Real-time Parameter Adjustment**
- Instant updates on chart
- Custom overbought/oversold levels
- Period adjustments for all indicators
- Reset to default values

✅ **Intelligent Signal Analysis**
- BUY/SELL/NEUTRAL signals for each indicator
- Signal strength (0-100%)
- Aggregated overall signal
- Detailed reasoning for each signal

✅ **Multiple Timeframes**
- 1 Day to Max (all available data)
- Automatic interval selection
- Seamless data fetching

## Architecture

```
equity-research/
├── components/
│   ├── CandlestickChart.tsx      # Main chart component
│   └── IndicatorControls.tsx     # Parameter controls sidebar
├── services/
│   └── momentumIndicators.ts     # Python backend integration
├── utils/
│   └── signalAnalyzer.ts         # Signal analysis logic
├── types/
│   └── index.ts                  # TypeScript type definitions
├── MomentumAnalysis.tsx          # Main container component
└── README.md                     # This file
```

## Usage

### Basic Usage

```typescript
import { MomentumAnalysis } from './equity-research/MomentumAnalysis';

function MyTab() {
  return <MomentumAnalysis />;
}
```

### Customizing Indicators

The component automatically loads with RSI, MACD, and Stochastic enabled. Users can:

1. **Enable/Disable Indicators**: Check/uncheck in the sidebar
2. **Adjust Parameters**: Click on indicator to expand settings
3. **Change Timeframe**: Select from dropdown
4. **Search Stocks**: Enter symbol and click Search

### Signal Interpretation

**BUY Signals** (Green):
- RSI < 30 (oversold)
- MACD bullish crossover
- Stochastic oversold with bullish divergence
- CCI < -100
- Williams %R < -80
- etc.

**SELL Signals** (Red):
- RSI > 70 (overbought)
- MACD bearish crossover
- Stochastic overbought with bearish divergence
- CCI > 100
- Williams %R > -20
- etc.

**NEUTRAL Signals** (Gray):
- Indicators in normal range
- No clear directional bias

## Python Backend

### Scripts Location
```
src-tauri/resources/scripts/Analytics/
├── technical_analysis/
│   └── momentum_indicators.py
└── equityInvestment/
    └── get_historical_data.py
```

### Data Flow

1. User searches for symbol
2. Frontend calls `fetchHistoricalData()` → Python script
3. yfinance fetches price data
4. Frontend calls `calculateAllMomentumIndicators()` → Python script
5. Python calculates indicators with custom parameters
6. Signal analyzer determines BUY/SELL/NEUTRAL
7. Charts update with new data

## Adding New Indicators

### 1. Update Types (`types/index.ts`)

```typescript
export interface MomentumIndicatorParams {
  // ... existing
  my_indicator?: {
    period: number;
    threshold: number;
  };
}

export const DEFAULT_MOMENTUM_PARAMS = {
  // ... existing
  my_indicator: {
    period: 14,
    threshold: 50,
  },
};
```

### 2. Add Signal Analyzer (`utils/signalAnalyzer.ts`)

```typescript
export function analyzeMyIndicator(
  values: IndicatorData[],
  params: MomentumIndicatorParams['my_indicator']
): SignalResult {
  // Your signal logic
  return { signal: 'BUY', strength: 80, reason: 'Condition met' };
}
```

### 3. Create Service Method (`services/momentumIndicators.ts`)

```typescript
export async function calculateMyIndicator(
  symbol: string,
  params: MomentumIndicatorParams['my_indicator'],
  period: string,
  interval: string
): Promise<MomentumIndicatorResult['my_indicator']> {
  // Call Python script and parse result
}
```

### 4. Add Python Implementation

Update `momentum_indicators.py`:

```python
def calculate_my_indicator(data, period=14, threshold=50):
    # Your calculation logic
    return {'values': result}
```

### 5. Update Chart Component

Add your indicator to `CandlestickChart.tsx` in the indicators rendering section.

### 6. Add Controls

Update `IndicatorControls.tsx` to include parameter controls for your indicator.

## Performance Optimization

- **Debounced parameter updates**: 300ms delay
- **Memoized chart data**: React.memo on expensive components
- **Efficient data structures**: Map for series management
- **Lazy loading**: Only active indicators are calculated
- **WebGL rendering**: Handled by lightweight-charts

## Dependencies

- `lightweight-charts`: Professional charting library
- `yfinance`: Python stock data fetching
- `pandas`, `numpy`: Data processing
- `lucide-react`: Icons
- `@tauri-apps/api`: Backend integration

## Color Scheme

```typescript
const CHART_COLORS = {
  upColor: '#10b981',        // Green candlesticks
  downColor: '#ef4444',      // Red candlesticks
  background: '#0a0a0a',     // Dark background
  rsi: '#FFA500',            // Orange
  macd: '#2962FF',           // Blue
  stochasticK: '#00CED1',    // Cyan
  // ... etc
};
```

## Future Enhancements

- [ ] Volume indicators (OBV, VWAP)
- [ ] Trend indicators (EMA, SMA, Bollinger Bands)
- [ ] Volatility indicators (ATR, Keltner Channels)
- [ ] Pattern recognition (Head & Shoulders, Triangles)
- [ ] Multi-symbol comparison
- [ ] Saved configurations
- [ ] Alert system
- [ ] Export to PNG/CSV

## Troubleshooting

### Charts not rendering
- Check browser console for errors
- Verify lightweight-charts is installed: `npm list lightweight-charts`
- Ensure container has height set

### Python scripts failing
- Check Python is bundled: `src-tauri/resources/python/`
- Verify yfinance is installed in bundled Python
- Check Tauri command output in terminal

### Indicators not updating
- Check network tab for Python script execution
- Verify symbol is valid
- Check parameter values are within valid ranges

## License

Part of Fincept Terminal - All Rights Reserved

---

**Last Updated**: 2025-01-30
**Version**: 1.0.0
**Author**: Fincept Corporation
