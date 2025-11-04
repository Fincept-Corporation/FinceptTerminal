# AI Prediction Module for FinceptTerminal

## 🎯 Overview

Kapsamlı AI tahmin modülü ile hisse senedi fiyatları, volatilite ve piyasa trendlerini tahmin edin. Multi-model ensemble yaklaşımı ile ARIMA, Prophet, LSTM ve XGBoost modellerini birleştirir.

## 📦 Modüller

### 1. Time Series Models (`time_series_models.py`)

Temel zaman serisi tahmin modelleri:
- **ARIMA**: Otoregresif entegre hareketli ortalama
- **SARIMA**: Mevsimsel ARIMA
- **Prophet**: Facebook Prophet (trend + mevsimsellik)
- **LSTM**: Long Short-Term Memory sinir ağları

**Kullanım:**
```python
from time_series_models import TimeSeriesPredictor

# ARIMA tahmin
predictor = TimeSeriesPredictor(model_type='arima')
fit_result = predictor.fit_arima(data, auto_order=True)
predictions = predictor.predict(steps_ahead=30)

# Prophet tahmin
predictor_prophet = TimeSeriesPredictor(model_type='prophet')
df = pd.DataFrame({'ds': dates, 'y': values})
predictor_prophet.fit_prophet(df)
forecast = predictor_prophet.predict(steps_ahead=30)

# LSTM tahmin
predictor_lstm = TimeSeriesPredictor(model_type='lstm')
predictor_lstm.fit_lstm(data, lookback=60, units=128, epochs=100)
predictions = predictor_lstm.predict(steps_ahead=30)
```

### 2. Stock Price Predictor (`stock_price_predictor.py`)

Multi-model hisse senedi fiyat tahmini:
- Technical indicator feature engineering
- XGBoost, Random Forest makine öğrenmesi
- ARIMA ve LSTM entegrasyonu
- Ensemble tahmin (tüm modellerin ağırlıklı ortalaması)
- Alım/satım sinyalleri

**Kullanım:**
```python
from stock_price_predictor import StockPricePredictor

# Predictor oluştur
predictor = StockPricePredictor(symbol='AAPL', lookback_days=252)

# Modelleri eğit
fit_result = predictor.fit(ohlcv_data, test_size=0.2)

# Tahmin yap
predictions = predictor.predict(steps_ahead=30, method='ensemble')

# Sinyal üret
signals = predictor.calculate_signals(predictions)

print(f"Forecast: {predictions['ensemble']['forecast']}")
print(f"Signal: {signals['signals'][0]['type']}")  # BUY/SELL/HOLD
```

**Özellikler:**
- 50+ teknik indikatör (SMA, EMA, RSI, MACD, Bollinger Bands, vb.)
- Lag features (1-20 gün)
- 4 farklı model (ARIMA, XGBoost, Random Forest, LSTM)
- Confidence intervals
- Model performans metrikleri (RMSE, MAE, MAPE)

### 3. Volatility Forecaster (`volatility_forecaster.py`)

GARCH ailesi volatilite modelleri:
- **GARCH**: Generalized Autoregressive Conditional Heteroskedasticity
- **EGARCH**: Exponential GARCH (asimetrik volatilite)
- **GJR-GARCH**: Threshold GARCH

**Kullanım:**
```python
from volatility_forecaster import VolatilityForecaster

# GARCH model
forecaster = VolatilityForecaster(model_type='garch')
returns = forecaster.calculate_returns(prices, method='log')
fit_result = forecaster.fit_garch(returns, p=1, q=1)

# Volatilite tahmini
vol_forecast = forecaster.predict_volatility(horizon=30)

# Value at Risk (VaR)
var_result = forecaster.calculate_var(
    confidence_level=0.95,
    horizon=1,
    portfolio_value=100000
)

# Volatilite rejimi (düşük/normal/yüksek)
regime = forecaster.detect_volatility_regime(prices)

print(f"Current Vol: {vol_forecast['current_volatility']:.2f}%")
print(f"VaR (95%): ${var_result['var_dollar']:,.0f}")
print(f"Regime: {regime['regime']}")
```

### 4. Backtesting Engine (`backtesting_engine.py`)

Strateji backtesting ve performans analizi:
- Walk-forward analysis
- Gerçekçi komisyon ve slippage
- Kapsamlı performans metrikleri

**Kullanım:**
```python
from backtesting_engine import BacktestingEngine, moving_average_crossover_strategy

# Engine oluştur
engine = BacktestingEngine(
    initial_capital=100000,
    commission=0.001,  # 0.1%
    slippage=0.0005    # 0.05%
)

# Backtest çalıştır
result = engine.run_backtest(
    data=ohlcv_data,
    strategy_func=moving_average_crossover_strategy,
    strategy_params={
        'fast_period': 20,
        'slow_period': 50,
        'symbol': 'AAPL'
    }
)

# Walk-forward analizi
wf_result = engine.walk_forward_analysis(
    data=ohlcv_data,
    strategy_func=moving_average_crossover_strategy,
    train_window=252,  # 1 yıl
    test_window=63,    # 3 ay
    step_size=21       # Her ay
)

print(f"Total Return: {result['metrics']['total_return']:.2f}%")
print(f"Sharpe Ratio: {result['metrics']['sharpe_ratio']:.2f}")
print(f"Max Drawdown: {result['metrics']['max_drawdown']:.2f}%")
print(f"Win Rate: {result['metrics']['win_rate']:.1f}%")
```

