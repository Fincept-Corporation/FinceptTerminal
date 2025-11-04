# 🇹🇷 Türk Piyasaları ve Kripto Para Desteği

## 📊 Genel Bakış

FinceptTerminal artık **Türkiye borsası (BIST)** ve **kripto para piyasaları** için tam destek sunuyor! AI prediction sistemi ile BIST hisseleri ve kripto paralar için gelişmiş tahminler yapabilirsiniz.

---

## 🎯 Özellikler

### BIST (Borsa Istanbul) 🇹🇷

#### ✅ Veri Kaynakları
- **BIST 100** endeks verileri
- **30+ popüler hisse**: Akbank, Garanti, THY, Aselsan, Ereğli, vb.
- Gerçek zamanlı fiyat ve hacim bilgileri
- Geçmiş fiyat verileri (1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y)

#### ✅ AI Tahmin Özellikleri
- Multi-model ensemble tahmin (ARIMA + XGBoost + LSTM + Random Forest)
- 30-90 günlük fiyat tahminleri
- Otomatik alım/satım sinyalleri
- Risk metrikleri ve volatilite analizi
- BIST 100 hisse önerileri

#### ✅ Piyasa Özeti
- BIST 100 endeks performansı
- En çok yükselenler (Top Gainers)
- En çok düşenler (Top Losers)
- Günlük değişim yüzdeleri

---

### Kripto Paralar ₿

#### ✅ Veri Kaynakları
- **Binance API**: Gerçek zamanlı fiyat ve hacim
- **CoinGecko API**: Detaylı coin bilgileri
- **Global Market Data**: Total market cap, dominance, vb.

#### ✅ Desteklenen Kripto Paralar
- **Major Coins**: BTC, ETH, USDT, BNB, XRP, ADA, SOL, DOGE, TRX, MATIC
- **DeFi**: UNI, LINK, AVAX, AAVE
- **Türk Projeleri**: CHZ (Chiliz)

#### ✅ Fiyat Çiftleri
- **USDT**: Dolar bazlı işlem
- **TRY**: Türk Lirası bazlı işlem ₺
- **BUSD**: Binance USD

#### ✅ AI Tahmin Özellikleri
- Multi-model kripto fiyat tahmini
- Volatilite analizi (kripto piyasalar için optimize edilmiş)
- Kripto momentum sinyalleri
- Fear & Greed Index entegrasyonu
- Top 10 kripto önerileri

---

## 🚀 Kullanım

### 1️⃣ BIST Hisse Analizi

#### TypeScript (Frontend)
```typescript
import bistService from '@/services/bistService';
import predictionService from '@/services/predictionService';

// Hisse bilgisi al
const akbankInfo = await bistService.getStockInfo('AKBNK.IS');
console.log(akbankInfo.current_price);

// Geçmiş veri
const hist = await bistService.getHistoricalData('GARAN.IS', '1y', '1d');

// BIST 100 özeti
const bist100 = await bistService.getBIST100Summary();
console.log(`BIST 100: ${bist100.current} (${bist100.change_percent}%)`);

// AI Tahmin
const prediction = await predictionService.predictBISTStock('THYAO.IS', 30, true);
console.log(prediction.signals[0].type); // BUY/SELL/HOLD

// En iyi öneriler
const recommendations = await predictionService.getBISTRecommendations(null, 10);
console.log(recommendations.recommendations);
```

#### Python (Backend)
```bash
# BIST hisse bilgisi
echo '{"action": "stock_info", "symbol": "AKBNK.IS"}' | python3 bist_data.py

# Geçmiş veri
echo '{"action": "historical", "symbol": "ASELS.IS", "period": "1y"}' | python3 bist_data.py

# BIST 100
echo '{"action": "bist_100"}' | python3 bist_data.py

# Top gainers
echo '{"action": "top_gainers", "limit": 10}' | python3 bist_data.py

# AI Tahmin
echo '{"market": "bist", "action": "predict", "symbol": "EREGL.IS", "days_ahead": 30}' | python3 turkish_market_predictor.py
```

---

### 2️⃣ Kripto Para Analizi

