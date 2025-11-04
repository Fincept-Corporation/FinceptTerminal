# 🚀 Quick Start Guide - AI Prediction Module

## ⚡ Hızlı Test (Dependency Yok!)

Dependency kurmadan hemen test edebilirsiniz:

```bash
cd fincept-terminal-desktop/src-tauri/resources/scripts/Analytics/prediction

# Test suite çalıştır
python3 test_predictions.py
```

**Sonuç**: ✅ Simple Prediction Demo çalışacak!

---

## 📦 Tam Kurulum (Tüm Özellikler İçin)

### Option 1: Otomatik Kurulum (Önerilen)

```bash
cd fincept-terminal-desktop/src-tauri/resources/scripts/Analytics/prediction

# Tüm dependencies'i kur (5-10 dakika)
./install_dependencies.sh
```

Bu script şunları kurar:
- ✅ numpy, pandas, scipy
- ✅ statsmodels (ARIMA/SARIMA)
- ✅ prophet (Facebook Prophet)
- ✅ scikit-learn (ML)
- ✅ xgboost (Gradient Boosting)
- ✅ arch (GARCH volatility models)
- ✅ yfinance (Market data)
- ⚠️ tensorflow (LSTM - opsiyonel, büyük dosya)

### Option 2: Manuel Kurulum

```bash
pip install -r requirements.txt

# TensorFlow için (opsiyonel - ~500MB):
pip install tensorflow>=2.13.0 keras>=2.13.0
```

---

## ✅ Kurulum Doğrulama

```bash
python3 test_predictions.py
```

**Başarılı Kurulum Çıktısı**:
```
✓ PASSED: Simple Prediction
✓ PASSED: Time Series Models
✓ PASSED: Stock Predictor
✓ PASSED: Volatility Forecaster
✓ PASSED: Backtesting Engine

Total: 5 | Passed: 5 | Failed: 0 | Skipped: 0
✓ All tests passed!
```

---

## 🎯 Hızlı Kullanım Örnekleri

### 1️⃣ Basit Tahmin (Dependency Yok)

```bash
echo '{
  "prices": [100, 102, 105, 108, 110, 112, 115, 118, 120],
  "steps_ahead": 5
}' | python3 simple_prediction_demo.py
```

### 2️⃣ Stock Price Prediction (Dependencies Gerekli)

```bash
echo '{
  "symbol": "AAPL",
  "ohlcv_data": {
    "dates": ["2024-01-01", "2024-01-02", ...],
    "open": [150, 152, ...],
    "high": [153, 155, ...],
    "low": [149, 151, ...],
    "close": [151, 154, ...],
    "volume": [1000000, 1200000, ...]
  },
  "action": "both",
  "steps_ahead": 30,
  "method": "ensemble"
}' | python3 stock_price_predictor.py
```

### 3️⃣ Volatility Forecast

```bash
echo '{
  "prices": [100, 102, 101, 105, ...],
  "dates": ["2024-01-01", "2024-01-02", ...],
  "action": "fit_garch",
  "params": {"p": 1, "q": 1, "horizon": 30}
}' | python3 volatility_forecaster.py
```

### 4️⃣ Backtesting

```bash
echo '{
  "data": {
    "dates": ["2024-01-01", ...],
    "close": [100, 102, ...],
    "high": [103, 105, ...],
    "low": [99, 101, ...],
    "volume": [1000000, ...]
  },
  "strategy": "ma_crossover",
  "strategy_params": {
    "fast_period": 20,
    "slow_period": 50,
    "symbol": "TEST"
  },
  "initial_capital": 100000,
  "action": "backtest"
}' | python3 backtesting_engine.py
```

---

## 🖥️ Frontend Kullanımı (TypeScript)