**Performans Metrikleri:**
- Total Return
- Sharpe Ratio
- Sortino Ratio
- Calmar Ratio
- Maximum Drawdown
- Win Rate
- Profit Factor
- Volatility

## 🤖 AI Agents

### Stock Prediction Agent

Multi-stock tahmin analizi:
```python
from agents.PredictionAgents.stock_prediction_agent import multi_stock_prediction_agent

state = {
    "tickers": ["AAPL", "GOOGL", "MSFT"],
    "mode": "multi"
}

result = multi_stock_prediction_agent(state)
print(result['reasoning'])
```

### Volatility Prediction Agent

Portföy volatilite analizi:
```python
from agents.PredictionAgents.volatility_prediction_agent import portfolio_volatility_agent

state = {
    "tickers": ["AAPL", "GOOGL", "MSFT"],
    "mode": "portfolio"
}

result = portfolio_volatility_agent(state)
print(result['risk_metrics'])
```

## 📊 Frontend Integration

TypeScript servis kullanımı:

```typescript
import predictionService from '@/services/predictionService';

// Hızlı tahmin
const result = await predictionService.quickPredict('AAPL', 30);
console.log(result.ensemble.forecast);

// Volatilite tahmini
const volForecast = await predictionService.predictVolatility({
  prices: priceData,
  dates: dateData,
  action: 'fit_garch',
  params: { p: 1, q: 1, horizon: 30 }
});

// Backtest
const backtestResult = await predictionService.runBacktest({
  data: { dates, close, high, low, volume },
  strategy: 'ma_crossover',
  strategy_params: { fast_period: 20, slow_period: 50 },
  initial_capital: 100000
});

// Agent kullanımı
const agentResult = await predictionService.executeStockPredictionAgent(
  ['AAPL', 'GOOGL', 'MSFT'],
  'multi'
);
```

## 🔧 Kurulum

### Python Dependencies

```bash
cd fincept-terminal-desktop/src-tauri/resources/scripts/Analytics/prediction
pip install -r requirements.txt
```

**Ana Gereksinimler:**
- `statsmodels>=0.14.0` - ARIMA, SARIMA
- `prophet>=1.1.4` - Facebook Prophet
- `tensorflow>=2.13.0` - LSTM
- `xgboost>=2.0.0` - XGBoost
- `scikit-learn>=1.3.0` - ML utilities
- `arch>=6.2.0` - GARCH modelleri
- `yfinance>=0.2.28` - Market data

## 📈 Prediction Tab Kullanımı

Frontend'de yeni **Prediction Tab** şunları sağlar:

1. **Price Prediction**
   - Sembol seçimi
   - Tahmin horizon (1-90 gün)
   - Model seçimi (Ensemble, ARIMA, LSTM, XGBoost)
   - Confidence intervals ile grafik
   - Alım/satım sinyalleri

2. **Volatility Forecast**
   - GARCH model volatilite tahmini
   - Current vs forecast volatility
   - VaR hesaplaması
   - Volatility regime detection

3. **Backtesting**
   - MA Crossover stratejisi
   - Equity curve
   - Performans metrikleri dashboard
   - Trade history

4. **Trading Signals**
   - Multi-stock karşılaştırma
   - Portfolio-wide analiz

## 🎓 Algoritma Detayları

### ARIMA Model
- **AR (Autoregressive)**: Geçmiş değerlerin linear kombinasyonu
- **I (Integrated)**: Differencing ile stationarity
- **MA (Moving Average)**: Geçmiş hataların linear kombinasyonu
- AIC/BIC ile optimal order seçimi

### LSTM Neural Network
- 60 günlük lookback window
- 2 katmanlı LSTM (128 units)
- Dropout (0.2) ile overfitting önleme
- Early stopping

### XGBoost
- Gradient boosting decision trees
- 50+ teknik indikatör features
- Feature importance analizi

### GARCH Volatility
- Conditional heteroskedasticity modeling
- Volatility clustering
- Mean reversion

## ⚠️ Önemli Notlar

1. **Tahminler kesin değildir** - Risk yönetimi kullanın
2. **Walk-forward validation** kullanarak model performansını test edin
3. **Commission ve slippage** gerçekçi backtest için önemlidir
4. **Model ensemble** genellikle tek modelden daha iyi performans gösterir
5. **Volatility forecasting** risk yönetimi için kritiktir

## 🚀 Gelecek Geliştirmeler

- [ ] Transformer modelleri (Attention mechanism)
- [ ] Multi-asset correlation modeling
- [ ] Sentiment analysis entegrasyonu
- [ ] Real-time prediction updates
- [ ] Options pricing integration
- [ ] Custom strategy builder UI
- [ ] ML model auto-tuning (Optuna)

## 📚 Referanslar

- **ARIMA**: Box, G. E. P., & Jenkins, G. M. (1970)
- **GARCH**: Bollerslev, T. (1986)
- **Prophet**: Taylor, S. J., & Letham, B. (2018)
- **LSTM**: Hochreiter, S., & Schmidhuber, J. (1997)

## 🤝 Katkıda Bulunma

Yeni modeller, stratejiler veya iyileştirmeler için pull request açabilirsiniz.

## 📄 Lisans

MIT License - Fincept Corporation © 2024-2025