#### TypeScript (Frontend)
```typescript
import cryptoService from '@/services/cryptoService';
import predictionService from '@/services/predictionService';

// Binance fiyat (USDT)
const btcPrice = await cryptoService.getBinancePrice('BTC', 'USDT');
console.log(`BTC: $${btcPrice.price}`);

// Türk Lirası fiyat
const btcTRY = await cryptoService.getBinanceTRYPrice('BTC');
console.log(`BTC: ₺${btcTRY.price}`);

// Geçmiş veri
const histData = await cryptoService.getHistoricalData('ETH', '1d', 365, 'USDT');

// CoinGecko detaylı bilgi
const bitcoinInfo = await cryptoService.getCoinGeckoInfo('bitcoin');
console.log(`Market Cap Rank: ${bitcoinInfo.market_cap_rank}`);

// Global market data
const globalData = await cryptoService.getGlobalMarketData();
console.log(`BTC Dominance: ${globalData.bitcoin_dominance}%`);

// Fear & Greed Index
const fearGreed = await cryptoService.getFearGreedIndex();
console.log(`Fear & Greed: ${fearGreed.value} (${fearGreed.classification})`);

// AI Tahmin
const prediction = await predictionService.predictCrypto('BTC', 30, 'USDT', true);
console.log(prediction.signals[0]);

// Kripto önerileri
const recommendations = await predictionService.getCryptoRecommendations(null, 'USDT', 10);
```

#### Python (Backend)
```bash
# Binance fiyat
echo '{"action": "binance_price", "symbol": "BTC", "quote": "USDT"}' | python3 crypto_data.py

# TRY fiyat
echo '{"action": "binance_try", "symbol": "ETH"}' | python3 crypto_data.py

# Geçmiş veri
echo '{"action": "historical", "symbol": "BTC", "interval": "1d", "limit": 365}' | python3 crypto_data.py

# Top 100 kripto
echo '{"action": "top_coins", "limit": 100, "currency": "usd"}' | python3 crypto_data.py

# Trending coinler
echo '{"action": "trending"}' | python3 crypto_data.py

# AI Tahmin
echo '{"market": "crypto", "action": "predict", "symbol": "ETH", "quote": "USDT", "days_ahead": 30}' | python3 turkish_market_predictor.py
```

---

## 🎨 Frontend Kullanımı

### Prediction Tab

Prediction Tab'da artık 3 piyasa seçeneği var:

1. **🇺🇸 US Stocks**: Amerikan hisse senetleri (AAPL, GOOGL, MSFT, vb.)
2. **🇹🇷 BIST (Türkiye)**: Borsa Istanbul hisseleri
3. **₿ Cryptocurrency**: Kripto paralar

#### Örnek Kullanım:

1. **Market** seçin: US / BIST / Crypto
2. **Symbol** girin:
   - BIST: `AKBNK.IS`, `GARAN.IS`, `THYAO.IS`
   - Crypto: `BTC`, `ETH`, `BNB`
3. Kripto için **Quote Currency** seçin: USDT / TRY / BUSD
4. **Forecast Horizon** belirleyin (1-90 gün)
5. **Run Prediction** butonuna basın

**Sonuç**:
- Price forecast grafiği
- BUY/SELL/HOLD sinyali
- Expected return tahmini
- Model ensemble ağırlıkları
- Confidence intervals

---

## 📚 Popüler BIST Hisseleri

| Sembol | Şirket | Sektör |
|--------|--------|--------|
| AKBNK.IS | Akbank | Bankacılık |
| GARAN.IS | Garanti BBVA | Bankacılık |
| THYAO.IS | Türk Hava Yolları | Ulaştırma |
| ASELS.IS | Aselsan | Savunma/Teknoloji |
| EREGL.IS | Ereğli Demir Çelik | Metal |
| KCHOL.IS | Koç Holding | Holding |
| SAHOL.IS | Sabancı Holding | Holding |
| BIMAS.IS | BİM | Perakende |
| TOASO.IS | Tofaş | Otomotiv |
| PETKM.IS | Petkim | Kimya |

---

## 🔐 API Anahtarları (Opsiyonel)

### CoinGecko API
Ücretsiz, API key gerektirmez. Rate limit: 50 çağrı/dakika

### Binance API
Ücretsiz, API key gerektirmez (public endpoints)

### Premium Özellikler
Daha yüksek rate limit için API key alabilirsiniz:
- CoinGecko Pro: https://www.coingecko.com/en/api/pricing
- Binance API: https://www.binance.com/en/my/settings/api-management

---

## ⚙️ Kurulum

### Python Dependencies