```typescript
import predictionService from '@/services/predictionService';

// Hızlı tahmin
const result = await predictionService.quickPredict('AAPL', 30);

if (result.success) {
  console.log('Forecast:', result.ensemble.forecast);
  console.log('Signal:', result.signals[0].type);  // BUY/SELL/HOLD
  console.log('Expected Return:', result.signals[0].expected_return_1d);
}

// Volatilite tahmini
const volForecast = await predictionService.predictVolatility({
  prices: priceArray,
  dates: dateArray,
  action: 'fit_garch',
  params: { p: 1, q: 1, horizon: 30 }
});

console.log('Volatility:', volForecast.current_volatility);

// Backtest
const backtest = await predictionService.runBacktest({
  data: { dates, close, high, low, volume },
  strategy: 'ma_crossover',
  strategy_params: { fast_period: 20, slow_period: 50 }
});

console.log('Return:', backtest.metrics.total_return);
console.log('Sharpe:', backtest.metrics.sharpe_ratio);
```

---

## 🎨 Prediction Tab (UI)

FinceptTerminal'de yeni **Prediction** tab'ını kullanmak için:

1. **DashboardScreen.tsx** dosyasını açın
2. PredictionTab'ı import edin:
   ```typescript
   import PredictionTab from '@/components/tabs/PredictionTab';
   ```
3. Tab listesine ekleyin:
   ```typescript
   {
     id: 'prediction',
     label: 'AI Prediction',
     icon: Brain,
     component: PredictionTab
   }
   ```

---

## 📊 Özellikler

| Özellik | Basit Demo | Tam Versiyon |
|---------|-----------|--------------|
| Fiyat Tahmini | ✅ Linear Regression + ES + MA | ✅ ARIMA + XGBoost + LSTM + RF |
| Confidence Intervals | ✅ ±5% | ✅ Model-based |
| Volatilite | ✅ Historical | ✅ GARCH forecast |
| Backtesting | ❌ | ✅ Walk-forward |
| Trading Signals | ✅ Basic | ✅ Advanced |
| Dependencies | ❌ None | ✅ Required |

---

## ⚠️ Troubleshooting

### Import Hatası

```
ModuleNotFoundError: No module named 'numpy'
```

**Çözüm**: Dependencies kurun:
```bash
./install_dependencies.sh
```

### TensorFlow Hatası (M1/M2 Mac)

```bash
# Apple Silicon için:
pip install tensorflow-macos
pip install tensorflow-metal
```

### Memory Hatası (LSTM)

LSTM eğitimi çok fazla RAM kullanıyorsa:
```python
# Daha küçük model:
fit_lstm(data, lookback=30, units=32, epochs=20)
```

---

## 📈 Performans Beklentileri

| Model | Eğitim Süresi | Tahmin Süresi | Accuracy |
|-------|---------------|---------------|----------|
| Simple Demo | < 1s | < 0.1s | ⭐⭐⭐ |
| ARIMA | 5-10s | < 1s | ⭐⭐⭐⭐ |
| XGBoost | 10-20s | < 1s | ⭐⭐⭐⭐ |
| LSTM | 1-3 min | < 1s | ⭐⭐⭐⭐ |
| Ensemble | 1-3 min | < 2s | ⭐⭐⭐⭐⭐ |

---

## 🎓 Sonraki Adımlar

1. ✅ Test suite'i çalıştırın
2. ✅ Dependencies kurun
3. ✅ Basit örnekleri deneyin
4. ✅ Kendi stratejilerinizi yazın
5. ✅ Prediction Tab'ı entegre edin

---

## 📚 Daha Fazla Bilgi

- **Detaylı Dokümantasyon**: [README.md](README.md)
- **API Referansı**: Her Python dosyasının başında docstring
- **Frontend Servisi**: `src/services/predictionService.ts`
- **Prediction Tab**: `src/components/tabs/PredictionTab.tsx`

---

## 🆘 Yardım

Sorun mu yaşıyorsunuz?

1. Test suite çalıştırın: `python3 test_predictions.py`
2. README.md dosyasını okuyun
3. GitHub Issues açın

---

**⚡ Pro Tip**: Basit demo ile başlayın, sonra ihtiyaç duydukça dependencies kurun!

```bash
# Hızlı test:
python3 test_predictions.py

# Basit tahmin:
echo '{"prices": [100,102,105,108,110], "steps_ahead": 5}' | python3 simple_prediction_demo.py

# Tam kurulum:
./install_dependencies.sh
```

🎉 **Başarılar!**
