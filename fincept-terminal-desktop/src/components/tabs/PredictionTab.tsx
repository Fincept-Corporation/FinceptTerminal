/**
 * AI Prediction Tab for FinceptTerminal
 * Stock price forecasting, volatility prediction, and backtesting
 */

import React, { useState, useEffect } from 'react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
  AreaChart,
  Area,
  BarChart,
  Bar,
  ComposedChart,
  ReferenceLine,
  Scatter,
  ScatterChart,
} from 'recharts';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import predictionService, {
  PredictionResult,
  VolatilityForecast,
  BacktestResult,
} from '@/services/predictionService';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Card, CardHeader, CardTitle, CardContent } from '@/components/ui/card';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select';
import { TrendingUp, TrendingDown, Activity, Brain, Target, LineChart as LineChartIcon } from 'lucide-react';

const PredictionTab: React.FC = () => {
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  // State
  const [market, setMarket] = useState<'US' | 'BIST' | 'Crypto'>('US');
  const [symbol, setSymbol] = useState('AAPL');
  const [cryptoQuote, setCryptoQuote] = useState<'USDT' | 'TRY' | 'BUSD'>('USDT');
  const [predictionHorizon, setPredictionHorizon] = useState(30);
  const [selectedModel, setSelectedModel] = useState<'ensemble' | 'arima' | 'lstm' | 'xgboost'>('ensemble');
  const [isLoading, setIsLoading] = useState(false);
  const [predictionResult, setPredictionResult] = useState<PredictionResult | null>(null);
  const [volatilityForecast, setVolatilityForecast] = useState<VolatilityForecast | null>(null);
  const [backtestResult, setBacktestResult] = useState<BacktestResult | null>(null);
  const [activeTab, setActiveTab] = useState('price-prediction');

  // Market-specific suggestions
  const getSymbolSuggestions = () => {
    if (market === 'BIST') {
      return ['AKBNK.IS', 'GARAN.IS', 'THYAO.IS', 'ASELS.IS', 'EREGL.IS'];
    } else if (market === 'Crypto') {
      return ['BTC', 'ETH', 'BNB', 'SOL', 'ADA'];
    } else {
      return ['AAPL', 'GOOGL', 'MSFT', 'TSLA', 'NVDA'];
    }
  };

  // Update symbol when market changes
  React.useEffect(() => {
    const suggestions = getSymbolSuggestions();
    setSymbol(suggestions[0]);
  }, [market]);

  // Execute price prediction
  const handlePricePredict = async () => {
    setIsLoading(true);
    try {
      let result;

      if (market === 'BIST') {
        result = await predictionService.predictBISTStock(symbol, predictionHorizon, true);
      } else if (market === 'Crypto') {
        result = await predictionService.predictCrypto(symbol, predictionHorizon, cryptoQuote, true);
      } else {
        result = await predictionService.quickPredict(symbol, predictionHorizon);
      }

      setPredictionResult(result as any);

      if (!result.success) {
        alert(`Prediction failed: ${result.error}`);
      }
    } catch (error: any) {
      alert(`Error: ${error.message}`);
    } finally {
      setIsLoading(false);
    }
  };

  // Execute volatility prediction
  const handleVolatilityPredict = async () => {
    setIsLoading(true);
    try {
      const histData = await predictionService.getHistoricalData(symbol);
      if (!histData) {
        alert('Failed to fetch historical data');
        return;
      }

      const result = await predictionService.predictVolatility({
        prices: histData.close,
        dates: histData.dates,
        action: 'fit_garch',
        params: {
          p: 1,
          q: 1,
          horizon: 30,
        },
      });

      setVolatilityForecast(result);

      if (!result.success) {
        alert(`Volatility forecast failed: ${result.error}`);
      }
    } catch (error: any) {
      alert(`Error: ${error.message}`);
    } finally {
      setIsLoading(false);
    }
  };

  // Execute backtest
  const handleBacktest = async () => {
    setIsLoading(true);
    try {
      const histData = await predictionService.getHistoricalData(symbol, '2y');
      if (!histData) {
        alert('Failed to fetch historical data');
        return;
      }

      const result = await predictionService.runBacktest({
        data: {
          dates: histData.dates,
          close: histData.close,
          high: histData.high,
          low: histData.low,
          volume: histData.volume,
        },
        strategy: 'ma_crossover',
        strategy_params: {
          fast_period: 20,
          slow_period: 50,
          symbol: symbol,
        },
        initial_capital: 100000,
        commission: 0.001,
        action: 'backtest',
      });

      setBacktestResult(result);

      if (!result.success) {
        alert(`Backtest failed: ${result.error}`);
      }
    } catch (error: any) {
      alert(`Error: ${error.message}`);
    } finally {
      setIsLoading(false);
    }
  };

  // Prepare chart data for price prediction
  const getPredictionChartData = () => {
    if (!predictionResult?.ensemble) return [];

    const { forecast, lower_ci, upper_ci, forecast_dates } = predictionResult.ensemble;

    return forecast.map((value, index) => ({
      date: forecast_dates?.[index] || `Day ${index + 1}`,
      forecast: value,
      lower: lower_ci[index],
      upper: upper_ci[index],
    }));
  };

  // Prepare volatility chart data
  const getVolatilityChartData = () => {
    if (!volatilityForecast) return [];

    const { volatility_forecast, forecast_dates } = volatilityForecast;

    return volatility_forecast.map((vol, index) => ({
      date: forecast_dates[index] || `Day ${index + 1}`,
      volatility: vol,
    }));
  };

  // Prepare backtest equity curve data
  const getEquityCurveData = () => {
    if (!backtestResult?.equity_curve) return [];

    return backtestResult.equity_curve.map((point) => ({
      date: new Date(point.date).toLocaleDateString(),
      value: point.value,
    }));
  };

  return (
    <div
      className="h-full w-full overflow-auto p-4"
      style={{
        backgroundColor: colors.background,
        color: colors.foreground,
        fontFamily,
        fontSize,
      }}
    >
      {/* Header */}
      <div className="mb-6">
        <h1 className="text-3xl font-bold mb-2 flex items-center gap-2">
          <Brain className="w-8 h-8" />
          AI Prediction Engine
        </h1>
        <p className="text-sm opacity-70">
          Multi-model forecasting powered by ARIMA, LSTM, XGBoost, and GARCH
        </p>
      </div>

      {/* Controls */}
      <Card className="mb-6" style={{ backgroundColor: colors.card, borderColor: colors.border }}>
        <CardHeader>
          <CardTitle>Prediction Controls</CardTitle>
        </CardHeader>
        <CardContent>
          <div className="grid grid-cols-1 md:grid-cols-5 gap-4 mb-4">
            <div>
              <label className="text-sm mb-2 block">Market</label>
              <Select value={market} onValueChange={(v: any) => setMarket(v)}>
                <SelectTrigger>
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="US">🇺🇸 US Stocks</SelectItem>
                  <SelectItem value="BIST">🇹🇷 BIST (Türkiye)</SelectItem>
                  <SelectItem value="Crypto">₿ Cryptocurrency</SelectItem>
                </SelectContent>
              </Select>
            </div>
            <div>
              <label className="text-sm mb-2 block">Symbol</label>
              <Input
                value={symbol}
                onChange={(e) => setSymbol(e.target.value.toUpperCase())}
                placeholder={market === 'BIST' ? 'AKBNK.IS' : market === 'Crypto' ? 'BTC' : 'AAPL'}
                className="uppercase"
              />
              <div className="text-xs opacity-60 mt-1">
                {getSymbolSuggestions().slice(0, 3).join(', ')}
              </div>
            </div>
            {market === 'Crypto' && (
              <div>
                <label className="text-sm mb-2 block">Quote Currency</label>
                <Select value={cryptoQuote} onValueChange={(v: any) => setCryptoQuote(v)}>
                  <SelectTrigger>
                    <SelectValue />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="USDT">USDT (Tether)</SelectItem>
                    <SelectItem value="TRY">TRY (₺ Türk Lirası)</SelectItem>
                    <SelectItem value="BUSD">BUSD</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            )}
            <div>
              <label className="text-sm mb-2 block">Forecast Horizon (Days)</label>
              <Input
                type="number"
                value={predictionHorizon}
                onChange={(e) => setPredictionHorizon(Number(e.target.value))}
                min={1}
                max={90}
              />
            </div>
            <div className="flex items-end">
              <Button
                onClick={handlePricePredict}
                disabled={isLoading}
                className="w-full"
                style={{ backgroundColor: colors.primary, color: colors.primaryForeground }}
              >
                {isLoading ? 'Predicting...' : 'Run Prediction'}
              </Button>
            </div>
          </div>
        </CardContent>
      </Card>

      {/* Main Tabs */}
      <Tabs value={activeTab} onValueChange={setActiveTab}>
        <TabsList className="mb-4">
          <TabsTrigger value="price-prediction">Price Prediction</TabsTrigger>
          <TabsTrigger value="volatility">Volatility Forecast</TabsTrigger>
          <TabsTrigger value="backtest">Backtesting</TabsTrigger>
          <TabsTrigger value="signals">Trading Signals</TabsTrigger>
        </TabsList>

        {/* Price Prediction Tab */}
        <TabsContent value="price-prediction">
          <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 mb-6">
            {/* Signal Card */}
            {predictionResult?.signals && predictionResult.signals.length > 0 && (
              <>
                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardHeader>
                    <CardTitle className="flex items-center gap-2">
                      <Target className="w-5 h-5" />
                      Trading Signal
                    </CardTitle>
                  </CardHeader>
                  <CardContent>
                    <div className="text-center">
                      <div
                        className={`text-4xl font-bold mb-2 ${
                          predictionResult.signals[0].type === 'BUY'
                            ? 'text-green-500'
                            : predictionResult.signals[0].type === 'SELL'
                            ? 'text-red-500'
                            : 'text-yellow-500'
                        }`}
                      >
                        {predictionResult.signals[0].type}
                      </div>
                      <div className="text-sm opacity-70">
                        Confidence: {predictionResult.signals[0].confidence.toUpperCase()}
                      </div>
                      <div className="text-sm mt-2">
                        Strength: {(predictionResult.signals[0].strength * 100).toFixed(0)}%
                      </div>
                    </div>
                  </CardContent>
                </Card>

                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardHeader>
                    <CardTitle className="flex items-center gap-2">
                      <TrendingUp className="w-5 h-5" />
                      Expected Return
                    </CardTitle>
                  </CardHeader>
                  <CardContent>
                    <div className="text-center">
                      <div
                        className={`text-3xl font-bold mb-2 ${
                          predictionResult.signals[0].expected_return_1d > 0
                            ? 'text-green-500'
                            : 'text-red-500'
                        }`}
                      >
                        {predictionResult.signals[0].expected_return_1d > 0 ? '+' : ''}
                        {predictionResult.signals[0].expected_return_1d.toFixed(2)}%
                      </div>
                      <div className="text-sm opacity-70">1-Day Expected Return</div>
                      <div className="text-xs mt-2">
                        Current: ${predictionResult.signals[0].current_price.toFixed(2)}
                      </div>
                      <div className="text-xs">
                        Target: ${predictionResult.signals[0].predicted_price_1d.toFixed(2)}
                      </div>
                    </div>
                  </CardContent>
                </Card>

                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardHeader>
                    <CardTitle className="flex items-center gap-2">
                      <Activity className="w-5 h-5" />
                      Model Ensemble
                    </CardTitle>
                  </CardHeader>
                  <CardContent>
                    {predictionResult.ensemble?.model_weights && (
                      <div className="space-y-2">
                        {Object.entries(predictionResult.ensemble.model_weights).map(([model, weight]) => (
                          <div key={model} className="flex justify-between items-center">
                            <span className="text-sm capitalize">{model}</span>
                            <div className="flex items-center gap-2">
                              <div
                                className="h-2 bg-blue-500 rounded"
                                style={{ width: `${weight * 100}px` }}
                              />
                              <span className="text-xs">{(weight * 100).toFixed(0)}%</span>
                            </div>
                          </div>
                        ))}
                      </div>
                    )}
                  </CardContent>
                </Card>
              </>
            )}
          </div>

          {/* Prediction Chart */}
          <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
            <CardHeader>
              <CardTitle>Price Forecast with Confidence Intervals</CardTitle>
            </CardHeader>
            <CardContent>
              <ResponsiveContainer width="100%" height={400}>
                <ComposedChart data={getPredictionChartData()}>
                  <CartesianGrid strokeDasharray="3 3" stroke={colors.border} />
                  <XAxis dataKey="date" stroke={colors.foreground} />
                  <YAxis stroke={colors.foreground} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: colors.card,
                      border: `1px solid ${colors.border}`,
                    }}
                  />
                  <Legend />
                  <Area
                    type="monotone"
                    dataKey="upper"
                    stroke="transparent"
                    fill={colors.primary}
                    fillOpacity={0.1}
                    name="Upper CI"
                  />
                  <Area
                    type="monotone"
                    dataKey="lower"
                    stroke="transparent"
                    fill={colors.primary}
                    fillOpacity={0.1}
                    name="Lower CI"
                  />
                  <Line
                    type="monotone"
                    dataKey="forecast"
                    stroke={colors.primary}
                    strokeWidth={2}
                    dot={false}
                    name="Forecast"
                  />
                </ComposedChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>
        </TabsContent>

        {/* Volatility Tab */}
        <TabsContent value="volatility">
          <div className="mb-4">
            <Button
              onClick={handleVolatilityPredict}
              disabled={isLoading}
              style={{ backgroundColor: colors.primary, color: colors.primaryForeground }}
            >
              {isLoading ? 'Forecasting...' : 'Run Volatility Forecast'}
            </Button>
          </div>

          {volatilityForecast && (
            <>
              <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mb-6">
                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardHeader>
                    <CardTitle>Current Volatility</CardTitle>
                  </CardHeader>
                  <CardContent>
                    <div className="text-3xl font-bold">
                      {volatilityForecast.current_volatility.toFixed(2)}%
                    </div>
                    <div className="text-sm opacity-70">Annualized</div>
                  </CardContent>
                </Card>

                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardHeader>
                    <CardTitle>Mean Forecast</CardTitle>
                  </CardHeader>
                  <CardContent>
                    <div className="text-3xl font-bold">
                      {volatilityForecast.mean_forecast_vol.toFixed(2)}%
                    </div>
                    <div className="text-sm opacity-70">30-Day Average</div>
                  </CardContent>
                </Card>

                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardHeader>
                    <CardTitle>Max Forecast</CardTitle>
                  </CardHeader>
                  <CardContent>
                    <div className="text-3xl font-bold text-red-500">
                      {volatilityForecast.max_forecast_vol.toFixed(2)}%
                    </div>
                    <div className="text-sm opacity-70">Peak Volatility</div>
                  </CardContent>
                </Card>
              </div>

              <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                <CardHeader>
                  <CardTitle>Volatility Forecast (GARCH)</CardTitle>
                </CardHeader>
                <CardContent>
                  <ResponsiveContainer width="100%" height={400}>
                    <LineChart data={getVolatilityChartData()}>
                      <CartesianGrid strokeDasharray="3 3" stroke={colors.border} />
                      <XAxis dataKey="date" stroke={colors.foreground} />
                      <YAxis stroke={colors.foreground} label={{ value: 'Volatility %', angle: -90 }} />
                      <Tooltip
                        contentStyle={{
                          backgroundColor: colors.card,
                          border: `1px solid ${colors.border}`,
                        }}
                      />
                      <Legend />
                      <Line
                        type="monotone"
                        dataKey="volatility"
                        stroke="#ef4444"
                        strokeWidth={2}
                        dot={false}
                        name="Volatility %"
                      />
                    </LineChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>
            </>
          )}
        </TabsContent>

        {/* Backtest Tab */}
        <TabsContent value="backtest">
          <div className="mb-4">
            <Button
              onClick={handleBacktest}
              disabled={isLoading}
              style={{ backgroundColor: colors.primary, color: colors.primaryForeground }}
            >
              {isLoading ? 'Running Backtest...' : 'Run Backtest (MA Crossover)'}
            </Button>
          </div>

          {backtestResult && backtestResult.metrics && (
            <>
              {/* Metrics Grid */}
              <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6">
                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardContent className="pt-4">
                    <div className="text-sm opacity-70">Total Return</div>
                    <div className={`text-2xl font-bold ${backtestResult.metrics.total_return > 0 ? 'text-green-500' : 'text-red-500'}`}>
                      {backtestResult.metrics.total_return.toFixed(2)}%
                    </div>
                  </CardContent>
                </Card>

                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardContent className="pt-4">
                    <div className="text-sm opacity-70">Sharpe Ratio</div>
                    <div className="text-2xl font-bold">
                      {backtestResult.metrics.sharpe_ratio.toFixed(2)}
                    </div>
                  </CardContent>
                </Card>

                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardContent className="pt-4">
                    <div className="text-sm opacity-70">Max Drawdown</div>
                    <div className="text-2xl font-bold text-red-500">
                      {backtestResult.metrics.max_drawdown.toFixed(2)}%
                    </div>
                  </CardContent>
                </Card>

                <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                  <CardContent className="pt-4">
                    <div className="text-sm opacity-70">Win Rate</div>
                    <div className="text-2xl font-bold">
                      {backtestResult.metrics.win_rate.toFixed(1)}%
                    </div>
                  </CardContent>
                </Card>
              </div>

              {/* Equity Curve */}
              <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
                <CardHeader>
                  <CardTitle>Equity Curve</CardTitle>
                </CardHeader>
                <CardContent>
                  <ResponsiveContainer width="100%" height={400}>
                    <AreaChart data={getEquityCurveData()}>
                      <CartesianGrid strokeDasharray="3 3" stroke={colors.border} />
                      <XAxis dataKey="date" stroke={colors.foreground} />
                      <YAxis stroke={colors.foreground} />
                      <Tooltip
                        contentStyle={{
                          backgroundColor: colors.card,
                          border: `1px solid ${colors.border}`,
                        }}
                      />
                      <Legend />
                      <Area
                        type="monotone"
                        dataKey="value"
                        stroke={colors.primary}
                        fill={colors.primary}
                        fillOpacity={0.3}
                        name="Portfolio Value"
                      />
                    </AreaChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>
            </>
          )}
        </TabsContent>

        {/* Signals Tab */}
        <TabsContent value="signals">
          <Card style={{ backgroundColor: colors.card, borderColor: colors.border }}>
            <CardHeader>
              <CardTitle>Multi-Stock Analysis</CardTitle>
            </CardHeader>
            <CardContent>
              <p className="text-sm opacity-70 mb-4">
                Run predictions on multiple stocks simultaneously to compare signals and identify opportunities.
              </p>
              <Button
                onClick={async () => {
                  const symbols = ['AAPL', 'GOOGL', 'MSFT', 'AMZN', 'TSLA'];
                  const result = await predictionService.getPortfolioPredictions(symbols);
                  console.log('Portfolio predictions:', result);
                  alert(JSON.stringify(result, null, 2));
                }}
                style={{ backgroundColor: colors.primary, color: colors.primaryForeground }}
              >
                Analyze Top 5 Tech Stocks
              </Button>
            </CardContent>
          </Card>
        </TabsContent>
      </Tabs>
    </div>
  );
};

export default PredictionTab;