BIST ve Kripto için ek dependency gerekmez! `requests` kütüphanesi yeterli:

```bash
pip install requests
```

AI Prediction için (opsiyonel):
```bash
cd fincept-terminal-desktop/src-tauri/resources/scripts/Analytics/prediction
pip install -r requirements.txt
```

---

## 🎯 Örnek Senaryolar

### Senaryo 1: BIST Portföyüm için AI Önerileri

```typescript
// Top 5 BIST hisse önerisi al
const recommendations = await predictionService.getBISTRecommendations(null, 5);

recommendations.recommendations.forEach(rec => {
  console.log(`${rec.symbol}: ${rec.signal} - Expected Return: ${rec.expected_return}%`);
});
```

### Senaryo 2: Kripto Fear & Greed ile Strateji

```typescript
const fearGreed = await cryptoService.getFearGreedIndex();

if (fearGreed.value < 25) {
  // Extreme Fear - Buying opportunity
  const btcPrediction = await predictionService.predictCrypto('BTC', 30, 'USDT');
  console.log('Extreme Fear detected! BTC Prediction:', btcPrediction.signals[0]);
}
```

### Senaryo 3: BIST 100 ve Kripto Market Karşılaştırma

```typescript
const bist100 = await bistService.getBIST100Summary();
const globalCrypto = await cryptoService.getGlobalMarketData();

console.log(`BIST 100: ${bist100.change_percent}%`);
console.log(`Crypto Market: ${globalCrypto.market_cap_change_24h}%`);
```

---

## 🔬 AI Prediction Detayları

### BIST için Özel Optimizasyonlar
- Türkiye piyasa saatlerine göre veri işleme
- BIST volatilitesine uygun model parametreleri
- Türk Lirası bazlı analiz

### Kripto için Özel Optimizasyonlar
- 24/7 piyasa desteği
- Yüksek volatilite için GARCH modeli optimizasyonu
- Multiple exchange data aggregation
- Sentiment analysis (Fear & Greed Index)

---

## 📈 Performans Beklentileri

| Piyasa | Veri Kaynağı | Gecikme | Tahmin Doğruluğu |
|--------|-------------|---------|------------------|
| BIST | Yahoo Finance | ~5s | ⭐⭐⭐⭐ |
| Crypto (USDT) | Binance | ~1s | ⭐⭐⭐⭐⭐ |
| Crypto (TRY) | Binance | ~1s | ⭐⭐⭐⭐ |
| Global Data | CoinGecko | ~3s | ⭐⭐⭐⭐⭐ |

---

## ⚠️ Yasal Uyarı

Bu tahminler yatırım tavsiyesi değildir. BIST ve kripto piyasalarında işlem yapmadan önce:
- Risk yönetimi kullanın
- Kendi araştırmanızı yapın
- Yalnızca kaybetmeyi göze alabileceğiniz parayla işlem yapın
- Gerekirse profesyonel finansal danışmanlık alın

---

## 🆘 Sorun Giderme

### BIST verileri gelmiyor
```bash
# Test edin:
echo '{"action": "stock_info", "symbol": "AKBNK.IS"}' | python3 scripts/DataSources/bist_data.py

# Hata varsa requests kurun:
pip install requests
```

### Kripto verileri gelmiyor
```bash
# Test edin:
echo '{"action": "binance_price", "symbol": "BTC", "quote": "USDT"}' | python3 scripts/DataSources/crypto_data.py

# Rate limit hatası alırsanız birkaç saniye bekleyin
```

---

## 🎉 Yeni Özellikler

✅ BIST 100 endeks takibi
✅ 30+ BIST hissesi
✅ 15+ kripto para
✅ TRY (₺) bazlı kripto fiyatları
✅ AI prediction ile BIST tahminleri
✅ AI prediction ile kripto tahminleri
✅ Fear & Greed Index
✅ Global crypto market data
✅ Top gainers/losers (BIST)
✅ Trending coins

---

## 📞 Destek

Sorularınız için:
- GitHub Issues: https://github.com/Fincept-Corporation/FinceptTerminal/issues
- Dokümantasyon: README.md
- Quick Start: QUICKSTART.md

---

**🇹🇷 Türk yatırımcılar için özel olarak geliştirildi!**

**₿ Kripto meraklıları için kapsamlı analiz araçları!**

🚀 **Başarılar!**
